"""Blueprint manifest authoring over named official MCP tools, with no replay."""
import json
import math
import re
from pathlib import Path
from .production_session import Session, arguments, package
from .result_contracts import require

TOOLS={
 'bp':'cubeless_mcp_ue58.registration.toolsets.CubelessBlueprintTools',
 'graph':'cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools',
 'component':'cubeless_mcp_ue58.registration.component_toolset.CubelessComponentTools',
 'assets':'editor_toolset.toolsets.asset.AssetTools',
 'scene':'editor_toolset.toolsets.scene.SceneTools',
 'app':'EditorToolset.EditorAppToolset',
 'niagara_runtime':'cubeless_mcp_ue58.registration.niagara_runtime_inspection_toolset.CubelessNiagaraRuntimeInspectionTools',
}
NODE_METHODS={
 'event':'add_blueprint_event_node', 'custom_event':'add_blueprint_custom_event_node',
 'call':'add_blueprint_call_function_node','get':'add_blueprint_variable_get_node',
 'set':'add_blueprint_variable_set_node','return':'add_blueprint_return_node',
 'branch':'add_blueprint_branch_node','sequence':'add_blueprint_sequence_node',
}

def data(value):
    return value['data']

def node_id(value):
    d=data(value)
    require(type(d.get('node_id')) is str and d['node_id'], 'Missing actual node identity; inspect receipt')
    return d['node_id']

def graph_state(value):
    d=data(value)
    require(d['node_count']==len(d['nodes']), 'Truncated graph observation')
    nodes={n['node_guid'].replace('-','').lower():n for n in d['nodes']}
    aliases={n['node_id']:key for key,n in nodes.items()}
    edges=set()
    for n in nodes.values():
        for p in n['pins']:
            if p['direction']=='output':
                edges.update((aliases[n['node_id']],p['name'],aliases[l['node_id']],l['pin_name']) for l in p['links'])
    return nodes,edges

