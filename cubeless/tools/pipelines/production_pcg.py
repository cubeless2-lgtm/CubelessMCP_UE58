"""Explicit PCG graph/volume authoring with terminal regeneration and placement gates."""
import json
import math
import re
from copy import deepcopy
from .production_session import Session,arguments,package
from .result_contracts import require

TOOLS={
 'pcg':'PCGToolset.PCGToolset',
 'contracts':'cubeless_mcp_ue58.registration.pcg_contract_authoring_toolset.CubelessPCGContractAuthoringTools',
 'generation':'cubeless_mcp_ue58.registration.pcg_component_generation_toolset.CubelessPCGComponentGenerationTools',
 'inspection':'cubeless_mcp_ue58.registration.pcg_inspection_toolset.CubelessPCGInspectionTools',
 'placement':'cubeless_mcp_ue58.registration.production_inspection_toolset.CubelessProductionInspectionTools',
 'scene':'editor_toolset.toolsets.scene.SceneTools',
 'assets':'editor_toolset.toolsets.asset.AssetTools',
 'object':'editor_toolset.toolsets.object.ObjectTools',
 'actors':'editor_toolset.toolsets.actor.ActorTools',
 'spline':'cubeless_mcp_ue58.registration.spline_component_points_toolset.CubelessSplineComponentPointsTools',
}

def ref(value):
    require(type(value) is dict and type(value.get('refPath')) is str and value['refPath'],'Missing actual engine object reference')
    return value

def validate_expect(c):
    require(type(c) is dict and set(c)<={'min_instances','instances','bounds_min','bounds_max','tolerance_cm','unique_positions'},'Unknown placement criterion')
    for k in ('min_instances','instances'):
        if k in c:require(type(c[k]) is int and 0<=c[k]<=10000,'Instance count must be 0..10000')
    if 'instances' in c:require(c['instances']>=c.get('min_instances',0),'Exact count contradicts minimum count')
    require(type(c.get('unique_positions',False)) is bool,'Boolean unique_positions required')
    eps=c.get('tolerance_cm',.01)
    require(type(eps) in (int,float) and math.isfinite(eps) and eps>=0,'Finite nonnegative position tolerance required')
    if 'bounds_min' in c or 'bounds_max' in c:
        require(all(type(c.get(k)) is list and len(c[k])==3 and all(type(v) in (int,float) and math.isfinite(v) for v in c[k]) for k in ('bounds_min','bounds_max')),
                'Both finite XYZ placement bounds are required')
        require(all(a<=b for a,b in zip(c['bounds_min'],c['bounds_max'])),'Invalid placement bounds')

