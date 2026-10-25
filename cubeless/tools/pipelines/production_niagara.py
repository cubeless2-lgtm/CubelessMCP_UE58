"""Preset/spec authoring and explicit Blueprint binding over official MCP."""
import json
from pathlib import Path
from .production_session import Session, arguments, package
from .result_contracts import require,canonical
from .production_blueprint import TOOLS as BP_TOOLS, run as blueprint_run

TOOLS={**BP_TOOLS,
 'preset':'cubeless_mcp_ue58.registration.niagara_sprite_preset_authoring_toolset.CubelessNiagaraSpritePresetTools',
 'parameters':'cubeless_mcp_ue58.registration.niagara_parameters_toolset.CubelessNiagaraParameterTools',
 'niagara_component':'NiagaraToolsets.NiagaraToolset_Component',
 'niagara_bp':'NiagaraToolsets.NiagaraToolset_Blueprint',
 'preview':'cubeless_mcp_ue58.registration.niagara_preview_workflows_toolset.CubelessNiagaraPreviewWorkflowTools',
 'compile_status':'cubeless_mcp_ue58.registration.niagara_authoring_inspection_toolset.CubelessNiagaraAuthoringInspectionTools',
 'critic':'cubeless_mcp_ue58.registration.niagara_preview_toolset.CubelessNiagaraPreviewTools',
 'planning':'cubeless_mcp_ue58.registration.niagara_planning_toolset.CubelessNiagaraPlanningTools',
 'module_inputs':'cubeless_mcp_ue58.registration.niagara_modules_toolset.CubelessNiagaraModuleTools',
 'scratch_recipes':'cubeless_mcp_ue58.registration.niagara_scratch_user_recipes_toolset.CubelessNiagaraScratchUserRecipeTools',
 'renderers':'cubeless_mcp_ue58.registration.niagara_renderers_toolset.CubelessNiagaraRendererTools',
 'hlsl':'cubeless_mcp_ue58.registration.niagara_custom_hlsl_conversion_toolset.CubelessNiagaraCustomHlslConversionTools',
}

SCRATCH_RECIPES={
 'vec2':'create_niagara_scratch_pad_vec2_user_parameter_module',
 'color':'create_niagara_scratch_pad_color_user_parameter_module',
 'bool':'create_niagara_scratch_pad_bool_user_parameter_module',
 'visible_toggle':'create_niagara_scratch_pad_bool_visible_toggle_module',
}
RENDERER_EDITS={
 'sprite_settings':'set_niagara_sprite_renderer_settings',
 'material':'set_niagara_renderer_material',
 'enabled':'set_niagara_renderer_enabled',
 'add_sprite':'add_niagara_sprite_renderer',
 'add_mesh':'add_niagara_mesh_renderer',
 'cleanup':'cleanup_niagara_renderers',
}

def owns_fields(values,fields):
    """Use the actual MCP argument normalization for target/policy ownership."""
    require(type(values) is dict,'Settings must be an object')
    return not {canonical(k) for k in values}&{canonical(k) for k in fields}

