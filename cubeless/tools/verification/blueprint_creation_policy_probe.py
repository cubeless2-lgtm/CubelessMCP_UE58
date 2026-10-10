"""Official MCP policy regression; fresh isolated assets, no automatic retries."""
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
from verification.compile_autosave_probe import PROJECT, BP, NATIVE, OBJECTS, ASSETS, SETTINGS, fingerprint

BATCH = 'cubeless_mcp_ue58.registration.batch_toolset.CubelessBatchTools'

def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('--endpoint', required=True); p.add_argument('--expected-pid', required=True, type=int); p.add_argument('--output', required=True, type=Path)
    a = p.parse_args(); assert not a.output.exists(); run = uuid.uuid4().hex
    folder = '/Game/CubelessMCPTests/CreationPolicy_' + run
    report = {'schema': 'cubeless_creation_policy_probe_v1', 'status': 'failed', 'run_id': run, 'project_file': str(PROJECT), 'stages': [], 'cases': [], 'automatic_retries': 0, 'outcome_unknown': False, 'settings_restored': False, 'packaging': 'not_run'}
    c = OfficialMCPClient(a.endpoint); original = None
    config = PROJECT.parent / 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini'
    report['project_user_config_before'] = fingerprint(config) if config.exists() else None
    def persist(): a.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    def call(label, ts, tool, arguments, mutation=False):
        row = {'name': label, 'toolset': ts, 'tool': tool, 'arguments': arguments, 'mutation': mutation}
        report['stages'].append(row); persist()
        try:
            row['raw'] = c.call_toolset(ts, tool, arguments, mutation=mutation)
            row['result'] = operation_result(row['raw']) if ts.startswith(('Cubeless','cubeless')) else tool_payload(row['raw']); return row['result']
        except Exception:
            if mutation: report['outcome_unknown'] = True
            raise
        finally: persist()
    def setting(label): return json.loads(call(label, OBJECTS, 'get_properties', {'instance': {'refPath': SETTINGS}, 'properties': ['SaveOnCompile']}))['SaveOnCompile']
    def policy(value, label):
        assert call(label+'_set', OBJECTS, 'set_properties', {'instance': {'refPath': SETTINGS}, 'values': json.dumps({'SaveOnCompile': value})}, True) is True
        assert setting(label+'_read') == value
    try:
        persist(); c.initialize()
        report['schemas'] = {n: tool_payload(c.call('describe_toolset', {'toolset_name': n})) for n in [NATIVE,BP,ASSETS,OBJECTS,BATCH]}
        create_schema = next(t for t in report['schemas'][BP]['tools'] if t['name'].endswith('.create_blueprint'))
        assert 'Requires SaveOnCompile=Never' in create_schema['description'], 'Need the new guard loaded before any call'
        diag = call('preflight', NATIVE, 'GetEditorDiagnostics', {})
        assert Path(diag['project_file']).resolve() == PROJECT.resolve() and diag['process_id'] == a.expected_pid and diag['dirty_packages'] == [] and not diag['pie_or_sie_active']
        report['editor'] = diag; original = setting('original'); report['original_setting'] = original
        for mode in ['SoC_Always','SoC_SuccessOnly']:
            policy(mode, mode)
            for batched in [False,True]:
                name = 'BP_' + mode + ('Batch' if batched else 'Direct'); package = folder + '/' + name
                disk = PROJECT.parent / ('Content/' + package[6:] + '.uasset')
                case = {'mode': mode, 'batched': batched, 'package': package}; report['cases'].append(case)
                assert not disk.exists() and call(name+'_absent_before', ASSETS, 'exists', {'path': package}) is False
                params = {'name': name, 'parent_class': 'Actor', 'package_path': folder}
                if batched:
                    result = call(name+'_create', BATCH, 'execute_batch', {'commands_json': json.dumps([{'type':'CubelessBlueprintTools.create_blueprint','params':params}]), 'operation_id': run+name}, True)
                    child = result.get('data', {}).get('batch', {}).get('results', [{}])[0].get('response', {})
                else:
                    result = call(name+'_create', BP, 'create_blueprint', {**params,'operation_id': run+name}, True); child = result
                # Always inspect the attempted target before concluding an expected rejection.
                case['exists_after'] = call(name+'_absent_after', ASSETS, 'exists', {'path': package}); case['file_exists_after'] = disk.exists()
                assert child.get('error_code') == 'creation_autosave_policy_unsupported' and child.get('success') is False and child.get('changes') == [], child
                assert result['success'] is False and result['changes'] == [] and not case['exists_after'] and not case['file_exists_after']
                assert setting(name+'_policy_unchanged') == mode
                assert call(name+'_clean', NATIVE, 'GetEditorDiagnostics', {})['dirty_packages'] == []
                case['passed'] = True; persist()
        policy('SoC_Never','never')
        name = 'BP_Never'; package = folder+'/'+name; disk = PROJECT.parent/('Content/'+package[6:]+'.uasset')
        assert not disk.exists() and call('positive_absent', ASSETS,'exists',{'path':package}) is False
        created = call('positive_create', BP,'create_blueprint',{'name':name,'parent_class':'Actor','package_path':folder,'operation_id':run+'_positive'},True)
        assert created['success'] and created['data']['creation_policy']['save_on_compile']=='SoC_Never' and created['compile']['implicit_factory_compile']['attempted'] and not created['save']['saved']
        assert not disk.exists() and call('positive_dirty',ASSETS,'is_dirty',{'asset_path':package}) is True
        saved=call('positive_save',BP,'compile_and_save_blueprint',{'blueprint_name':package,'operation_id':run+'_save'},True)
        assert saved['success'] and saved['save']['saved'] and call('positive_clean',ASSETS,'is_dirty',{'asset_path':package}) is False
        report['positive_asset']={'package':package,'path':str(disk),'file':fingerprint(disk)}
        long_result=call('oversized_name',BP,'create_blueprint',{'name':'B'*1024,'parent_class':'Actor','package_path':folder,'operation_id':run+'_long'},True)
        assert not long_result['success'] and long_result['error_code']=='invalid_asset_name' and long_result['changes']==[]
        policy(original,'restore'); report['settings_restored']=True
        assert call('final_clean',NATIVE,'GetEditorDiagnostics',{})['dirty_packages']==[]
        report['status']='pass'
    except Exception as e: report['error']=str(e)
    finally:
        if original is not None and not report['settings_restored']:
            try: policy(original,'failure_restore'); report['settings_restored']=True
            except Exception as e: report['settings_restore_error']=str(e)
        report['project_user_config_after']=fingerprint(config) if config.exists() else None
        if report['project_user_config_after']!=report['project_user_config_before']:report.update(status='failed',settings_file_changed=True)
        report.update(finished_ns=time.time_ns(),http_observations=c.observations,source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest());persist();c.close()
    print(json.dumps({'status':report['status'],'cases':len(report['cases']),'stages':len(report['stages']),'error':report.get('error'),'outcome_unknown':report['outcome_unknown']}))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__':raise SystemExit(main())