def run(s,c):
    if c.get('mode')=='edit':return edit_graph(s,c)
    require(c.get('mode','create')=='create','PCG mode must be create or edit')
    require(set(c)<={'mode','graph','world','actor_label','transform','nodes','links','cycles','save_graph','expect','spline'},'Unknown PCG section')
    graph=package(c['graph']);world=package(c['world']);label=c['actor_label']
    require(type(label) is str and label and len(label)<=100,'Explicit bounded actor label required')
    nodes=c['nodes'];ids=[n['id'] for n in nodes]
    require(len(nodes)<=64 and len(ids)==len(set(ids)) and not set(ids)&{'input','output'},'Unique node IDs required; input/output are reserved')
    require(type(c.get('save_graph',False)) is bool,'Explicit boolean graph save policy required')
    require(len(c.get('cycles',[]))<=8,'At most 8 explicit non-retry input-change cycles')
    validate_expect(c.get('expect',{}))
    if 'spline' in c:
        spline=c['spline']
        require(set(spline)=={'component_name','points','closed_loop','allow_official_component_write'}
                and spline['allow_official_component_write'] is True,'Explicit spline creation fields and official component-write acknowledgement required')
        require(type(spline['closed_loop']) is bool and re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,63}',spline['component_name']),
                'Boolean loop and exact bounded component name required')
        validate_spline_points(spline['points'])
        s.schema('actors','add_component',dict(owner={'refPath':'/Game/SchemaOnly.SchemaOnly:PersistentLevel.SchemaOnly'},
                    component_type={'refPath':'/Script/Engine.SplineComponent'},name=spline['component_name']))
        s.schema('spline','set_spline_component_points',spline_arguments(c,{'refPath':'/Game/SchemaOnly.SchemaOnly:PersistentLevel.SchemaOnly'},spline['points']))
        require(any(n.get('type')=='Get Spline Data' and n.get('parameters',{}).get('bAlwaysRequeryActors') is True
                    and n.get('parameters',{}).get('ActorSelector',{}).get('ActorFilter')=='Self' for n in nodes),
                'Spline input requires explicit Self Get Spline Data with bAlwaysRequeryActors=true; stale cached data is not accepted')
    for n in nodes:
        require(set(n)<={'id','type','parameters','mesh_entries','subgraph'},'Unknown node section')
        if 'subgraph' not in n:s.call('node_schema_'+n['id'],'pcg','GetNativeNodeSchema',dict(nodeName=n['type']))
        values=dict(graph={'refPath':'/Game/SchemaOnly.SchemaOnly'},nodeName=n['id'],
                    jsonParams=json.dumps(n.get('parameters',{}),allow_nan=False),nodeTitle=n['id'],nodeComment='')
        if 'subgraph' in n:method='AddSubgraphNode';values['subGraphForNode']={'refPath':package(n['subgraph'])}
        else:method='AddNode';values['nativeNodeType']=n['type']
        s.schema('pcg',method,values)
        if 'mesh_entries' in n:s.schema('contracts','set_pcg_static_mesh_spawner_entries',dict(graph_path=graph,node_id=n['id'],mesh_entries=n['mesh_entries'],save=False))
    for link in c['links']:
        require(len(link)==4 and link[0] in ids+['input','output'] and link[2] in ids+['input','output'],'Exact logical nodes/pin labels required')
    for cycle in c.get('cycles',[]):
        require(set(cycle)<={'updates','expect','same_as_previous','spline_points'},'Unknown regeneration cycle field')
        if 'spline_points' in cycle:
            require('spline' in c,'Spline cycles require a declared owned spline input');validate_spline_points(cycle['spline_points'])
        validate_expect(cycle.get('expect',c.get('expect',{})))
        for update in cycle.get('updates',[]):
            require(set(update)=={'node','parameters'} and update['node'] in ids and type(update['parameters']) is dict,'Cycle updates target manifest node parameter objects only')
            if 'spline' in c and any(n['id']==update['node'] and n.get('type')=='Get Spline Data' for n in nodes):
                from .result_contracts import canonical
                require(not {canonical(k) for k in update['parameters']}&{'actorselector','balwaysrequeryactors'},
                        'Spline cycles cannot change source actor selection/requery policy; rejected before graph creation')
    s.schema('pcg','SpawnGraphInstance',dict(graph={'refPath':'/Game/SchemaOnly.SchemaOnly'},name=label,transform=c['transform'],jsonParams='{}'))
    current=s.call('current_world','scene','get_current_level',plain_string=True)
    require(current==world,'Wrong current world. Pipeline never changes/saves the user world implicitly')
    require(s.call('graph_absent','assets','exists',dict(path=graph)) is False,'Existing graph refused; explicit graph editing is not implemented in this entrypoint yet')
    require(s.call('actor_absent','scene','find_actors',dict(name=label,tag='',collision_channels=[]))==[],'Actor label collision; no append/overwrite of unknown actor')
    s.checkpoint(graph=graph,world=world,actor_label=label,full_legacy_parity=False,
                 world_save='not_requested',reload='not_run',runtime_generation='not_run')
    if not s.args.apply:return
    g=ref(s.call('create_graph','pcg','CreateGraph',dict(name=graph.rsplit('/',1)[1],path=graph.rsplit('/',1)[0]),mutation=True))
    s.checkpoint(actual_created_graph=g)
    require(g['refPath']==graph+'.'+graph.rsplit('/',1)[1],
            'Engine returned a different unique graph; preserve it and stop before touching the requested path')
    structure=s.call('initial_structure','pcg','GetGraphStructure',dict(graph=g))
    # Stock graph input/output names are returned by Engine, not fabricated IDs.
    refs={}
    for n in structure['nodes']:
        # UE58 PCGGraph constructor creates these two exact default subobjects;
        # localized NodeType/GetDefaultTitle is not their logical identity.
        if n['name'] in ('DefaultInputNode','DefaultOutputNode'):
            refs['input' if n['name']=='DefaultInputNode' else 'output']={'refPath':n['path']}
    require(set(refs)=={'input','output'},'Could not resolve exact stock input/output roles')
    for i,n in enumerate(nodes):
        values=dict(graph=g,nodeName=n['id'],jsonParams=json.dumps(n.get('parameters',{})),nodeTitle=n['id'],nodeComment='',xPositionIdx=i*300,yPositionIdx=0)
        if 'subgraph' in n:method='AddSubgraphNode';values['subGraphForNode']={'refPath':package(n['subgraph'])}
        else:method='AddNode';values['nativeNodeType']=n['type']
        refs[n['id']]=ref(s.call('node_'+n['id'],'pcg',method,values,mutation=True))
        if 'mesh_entries' in n:
            s.call('mesh_entries_'+n['id'],'contracts','set_pcg_static_mesh_spawner_entries',dict(graph_path=graph,node_id=n['id'],mesh_entries=n['mesh_entries'],save=False),mutation=True)
    added_for_links=[]
    for i,(a,ap,b,bp) in enumerate(c['links']):
        added=s.call('link_'+str(i),'pcg','ConnectNodePins',dict(fromNode=refs[a],fromPinLabel=ap,toNode=refs[b],toPinLabel=bp),mutation=True)
        # UE58 returns conversion/filter nodes, not a success boolean. [] is
        # normal for a direct edge; prove the actual authored connection below.
        require(type(added) is list,'Unexpected ConnectNodePins receipt; inspect partial graph')
        added_for_links.append([ref(v)['refPath'].rsplit(':',1)[-1] for v in added])
    linked=s.call('linked_structure','pcg','GetGraphStructure',dict(graph=g))
    for (a,ap,b,bp),added in zip(c['links'],added_for_links):
        start=refs[a]['refPath'].rsplit(':',1)[-1];end=refs[b]['refPath'].rsplit(':',1)[-1]
        frontier=[start];seen=set();connected=False
        while frontier:
            source=frontier.pop()
            if source in seen:continue
            seen.add(source)
            for edge in linked['edges']:
                if edge['srcNode']!=source or (source==start and edge['srcPin']!=ap):continue
                if edge['destNode']==end and edge['destPin']==bp:connected=True
                elif edge['destNode'] in added:frontier.append(edge['destNode'])
        require(connected,'Requested connection missing after actual Engine authoring; inspect partial graph')
    s.report['graph_audit']=s.call('graph_audit','contracts','audit_pcg_graph_contract',dict(graph_path=graph))
    volume=s.call('spawn_volume','pcg','SpawnGraphInstance',dict(graph=g,name=label,transform=c['transform'],jsonParams='{}'),mutation=True)
    actor=ref(volume['actor']);actor_path=actor['refPath']
    selected=s.call('spawned_actor_selection','scene','find_actors',dict(name=label,tag='',collision_channels=[]))
    require([ref(v)['refPath'] for v in selected]==[actor_path],
            'Label-prefix generation must resolve exactly the actor actually spawned')
    s.checkpoint(actor_path=actor_path,graph_refs=refs)
    if 'spline' in c:
        component=ref(s.call('create_owned_spline','actors','add_component',dict(owner=actor,
            component_type={'refPath':'/Script/Engine.SplineComponent'},name=c['spline']['component_name']),mutation=True))
        s.checkpoint(spline_component=component)
        require(component['refPath']==actor_path+'.'+c['spline']['component_name'],'Actual spline component differs; inspect partial world before another mutation')
        set_owned_spline(s,c,actor,c['spline']['points'],'initial_spline_points')
    generate_cycles(s,c,g,actor,refs)