def authoring_plan(s,c,system):
    """Named domain batches and API-owned Scratch/User recipes; never RPC code."""
    plan=[]
    if 'module_inputs' in c:
        row=c['module_inputs']
        require(type(row) is dict and {'edits'}<=set(row)<={'edits','operation','overwrite_existing','continue_on_error'},'Unknown module-input batch field')
        require(type(row['edits']) is list and 1<=len(row['edits'])<=256,'One ordered module batch supports 1..256 explicit edits')
        args=dict(system_path=system,save=False,request_compile=True,wait_for_completion=True,allow_source_edit=True,**row)
        s.schema('module_inputs','set_niagara_module_inputs_batch',args)
        plan.append(('module_input_batch','module_inputs','set_niagara_module_inputs_batch',args))
    rows=c.get('scratch_user_modules',[])
    require(type(rows) is list and len(rows)<=8,'At most 8 explicit Scratch/User modules')
    for i,row in enumerate(rows):
        require(type(row) is dict and set(row)=={'kind','settings'} and row['kind'] in SCRATCH_RECIPES,'Named Scratch/User recipe required')
        args=row['settings'];require(owns_fields(args,{'target_system_path','allow_source_edit','save','request_compile','wait_for_completion'}),
                'Pipeline owns recipe target/compile/wait/save; remaining public recipe settings are preserved')
        args=dict(target_system_path=system,save=False,request_compile=True,wait_for_completion=True,allow_source_edit=True,**args)
        method=SCRATCH_RECIPES[row['kind']];s.schema('scratch_recipes',method,args)
        plan.append(('scratch_user_module_'+str(i),'scratch_recipes',method,args))
    rows=c.get('hlsl_replacements',[])
    require(type(rows) is list and len(rows)<=16,'At most 16 explicit HLSL replacements')
    for i,row in enumerate(rows):
        require(owns_fields(row,{'system_path','dry_run','allow_source_edit','save','request_compile'}),
                'Pipeline owns HLSL target/write/compile/save; keep native selector and timeout settings')
        reference=row.get('recipe_index')
        if reference is not None:
            require(type(reference) is int and 0<=reference<len(c.get('scratch_user_modules',[]))
                    and c['scratch_user_modules'][reference]['kind']=='visible_toggle'
                    and owns_fields(row,{'node_guid','scratch_pad_name'}),
                    'HLSL recipe_index must reference this run\'s visible_toggle receipt; do not mix literal identity selectors')
        require((reference is not None or bool(row.get('node_guid') or row.get('signature_name'))) and bool(row.get('expected_contains')),
                'HLSL replacement requires an explicit node/signature or recipe receipt and expected current text')
        require(type(row.get('replacement_hlsl')) is str and len(row['replacement_hlsl'].encode('utf-16-le'))//2<=200000,
                'Pipeline HLSL readback supports at most 200000 UTF-16 code units; reject before any authoring writes')
        args=dict(system_path=system,dry_run=False,allow_source_edit=True,save=False,request_compile=True,**{k:v for k,v in row.items() if k!='recipe_index'})
        if reference is not None:s.report.setdefault('hlsl_recipe_references',{})['hlsl_replacement_'+str(i)]=reference
        s.schema('hlsl','replace_niagara_custom_hlsl',args)
        plan.append(('hlsl_replacement_'+str(i),'hlsl','replace_niagara_custom_hlsl',args))
    rows=c.get('renderer_edits',[])
    require(type(rows) is list and len(rows)<=16,'At most 16 explicit renderer edits')
    for i,row in enumerate(rows):
        require(type(row) is dict and set(row)=={'operation','settings'} and row['operation'] in RENDERER_EDITS,
                'Named renderer operation/settings required')
        settings=row['settings'];require(owns_fields(settings,{'system_path','allow_source_edit','save','request_compile','wait_for_completion','dry_run'}),
                'Pipeline owns renderer target/compile/save/write controls')
        method=RENDERER_EDITS[row['operation']];args=dict(system_path=system,allow_source_edit=True,save=False,**settings)
        if row['operation']!='material':args['request_compile']=False
        if row['operation']=='cleanup':
            require(bool(settings.get('remove_renderer_indices') or settings.get('clear_meshes_binding_renderer_indices'))
                    and (bool(settings.get('expected_renderers')) or type(settings.get('expected_renderer_count')) is int),
                    'Renderer cleanup requires explicit changes and expected existing topology')
            args.update(dry_run=False,wait_for_completion=True)
        s.schema('renderers',method,args)
        plan.append(('renderer_edit_'+str(i),'renderers',method,args))
    return plan

