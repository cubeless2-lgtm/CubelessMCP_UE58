"""GW Preview Lab4 with original Named/direct/raw presence and actual actor/viewport/file receipts. Positive warmup remains Native unsupported until a real continuation; no synthetic completion."""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import niagara_preview_lab as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"get_niagara_preview_lab_state": [], "cleanup_niagara_preview_lab": [], "capture_niagara_preview_lab_view": ["filepath", "view", "auto_frame", "camera_location", "camera_rotation", "camera_fov", "allow_current_map_preview"], "preview_niagara_system_in_preview_lab": ["system_path", "filepath", "view", "label", "warmup_time", "warmup_tick_delta", "cleanup_before", "cleanup_after", "location", "scale", "auto_frame", "camera_location", "camera_rotation", "camera_fov", "user_parameter_overrides", "preserve_override_parameters", "allow_current_map_preview"]}
DEFAULTS={"get_niagara_preview_lab_state": {}, "cleanup_niagara_preview_lab": {}, "capture_niagara_preview_lab_view": {"view": 1, "auto_frame": True, "camera_location": None, "camera_rotation": None, "camera_fov": None, "allow_current_map_preview": False}, "preview_niagara_system_in_preview_lab": {"view": 1, "label": "", "warmup_time": 0.35, "warmup_tick_delta": 0.03333333333333333, "cleanup_before": True, "cleanup_after": True, "location": None, "scale": None, "auto_frame": True, "camera_location": None, "camera_rotation": None, "camera_fov": None, "user_parameter_overrides": None, "preserve_override_parameters": True, "allow_current_map_preview": False}}
REQUIRED={"get_niagara_preview_lab_state": [], "cleanup_niagara_preview_lab": [], "capture_niagara_preview_lab_view": ["filepath"], "preview_niagara_system_in_preview_lab": ["system_path", "filepath"]}
FIELD_TYPES={"get_niagara_preview_lab_state": {}, "cleanup_niagara_preview_lab": {}, "capture_niagara_preview_lab_view": {"filepath": "string", "view": "integer", "auto_frame": "boolean", "camera_location": "array", "camera_rotation": "array", "camera_fov": "number", "allow_current_map_preview": "boolean"}, "preview_niagara_system_in_preview_lab": {"system_path": "string", "filepath": "string", "view": "integer", "label": "string", "warmup_time": "number", "warmup_tick_delta": "number", "cleanup_before": "boolean", "cleanup_after": "boolean", "location": "array", "scale": "array", "auto_frame": "boolean", "camera_location": "array", "camera_rotation": "array", "camera_fov": "number", "user_parameter_overrides": "object", "preserve_override_parameters": "boolean", "allow_current_map_preview": "boolean"}}
NULLABLE={"get_niagara_preview_lab_state": [], "cleanup_niagara_preview_lab": [], "capture_niagara_preview_lab_view": ["camera_location", "camera_rotation", "camera_fov"], "preview_niagara_system_in_preview_lab": ["location", "scale", "camera_location", "camera_rotation", "camera_fov", "user_parameter_overrides"]}
ITEMS={"get_niagara_preview_lab_state": {}, "cleanup_niagara_preview_lab": {}, "capture_niagara_preview_lab_view": {"camera_location": "number", "camera_rotation": "number"}, "preview_niagara_system_in_preview_lab": {"location": "number", "scale": "number", "camera_location": "number", "camera_rotation": "number"}}
OBSERVERS=["get_niagara_preview_lab_state"]
NO_JOURNAL_METHODS=["get_niagara_preview_lab_state"]
WRITERS=["cleanup_niagara_preview_lab", "capture_niagara_preview_lab_view", "preview_niagara_system_in_preview_lab"]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_preview_lab_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_preview_lab_public_argument_invalid',field+': '+message)
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

def _public_float(field,value):
    if type(value) in (int,float,bool):
        try:return float(value)
        except (ValueError,OverflowError):raise _public_error(field,'float_type','Input should be a valid number')
    if type(value) is str:
        try:
            if not value.isascii():raise ValueError()
            return float(value)
        except (ValueError,OverflowError):raise _public_error(field,'float_parsing','Input should be a valid number, unable to parse string as a number')
    raise _public_error(field,'float_type','Input should be a valid number')