def validate_spline_points(points):
    require(type(points) is list and 2<=len(points)<=4096 and all(type(p) is list and len(p)==3
            and all(type(v) in (int,float) and math.isfinite(v) for v in p) for p in points),
            'Spline input requires 2..4096 finite world-space XYZ points')

def spline_arguments(c,actor,points):
    return dict(points=points,actor_name=actor['refPath'].rsplit('.',1)[-1],component_name=c['spline']['component_name'],
                closed_loop=c['spline']['closed_loop'],selected_only=False,native_first=True)

def set_owned_spline(s,c,actor,points,label):
    component=s.report['spline_component']
    require(s.call(label+'_class','object','get_class',dict(instance=component))=={'refPath':'/Script/Engine.SplineComponent'},
            'Exact created native spline is no longer available; no point-write replay')
    s.call(label,'spline','set_spline_component_points',spline_arguments(c,actor,points),mutation=True)

def edit_graph(s,c):
    """Edit exact existing native-node settings, preserving unrelated authored state.

    Official UpdateNode owns the writes/transaction. This is not a claim that
    it inherits Cubeless callback guards or offers atomic rollback.
    """
    from .result_contracts import canonical
    require(set(c)<={'mode','graph','world','actor_path','updates','save_graph','expect','allow_official_write'},'Unknown edit field; graph topology edits are not silently accepted')
    graph=package(c['graph']);world=package(c['world']);g={'refPath':graph+'.'+graph.rsplit('/',1)[1]}
    validate_expect(c.get('expect',{}))
    require(c.get('allow_official_write') is True,'Explicit acknowledgement of official UpdateNode mutation semantics required')
    require(type(c.get('save_graph',False)) is bool,'Boolean graph save policy required')
    actor=ref({'refPath':c['actor_path']})
    require(actor['refPath'].startswith(world+'.'+world.rsplit('/',1)[1]+':PersistentLevel.'),'Exact actor in the requested Editor world required')
    require(s.call('edit_world','scene','get_current_level',plain_string=True)==world,'Wrong current world')
    require(s.call('edit_exists','assets','exists',dict(path=graph)) is True,'Existing graph is required')
    require(s.call('edit_clean','assets','is_dirty',dict(asset_path=graph)) is False,'Dirty existing graph refused; inspect/save explicitly')
    before=s.call('edit_structure_before','pcg','GetGraphStructure',dict(graph=g))
    require(len(before['nodes'])<=128,'Bounded edit supports at most 128 existing nodes')
    refs={n['name']:{'refPath':n['path']} for n in before['nodes']}
    require(len(refs)==len(before['nodes']),'Ambiguous existing node names')
    updates=c['updates'];require(type(updates) is list and 1<=len(updates)<=32,'One bounded settings batch required')
    names=[u['node'] for u in updates]
    require(len(names)==len(set(names)),'Each existing node appears once in the batch')
    plans=[]
    for u in updates:
        require(set(u)=={'node','parameters'} and u['node'] in refs,'Exact existing node and parameter object required')
        values=u['parameters'];require(type(values) is dict and 1<=len(values)<=32,'Bounded nonempty parameter object required')
        require(all(type(k) is str and k for k in values) and len({canonical(k) for k in values})==len(values),'Ambiguous parameter names')
        settings=s.call('edit_settings_'+u['node'],'object','get_properties',dict(instance=refs[u['node']],properties=['SettingsInterface']))['SettingsInterface']
        require(ref(settings)['refPath'].startswith(refs[u['node']]['refPath']+'.'),'Shared/external settings cannot be edited through this entrypoint')
        cls=ref(s.call('edit_class_'+u['node'],'object','get_class',dict(instance=settings)))
        require(not cls['refPath'].endswith(('PCGSettingsInstance','PCGSubgraphSettings')),'Settings instances/subgraph parameters need their dedicated edit contract')
        observed=s.call('edit_values_before_'+u['node'],'object','get_properties',dict(instance=settings,properties=list(values)))
        require({canonical(k) for k in observed}=={canonical(k) for k in values},'Every requested field must be readable before any write')
        raw_title=s.call('edit_authored_title_'+u['node'],'object','get_properties',dict(instance=refs[u['node']],properties=['NodeTitle']))
        require(set(raw_title)=={'NodeTitle'},'Authored node title unavailable before write')
        args=dict(node=refs[u['node']],jsonParams=json.dumps(values,allow_nan=False),nodeTitle='')
        s.schema('pcg','UpdateNode',args);plans.append((u,settings,cls,args,raw_title))
    def idle(label):
        v=s.call(label,'inspection','inspect_pcg_component_outputs',dict(actor_path=actor['refPath'],max_points=8))
        rows=v['data']['legacy_result']['components']
        require(len(rows)==1 and rows[0]['graph']==g['refPath'],'Selected actor must own exactly one component using the exact graph')
        require(not any(rows[0][k] for k in ('generating','cleaning_up','refresh_in_progress')) and rows[0]['managed_resources_accessible'],
                'Existing generation/cleanup is active; no edit or retry')
    idle('edit_idle_before')
    s.checkpoint(graph_structure_before=before,planned_updates=updates,
        official_write_scope='Official ToolsetAPI UpdateNode -> Engine SetObjectProperties and graph notification. No inherited Cubeless callback-purity or atomic rollback claim.',
        preservation_scope='All observed node identities/layout/links and unrelated serialized paramOverrides; exact edited fields additionally read through public property API. Not arbitrary private graph memory equality.',
        world_save='not_requested',full_legacy_parity=False)
    if not s.args.apply:return
    def same(a,b):
        if type(a) in (float,int) and type(b) in (float,int):return math.isfinite(a) and math.isclose(a,b,abs_tol=1e-5,rel_tol=0)
        if type(a) is dict and type(b) is dict:
            aa={canonical(k):v for k,v in a.items()};bb={canonical(k):v for k,v in b.items()}
            return aa.keys()==bb.keys() and all(same(aa[k],bb[k]) for k in aa)
        if type(a) is list and type(b) is list:return len(a)==len(b) and all(same(x,y) for x,y in zip(a,b))
        return type(a) is type(b) and a==b
    for u,settings,cls,args,raw_title in plans:
        idle('edit_idle_'+u['node'])
        current=s.call('edit_settings_current_'+u['node'],'object','get_properties',dict(instance=refs[u['node']],properties=['SettingsInterface']))
        require(current['SettingsInterface']==settings,'Settings object replaced since admission; no write')
        require(s.call('edit_write_'+u['node'],'pcg','UpdateNode',args,mutation=True) is True,'Official node update failed; preserve partial graph and stop')
        current=s.call('edit_settings_after_'+u['node'],'object','get_properties',dict(instance=refs[u['node']],properties=['SettingsInterface']))
        require(current['SettingsInterface']==settings and s.call('edit_class_after_'+u['node'],'object','get_class',dict(instance=settings))==cls,'Settings identity/class changed after official callback')
        require(s.call('edit_authored_title_after_'+u['node'],'object','get_properties',dict(instance=refs[u['node']],properties=['NodeTitle']))==raw_title,'Unrequested authored node title changed')
        read=s.call('edit_values_after_'+u['node'],'object','get_properties',dict(instance=settings,properties=list(u['parameters'])))
        require(same(read,u['parameters']),'Actual authored settings differ after update; no regenerate/retry')
    after=s.call('edit_structure_after','pcg','GetGraphStructure',dict(graph=g))
    def authored(v):
        result=deepcopy(v);by_name={n['name']:n for n in result['nodes']}
        for u in updates:
            node=by_name.get(u['node']);require(node is not None,'Edited node disappeared')
            touched={canonical(k) for k in u['parameters']}
            node['paramOverrides']={k:v for k,v in node['paramOverrides'].items() if canonical(k) not in touched}
            # GetNodeInfo exposes GetDefaultTitle-derived fields (for example
            # FilterByType.TargetType). Preserve raw authored NodeTitle above,
            # not an engine-derived display label that the request may change.
            node.pop('nodeType',None);node.pop('title',None)
        result['nodes']=sorted(result['nodes'],key=lambda n:n['path'])
        result['edges']=sorted(result['edges'],key=lambda e:json.dumps(e,sort_keys=True))
        return result
    require(authored(before)==authored(after),'Unrequested graph state changed; preserve actual graph and stop')
    s.checkpoint(graph_structure_after=after,requested_parameter_values_verified=True,unrequested_graph_state_verified=True)
    idle('edit_idle_after')
    generate_cycles(s,c,g,actor,refs)

