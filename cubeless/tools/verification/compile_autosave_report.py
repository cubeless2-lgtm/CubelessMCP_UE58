"""Independently decode autosave probe evidence; optionally verify cold graph values."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import operation_result
from verification.compile_autosave_probe import PROJECT, NATIVE, BP, GRAPH, ASSETS, OBJECTS, SETTINGS, fingerprint

def require(condition, message):
    if not condition: raise ValueError(message)


def expected_calls(report):
    """Independent request contract; labels may repeat only at restoration."""
    run = report['run_id']; package = report['package']; rows = []
    def add(name, ts, tool, args, mutation=False):
        rows.append({'name': name, 'toolset': ts, 'tool': tool, 'arguments': args, 'mutation': mutation})
    def get_setting(name): add(name, OBJECTS, 'get_properties', {'instance': {'refPath': SETTINGS}, 'properties': ['SaveOnCompile']})
    def set_setting(mode):
        add('set_setting_' + mode, OBJECTS, 'set_properties', {'instance': {'refPath': SETTINGS}, 'values': json.dumps({'SaveOnCompile': mode})}, True)
        get_setting('verify_setting_' + mode)
    add('preflight', NATIVE, 'GetEditorDiagnostics', {})
    get_setting('original_setting'); set_setting('SoC_Never')
    add('absent', ASSETS, 'exists', {'path': package})
    add('create_fixture', BP, 'create_blueprint', {'name': 'BP_CompileAutoSave', 'parent_class': 'Actor', 'package_path': package.rsplit('/', 1)[0], 'operation_id': run + '_create'}, True)
    add('baseline_save', BP, 'compile_and_save_blueprint', {'blueprint_name': package, 'operation_id': run + '_baseline'}, True)
    for mode in ('SoC_Always', 'SoC_SuccessOnly'):
        set_setting(mode)
        for index, tool in enumerate(('compile_blueprint', 'compile_and_save_blueprint', 'compile_and_validate_blueprint')):
            key = mode + '_' + str(index)
            add(key + '_add', GRAPH, 'add_branch_node', {'blueprint_name': package, 'x': index * 200, 'y': 100 if mode == 'SoC_Always' else 500, 'operation_id': run + key + '_add'}, True)
            add(key + '_dirty_before', ASSETS, 'is_dirty', {'asset_path': package})
            args = {'blueprint_name': package, 'operation_id': run + key + '_compile'}
            if tool == 'compile_and_save_blueprint': args['save'] = False
            add(key + '_compile_no_save', BP, tool, args, True)
            add(key + '_dirty_after', ASSETS, 'is_dirty', {'asset_path': package})
            get_setting(key + '_setting_unchanged')
            add(key + '_explicit_save', BP, 'compile_and_save_blueprint', {'blueprint_name': package, 'operation_id': run + key + '_save'}, True)
            add(key + '_clean_after_save', ASSETS, 'is_dirty', {'asset_path': package})
    set_setting(report['original_setting']); add('final_clean', NATIVE, 'GetEditorDiagnostics', {})
    return rows


def bind_contract(report):
    require(report.get('schema') == 'cubeless_compile_autosave_probe_v1', 'Wrong probe schema')
    run = report['run_id']; require(isinstance(run, str) and re.fullmatch(r'[a-f0-9]{32}', run), 'Invalid run UUID')
    require(report['package'] == '/Game/CubelessMCPTests/CompileAutoSave_' + run + '/BP_CompileAutoSave', 'Package is not owned by exact run UUID')
    require(report['original_setting'] in ('SoC_Never', 'SoC_Always', 'SoC_SuccessOnly'), 'Invalid initial policy')
    require(report['outcome_unknown'] is False and type(report['automatic_retries']) is int and report['automatic_retries'] == 0 and report['settings_restored'] is True, 'Outcome/retry/restoration contract differs')
    require(Path(report['project_file']).resolve() == PROJECT.resolve(), 'Wrong pinned project')
    schemas = report['schemas']; require(set(schemas) == {NATIVE, BP, GRAPH, ASSETS, OBJECTS}, 'Missing/extra described toolset')
    catalog = {}
    for ts, description in schemas.items():
        require(description['name'] == ts, 'Described toolset FQN differs')
        tools = description['tools']; require(len({tool['name'] for tool in tools}) == len(tools), 'Duplicate described tool')
        catalog[ts] = {tool['name']: tool for tool in tools}
    planned = expected_calls(report); require(len(planned) == len(report['stages']) == 56, 'Incomplete call contract')
    ids = []
    for actual, expected in zip(report['stages'], planned):
        require({key: actual[key] for key in expected} == expected and type(actual['mutation']) is bool, 'Call target/arguments/mutation/order differs: ' + expected['name'])
        # bool compares equal to 0/1 in Python; exact JSON types are part of the contract.
        require(json.dumps(actual['arguments'], sort_keys=True) == json.dumps(expected['arguments'], sort_keys=True), 'Argument JSON types differ')
        tool = catalog[expected['toolset']].get(expected['toolset'] + '.' + expected['tool'])
        require(tool is not None and tool['inputSchema']['type'] == 'object', 'Named tool absent from recorded schema')
        schema = tool['inputSchema']; props = schema.get('properties', {})
        require(set(schema.get('required', [])) <= set(expected['arguments']) <= set(props), 'Call does not match described input fields')
        if 'operation_id' in expected['arguments']: ids.append(expected['arguments']['operation_id'])
        require(not actual['raw'].get('isError'), 'Raw official MCP error')
    require(len(ids) == len(set(ids)), 'Mutation operation ID reused')
    bp = catalog[BP]
    require(bp[BP + '.compile_and_save_blueprint']['inputSchema']['properties']['save'].get('default') is True, 'Explicit-save default changed')
    props = bp[BP + '.compile_and_validate_blueprint']['inputSchema']['properties']
    require(props['save'].get('default') is False and props['refresh_nodes'].get('default') is True, 'Validate wrapper defaults changed')
    for key in ('baseline_file', 'asset_file', 'plugin_dll'):
        value = report[key]
        require(re.fullmatch(r'[a-f0-9]{64}', value['sha256']) and type(value['mtime_ns']) is int and value['mtime_ns'] > 0 and type(value['bytes']) is int and value['bytes'] > 0, 'Invalid file fingerprint: ' + key)
    probe = Path(__file__).with_name('compile_autosave_probe.py')
    require(fingerprint(probe)['sha256'] == report['source_sha256'], 'Frozen probe source differs')


def compile_contract(result, package, *, save, refresh):
    require(result['stage'] == 'complete' and result['error_code'] == result['error'] == '', 'Compile did not complete cleanly')
    compiled = result['compile']; persisted = result['save']; data = result['data']
    require(compiled['requested'] is True and compiled['attempted'] is True and compiled['compiled'] is True and compiled['error_count'] == 0, 'Compilation failed/not attempted')
    require(compiled['status'] in ('up_to_date', 'up_to_date_with_warnings') and compiled['refreshed_nodes'] is refresh and data['validation_pass'] is True, 'Compile validation/status contradicts success')
    require(all(persisted[key] is save for key in ('requested', 'attempted', 'saved')), 'Save contract differs')
    require(data['blueprint_path'] == package + '.BP_CompileAutoSave' and data['after']['path'] == package and data['after']['dirty'] is (not save), 'Compile asset/dirty readback differs')
    require(data['compiled'] is True and data['error_count'] == 0 and data['saved'] is save and data['save_requested'] is save and data['refreshed_nodes'] is refresh, 'Native outcome contradicts wrapper')
    require(result['retry']['automatic_retry'] is False and result['retry']['replayed'] is False and result['retry']['atomic_rollback'] is False, 'Compile was retried/replayed')

def verify(report):
    errors = []
    try:
        bind_contract(report)
        require(report['status'] == 'pass' and not report['outcome_unknown'] and report['automatic_retries'] == 0, 'Probe not conclusively passed')
        require(Path(report['project_file']).resolve() == PROJECT.resolve(), 'Wrong project')
        require(report['settings_restored'] and report['project_user_config_before'] == report['project_user_config_after'], 'Settings not restored or config changed')
        require(len(report['cases']) == 6 and len(report['stages']) == 56, 'Incomplete cases/stages')
        names = [r['name'] for r in report['stages']]
        expected_names = ['preflight', 'original_setting', 'set_setting_SoC_Never', 'verify_setting_SoC_Never', 'absent', 'create_fixture', 'baseline_save']
        for mode in ['SoC_Always', 'SoC_SuccessOnly']:
            expected_names.extend(['set_setting_' + mode, 'verify_setting_' + mode])
            for index in range(3):
                key = mode + '_' + str(index)
                expected_names.extend(key + suffix for suffix in ['_add', '_dirty_before', '_compile_no_save', '_dirty_after', '_setting_unchanged', '_explicit_save', '_clean_after_save'])
        expected_names.extend(['set_setting_' + report['original_setting'], 'verify_setting_' + report['original_setting'], 'final_clean'])
        require(names == expected_names, 'Stage sequence differs; repeated initial/restored settings labels are intentional')
        stages = {r['name']: r for r in report['stages']}
        for row in report['stages']:
            value = operation_result(row['raw']) if row['toolset'].startswith(('Cubeless', 'cubeless')) else tool_payload(row['raw'])
            require(value == row['result'], 'Raw response disagrees: ' + row['name'])
            if isinstance(value, dict) and row['toolset'].startswith(('Cubeless', 'cubeless')):
                require(value['success'] is True, 'Tool failure in passing report')
                require(value.get('operation_id', row['arguments'].get('operation_id')) == row['arguments'].get('operation_id'), 'Result operation ID differs')
                if 'retry' in value: require(value['retry']['replayed'] is False and value['retry']['automatic_retry'] is False, 'Mutation replay/retry in fixture creation')
            if row['name'].startswith('set_setting_'): require(value is True, 'Setting write failed')
            if row['name'].startswith('verify_setting_'): require(json.loads(value)['SaveOnCompile'] == row['name'].removeprefix('verify_setting_'), 'Setting write/read mismatch')
        require(stages['preflight']['result'] == report['editor'] and report['editor']['dirty_packages'] == [], 'Preflight mismatch')
        require(Path(report['editor']['project_file']).resolve() == PROJECT.resolve() and report['editor']['engine_version'].startswith('5.8.3-') and report['editor']['game_thread'] is True and report['editor']['pie_or_sie_active'] is False and type(report['editor']['process_id']) is int and report['editor']['process_id'] > 0 and bool(report['editor']['instance_id']), 'Wrong editor identity/execution mode')
        require(stages['final_clean']['result']['dirty_packages'] == [] and stages['final_clean']['result']['instance_id'] == report['editor']['instance_id'], 'Final dirty/editor mismatch')
        require(json.loads(stages['original_setting']['result'])['SaveOnCompile'] == report['original_setting'], 'Initial setting mismatch')
        require(json.loads(stages['verify_setting_' + report['original_setting']]['result'])['SaveOnCompile'] == report['original_setting'], 'Restoration readback missing')
        require(stages['absent']['result'] is False, 'Fixture was not initially absent')
        compile_contract(stages['baseline_save']['result'], report['package'], save=True, refresh=False)
        modes = ['SoC_Always', 'SoC_SuccessOnly']
        tools = ['compile_blueprint', 'compile_and_save_blueprint', 'compile_and_validate_blueprint']
        last = report['baseline_file']
        for index, case in enumerate(report['cases']):
            mode, tool = modes[index // 3], tools[index % 3]; key = mode + '_' + str(index % 3)
            require(case['mode'] == mode and case['tool'] == tool and case['passed'], 'Wrong policy/wrapper case')
            require(case['before_file'] == last and case['after_no_save_file'] == last, 'No-save file bytes/time changed')
            require(stages[key + '_compile_no_save']['tool'] == tool, 'Wrong named wrapper')
            require(stages[key + '_compile_no_save']['result'] == case['compile_result'], 'Compile summary differs')
            result = case['compile_result']; save = result['save']; data = result['data']
            compile_contract(result, report['package'], save=False, refresh=tool == 'compile_and_validate_blueprint')
            require(save['requested'] is False and save['saved'] is False and save['attempted'] is False, 'No-save wrapper saved')
            require(result['compile']['compiled'] and result['compile']['error_count'] == 0 and result['compile']['refreshed_nodes'] == (tool == 'compile_and_validate_blueprint'), 'Compile/default refresh contract differs')
            require(data['package_dirty_before'] and data['package_dirty_after'] and data['blueprint_path'] == report['package'] + '.BP_CompileAutoSave', 'Compiler asset/dirty mismatch')
            require(stages[key + '_dirty_before']['result'] is True and stages[key + '_dirty_after']['result'] is True, 'Independent dirty check failed')
            require(json.loads(stages[key + '_setting_unchanged']['result'])['SaveOnCompile'] == mode, 'Requested autosave policy not active')
            require(stages[key + '_add']['result']['data']['node_id'] == case['node_id'], 'Added node identity mismatch')
            saved = stages[key + '_explicit_save']['result']
            compile_contract(saved, report['package'], save=True, refresh=False)
            require(saved['save']['requested'] and saved['save']['attempted'] and saved['save']['saved'] and stages[key + '_clean_after_save']['result'] is False, 'Explicit save not verified')
            require(case['after_save_file']['sha256'] != case['before_file']['sha256'], 'Explicit save persisted no changed bytes')
            last = case['after_save_file']
        require(len({case['node_id'] for case in report['cases']}) == 6, 'Branch identities duplicate')
        prefix = report['package'] + '.BP_CompileAutoSave:EventGraph.'
        require(all(case['node_id'].startswith(prefix + 'K2Node_IfThenElse_') for case in report['cases']), 'Branch escaped owned graph')
        require(last == report['asset_file'], 'Final file summary differs')
    except (ValueError, KeyError, TypeError, IndexError, AttributeError, OSError) as error: errors.append(str(error))
    return {'passed': not errors, 'errors': errors}

def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('report', type=Path); p.add_argument('--output', required=True, type=Path)
    p.add_argument('--cold-endpoint'); a = p.parse_args(); require(not a.output.exists(), 'Refusing overwrite')
    raw = a.report.read_bytes(); parent = json.loads(raw); result = verify(parent)
    result.update(schema='cubeless_compile_autosave_verification_v1', report_sha256=hashlib.sha256(raw).hexdigest(), cold_reload='not_run')
    result['verifier_revision'] = 'R3_exact_call_contract'
    result['verifier_sha256'] = fingerprint(Path(__file__))['sha256']
    if result['passed']:
        tamper = []
        for kind in ['write_time', 'save_flag', 'config', 'mode', 'raw', 'replay', 'target', 'missing_save_false', 'setting_arguments', 'toolset', 'mutation', 'uuid', 'schema', 'stage_order', 'schema_fqn', 'schema_default', 'result_operation_id']:
            damaged = copy.deepcopy(parent)
            if kind == 'write_time': damaged['cases'][0]['after_no_save_file']['mtime_ns'] += 1
            elif kind == 'save_flag': damaged['cases'][0]['compile_result']['save']['saved'] = True
            elif kind == 'config': damaged['project_user_config_after'] = None
            elif kind == 'mode': damaged['cases'][3]['mode'] = 'SoC_Never'
            elif kind == 'raw': damaged['stages'][0]['raw'] = {'content': [{'type': 'text', 'text': '{}'}]}
            elif kind == 'replay': damaged['automatic_retries'] = 1
            elif kind == 'target': damaged['stages'][11]['arguments']['blueprint_name'] = '/Game/UserAsset'
            elif kind == 'missing_save_false': del damaged['stages'][18]['arguments']['save']
            elif kind == 'setting_arguments': damaged['stages'][7]['arguments']['values'] = json.dumps({'SaveOnCompile': 'SoC_Never'})
            elif kind == 'toolset': damaged['stages'][11]['toolset'] = 'cubeless_other.CubelessBlueprintTools'
            elif kind == 'mutation': damaged['stages'][11]['mutation'] = False
            elif kind == 'uuid': damaged['run_id'] = '0' * 32
            elif kind == 'schema': damaged['schema'] = 'another_report'
            elif kind == 'stage_order': damaged['stages'][2], damaged['stages'][53] = damaged['stages'][53], damaged['stages'][2]; damaged['stages'][2]['arguments']['values'] = json.dumps({'SaveOnCompile':'SoC_Always'})
            elif kind == 'schema_fqn': damaged['schemas'][BP]['tools'][0]['name'] = 'another.Tool'
            elif kind == 'schema_default': next(t for t in damaged['schemas'][BP]['tools'] if t['name'].endswith('.compile_and_save_blueprint'))['inputSchema']['properties']['save']['default'] = False
            elif kind == 'result_operation_id': damaged['stages'][11]['result']['operation_id'] = 'replayed_old_request'
            rejected = not verify(damaged)['passed']; tamper.append({'kind': kind, 'rejected': rejected})
        result['tamper_checks'] = tamper; result['passed'] = all(x['rejected'] for x in tamper)
    if result['passed']:
        paths = {'asset_file': PROJECT.parent / ('Content/' + parent['package'][6:] + '.uasset'), 'project_user_config_after': PROJECT.parent / 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini', 'plugin_dll': PROJECT.parent / 'Plugins/CubelessMCP_UE58/Binaries/Win64/UnrealEditor-CubelessMCPUE58Editor.dll'}
        result['local_file_observations'] = {}
        for key, path in paths.items():
            observed = fingerprint(path) if path.is_file() else None
            result['local_file_observations'][key] = {'path':str(path),'observed':observed,'matches_probe':observed==parent[key]}
        if not result['local_file_observations']['asset_file']['matches_probe']:
            result['passed'] = False; result['errors'].append('Current final asset bytes/time differ from probe')
        result['local_file_scope'] = 'Final asset must still match. Current configuration/DLL differences are observations, not retrospective proof that they differed during the recorded run.'
    if a.cold_endpoint and result['passed']:
        c = OfficialMCPClient(a.cold_endpoint); result['cold_calls'] = []
        def call(ts, tool, args):
            raw = c.call_toolset(ts, tool, args, mutation=False)
            value = operation_result(raw) if ts.startswith(('Cubeless', 'cubeless')) else tool_payload(raw)
            result['cold_calls'].append({'toolset': ts, 'tool': tool, 'arguments': args, 'raw': raw, 'result': value}); return value
        try:
            c.initialize()
            before = call(NATIVE, 'GetEditorDiagnostics', {})
            require(before['success'] is True and before['engine_version'].startswith('5.8.3-') and before['game_thread'] is True and before['pie_or_sie_active'] is False and Path(before['project_file']).resolve() == PROJECT.resolve() and before['instance_id'] != parent['editor']['instance_id'] and before['process_id'] != parent['editor']['process_id'] and before['dirty_packages'] == [], 'Cold process mismatch')
            graph = call(GRAPH, 'inspect_graph', {'blueprint_name': parent['package']})
            require(graph['success'] is True and graph['data']['graph_path'] == parent['package'] + '.BP_CompileAutoSave:EventGraph', 'Cold graph identity differs')
            nodes = graph['data']['nodes']; branches = {n['node_id']: n for n in nodes if n['class'] == 'K2Node_IfThenElse'}
            require(len(branches) == 6 and set(branches) == {x['node_id'] for x in parent['cases']}, 'Cold branches differ')
            for index, case in enumerate(parent['cases']):
                require([branches[case['node_id']]['x'], branches[case['node_id']]['y']] == [(index % 3) * 200, 100 if index < 3 else 500], 'Cold position differs')
            require(call(ASSETS, 'is_dirty', {'asset_path': parent['package']}) is False, 'Cold asset dirty')
            require(json.loads(call(OBJECTS, 'get_properties', {'instance': {'refPath': SETTINGS}, 'properties': ['SaveOnCompile']}))['SaveOnCompile'] == parent['original_setting'], 'Cold settings differ')
            after = call(NATIVE, 'GetEditorDiagnostics', {})
            require(after['success'] is True and after['dirty_packages'] == [] and after['instance_id'] == before['instance_id'] and after['process_id'] == before['process_id'] and Path(after['project_file']).resolve() == PROJECT.resolve(), 'Cold final dirty/identity changed')
            disk = PROJECT.parent / ('Content/' + parent['package'][6:] + '.uasset')
            require(fingerprint(disk) == parent['asset_file'], 'Cold disk file changed')
            result['cold_reload'] = 'passed'
        except Exception as error:
            result['passed'] = False; result['errors'].append(str(error)); result['cold_reload'] = 'failed'
        finally: c.close()
    with a.output.open('x', encoding='utf-8') as f: json.dump(result, f, ensure_ascii=False, indent=2)
    print(json.dumps({k: result[k] for k in ['passed', 'errors', 'cold_reload']}))
    return 0 if result['passed'] else 1

if __name__ == '__main__': raise SystemExit(main())