def compile_plan(s,c):
    require(set(c)<={'blueprint','mode','parent_class','save','components','variables','defaults','graphs','playback'},
            'Unknown manifest section; legacy unsupported sections are not silently dropped')
    bp=package(c['blueprint']); mode=c.get('mode','create')
    require(mode in ('create','edit') and type(c.get('save',False)) is bool, 'Explicit create/edit and boolean save required')
    actions=[]
    def add(label,group,method,values):
        s.schema(group,method,values); actions.append((label,group,method,values))
    if mode=='create':
        add('create','bp','create_blueprint',dict(name=bp.rsplit('/',1)[1],package_path=bp.rsplit('/',1)[0],parent_class=c.get('parent_class','/Script/Engine.Actor')))
    for i,row in enumerate(c.get('components',[])):
        add(f'component_{i}','component','add_component_to_blueprint',dict(blueprint_name=bp,compile_blueprint=False,**row))
    for i,row in enumerate(c.get('variables',[])):
        add(f'variable_{i}','bp','add_blueprint_variable',dict(blueprint_name=bp,**row))
    for i,row in enumerate(c.get('defaults',[])):
        add(f'default_{i}','bp','set_blueprint_variable_default',dict(blueprint_name=bp,compile=False,save=False,**row))
    seen_graphs=set()
    for g in c.get('graphs',[]):
        require(set(g)<={'name','type','create','nodes','parameters','links'},'Unknown graph section')
        name=g['name'];require(name not in seen_graphs,'Duplicate graph request');seen_graphs.add(name)
        selector=dict(blueprint_name=bp,graph_name=name,graph_type=g.get('type','event'))
        add('resolve_'+name,'graph','resolve_blueprint_graph',dict(**selector,create_if_missing=g.get('create',False)))
        for i,p in enumerate(g.get('parameters',[])):
            add(f'parameter_{name}_{i}','graph','add_blueprint_function_parameter',dict(**selector,**p))
        ids=set()
        for i,n in enumerate(g.get('nodes',[])):
            require(set(n)<={'id','kind','arguments','existing_node_id'},'Unknown node section')
            require(re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*',n['id']) and n['id'] not in ids,'Unique logical node IDs required');ids.add(n['id'])
            if 'existing_node_id' in n:
                require(mode=='edit' and set(n)=={'id','existing_node_id'},'Existing node reference must be explicit, edit-only');continue
            method=NODE_METHODS.get(n['kind']);require(method is not None,'Unimplemented node kind; use a supported domain node, not arbitrary execution')
            add(f'node_{name}_{n["id"]}','graph',method,dict(**selector,**n.get('arguments',{})))
        for l in g.get('links',[]):
            require(len(l)==4 and l[0] in ids and l[2] in ids,'Links reference logical node IDs and exact pin names')
            s.schema('graph','connect_graph_pins',dict(blueprint_name=bp,graph_name=name,source_node_id=l[0],source_pin_name=l[1],target_node_id=l[2],target_pin_name=l[3]))
    add('compile_save','bp','compile_and_save_blueprint',dict(blueprint_name=bp,save=c.get('save',False)))
    if c.get('playback'):
        validate_playback(c['playback'])
        require(c.get('save',False),'Playback requires an explicitly saved compiled Blueprint')
        for check in c['playback'].get('niagara_checks',[]):
            s.schema('niagara_runtime','inspect_niagara_component_runtime_state',runtime_check_arguments(c['playback'],check))
    return actions

def run(s,c):
    bp=package(c['blueprint']);actions=compile_plan(s,c)
    exists=s.call('target_exists','assets','exists',{'path':bp})
    require(exists is (c.get('mode','create')=='edit'),'Target existence disagrees with create/edit; no overwrite or replay')
    s.checkpoint(planned_actions=actions,full_legacy_parity=False,PIE='not_run',reload='not_run')
    if not s.args.apply:return
    before={}
    if exists:
        require(s.call('target_clean','assets','is_dirty',dict(asset_path=bp)) is False,
                'Existing BP is dirty; save/inspect it explicitly before a new edit')
        before['variables']=s.call('variables_before','bp','inspect_blueprint_variables',dict(blueprint_name=bp))
        before['components']=s.call('components_before','component','list_blueprint_components',dict(blueprint_name=bp))
        before['graphs']={g['name']:s.call('graph_before_'+g['name'],'graph','inspect_graph',dict(blueprint_name=bp,graph_name=g['name'])) for g in c.get('graphs',[]) if not g.get('create',False)}
    for label,group,method,values in actions:
        if label=='compile_save' or group=='graph':continue
        s.call(label,group,method,values,mutation=True)
    for g in c.get('graphs',[]):
        name=g['name'];selector=dict(blueprint_name=bp,graph_name=name,graph_type=g.get('type','event'))
        s.call('resolve_'+name,'graph','resolve_blueprint_graph',dict(**selector,create_if_missing=g.get('create',False)),mutation=g.get('create',False))
        for i,p in enumerate(g.get('parameters',[])):
            s.call(f'parameter_{name}_{i}','graph','add_blueprint_function_parameter',dict(**selector,**p),mutation=True)
        ids={}
        current=s.call('graph_pre_nodes_'+name,'graph','inspect_graph',dict(blueprint_name=bp,graph_name=name))
        current_nodes,_=graph_state(current)
        for n in g.get('nodes',[]):
            if 'existing_node_id' in n:
                require(n['existing_node_id'].replace('-','').lower() in current_nodes,'Referenced node GUID not present');ids[n['id']]=n['existing_node_id']
            else:
                ids[n['id']]=node_id(s.call('node_'+name+'_'+n['id'],'graph',NODE_METHODS[n['kind']],dict(**selector,**n.get('arguments',{})),mutation=True))
        for i,(a,ap,b,bp_pin) in enumerate(g.get('links',[])):
            s.call(f'link_{name}_{i}','graph','connect_graph_pins',dict(blueprint_name=bp,graph_name=name,source_node_id=ids[a],source_pin_name=ap,target_node_id=ids[b],target_pin_name=bp_pin),mutation=True)
        s.report.setdefault('node_ids',{})[name]=ids
    final=s.call('compile_save','bp','compile_and_save_blueprint',dict(blueprint_name=bp,save=c.get('save',False)),mutation=True)
    require(final['compile'].get('compiled') is True,'Compile acceptance is not compile success')
    if c.get('save',False):require(final['save'].get('saved') is True,'Save did not complete')
    after={
      'variables':s.call('variables_after','bp','inspect_blueprint_variables',dict(blueprint_name=bp)),
      'components':s.call('components_after','component','list_blueprint_components',dict(blueprint_name=bp)),
      'graphs':{g['name']:s.call('graph_after_'+g['name'],'graph','inspect_graph',dict(blueprint_name=bp,graph_name=g['name'])) for g in c.get('graphs',[])}}
    def variables(receipt):return {v['name']:v for v in receipt['data']['after']['new_variables']}
    actual=variables(after['variables'])
    for row in c.get('variables',[])+c.get('defaults',[]):
        name=row['variable_name'];require(name in actual,'Requested variable missing after compile')
        if 'default_value_json' in row:
            expected=json.loads(row['default_value_json'])
            require(actual[name].get('default_value_available') is True,'Current compiled default is unavailable')
            require(actual[name].get('default_value')==expected,'Requested compiled default differs: '+name)
    if exists:
        touched={r['variable_name'] for r in c.get('defaults',[])}
        for name,row in variables(before['variables']).items():
            require(name in actual,'Existing variable lost')
            keys=['guid','pin_type','metadata','category','property_flags_string','is_transient']
            if name not in touched:keys+=['default_value']
            require(all(row.get(k)==actual[name].get(k) for k in keys),'Unrequested variable state changed: '+name)
        old_components={r['node_guid']:r for r in before['components']['data']['components']}
        new_components={r['node_guid']:r for r in after['components']['data']['components']}
        require(all(new_components.get(k)==v for k,v in old_components.items()),'Unrequested component identity/hierarchy/transform changed')
    for name,old in before.get('graphs',{}).items():
        oldnodes,oldedges=graph_state(old);newnodes,newedges=graph_state(after['graphs'][name])
        require(oldedges<=newedges,'Unrequested existing graph connection lost')
        for key,node in oldnodes.items():
            require(key in newnodes and all(node.get(k)==newnodes[key].get(k) for k in ('node_guid','class','x','y','variable_name')),'Unrequested existing node changed')
    for g in c.get('graphs',[]):
        nodes,edges=graph_state(after['graphs'][g['name']]);ids=s.report['node_ids'][g['name']]
        require(all((ids[a].replace('-','').lower(),ap,ids[b].replace('-','').lower(),bp_pin) in edges for a,ap,b,bp_pin in g.get('links',[])),'Requested edge missing after compile')
    s.checkpoint(before=before,after=after,compile=final['compile'],save=final['save'],
                 requested_graph_edges_verified=True,preservation_scope='Existing nodes and edges in touched graphs; native per-operation preservation receipts. Full arbitrary Blueprint field equality is not claimed.')
    if c.get('playback'):playback(s,bp,c['playback'])

def validate_playback(c):
    require(set(c)<={'world','actor_label','transform','warmup_seconds','engine_log_path','expected_log_marker','niagara_checks'},'Unknown playback field')
    package(c['world']);require(type(c['actor_label']) is str and c['actor_label'],'Playback label required')
    require(type(c.get('warmup_seconds',1.0)) in (int,float) and 0<=c.get('warmup_seconds',1.0)<=5,'Playback warmup must be 0..5 seconds')
    if 'expected_log_marker' in c:
        require(type(c['expected_log_marker']) is str and c['expected_log_marker'] and Path(c['engine_log_path']).is_file(),
                'Explicit existing local Editor log and nonempty marker required')
    checks=c.get('niagara_checks',[])
    require(type(checks) is list and len(checks)<=16,'At most 16 explicit Niagara component checks')
    for check in checks:
        require(type(check) is dict and set(check)<={'component','system','parameters','require_nondefault_override'},'Unknown Niagara runtime check field')
        require(type(check.get('require_nondefault_override',True)) is bool,'Boolean nondefault override policy required')
        require(type(check['component']) is str and check['component'],'Exact Niagara component name required')
        package(check['system'])
        parameters=check.get('parameters',[])
        require(type(parameters) is list and len(parameters)<=32,'At most 32 requested parameter values per component')
        for p in parameters:
            require(type(p) is dict and {'name','value'}<=set(p)<={'name','value','tolerance'},'Exact parameter name/value required')
            require(type(p['name']) is str and p['name'].startswith('User.'),'Canonical User. parameter name required')
            require(type(p.get('tolerance',1e-5)) in (int,float) and math.isfinite(p.get('tolerance',1e-5)) and p.get('tolerance',1e-5)>=0,'Finite nonnegative value tolerance required')

def runtime_check_arguments(playback,check):
    return dict(actor_label=playback['actor_label'],component_name=check['component'],system_path=check['system'],
                include_emitter_runtime=False,include_instance_parameters=False,include_asset_exposed_parameters=True,
                include_parameter_values=True,max_components=2,max_parameters=256)

def check_niagara_playback(s,c):
    def same(actual,expected,tolerance):
        if type(expected) in (int,float) and type(actual) in (int,float):
            return math.isfinite(actual) and math.isclose(actual,expected,abs_tol=tolerance,rel_tol=0)
        if type(expected) is list and type(actual) is list:
            return len(actual)==len(expected) and all(same(a,b,tolerance) for a,b in zip(actual,expected))
        return type(actual) is type(expected) and actual==expected
    for i,check in enumerate(c.get('niagara_checks',[])):
        receipt=s.call('playback_niagara_'+str(i),'niagara_runtime','inspect_niagara_component_runtime_state',runtime_check_arguments(c,check))
        observed=receipt['data']['legacy_result']
        require(observed['success'] and observed['sampled_world_type']=='play' and observed['is_play_session_active']
                and not observed['truncated'] and len(observed['components'])==1,'No exact current PIE Niagara component snapshot')
        row=observed['components'][0]
        expected=check['system']+'.'+check['system'].rsplit('/',1)[1]
        require(row['asset_path']==expected and row['component_name']==check['component'] and row['actor_label']==c['actor_label']
                and row['registered'] and row['active'],'Current PIE component system/identity/active state differs')
        overrides=row['override_parameters'];exposed=row['asset_exposed_parameters']
        require(not overrides['truncated'] and not exposed['truncated'],'Parameter snapshot is incomplete')
        for p in check.get('parameters',[]):
            matches=[v for v in overrides['parameters'] if v['name']==p['name']]
            require(len(matches)==1 and matches[0].get('value_available') is not False and 'value' in matches[0] and same(matches[0]['value'],p['value'],p.get('tolerance',1e-5)),
                    'PIE Niagara override differs or is unavailable: '+p['name'])
            if check.get('require_nondefault_override',True):
                baseline=[v for v in exposed['parameters'] if v['name']==p['name']]
                require(len(baseline)==1 and baseline[0].get('value_available') is not False and 'value' in baseline[0]
                        and not same(matches[0]['value'],baseline[0]['value'],p.get('tolerance',1e-5)),
                        'Component value equals asset default or baseline is unavailable; BP delivery is not established: '+p['name'])
        s.report.setdefault('playback_niagara_checks',[]).append(dict(component=check['component'],system=expected,
            verified_parameters=check.get('parameters',[]),receipt=receipt,
            nondefault_override_required=check.get('require_nondefault_override',True),
            scope='Current public GT override values; nondefault policy distinguishes asset-default copying. Setter provenance requires authored graph evidence; no GPU completion, particle callback or visual quality claim.'))
        s.calls.persist()

def playback(s,bp,c):
    validate_playback(c)
    require(s.call('playback_world','scene','get_current_level',plain_string=True)==c['world'],'Wrong playback world')
    require(not s.call('playback_idle','app','IsPIERunning'),'Existing PIE is not owned by this pipeline')
    require(s.call('playback_label_absent','scene','find_actors',dict(name=c['actor_label'],tag='',collision_channels=[]))==[],
            'Playback actor label already exists; no duplicate spawn')
    actor=s.call('playback_spawn','scene','add_to_scene_from_asset',dict(asset_path=bp,name=c['actor_label'],xform=c.get('transform',{})),mutation=True)
    require(type(actor) is dict and actor.get('refPath'),'No actual spawned actor reference; inspect partial world')
    s.checkpoint(playback_actor=actor,playback_world_save='not_requested; spawned actor retained for inspection')
    log=Path(c['engine_log_path']) if 'expected_log_marker' in c else None
    offset=log.stat().st_size if log else None
    s.call('playback_start','app','StartPIE',dict(options=dict(bSimulate=False,playMode='PlayMode_InViewPort',warmupSeconds=c.get('warmup_seconds',1.0))),mutation=True)
    # Only a successfully returned StartPIE owns this stop; timeout/disconnect
    # exits above and is not treated as cancellation or automatically replayed.
    primary_failure=None
    try:
        require(s.call('playback_running','app','IsPIERunning') is True,'PIE did not remain active after startup')
        check_niagara_playback(s,c)
        if log:
            with log.open('rb') as stream:
                stream.seek(offset);tail=stream.read(4*1024*1024+1)
            require(len(tail)<=4*1024*1024,'PIE log observation exceeded bound')
            text=tail.decode('utf8',errors='replace')
            require(c['expected_log_marker'] in text,'Expected BeginPlay log marker missing in this request log interval')
            s.report['playback_log_matches']=[line for line in text.splitlines() if c['expected_log_marker'] in line]
        s.checkpoint(PIE='startup_and_running_verified',playback_quality='No rendered quality or packaged execution claim')
    except Exception as error:
        primary_failure=dict(stage=s.report['stage'],error=str(error),error_type=type(error).__name__)
        s.report['playback_primary_failure']=primary_failure
        raise
    finally:
        try:
            running=s.call('playback_running_before_stop','app','IsPIERunning')
            require(type(running) is bool,'PIE state unavailable before owned cleanup')
            if running:s.call('playback_stop','app','StopPIE',mutation=True)
            stopped=s.call('playback_stopped','app','IsPIERunning')
            require(stopped is False,'PIE stop not observed; no stop retry')
            s.report['playback_cleanup']=dict(status='stopped' if running else 'already_stopped',pie_running=False)
        except Exception as cleanup_error:
            cleanup=dict(status='failed',error=str(cleanup_error),stage=s.report['stage'],pie_running=None,automatic_retry=False)
            s.report['playback_cleanup']=cleanup
            # Observation is separate from StopPIE; an uncertain stop is never
            # replayed and never replaces the original validation failure.
            try:cleanup['pie_running']=s.call('playback_state_after_cleanup_error','app','IsPIERunning')
            except Exception as observation_error:cleanup['state_observation_error']=str(observation_error)
            if primary_failure is None:raise
        finally:
            if primary_failure:s.report['stage']=primary_failure['stage']
            s.calls.persist()

def main(argv=None):
    args=arguments(__doc__,argv)
    with Session(args,'blueprint',TOOLS) as s:run(s,s.config)

if __name__=='__main__':main()