def generate_cycles(s,c,g,actor,refs):
    actor_path=actor['refPath'];graph=c['graph']
    def component_state(label):
        observed=s.call(label,'inspection','inspect_pcg_component_outputs',dict(actor_path=actor_path,max_points=8))
        rows=observed['data']['legacy_result']['components']
        require(len(rows)==1,'Exactly one observed PCG component required')
        row=rows[0]
        require(not row['generating'] and not row['cleaning_up'] and not row['refresh_in_progress'] and row['managed_resources_accessible'],
                'Existing PCG work is active or resources unavailable; no new generation')
        require(row['graph']==g['refPath'],'Component source changed; no generation replay')
        return observed
    def generate(index,expect):
        component_state('before_generate_'+str(index))
        # Installed PCGToolsetAsync.cpp schedules its result against the actual
        # GenerateLocalGetTaskId. Engine post-generation releases unused owned
        # resources; this is not Cubeless Refresh or explicit hard cleanup.
        messages=s.call('generate_'+str(index),'pcg','ExecuteGraphInstance',dict(pcgVolume=actor),mutation=True)
        require(type(messages) is list,'No terminal official generation receipt; inspect tasks without replay')
        require(not any(str(m.get('severity','')).lower() in ('error','fatal') for m in messages),
                'Engine reported generation errors; inspect actual partial output')
        outputs=component_state('outputs_'+str(index))
        row=outputs['data']['legacy_result']['components'][0]
        require(row['was_generated_this_session'],'Official terminal returned without current generation evidence')
        expects_empty=expect.get('instances')==0
        if not expects_empty:
            require(row['generated_output_data_count']>0 and row['managed_resource_count']>0,
                    'Official terminal returned without current mesh output/resource evidence')
        actual=s.call('placement_'+str(index),'placement','inspect_pcg_placement',dict(actor_path=actor_path))['data']
        count=actual['total_instances'];require(actual['complete'] and count>=expect.get('min_instances',0 if expects_empty else 1),'No complete actual mesh output')
        if 'instances' in expect:require(count==expect['instances'],'Actual instance count differs')
        if expect.get('unique_positions',False):
            positions=[tuple(round(v,4) for v in t['position']) for comp in actual['components'] for t in comp['instances']]
            require(len(set(positions))==len(positions),'Duplicate instance positions at 0.0001cm rounding; preserve actual output without retry')
        if 'bounds_min' in expect or 'bounds_max' in expect:
            low,high=expect['bounds_min'],expect['bounds_max'];eps=expect.get('tolerance_cm',.01)
            require(all(all(low[i]-eps<=v<=high[i]+eps for i,v in enumerate(t['position'])) for comp in actual['components'] for t in comp['instances']),'Actual world placement outside requested bounds')
        s.report.setdefault('generations',[]).append(dict(cycle=index,placement=actual,outputs=outputs,
            completion=True,expected_empty=expects_empty,official_messages=messages,
            completion_scope='Official task-dependent async result + current idle/source/resource/output readback; no Cubeless Refresh delegate proof'))
        s.calls.persist()
    generate(0,c.get('expect',{}))
    for i,cycle in enumerate(c.get('cycles',[]),1):
        if 'spline_points' in cycle:
            component_state('before_spline_update_'+str(i))
            set_owned_spline(s,c,actor,cycle['spline_points'],'spline_update_'+str(i))
        for update in cycle.get('updates',[]):
            ok=s.call('update_'+str(i)+'_'+update['node'],'pcg','UpdateNode',dict(node=refs[update['node']],jsonParams=json.dumps(update['parameters']),nodeTitle=update['node']),mutation=True)
            require(ok is True,'UpdateNode failed; do not regenerate uncertain input')
        generate(i,cycle.get('expect',c.get('expect',{})))
        if cycle.get('same_as_previous',False):
            def positions(value):
                return sorted(tuple(round(v,4) for v in t['position']) for comp in value['components'] for t in comp['instances'])
            a,b=s.report['generations'][-2:]
            require(positions(a['placement'])==positions(b['placement']),
                    'Same-input regeneration changed actual placement/count; no extra retry')
    if c.get('save_graph',False):
        s.call('save_graph','assets','save_assets',dict(asset_paths=[graph]),mutation=True)
        require(s.call('graph_clean','assets','is_dirty',dict(asset_path=graph)) is False,'Graph remained dirty after explicit save')
    s.checkpoint(graph_structure=s.call('final_structure','pcg','GetGraphStructure',dict(graph=g)),
                 cleanup_scope='Engine generation reuses/releases selected managed resources. No explicit hard cleanup or arbitrary actor deletion. Counts and world placement checked after each generation.',
                 generation_dependency_layer='Official ToolsetAPI ExecuteGraphInstance -> Engine GenerateLocalGetTaskId/ScheduleGeneric')

def main(argv=None):
    args=arguments(__doc__,argv)
    with Session(args,'pcg',TOOLS) as s:run(s,s.config)

if __name__=='__main__':main()
