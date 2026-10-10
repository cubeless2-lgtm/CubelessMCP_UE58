"""Exercise no-save compile wrappers with Always/SuccessOnly in an isolated editor.

Uses the official MCP endpoint and named tools. Changes the owned test editor's
Blueprint setting in memory through its public property API, then restores it.
No settings file save, mutation retries, process control or user assets.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import operation_result

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / 'cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject'
NATIVE = 'CubelessMCPUE58Editor.CubelessNativeToolset'
BP = 'cubeless_mcp_ue58.registration.toolsets.CubelessBlueprintTools'
GRAPH = 'cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools'
ASSETS = 'editor_toolset.toolsets.asset.AssetTools'
OBJECTS = 'editor_toolset.toolsets.object.ObjectTools'
SETTINGS = '/Script/BlueprintGraph.Default__BlueprintEditorSettings'

def fingerprint(path):
    return {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'mtime_ns': path.stat().st_mtime_ns, 'bytes': path.stat().st_size}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint', required=True); parser.add_argument('--expected-pid', type=int, required=True)
    parser.add_argument('--expected-dll-sha256', required=True); parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(); assert not args.output.exists()
    run = uuid.uuid4().hex; folder = '/Game/CubelessMCPTests/CompileAutoSave_' + run
    package = folder + '/BP_CompileAutoSave'; disk = PROJECT.parent / ('Content/' + package[6:] + '.uasset')
    report = {'schema': 'cubeless_compile_autosave_probe_v1', 'status': 'failed', 'run_id': run,
              'project_file': str(PROJECT), 'package': package, 'stages': [], 'cases': [],
              'automatic_retries': 0, 'outcome_unknown': False, 'settings_restored': False,
              'scope': 'Three compile wrappers, two successful compile autosave policies, one isolated Actor BP; compile-error autosave and full legacy parity not verified.'}
    c = OfficialMCPClient(args.endpoint); original = None
    config = PROJECT.parent / 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini'
    report['project_user_config_before'] = fingerprint(config) if config.is_file() else None
    def persist(): args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    def call(label, toolset, tool, arguments, mutation=False):
        row = {'name': label, 'toolset': toolset, 'tool': tool, 'arguments': arguments, 'mutation': mutation}
        report['stages'].append(row); persist()
        try:
            row['raw'] = c.call_toolset(toolset, tool, arguments, mutation=mutation)
            row['result'] = operation_result(row['raw']) if toolset.startswith(('Cubeless', 'cubeless')) else tool_payload(row['raw'])
            return row['result']
        except Exception:
            if mutation: report['outcome_unknown'] = True
            raise
        finally: persist()
    def setting(label):
        return json.loads(call(label, OBJECTS, 'get_properties', {'instance': {'refPath': SETTINGS}, 'properties': ['SaveOnCompile']}))['SaveOnCompile']
    def set_setting(value):
        assert call('set_setting_' + value, OBJECTS, 'set_properties', {'instance': {'refPath': SETTINGS}, 'values': json.dumps({'SaveOnCompile': value})}, True) is True
        assert setting('verify_setting_' + value) == value
    def compiled(value, save):
        assert value['success'] is True and value['compile']['attempted'] is True, value
        assert value['save']['requested'] is save and value['save']['saved'] is save, value
        return value
    try:
        persist(); c.initialize()
        report['schemas'] = {n: tool_payload(c.call('describe_toolset', {'toolset_name': n})) for n in [NATIVE,BP,GRAPH,ASSETS,OBJECTS]}
        diag = call('preflight', NATIVE, 'GetEditorDiagnostics', {})
        assert Path(diag['project_file']).resolve() == PROJECT.resolve() and diag['process_id'] == args.expected_pid
        assert diag['dirty_packages'] == [] and diag['pie_or_sie_active'] is False and diag['game_thread'] is True
        dll = PROJECT.parent / 'Plugins/CubelessMCP_UE58/Binaries/Win64/UnrealEditor-CubelessMCPUE58Editor.dll'
        assert fingerprint(dll)['sha256'] == args.expected_dll_sha256
        report.update(editor=diag, plugin_dll=fingerprint(dll), source_sha256=fingerprint(Path(__file__))['sha256'])
        original = setting('original_setting'); report['original_setting'] = original
        assert original in ['SoC_Never', 'SoC_Always', 'SoC_SuccessOnly']
        set_setting('SoC_Never')
        assert not disk.exists() and call('absent', ASSETS, 'exists', {'path': package}) is False
        created = call('create_fixture', BP, 'create_blueprint', {'name': 'BP_CompileAutoSave', 'parent_class': 'Actor', 'package_path': folder, 'operation_id': run + '_create'}, True)
        assert created['success'] is True
        compiled(call('baseline_save', BP, 'compile_and_save_blueprint', {'blueprint_name': package, 'operation_id': run + '_baseline'}, True), True)
        report['baseline_file'] = fingerprint(disk)
        for mode in ['SoC_Always', 'SoC_SuccessOnly']:
            set_setting(mode)
            for index, (tool, extra) in enumerate([
                    ('compile_blueprint', {}), ('compile_and_save_blueprint', {'save': False}),
                    ('compile_and_validate_blueprint', {})]):
                key = mode + '_' + str(index)
                case = {'mode': mode, 'tool': tool, 'before_file': fingerprint(disk)}
                report['cases'].append(case); persist()
                added = call(key + '_add', GRAPH, 'add_branch_node', {'blueprint_name': package, 'x': index * 200, 'y': 100 if mode == 'SoC_Always' else 500, 'operation_id': run + key + '_add'}, True)
                assert added['success'] is True
                case['node_id'] = added['data']['node_id']
                assert call(key + '_dirty_before', ASSETS, 'is_dirty', {'asset_path': package}) is True
                assert fingerprint(disk) == case['before_file'], 'Graph edit unexpectedly saved'
                result = compiled(call(key + '_compile_no_save', BP, tool, {'blueprint_name': package, 'operation_id': run + key + '_compile', **extra}, True), False)
                case['compile_result'] = result
                case['after_no_save_file'] = fingerprint(disk)
                assert case['after_no_save_file'] == case['before_file'], 'No-save compiler wrote the package'
                assert call(key + '_dirty_after', ASSETS, 'is_dirty', {'asset_path': package}) is True
                assert setting(key + '_setting_unchanged') == mode
                # A distinct explicit save proves the changed package can still be saved.
                compiled(call(key + '_explicit_save', BP, 'compile_and_save_blueprint', {'blueprint_name': package, 'operation_id': run + key + '_save'}, True), True)
                assert call(key + '_clean_after_save', ASSETS, 'is_dirty', {'asset_path': package}) is False
                case['after_save_file'] = fingerprint(disk)
                assert case['after_save_file']['sha256'] != case['before_file']['sha256']
                case['passed'] = True; persist()
        set_setting(original); report['settings_restored'] = True
        final = call('final_clean', NATIVE, 'GetEditorDiagnostics', {})
        assert final['dirty_packages'] == [] and final['instance_id'] == diag['instance_id']
        report.update(status='pass', asset_file=fingerprint(disk))
    except Exception as error: report['error'] = str(error)
    finally:
        if original is not None and not report['settings_restored']:
            # A distinct settings restoration, never a retry of any failed asset write.
            report['settings_restoration_attempted'] = True
            try:
                set_setting(original); report['settings_restored'] = True
            except Exception as error:
                report['settings_restore_error'] = str(error); report['status'] = 'failed'
        report['project_user_config_after'] = fingerprint(config) if config.is_file() else None
        if report['project_user_config_before'] != report['project_user_config_after']:
            report['status'] = 'failed'; report['settings_file_changed'] = True
        report['finished_ns'] = time.time_ns(); report['http_observations'] = c.observations; persist(); c.close()
    print(json.dumps({'status': report['status'], 'cases': len(report['cases']), 'stages': len(report['stages']), 'error': report.get('error'), 'outcome_unknown': report['outcome_unknown']}))
    return 0 if report['status'] == 'pass' else 1

if __name__ == '__main__': raise SystemExit(main())