def run(s,c):
    authoring_sections={'module_inputs','scratch_user_modules','hlsl_replacements','renderer_edits'}
    require(set(c)<={'system','preset','settings','save','user_parameters','blueprint','visual','mode','allow_source_edit'}|authoring_sections,'Unknown Niagara section')
    mode=c.get('mode','create');require(mode in ('create','edit','review'),'Niagara mode must be create, edit or review')
    require(mode=='edit' or 'allow_source_edit' not in c,'allow_source_edit belongs to explicit existing-system edit mode')
    system=package(c['system']);save=c.get('save',mode=='create')
    if mode=='review':require(not save and c.get('visual') and not set(c)&({'settings','preset','user_parameters','blueprint'}|authoring_sections),
            'Review only accepts an existing system and fixed visual settings; no asset-edit replay')
    if mode=='edit':require(c.get('allow_source_edit') is True and not set(c)&{'preset','settings'}
            and bool(c.get('user_parameters') or any(c.get(k) for k in authoring_sections)),
            'Existing system edit requires explicit allow_source_edit=true and named authoring sections, not preset recreation')
    require(type(save) is bool,'Boolean save required')
    settings=c.get('settings',{})
    require(owns_fields(settings,{'target_system_path','save','wait_for_completion','overwrite_existing'}),'Pipeline owns target/save/wait/no-overwrite controls')
    require(mode!='create' or system.startswith('/Game/_MCP_Temp/NiagaraGenerated/') or settings.get('allow_non_temp_path') is True,
            'Preset target must use /Game/_MCP_Temp/NiagaraGenerated/ or explicitly allow_non_temp_path before Engine admission')
    plan=authoring_plan(s,c,system)
    values=dict(target_system_path=system,preset=c.get('preset','basic'),save=save and not bool(c.get('user_parameters') or plan),wait_for_completion=True,overwrite_existing=False,**settings)
    s.schema('preset','create_sprite_niagara_preset_system',values)
    require(s.call('system_existence','assets','exists',{'path':system}) is (mode!='create'),'System existence disagrees with create/edit/review; no overwrite/replay')
    if mode!='create':
        loaded=s.call('load_existing_system','assets','load_asset',dict(asset_path=system))
        require(type(loaded) is dict and loaded.get('refPath')==system+'.'+system.rsplit('/',1)[-1],
                'Official load returned a different system reference; no authoring writes dispatched')
    if mode=='edit':require(s.call('existing_system_clean','assets','is_dirty',dict(asset_path=system)) is False,'Dirty system refused before editing; inspect/save explicitly')
    binding=c.get('blueprint')
    if binding:
        require(set(binding)<={'path','component','mode','manifest','allow_official_template_write'},'Unknown BP binding section')
        bp=package(binding['path']);binding_mode=binding.get('mode','existing')
        require(binding_mode in ('existing','wrapper'),'BP mode must be existing or wrapper')
        exists=s.call('bp_exists','assets','exists',{'path':bp})
        require(exists is (binding_mode=='existing'),'Blueprint existence mismatch')
        if binding_mode=='existing':
            require(binding.get('allow_official_template_write') is True,'Explicit official SetSystem template write acknowledgement required')
            require(s.call('bp_clean','assets','is_dirty',dict(asset_path=bp)) is False,
                    'Existing BP is dirty; inspect/save explicitly before starting this pipeline')
            binding_rows=s.call('binding_preflight','component','list_blueprint_components',dict(blueprint_name=bp))['data']['components']
            matching=[r for r in binding_rows if r['component_name']==binding['component'] and r['component_class_path']=='/Script/Niagara.NiagaraComponent']
            require(len(matching)==1,'Exactly one own Niagara component is required before creation')
        if 'manifest' in binding:
            from .production_blueprint import compile_plan
            require(binding['manifest'].get('blueprint')==bp and binding['manifest'].get('mode')=='edit','Binding manifest must edit exactly the selected BP')
            require(binding['manifest'].get('save',False)==save,'BP manifest and outer save policy must match before any creation')
            require(binding_mode!='wrapper' or save,'Unsaved wrapper+manifest handoff is not implemented; rejected before creation')
            compile_plan(s,binding['manifest'])
    visual=c.get('visual')
    if visual:
        require(set(visual)<={'output_dir','warmup_times','views','camera_location','camera_rotation','camera_fov','auto_frame','location','scale','warmup_tick_delta','allow_current_map_preview','criteria','create_refinement_ledger','max_iterations'},'Fixed capture settings only; refinement never automatically mutates on a failed capture')
        if visual.get('create_refinement_ledger'):
            require(system.startswith('/Game/_MCP_Temp/NiagaraGenerated/'),'Existing refinement ledger contract requires the generated Niagara temp root')
            require(type(visual.get('max_iterations',4)) is int and 1<=visual.get('max_iterations',4)<=16,'Refinement ledger iteration bound must be 1..16')
        s.schema('preview','sample_niagara_system_in_preview_lab',dict(system_path=system,**{k:v for k,v in visual.items() if k not in ('criteria','create_refinement_ledger','max_iterations')}))
    s.checkpoint(system=system,requested_mode=mode,resolved_save=save,planned_preset=values if mode=='create' else None,planned_authoring_steps=plan,
                 full_legacy_parity=False,rendering_quality='not_judged',
                 refinement_iterations=1,max_refinement_iterations=1,PIE='not_run',reload='not_run')
    if not s.args.apply:return
    if mode=='review':
        verify_cpu_compile(s,system,'review_execution_scripts')
        bind_and_capture(s,c,save=save)
        return
    if mode=='create':
        result=s.call('create_system','preset','create_sprite_niagara_preset_system',values,mutation=True)
        d=result['data']
        require(d.get('requested_package',system)==system,'Created system differs from requested path; inspect returned actual target')
        require(d.get('operation_complete') is not False and not d.get('in_flight') and not d.get('command_may_still_be_in_flight'),
                'Preset did not reach a terminal outcome; inspect operation, never replay')
        # Nullable compiled is retained; actual CPU execution scripts follow.
        verify_cpu_compile(s,system,'created_system_compile')
        if values['save']:require(result['save'].get('saved') is True,'Niagara save not confirmed')
        s.checkpoint(niagara_compile=result['compile'],niagara_save=result['save'])
    else:
        s.report['user_parameter_readback']=s.call('existing_user_parameters','parameters','inspect_niagara_user_parameters',dict(system_path=system))
        verify_cpu_compile(s,system,'existing_system_compile')
    if c.get('user_parameters'):
        s.call('user_parameters','parameters','upsert_niagara_user_parameters',dict(system_path=system,
            parameters_json=json.dumps(c['user_parameters']),allow_source_edit=True,save=False,request_compile=True),mutation=True)
        waited=s.call('user_parameter_compile','compile_status','inspect_niagara_compile_status',dict(system_path=system,
            request_compile=False,wait_for_completion=True))
        check_cpu_compile(waited)
        s.report['user_parameter_compile']=waited
        s.report['user_parameter_readback']=s.call('parameter_readback','parameters','inspect_niagara_user_parameters',dict(system_path=system))
        if save and not plan:
            s.call('save_parameterized_system','assets','save_assets',dict(asset_paths=[system]),mutation=True)
            require(s.call('parameterized_system_clean','assets','is_dirty',dict(asset_path=system)) is False,
                    'Parameterized system remained dirty after explicit save')
    touched_user_parameters=set()
    created_scratch_recipes=[]
    for label,group,method,args in plan:
        if group=='hlsl' and label in s.report.get('hlsl_recipe_references',{}):
            receipt=created_scratch_recipes[s.report['hlsl_recipe_references'][label]]
            args=dict(args,node_guid=receipt['custom_hlsl_node_guid'],scratch_pad_name=receipt['scratch_pad_script_name'])
            s.schema(group,method,args)
        authored=s.call(label,group,method,args,mutation=True)
        d=authored.get('data',{})
        if group!='renderers' or method=='cleanup_niagara_renderers':
            require(d.get('operation_complete') is True and not d.get('in_flight') and not d.get('command_may_still_be_in_flight'),
                    'Authoring acceptance is not final completion; inspect actual state without replay')
        else:
            require(not d.get('in_flight') and not d.get('command_may_still_be_in_flight'),
                    'Renderer acceptance has outstanding work; inspect without replay')
        if group=='module_inputs':
            require(d.get('failed_count')==0 and d.get('all_processed_edits_succeeded') is True
                    and d.get('processed_count')==d.get('requested_count')==len(args['edits']),
                    'Module batch has failed/unprocessed edits; preserve actual partial prefix and stop without replay')
        elif group=='scratch_recipes':
            require(d.get('legacy_result',{}).get('success') is True,'Scratch recipe did not complete its domain steps')
            created_scratch_recipes.append(d['legacy_result'])
            touched_user_parameters.add(d['legacy_result']['user_parameter_name'])
        verify_cpu_compile(s,system,label+'_execution_compile',request_compile=group=='renderers')
        if group=='renderers':
            s.report.setdefault('renderer_observations',[]).append(s.call(label+'_readback','renderers','inspect_niagara_renderers',dict(system_path=system)))
        elif group=='hlsl':
            published=d['legacy_result']['after']
            observed=s.call(label+'_readback','compile_status','inspect_niagara_custom_hlsl_nodes',dict(system_path=system,
                node_guid=published['node_guid'],max_hlsl_chars=len(args['replacement_hlsl'].encode('utf-16-le'))//2))
            nodes=[n for n in observed['data']['legacy_result']['nodes']
                   if n['source_path']==published['source_path'] and n['graph_path']==published['graph_path']]
            require(len(nodes)==1 and not nodes[0].get('hlsl_truncated') and nodes[0]['custom_hlsl']==args['replacement_hlsl'],
                    'Actual HLSL body differs after compilation; preserve native change/save prefixes without replay')
    if plan:
        s.report['final_user_parameters']=s.call('authored_parameter_readback','parameters','inspect_niagara_user_parameters',dict(system_path=system))
        previous=s.report.get('user_parameter_readback',{}).get('data',{}).get('parameters',[])
        actual={row['name']:row for row in s.report['final_user_parameters']['data']['parameters']}
        for row in previous:
            if row['name'] in touched_user_parameters:continue
            require(row['name'] in actual and all(actual[row['name']].get(k)==row.get(k) for k in ('canonical_type','type_object_path','value')),
                    'Unrequested user parameter changed after module/recipe authoring: '+row['name'])
        if save:
            s.call('save_authored_system','assets','save_assets',dict(asset_paths=[system]),mutation=True)
            require(s.call('authored_system_clean','assets','is_dirty',dict(asset_path=system)) is False,'Authored system remained dirty after explicit save')
    bind_and_capture(s,c,locals().get('binding_rows'),save=save)

def bind_and_capture(s,c,binding_rows=None,*,save):
    system=package(c['system']);binding=c.get('blueprint');visual=c.get('visual')
    if binding:
        bp=binding['path'];mode=binding.get('mode','existing')
        owned_manifest_handoff=False
        # Apply a known manifest while the BP is still clean, before this
        # pipeline dirties it with SetSystem. Never waive the edit dirty guard.
        if mode=='existing' and 'manifest' in binding:
            blueprint_run(s,binding['manifest'])
            binding_rows=s.call('components_after_requested_manifest','component','list_blueprint_components',dict(blueprint_name=bp))['data']['components']
            owned_manifest_handoff=True
            s.checkpoint(bp_handoff='Exact BP just authored and verified by this same serialized request; unrelated dirty admission stays rejected')
        if mode=='wrapper':
            result=s.call('bp_wrapper','niagara_bp','ConstructNiagaraBPWrapperFromSystem',dict(newAssetPath=bp,
                system={'refPath':system+'.'+system.rsplit('/',1)[1]},parentClass={'refPath':'/Script/Engine.Actor'}),mutation=True)
            s.checkpoint(actual_wrapper_result=result)
            require(type(result) is dict and result.get('refPath')==bp+'.'+bp.rsplit('/',1)[1],
                    'Official wrapper returned a different target; preserve it and stop before touching the requested path')
            rows=s.call('wrapper_components','component','list_blueprint_components',dict(blueprint_name=bp))['data']['components']
            own=[row for row in rows if row['component_class_path']=='/Script/Niagara.NiagaraComponent']
            require(len(own)==1,'New wrapper must expose exactly one actual Niagara component')
            selected=own[0]['component_name']
        else:
            selected=binding['component']
            require(binding.get('allow_official_template_write') is True,'Explicit official template write acknowledgement required')
            require(owned_manifest_handoff or s.call('bp_clean_before_assignment','assets','is_dirty',dict(asset_path=bp)) is False,
                    'Blueprint dirty before binding; inspect it before any assignment')
            rows=s.call('template_before','component','list_blueprint_components',dict(blueprint_name=bp))['data']['components']
            found=[r for r in rows if r['component_name']==selected and r['component_class_path']=='/Script/Niagara.NiagaraComponent']
            require(len(found)==1,'Exactly one own native Niagara component required')
            if binding_rows is None:binding_rows=rows
            require(rows==binding_rows,'BP component identities changed since preflight; do not assign')
            template=found[0]['template_path']
            s.checkpoint(binding_before=rows,official_template_write_scope='Official Engine SetSystem/SetAsset(reset=False), explicit own SCS reference. Cubeless raw-reference empty-System restriction is not this call path. No atomic/callback-purity guarantee.')
            s.call('assign_system','niagara_component','SetSystem',dict(niagaraComponent={'refPath':template},
                system={'refPath':system+'.'+system.rsplit('/',1)[1]},bResetExistingOverrideParameters=False),mutation=True)
        final=s.call('compile_bound_bp','bp','compile_and_save_blueprint',dict(blueprint_name=bp,save=save),mutation=True)
        require(final['compile'].get('compiled') is True and (not save or final['save'].get('saved') is True),'BP compile/save did not complete')
        after=s.call('bound_asset_readback','component','get_component_property',dict(blueprint_name=bp,component_name=selected,property_name='Asset'))
        s.checkpoint(binding_after=after,bp_compile=final['compile'],bp_save=final['save'])
        # This receipt is checked against its actual current public property
        # representation, rather than interpreting void SetSystem as success.
        require(after['data'].get('property_value')==system+'.'+system.rsplit('/',1)[1],
                'Assigned system exact object reference was not read back')
        components_after=s.call('binding_components_after','component','list_blueprint_components',dict(blueprint_name=bp))['data']['components']
        if mode=='existing':require(components_after==binding_rows,'BP component identity/hierarchy/transform changed during assignment')
        if mode=='wrapper' and 'manifest' in binding:blueprint_run(s,binding['manifest'])
    if visual:
        capture_values={k:v for k,v in visual.items() if k not in ('criteria','create_refinement_ledger','max_iterations')}
        capture_values.setdefault('output_dir',str(s.args.project.resolve().parent/'Saved/MCP/NiagaraReviews'/s.report['operation_id']))
        captured=s.call('fixed_condition_capture','preview','sample_niagara_system_in_preview_lab',dict(system_path=system,**capture_values),mutation=True)
        require(captured['data'].get('operation_complete') is not False,'Capture acceptance is not completion')
        s.checkpoint(capture=captured,visual_status='files_captured; image critique pending')
        samples=captured['data']['legacy_result']['samples']
        require(samples and all(r['success'] for r in samples),'Capture sample failure; preserve actual partial frames')
        criteria=visual.get('criteria',[dict(objective_id='Visible',criterion_id='NonBlank',metric='visible_frame_ratio',comparison='at_least',target=.5)])
        critic=s.call('actual_frame_critique','critic','critique_niagara_preview_evidence',dict(
            candidate_frame_paths_json=json.dumps([r['requested_filepath'] for r in samples]),criteria_json=json.dumps(criteria)))
        s.checkpoint(critic=critic,visual_status='metrics_passed_not_aesthetic_approval' if critic['data']['critic_passed'] else 'metrics_failed; no automatic authoring retry')
        if visual.get('create_refinement_ledger'):
            actual=s.report['execution_compile']['data']['legacy_result']
            compile_evidence=dict(error_count=actual['error_count'],warning_count=actual['warning_count'],
                outstanding_requests=int(actual['outstanding_compilation_requests_after']),wait_timed_out=actual['wait_timed_out'])
            objectives=[]
            for criterion in critic['data']['criterion_results']:
                require(criterion['comparison'] in ('target','at_least','at_most'),'Delta criteria require a separately observed baseline; not synthesized')
                objectives.append(dict(objective_id=criterion['objective_id']+'.'+criterion['criterion_id'],
                    metric=criterion['ledger_metric_key'],comparison=criterion['comparison'],target=criterion['target'],
                    tolerance=criterion['tolerance'],acceptance='target'))
            baseline=dict(state_id='observed_'+s.report['operation_id'],system_path=system,
                metrics=critic['data']['ledger_metric_updates']['candidate'],compile=compile_evidence,
                evidence=dict(frames=critic['data']['candidate_frames'],critic_passed=critic['data']['critic_passed'],
                    compile_receipt_operation=s.report['execution_compile']['operation_id']))
            ledger=s.call('real_evidence_refinement_baseline','planning','create_niagara_refinement_ledger',dict(
                system_path=system,objectives_json=json.dumps(objectives),baseline_state_json=json.dumps(baseline),
                run_id='production_'+s.report['operation_id'],max_iterations=visual.get('max_iterations',4)))
            s.checkpoint(refinement_ledger=ledger['data']['ledger'],
                refinement_scope='Real observed baseline only; selecting a baseline is not quality approval. Candidate mutation/evaluation integration is pending; no automatic retries.')
        require(critic['data']['evidence_complete'] and critic['data']['critic_passed'],
                'Actual rendered evidence did not pass declared image criteria; saved/compiled assets are preserved, no automatic mutation retry')

def check_cpu_compile(waited):
    data=waited['data'];actual=data['legacy_result']
    require(data.get('operation_complete') is True and data.get('compile_idle_observed_at_wait_end') is True,
            'Compile did not reach actual idle; stop without replay')
    require(actual['error_count']==0 and actual['missing_count']==0,'Compile contains error/missing scripts')
    required={'SystemSpawnScript','SystemUpdateScript','ParticleSpawnScript','ParticleSpawnScriptInterpolated','ParticleUpdateScript'}
    scripts=[r for r in actual['scripts'] if r['usage'] in required]
    require(any(r['usage']=='SystemSpawnScript' for r in scripts) and any(r['usage']=='SystemUpdateScript' for r in scripts)
            and any(r['usage'].startswith('ParticleSpawnScript') for r in scripts) and any(r['usage']=='ParticleUpdateScript' for r in scripts),
            'Required CPU preset execution scripts missing')
    require(all(r['compile_status'] in ('NCS_UpToDate','NCS_UpToDateWithWarnings') and r['is_ready_cpu'] and not r['has_error'] for r in scripts),
            'CPU execution scripts are not up to date/ready')

def verify_cpu_compile(s,system,label,*,request_compile=False):
    waited=s.call(label,'compile_status','inspect_niagara_compile_status',dict(system_path=system,
        request_compile=request_compile,wait_for_completion=True))
    check_cpu_compile(waited)
    s.report['execution_compile']=waited
    return waited

def main(argv=None):
    args=arguments(__doc__,argv)
    with Session(args,'niagara',TOOLS) as s:run(s,s.config)

if __name__=='__main__':main()
