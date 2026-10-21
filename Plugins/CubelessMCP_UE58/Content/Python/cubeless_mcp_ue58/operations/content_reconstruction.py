"""Four content reconstruction APIs with original Named/raw contracts, actual partial/save/report receipts and one parent journal. No transport."""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import content_reconstruction as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"recreate_content_folder_mcp": ["source_root", "target_root", "suffix", "allow_fallback_duplicate", "dry_run", "overwrite_existing", "delete_target_root_first", "remap_references", "compile_blueprints", "log_archive_remap_objects", "refresh_blueprint_nodes", "refresh_failed_blueprint_nodes", "save_assets", "repair_level_actors", "force_repair_level_actors", "compile_passes", "check_editor_log_health", "fail_on_editor_log_issues", "suppress_editor_prompts"], "postprocess_content_folder_mcp": ["source_root", "target_root", "suffix", "dry_run", "remap_references", "compile_blueprints", "refresh_blueprint_nodes", "refresh_failed_blueprint_nodes", "save_assets", "repair_level_actors", "force_repair_level_actors", "compile_passes", "check_editor_log_health", "fail_on_editor_log_issues", "suppress_editor_prompts"], "repair_world_actor_instances_mcp": ["map_path", "source_root", "target_root", "suffix", "dry_run", "save_map", "remap_level_blueprint", "repair_actor_classes", "repair_component_classes", "remap_actor_references", "remove_source_object_map_keys", "check_editor_log_health", "fail_on_editor_log_issues", "suppress_editor_prompts"], "run_content_validation_pipeline_mcp": ["source_root", "target_root", "suffix", "map_path", "continue_on_failure", "run_world_repair", "allow_fallback_duplicate", "dry_run", "overwrite_existing", "delete_target_root_first", "remap_references", "compile_blueprints", "refresh_blueprint_nodes", "refresh_failed_blueprint_nodes", "save_assets", "repair_level_actors", "force_repair_level_actors", "compile_passes", "check_editor_log_health", "fail_on_editor_log_issues", "suppress_editor_prompts", "save_map", "repair_actor_classes", "repair_component_classes", "remap_actor_references", "remove_source_object_map_keys"]}
DEFAULTS={"recreate_content_folder_mcp": {"source_root": "/Game/UltraDynamicSky", "target_root": "/Game/_MCP_Temp/UltraDynamicSky_MCP", "suffix": "_MCP", "allow_fallback_duplicate": True, "dry_run": False, "overwrite_existing": False, "delete_target_root_first": False, "remap_references": True, "compile_blueprints": True, "log_archive_remap_objects": False, "refresh_blueprint_nodes": False, "refresh_failed_blueprint_nodes": True, "save_assets": True, "repair_level_actors": True, "force_repair_level_actors": True, "compile_passes": 2, "check_editor_log_health": True, "fail_on_editor_log_issues": True, "suppress_editor_prompts": True}, "postprocess_content_folder_mcp": {"source_root": "/Game/UltraDynamicSky", "target_root": "/Game/_MCP_Temp/UltraDynamicSky_MCP", "suffix": "_MCP", "dry_run": False, "remap_references": True, "compile_blueprints": True, "refresh_blueprint_nodes": False, "refresh_failed_blueprint_nodes": True, "save_assets": True, "repair_level_actors": False, "force_repair_level_actors": False, "compile_passes": 2, "check_editor_log_health": True, "fail_on_editor_log_issues": True, "suppress_editor_prompts": True}, "repair_world_actor_instances_mcp": {"source_root": "/Game/UltraDynamicSky", "target_root": "/Game/_MCP_Temp/UltraDynamicSky_MCP", "suffix": "_MCP", "dry_run": False, "save_map": True, "remap_level_blueprint": False, "repair_actor_classes": True, "repair_component_classes": True, "remap_actor_references": True, "remove_source_object_map_keys": True, "check_editor_log_health": True, "fail_on_editor_log_issues": True, "suppress_editor_prompts": True}, "run_content_validation_pipeline_mcp": {"suffix": "_MCP", "map_path": "", "continue_on_failure": False, "run_world_repair": False, "allow_fallback_duplicate": True, "dry_run": False, "overwrite_existing": True, "delete_target_root_first": True, "remap_references": True, "compile_blueprints": True, "refresh_blueprint_nodes": False, "refresh_failed_blueprint_nodes": True, "save_assets": True, "repair_level_actors": True, "force_repair_level_actors": True, "compile_passes": 3, "check_editor_log_health": True, "fail_on_editor_log_issues": True, "suppress_editor_prompts": True, "save_map": True, "repair_actor_classes": True, "repair_component_classes": True, "remap_actor_references": True, "remove_source_object_map_keys": True}}
REQUIRED={"recreate_content_folder_mcp": [], "postprocess_content_folder_mcp": [], "repair_world_actor_instances_mcp": ["map_path"], "run_content_validation_pipeline_mcp": ["source_root", "target_root"]}
FIELD_TYPES={"recreate_content_folder_mcp": {"source_root": "string", "target_root": "string", "suffix": "string", "allow_fallback_duplicate": "boolean", "dry_run": "boolean", "overwrite_existing": "boolean", "delete_target_root_first": "boolean", "remap_references": "boolean", "compile_blueprints": "boolean", "log_archive_remap_objects": "boolean", "refresh_blueprint_nodes": "boolean", "refresh_failed_blueprint_nodes": "boolean", "save_assets": "boolean", "repair_level_actors": "boolean", "force_repair_level_actors": "boolean", "compile_passes": "integer", "check_editor_log_health": "boolean", "fail_on_editor_log_issues": "boolean", "suppress_editor_prompts": "boolean"}, "postprocess_content_folder_mcp": {"source_root": "string", "target_root": "string", "suffix": "string", "dry_run": "boolean", "remap_references": "boolean", "compile_blueprints": "boolean", "refresh_blueprint_nodes": "boolean", "refresh_failed_blueprint_nodes": "boolean", "save_assets": "boolean", "repair_level_actors": "boolean", "force_repair_level_actors": "boolean", "compile_passes": "integer", "check_editor_log_health": "boolean", "fail_on_editor_log_issues": "boolean", "suppress_editor_prompts": "boolean"}, "repair_world_actor_instances_mcp": {"map_path": "string", "source_root": "string", "target_root": "string", "suffix": "string", "dry_run": "boolean", "save_map": "boolean", "remap_level_blueprint": "boolean", "repair_actor_classes": "boolean", "repair_component_classes": "boolean", "remap_actor_references": "boolean", "remove_source_object_map_keys": "boolean", "check_editor_log_health": "boolean", "fail_on_editor_log_issues": "boolean", "suppress_editor_prompts": "boolean"}, "run_content_validation_pipeline_mcp": {"source_root": "string", "target_root": "string", "suffix": "string", "map_path": "string", "continue_on_failure": "boolean", "run_world_repair": "boolean", "allow_fallback_duplicate": "boolean", "dry_run": "boolean", "overwrite_existing": "boolean", "delete_target_root_first": "boolean", "remap_references": "boolean", "compile_blueprints": "boolean", "refresh_blueprint_nodes": "boolean", "refresh_failed_blueprint_nodes": "boolean", "save_assets": "boolean", "repair_level_actors": "boolean", "force_repair_level_actors": "boolean", "compile_passes": "integer", "check_editor_log_health": "boolean", "fail_on_editor_log_issues": "boolean", "suppress_editor_prompts": "boolean", "save_map": "boolean", "repair_actor_classes": "boolean", "repair_component_classes": "boolean", "remap_actor_references": "boolean", "remove_source_object_map_keys": "boolean"}}
OBSERVERS=[]
WRITERS=["recreate_content_folder_mcp", "postprocess_content_folder_mcp", "repair_world_actor_instances_mcp", "run_content_validation_pipeline_mcp"]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('content_reconstruction_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('content_reconstruction_public_argument_invalid',field+': '+message)
    error.public_validation_errors=[dict(field=[field],type=kind,message=message)];return error

def _public_bool(field,value):
    if type(value) is bool:return value
    if type(value) in (int,float) and value in (0,1):return bool(value)
    if type(value) is str:
        lower=value.lower()
        if lower in ('true','1','t','yes','y','on'):return True
        if lower in ('false','0','f','no','n','off'):return False
        raise _public_error(field,'bool_parsing','Input should be a valid boolean, unable to interpret input')
    if type(value) in (int,float):raise _public_error(field,'bool_parsing','Input should be a valid boolean, unable to interpret input')
    raise _public_error(field,'bool_type','Input should be a valid boolean')

def _public_int(field,value):
    if type(value) in (int,bool):return int(value)
    if type(value) is float:
        if not math.isfinite(value):raise _public_error(field,'finite_number','Input should be a finite number')
        if not value.is_integer():raise _public_error(field,'int_from_float','Input should be a valid integer, got a number with a fractional part')
        return int(value)
    if type(value) is str:
        stripped=value.strip()
        if stripped.isascii() and re.fullmatch(r'[+-]?[0-9](?:_?[0-9])*(?:\.0+)?',stripped):return int(stripped.replace('_','').split('.')[0])
        raise _public_error(field,'int_parsing','Input should be a valid integer, unable to parse string as an integer')
    raise _public_error(field,'int_type','Input should be a valid integer')

def normalize_public_mcp_arguments(method,arguments):
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('content_reconstruction_public_argument_invalid','Known method and public object required')
    values=copy.deepcopy(DEFAULTS[method])
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:
            if field in REQUIRED[method]:raise _public_error(field,'missing','Field required')
            continue
        value=copy.deepcopy(arguments[field])
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        if FIELD_TYPES[method][field]=='boolean':value=_public_bool(field,value)
        elif FIELD_TYPES[method][field]=='integer':value=_public_int(field,value)
        elif type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        values[field]=value
    return values

LEGACY_MESSAGES={"recreate_content_folder_mcp": "Error recreating content folder: ", "postprocess_content_folder_mcp": "Error postprocessing content folder: ", "repair_world_actor_instances_mcp": "Error repairing world actor instances: ", "run_content_validation_pipeline_mcp": "Error running content validation pipeline: "}

def _legacy_exception(method,error):
    message=LEGACY_MESSAGES[method]+str(error)
    return dict(success=False,message=message,error=message,error_code="project_tool_failed",details=dict(exception_type=type(error).__name__))

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('content_reconstruction_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('content_reconstruction_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params=copy.deepcopy(values)
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,
        original_named_passed_as_received=True,raw_locale_boolean_integer_and_presence_Native_owned=True,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        operational_effect='read_world' if args['params'].get('dry_run') is True else 'write',original_inferred_risk='high',dry_run_report_write_possible=True,registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_content_reconstruction_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_content_reconstruction_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_content_reconstruction_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_compile_save_audit_report_cleanup_child_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_content_reconstruction_compile_save_report_cleanup_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_content_reconstruction_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_content_reconstruction_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_content_reconstruction_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_content_reconstruction_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'content_reconstruction_native_failed',native.get('error') or 'Native content_reconstruction failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('content_reconstruction_postcondition_unverified','Native content_reconstruction postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted'] or native['compiled'] is True or native['saved']):
        raise OperationError('content_reconstruction_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='content_reconstruction_input_validation';result['changes']=[]
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['data'].update(operation=method,admission_rejected=True,native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False)
    if hasattr(error,'legacy_result'):result['data']['legacy_result']=copy.deepcopy(error.legacy_result)
    if hasattr(error,'public_validation_errors'):result['data'].update(public_validation_errors=copy.deepcopy(error.public_validation_errors),original_framework_error_text_reproduced=False,tool_body_invoked=False)
    return result

def _rejected(method,error):return _rejection(state.new_result(),method,error)

def _named(method,arguments,_public_input=False):
    try:args,original,legacy=_arguments(method,arguments,None,_public_input)
    except Exception as error:
        rejected=_rejected(method,error);rejected['data']['legacy_Named_result']=copy.deepcopy(getattr(error,'legacy_result',None if hasattr(error,'public_validation_errors') else _legacy_exception(method,error)));return rejected
    if method in OBSERVERS:
        result=state.new_result()
        try:return _run(result,method,args,original,legacy)
        except Exception as error:return state.fail(result,error)
    return state.mutate(method,arguments,None,lambda result:_run(result,method,args,original,legacy,state.checkpoint))

def _action(result,method,arguments,callback,legacy,public_input=False):
    result["data"]["parent_journal_only"]=True
    try:args,original,legacy=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        _rejection(result,method,error);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,args,original,legacy,callback)

def _named_mcp(method,arguments):return _named(method,arguments,_public_input=True)

def recreate_content_folder_mcp(source_root="/Game/UltraDynamicSky",target_root="/Game/_MCP_Temp/UltraDynamicSky_MCP",suffix="_MCP",allow_fallback_duplicate=True,dry_run=False,overwrite_existing=False,delete_target_root_first=False,remap_references=True,compile_blueprints=True,log_archive_remap_objects=False,refresh_blueprint_nodes=False,refresh_failed_blueprint_nodes=True,save_assets=True,repair_level_actors=True,force_repair_level_actors=True,compile_passes=2,check_editor_log_health=True,fail_on_editor_log_issues=True,suppress_editor_prompts=True):
    return _named("recreate_content_folder_mcp",dict(source_root=source_root,target_root=target_root,suffix=suffix,allow_fallback_duplicate=allow_fallback_duplicate,dry_run=dry_run,overwrite_existing=overwrite_existing,delete_target_root_first=delete_target_root_first,remap_references=remap_references,compile_blueprints=compile_blueprints,log_archive_remap_objects=log_archive_remap_objects,refresh_blueprint_nodes=refresh_blueprint_nodes,refresh_failed_blueprint_nodes=refresh_failed_blueprint_nodes,save_assets=save_assets,repair_level_actors=repair_level_actors,force_repair_level_actors=force_repair_level_actors,compile_passes=compile_passes,check_editor_log_health=check_editor_log_health,fail_on_editor_log_issues=fail_on_editor_log_issues,suppress_editor_prompts=suppress_editor_prompts))

def recreate_content_folder_mcp_action(result,source_root="/Game/UltraDynamicSky",target_root="/Game/_MCP_Temp/UltraDynamicSky_MCP",suffix="_MCP",allow_fallback_duplicate=True,dry_run=False,overwrite_existing=False,delete_target_root_first=False,remap_references=True,compile_blueprints=True,log_archive_remap_objects=False,refresh_blueprint_nodes=False,refresh_failed_blueprint_nodes=True,save_assets=True,repair_level_actors=True,force_repair_level_actors=True,compile_passes=2,check_editor_log_health=True,fail_on_editor_log_issues=True,suppress_editor_prompts=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"recreate_content_folder_mcp",dict(source_root=source_root,target_root=target_root,suffix=suffix,allow_fallback_duplicate=allow_fallback_duplicate,dry_run=dry_run,overwrite_existing=overwrite_existing,delete_target_root_first=delete_target_root_first,remap_references=remap_references,compile_blueprints=compile_blueprints,log_archive_remap_objects=log_archive_remap_objects,refresh_blueprint_nodes=refresh_blueprint_nodes,refresh_failed_blueprint_nodes=refresh_failed_blueprint_nodes,save_assets=save_assets,repair_level_actors=repair_level_actors,force_repair_level_actors=force_repair_level_actors,compile_passes=compile_passes,check_editor_log_health=check_editor_log_health,fail_on_editor_log_issues=fail_on_editor_log_issues,suppress_editor_prompts=suppress_editor_prompts),checkpoint_callback,_legacy_command_params,_public_input)

def postprocess_content_folder_mcp(source_root="/Game/UltraDynamicSky",target_root="/Game/_MCP_Temp/UltraDynamicSky_MCP",suffix="_MCP",dry_run=False,remap_references=True,compile_blueprints=True,refresh_blueprint_nodes=False,refresh_failed_blueprint_nodes=True,save_assets=True,repair_level_actors=False,force_repair_level_actors=False,compile_passes=2,check_editor_log_health=True,fail_on_editor_log_issues=True,suppress_editor_prompts=True):
    return _named("postprocess_content_folder_mcp",dict(source_root=source_root,target_root=target_root,suffix=suffix,dry_run=dry_run,remap_references=remap_references,compile_blueprints=compile_blueprints,refresh_blueprint_nodes=refresh_blueprint_nodes,refresh_failed_blueprint_nodes=refresh_failed_blueprint_nodes,save_assets=save_assets,repair_level_actors=repair_level_actors,force_repair_level_actors=force_repair_level_actors,compile_passes=compile_passes,check_editor_log_health=check_editor_log_health,fail_on_editor_log_issues=fail_on_editor_log_issues,suppress_editor_prompts=suppress_editor_prompts))

def postprocess_content_folder_mcp_action(result,source_root="/Game/UltraDynamicSky",target_root="/Game/_MCP_Temp/UltraDynamicSky_MCP",suffix="_MCP",dry_run=False,remap_references=True,compile_blueprints=True,refresh_blueprint_nodes=False,refresh_failed_blueprint_nodes=True,save_assets=True,repair_level_actors=False,force_repair_level_actors=False,compile_passes=2,check_editor_log_health=True,fail_on_editor_log_issues=True,suppress_editor_prompts=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"postprocess_content_folder_mcp",dict(source_root=source_root,target_root=target_root,suffix=suffix,dry_run=dry_run,remap_references=remap_references,compile_blueprints=compile_blueprints,refresh_blueprint_nodes=refresh_blueprint_nodes,refresh_failed_blueprint_nodes=refresh_failed_blueprint_nodes,save_assets=save_assets,repair_level_actors=repair_level_actors,force_repair_level_actors=force_repair_level_actors,compile_passes=compile_passes,check_editor_log_health=check_editor_log_health,fail_on_editor_log_issues=fail_on_editor_log_issues,suppress_editor_prompts=suppress_editor_prompts),checkpoint_callback,_legacy_command_params,_public_input)

def repair_world_actor_instances_mcp(map_path,source_root="/Game/UltraDynamicSky",target_root="/Game/_MCP_Temp/UltraDynamicSky_MCP",suffix="_MCP",dry_run=False,save_map=True,remap_level_blueprint=False,repair_actor_classes=True,repair_component_classes=True,remap_actor_references=True,remove_source_object_map_keys=True,check_editor_log_health=True,fail_on_editor_log_issues=True,suppress_editor_prompts=True):
    return _named("repair_world_actor_instances_mcp",dict(map_path=map_path,source_root=source_root,target_root=target_root,suffix=suffix,dry_run=dry_run,save_map=save_map,remap_level_blueprint=remap_level_blueprint,repair_actor_classes=repair_actor_classes,repair_component_classes=repair_component_classes,remap_actor_references=remap_actor_references,remove_source_object_map_keys=remove_source_object_map_keys,check_editor_log_health=check_editor_log_health,fail_on_editor_log_issues=fail_on_editor_log_issues,suppress_editor_prompts=suppress_editor_prompts))

def repair_world_actor_instances_mcp_action(result,map_path,source_root="/Game/UltraDynamicSky",target_root="/Game/_MCP_Temp/UltraDynamicSky_MCP",suffix="_MCP",dry_run=False,save_map=True,remap_level_blueprint=False,repair_actor_classes=True,repair_component_classes=True,remap_actor_references=True,remove_source_object_map_keys=True,check_editor_log_health=True,fail_on_editor_log_issues=True,suppress_editor_prompts=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"repair_world_actor_instances_mcp",dict(map_path=map_path,source_root=source_root,target_root=target_root,suffix=suffix,dry_run=dry_run,save_map=save_map,remap_level_blueprint=remap_level_blueprint,repair_actor_classes=repair_actor_classes,repair_component_classes=repair_component_classes,remap_actor_references=remap_actor_references,remove_source_object_map_keys=remove_source_object_map_keys,check_editor_log_health=check_editor_log_health,fail_on_editor_log_issues=fail_on_editor_log_issues,suppress_editor_prompts=suppress_editor_prompts),checkpoint_callback,_legacy_command_params,_public_input)

def run_content_validation_pipeline_mcp(source_root,target_root,suffix="_MCP",map_path="",continue_on_failure=False,run_world_repair=False,allow_fallback_duplicate=True,dry_run=False,overwrite_existing=True,delete_target_root_first=True,remap_references=True,compile_blueprints=True,refresh_blueprint_nodes=False,refresh_failed_blueprint_nodes=True,save_assets=True,repair_level_actors=True,force_repair_level_actors=True,compile_passes=3,check_editor_log_health=True,fail_on_editor_log_issues=True,suppress_editor_prompts=True,save_map=True,repair_actor_classes=True,repair_component_classes=True,remap_actor_references=True,remove_source_object_map_keys=True):
    return _named("run_content_validation_pipeline_mcp",dict(source_root=source_root,target_root=target_root,suffix=suffix,map_path=map_path,continue_on_failure=continue_on_failure,run_world_repair=run_world_repair,allow_fallback_duplicate=allow_fallback_duplicate,dry_run=dry_run,overwrite_existing=overwrite_existing,delete_target_root_first=delete_target_root_first,remap_references=remap_references,compile_blueprints=compile_blueprints,refresh_blueprint_nodes=refresh_blueprint_nodes,refresh_failed_blueprint_nodes=refresh_failed_blueprint_nodes,save_assets=save_assets,repair_level_actors=repair_level_actors,force_repair_level_actors=force_repair_level_actors,compile_passes=compile_passes,check_editor_log_health=check_editor_log_health,fail_on_editor_log_issues=fail_on_editor_log_issues,suppress_editor_prompts=suppress_editor_prompts,save_map=save_map,repair_actor_classes=repair_actor_classes,repair_component_classes=repair_component_classes,remap_actor_references=remap_actor_references,remove_source_object_map_keys=remove_source_object_map_keys))

def run_content_validation_pipeline_mcp_action(result,source_root,target_root,suffix="_MCP",map_path="",continue_on_failure=False,run_world_repair=False,allow_fallback_duplicate=True,dry_run=False,overwrite_existing=True,delete_target_root_first=True,remap_references=True,compile_blueprints=True,refresh_blueprint_nodes=False,refresh_failed_blueprint_nodes=True,save_assets=True,repair_level_actors=True,force_repair_level_actors=True,compile_passes=3,check_editor_log_health=True,fail_on_editor_log_issues=True,suppress_editor_prompts=True,save_map=True,repair_actor_classes=True,repair_component_classes=True,remap_actor_references=True,remove_source_object_map_keys=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"run_content_validation_pipeline_mcp",dict(source_root=source_root,target_root=target_root,suffix=suffix,map_path=map_path,continue_on_failure=continue_on_failure,run_world_repair=run_world_repair,allow_fallback_duplicate=allow_fallback_duplicate,dry_run=dry_run,overwrite_existing=overwrite_existing,delete_target_root_first=delete_target_root_first,remap_references=remap_references,compile_blueprints=compile_blueprints,refresh_blueprint_nodes=refresh_blueprint_nodes,refresh_failed_blueprint_nodes=refresh_failed_blueprint_nodes,save_assets=save_assets,repair_level_actors=repair_level_actors,force_repair_level_actors=force_repair_level_actors,compile_passes=compile_passes,check_editor_log_health=check_editor_log_health,fail_on_editor_log_issues=fail_on_editor_log_issues,suppress_editor_prompts=suppress_editor_prompts,save_map=save_map,repair_actor_classes=repair_actor_classes,repair_component_classes=repair_component_classes,remap_actor_references=remap_actor_references,remove_source_object_map_keys=remove_source_object_map_keys),checkpoint_callback,_legacy_command_params,_public_input)