def normalize_public_mcp_arguments(method,arguments):
    """Field-specific original FastMCP preparse/Pydantic profile; no Engine/raw coercion clone."""
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_preview_lab_public_argument_invalid','Known method and public argument object required')
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
        kind=FIELD_TYPES[method][field]
        if value is None and field in NULLABLE[method]:pass
        elif kind=='array':
            if not isinstance(value,(list,tuple)):raise _public_error(field,'list_type','Input should be a valid list')
            rows=[]
            for index,item in enumerate(value):
                try:
                    if ITEMS[method][field]=='string':
                        if type(item) is not str:raise _public_error(field,'string_type','Input should be a valid string')
                    elif ITEMS[method][field]=='integer':item=_public_int(field,item)
                    elif ITEMS[method][field]=='object':
                        if type(item) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
                    elif ITEMS[method][field]=='number':item=_public_float(field,item)
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_preview_lab_public_argument_invalid','Unexpected frozen item type')
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='object':
            if type(value) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
            if any(type(key) is not str for key in value):raise _public_error(field,'string_type','Dictionary keys should be valid strings')
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('niagara_preview_lab_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

def _legacy_exception(method,error):
    message=str(error)
    return dict(success=False,message=message,error=message,error_code='editor_tool_failed',details=dict(exception_type=type(error).__name__))

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('niagara_preview_lab_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_preview_lab_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={}
    if method in METHODS[2:]:
        params=dict(filepath=values['filepath'],view=int(values['view']),auto_frame=bool(values['auto_frame']),allow_current_map_preview=bool(values['allow_current_map_preview']))
        for field in ('camera_location','camera_rotation'):
            if values[field] is not None:params[field]=copy.deepcopy(values[field])
        if values['camera_fov'] is not None:params['camera_fov']=float(values['camera_fov'])
    if method==METHODS[3]:
        params.update(system_path=values['system_path'],warmup_time=float(values['warmup_time']),warmup_tick_delta=float(values['warmup_tick_delta']),cleanup_before=bool(values['cleanup_before']),cleanup_after=bool(values['cleanup_after']),preserve_override_parameters=bool(values['preserve_override_parameters']))
        if values['label']:params['label']=values['label']
        for field in ('location','scale'):
            if values[field] is not None:params[field]=copy.deepcopy(values[field])
        if values['user_parameter_overrides']:params['user_parameter_overrides']=copy.deepcopy(values['user_parameter_overrides'])
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),
        original_model_and_body_presence_preserved=True,raw_camera_alias_precedence_Native_owned=True,Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,operational_effect='read_world' if method in NO_JOURNAL_METHODS else 'write',original_inferred_effect='read' if method==METHODS[0] else 'destructive' if method==METHODS[1] else 'write',original_inferred_risk='low' if method==METHODS[0] else 'high' if method==METHODS[1] else 'medium',original_native_observer=method in OBSERVERS,
        preview_actor_component_viewport_and_file_stages_Native_owned=True,original_warmup_default_0_35_preserved=True,warmup_positive_prewrite_unsupported_until_real_continuation=not (method==METHODS[3] and args['entry']=='named_mcp' and args['params'].get('warmup_time',0)>0),PNG_file_write_is_not_asset_package_save=True,source_system_load_cache_global_effects_may_be_unknown=True,cleanup_prefix_labels_are_not_actual_destroy_completion_proof=True,ReadPixels_internal_render_flush_not_nonblocking=True,GPU_simulation_complete_not_claimed=True,registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_niagara_preview_lab_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_niagara_preview_lab_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_niagara_preview_lab_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        original_preview_lab_projection_and_partial_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_niagara_preview_lab_load_getter_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_niagara_preview_lab_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_niagara_preview_lab_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_niagara_preview_lab_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_niagara_preview_lab_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if native.get('operation_complete') is False or native['data'].get('operation_complete') is False:
        result['data'].update(operation_complete=False,in_flight=True,command_may_still_be_in_flight=True,automatic_action_retry=False,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        result['changes']=[dict(kind=method,attempted=native['mutation_attempted'],changed=None,change_state_known=False)]
        result['retry'].update(automatic_retry=False,duplicate_risk='preview_completion_in_flight_no_replay')
        if callback:callback(result)
        raise OperationError('niagara_preview_lab_incomplete','Native observation is incomplete; actual partial prefix retained without synthetic completion or replay')
    if not native['success']:raise OperationError(native.get('error_code') or 'niagara_preview_lab_native_failed',native.get('error') or 'Native niagara_preview_lab failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('niagara_preview_lab_postcondition_unverified','Native niagara_preview_lab postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if native['compile_requested'] or native['compile_attempted'] or native['save_requested'] or native['save_attempted'] or native['compiled'] is not False or native['saved']:
        raise OperationError('niagara_preview_lab_unexpected_asset_work','Preview Lab has no explicit asset compile/save contract; actual returned history retained')
    if method in OBSERVERS and native['mutation_attempted']:
        raise OperationError('niagara_preview_lab_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_preview_lab_input_validation';result['changes']=[]
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
    if method in NO_JOURNAL_METHODS:
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

def _named_mcp(method,arguments):
    if method=="preview_niagara_system_in_preview_lab":return _named_preview_lab_async(arguments)
    return _named(method,arguments,_public_input=True)

def completed_result(record):
    from . import niagara_preview_workflows
    return niagara_preview_workflows.completed_result(record)

def _named_preview_lab_async(arguments):
    from . import niagara_preview_workflows
    method="preview_niagara_system_in_preview_lab"
    try:args,original,legacy=_arguments(method,arguments,None,True)
    except Exception as error:
        rejected=_rejected(method,error)
        rejected['data']['legacy_Named_result']=copy.deepcopy(getattr(error,'legacy_result',None if hasattr(error,'public_validation_errors') else _legacy_exception(method,error)))
        return niagara_preview_workflows.completed_result(rejected)
    if args['params']['warmup_time']>0:return niagara_preview_workflows.begin(method,arguments)
    # Explicit zero/negative warmup retains the existing synchronous Native path.
    return niagara_preview_workflows.completed_result(_named(method,arguments,_public_input=True))

def get_niagara_preview_lab_state():
    return _named("get_niagara_preview_lab_state",dict())

def get_niagara_preview_lab_state_action(result,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"get_niagara_preview_lab_state",dict(),checkpoint_callback,_legacy_command_params,_public_input)

def cleanup_niagara_preview_lab():
    return _named("cleanup_niagara_preview_lab",dict())

def cleanup_niagara_preview_lab_action(result,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"cleanup_niagara_preview_lab",dict(),checkpoint_callback,_legacy_command_params,_public_input)

def capture_niagara_preview_lab_view(filepath,view=1,auto_frame=True,camera_location=None,camera_rotation=None,camera_fov=None,allow_current_map_preview=False):
    return _named("capture_niagara_preview_lab_view",dict(filepath=filepath,view=view,auto_frame=auto_frame,camera_location=camera_location,camera_rotation=camera_rotation,camera_fov=camera_fov,allow_current_map_preview=allow_current_map_preview))

def capture_niagara_preview_lab_view_action(result,filepath,view=1,auto_frame=True,camera_location=None,camera_rotation=None,camera_fov=None,allow_current_map_preview=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"capture_niagara_preview_lab_view",dict(filepath=filepath,view=view,auto_frame=auto_frame,camera_location=camera_location,camera_rotation=camera_rotation,camera_fov=camera_fov,allow_current_map_preview=allow_current_map_preview),checkpoint_callback,_legacy_command_params,_public_input)

def preview_niagara_system_in_preview_lab(system_path,filepath,view=1,label="",warmup_time=0.35,warmup_tick_delta=1.0 / 30.0,cleanup_before=True,cleanup_after=True,location=None,scale=None,auto_frame=True,camera_location=None,camera_rotation=None,camera_fov=None,user_parameter_overrides=None,preserve_override_parameters=True,allow_current_map_preview=False):
    return _named("preview_niagara_system_in_preview_lab",dict(system_path=system_path,filepath=filepath,view=view,label=label,warmup_time=warmup_time,warmup_tick_delta=warmup_tick_delta,cleanup_before=cleanup_before,cleanup_after=cleanup_after,location=location,scale=scale,auto_frame=auto_frame,camera_location=camera_location,camera_rotation=camera_rotation,camera_fov=camera_fov,user_parameter_overrides=user_parameter_overrides,preserve_override_parameters=preserve_override_parameters,allow_current_map_preview=allow_current_map_preview))

def preview_niagara_system_in_preview_lab_action(result,system_path,filepath,view=1,label="",warmup_time=0.35,warmup_tick_delta=1.0 / 30.0,cleanup_before=True,cleanup_after=True,location=None,scale=None,auto_frame=True,camera_location=None,camera_rotation=None,camera_fov=None,user_parameter_overrides=None,preserve_override_parameters=True,allow_current_map_preview=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"preview_niagara_system_in_preview_lab",dict(system_path=system_path,filepath=filepath,view=view,label=label,warmup_time=warmup_time,warmup_tick_delta=warmup_tick_delta,cleanup_before=cleanup_before,cleanup_after=cleanup_after,location=location,scale=scale,auto_frame=auto_frame,camera_location=camera_location,camera_rotation=camera_rotation,camera_fov=camera_fov,user_parameter_overrides=user_parameter_overrides,preserve_override_parameters=preserve_override_parameters,allow_current_map_preview=allow_current_map_preview),checkpoint_callback,_legacy_command_params,_public_input)

