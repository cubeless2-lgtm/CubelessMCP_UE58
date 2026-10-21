"""GW Preview workflow4 with original Named/direct/raw presence and actual actor/viewport/file receipts. Positive warmup remains Native unsupported until a real continuation; no synthetic completion."""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import niagara_preview_workflows as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"open_niagara_preview_player": ["system_path"], "get_niagara_preview_player_state": [], "preview_niagara_component_asset_swap": ["replacement_system_path", "actor_name", "actor_label", "component_name", "system_path", "parameter_name_contains", "restore_original_asset", "preserve_override_parameters", "activate_after_swap", "advance_time", "advance_tick_delta", "max_parameters", "max_emitter_instances", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_emitter_runtime", "max_object_properties", "max_array_elements", "max_property_value_chars"], "sample_niagara_system_in_preview_lab": ["system_path", "output_dir", "label", "warmup_times", "views", "warmup_tick_delta", "cleanup_before", "cleanup_after_each", "cleanup_after_all", "location", "scale", "auto_frame", "camera_location", "camera_rotation", "camera_fov", "user_parameter_overrides", "preserve_override_parameters", "allow_current_map_preview"]}
DEFAULTS={"open_niagara_preview_player": {"system_path": ""}, "get_niagara_preview_player_state": {}, "preview_niagara_component_asset_swap": {"actor_name": "", "actor_label": "", "component_name": "", "system_path": "", "parameter_name_contains": "", "restore_original_asset": True, "preserve_override_parameters": True, "activate_after_swap": True, "advance_time": 0.35, "advance_tick_delta": 0.03333333333333333, "max_parameters": 256, "max_emitter_instances": 128, "include_parameter_values": True, "include_raw_bytes": False, "max_raw_bytes": 64, "include_parameter_object_properties": False, "include_emitter_runtime": True, "max_object_properties": 80, "max_array_elements": 16, "max_property_value_chars": 512}, "sample_niagara_system_in_preview_lab": {"output_dir": "", "label": "", "warmup_times": None, "views": None, "warmup_tick_delta": 0.03333333333333333, "cleanup_before": True, "cleanup_after_each": True, "cleanup_after_all": True, "location": None, "scale": None, "auto_frame": True, "camera_location": None, "camera_rotation": None, "camera_fov": None, "user_parameter_overrides": None, "preserve_override_parameters": True, "allow_current_map_preview": False}}
REQUIRED={"open_niagara_preview_player": [], "get_niagara_preview_player_state": [], "preview_niagara_component_asset_swap": ["replacement_system_path"], "sample_niagara_system_in_preview_lab": ["system_path"]}
FIELD_TYPES={"open_niagara_preview_player": {"system_path": "string"}, "get_niagara_preview_player_state": {}, "preview_niagara_component_asset_swap": {"replacement_system_path": "string", "actor_name": "string", "actor_label": "string", "component_name": "string", "system_path": "string", "parameter_name_contains": "string", "restore_original_asset": "boolean", "preserve_override_parameters": "boolean", "activate_after_swap": "boolean", "advance_time": "number", "advance_tick_delta": "number", "max_parameters": "integer", "max_emitter_instances": "integer", "include_parameter_values": "boolean", "include_raw_bytes": "boolean", "max_raw_bytes": "integer", "include_parameter_object_properties": "boolean", "include_emitter_runtime": "boolean", "max_object_properties": "integer", "max_array_elements": "integer", "max_property_value_chars": "integer"}, "sample_niagara_system_in_preview_lab": {"system_path": "string", "output_dir": "string", "label": "string", "warmup_times": "array", "views": "array", "warmup_tick_delta": "number", "cleanup_before": "boolean", "cleanup_after_each": "boolean", "cleanup_after_all": "boolean", "location": "array", "scale": "array", "auto_frame": "boolean", "camera_location": "array", "camera_rotation": "array", "camera_fov": "number", "user_parameter_overrides": "object", "preserve_override_parameters": "boolean", "allow_current_map_preview": "boolean"}}
NULLABLE={"open_niagara_preview_player": [], "get_niagara_preview_player_state": [], "preview_niagara_component_asset_swap": [], "sample_niagara_system_in_preview_lab": ["warmup_times", "views", "location", "scale", "camera_location", "camera_rotation", "camera_fov", "user_parameter_overrides"]}
ITEMS={"open_niagara_preview_player": {}, "get_niagara_preview_player_state": {}, "preview_niagara_component_asset_swap": {}, "sample_niagara_system_in_preview_lab": {"warmup_times": "number", "views": "integer", "location": "number", "scale": "number", "camera_location": "number", "camera_rotation": "number"}}
OBSERVERS=["get_niagara_preview_player_state"]
NO_JOURNAL_METHODS=["get_niagara_preview_player_state"]
WRITERS=["open_niagara_preview_player", "preview_niagara_component_asset_swap", "sample_niagara_system_in_preview_lab"]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_preview_workflows_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_preview_workflows_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_preview_workflows_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_preview_workflows_public_argument_invalid','Unexpected frozen item type')
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
        else:raise OperationError('niagara_preview_workflows_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

def _legacy_exception(method,error):
    message=str(error)
    return dict(success=False,message=message,error=message,error_code='editor_tool_failed',details=dict(exception_type=type(error).__name__))

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('niagara_preview_workflows_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_preview_workflows_entry_invalid','Raw and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={}
    if method==METHODS[0]:
        if values['system_path']:params['system_path']=values['system_path']
    elif method==METHODS[2]:
        params['replacement_system_path']=values['replacement_system_path']
        for field in ('restore_original_asset','preserve_override_parameters','activate_after_swap','include_parameter_values','include_raw_bytes','include_parameter_object_properties','include_emitter_runtime'):params[field]=bool(values[field])
        for field in ('advance_time','advance_tick_delta'):params[field]=float(values[field])
        for field in ('max_parameters','max_emitter_instances','max_raw_bytes','max_object_properties','max_array_elements','max_property_value_chars'):params[field]=int(values[field])
        for field in ('actor_name','actor_label','component_name','system_path','parameter_name_contains'):
            if values[field]:params[field]=values[field]
    elif method==METHODS[3]:
        params.update(system_path=values['system_path'],warmup_tick_delta=float(values['warmup_tick_delta']))
        for field in ('cleanup_before','cleanup_after_each','cleanup_after_all','auto_frame','preserve_override_parameters','allow_current_map_preview'):params[field]=bool(values[field])
        for field in ('output_dir','label','user_parameter_overrides'):
            if values[field]:params[field]=copy.deepcopy(values[field])
        for field in ('warmup_times','views','location','scale','camera_location','camera_rotation'):
            if values[field] is not None:params[field]=copy.deepcopy(values[field])
        if values['camera_fov'] is not None:params['camera_fov']=float(values['camera_fov'])
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),
        original_model_and_body_presence_preserved=True,raw_camera_alias_precedence_Native_owned=True,Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,operational_effect='read_world' if method in NO_JOURNAL_METHODS else 'write',original_native_observer=method in OBSERVERS,
        owned_UI_world_lifetime_and_borrowed_component_restore_Native_owned=True,original_positive_warmup_defaults_preserved=True,PNG_file_write_is_not_asset_package_save=True,source_load_cache_global_effects_may_be_unknown=True,cleanup_prefix_is_not_actual_destroy_completion_proof=True,ReadPixels_internal_render_flush_not_nonblocking=True,GPU_simulation_complete_not_claimed=True,SetAsset_resetFalse_not_full_store_DI_restore_proof=True,original_clean_map_dirty_flag_not_forcibly_cleared=True,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_niagara_preview_workflows_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_niagara_preview_workflows_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_niagara_preview_workflows_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        original_preview_lab_projection_and_partial_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_niagara_preview_workflows_load_getter_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_niagara_preview_workflows_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_niagara_preview_workflows_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_niagara_preview_workflows_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_niagara_preview_workflows_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if native.get('operation_complete') is False or native['data'].get('operation_complete') is False:
        result['data'].update(operation_complete=False,in_flight=True,command_may_still_be_in_flight=True,automatic_action_retry=False,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        result['changes']=[dict(kind=method,attempted=native['mutation_attempted'],changed=None,change_state_known=False)]
        result['retry'].update(automatic_retry=False,duplicate_risk='preview_completion_in_flight_no_replay')
        if callback:callback(result)
        raise OperationError('niagara_preview_workflows_incomplete','Native observation is incomplete; actual partial prefix retained without synthetic completion or replay')
    if not native['success']:raise OperationError(native.get('error_code') or 'niagara_preview_workflows_native_failed',native.get('error') or 'Native niagara_preview_workflows failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('niagara_preview_workflows_postcondition_unverified','Native niagara_preview_workflows postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if native['compile_requested'] or native['compile_attempted'] or native['save_requested'] or native['save_attempted'] or native['compiled'] is not False or native['saved']:
        raise OperationError('niagara_preview_workflows_unexpected_asset_work','Preview Lab has no explicit asset compile/save contract; actual returned history retained')
    if method in OBSERVERS and native['mutation_attempted']:
        raise OperationError('niagara_preview_workflows_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_preview_workflows_input_validation';result['changes']=[]
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

def _named_mcp(method,arguments):return _named(method,arguments,_public_input=True)

def open_niagara_preview_player(system_path=""):
    return _named("open_niagara_preview_player",{"system_path":system_path})

def open_niagara_preview_player_action(result,system_path="",checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"open_niagara_preview_player",{"system_path":system_path},checkpoint_callback,_legacy_command_params,_public_input)

def get_niagara_preview_player_state():
    return _named("get_niagara_preview_player_state",{})

def get_niagara_preview_player_state_action(result,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"get_niagara_preview_player_state",{},checkpoint_callback,_legacy_command_params,_public_input)

def preview_niagara_component_asset_swap(replacement_system_path,actor_name="",actor_label="",component_name="",system_path="",parameter_name_contains="",restore_original_asset=True,preserve_override_parameters=True,activate_after_swap=True,advance_time=0.35,advance_tick_delta=0.03333333333333333,max_parameters=256,max_emitter_instances=128,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_parameter_object_properties=False,include_emitter_runtime=True,max_object_properties=80,max_array_elements=16,max_property_value_chars=512):
    return _named("preview_niagara_component_asset_swap",{"replacement_system_path":replacement_system_path,"actor_name":actor_name,"actor_label":actor_label,"component_name":component_name,"system_path":system_path,"parameter_name_contains":parameter_name_contains,"restore_original_asset":restore_original_asset,"preserve_override_parameters":preserve_override_parameters,"activate_after_swap":activate_after_swap,"advance_time":advance_time,"advance_tick_delta":advance_tick_delta,"max_parameters":max_parameters,"max_emitter_instances":max_emitter_instances,"include_parameter_values":include_parameter_values,"include_raw_bytes":include_raw_bytes,"max_raw_bytes":max_raw_bytes,"include_parameter_object_properties":include_parameter_object_properties,"include_emitter_runtime":include_emitter_runtime,"max_object_properties":max_object_properties,"max_array_elements":max_array_elements,"max_property_value_chars":max_property_value_chars})

def preview_niagara_component_asset_swap_action(result,replacement_system_path,actor_name="",actor_label="",component_name="",system_path="",parameter_name_contains="",restore_original_asset=True,preserve_override_parameters=True,activate_after_swap=True,advance_time=0.35,advance_tick_delta=0.03333333333333333,max_parameters=256,max_emitter_instances=128,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_parameter_object_properties=False,include_emitter_runtime=True,max_object_properties=80,max_array_elements=16,max_property_value_chars=512,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"preview_niagara_component_asset_swap",{"replacement_system_path":replacement_system_path,"actor_name":actor_name,"actor_label":actor_label,"component_name":component_name,"system_path":system_path,"parameter_name_contains":parameter_name_contains,"restore_original_asset":restore_original_asset,"preserve_override_parameters":preserve_override_parameters,"activate_after_swap":activate_after_swap,"advance_time":advance_time,"advance_tick_delta":advance_tick_delta,"max_parameters":max_parameters,"max_emitter_instances":max_emitter_instances,"include_parameter_values":include_parameter_values,"include_raw_bytes":include_raw_bytes,"max_raw_bytes":max_raw_bytes,"include_parameter_object_properties":include_parameter_object_properties,"include_emitter_runtime":include_emitter_runtime,"max_object_properties":max_object_properties,"max_array_elements":max_array_elements,"max_property_value_chars":max_property_value_chars},checkpoint_callback,_legacy_command_params,_public_input)

def sample_niagara_system_in_preview_lab(system_path,output_dir="",label="",warmup_times=None,views=None,warmup_tick_delta=0.03333333333333333,cleanup_before=True,cleanup_after_each=True,cleanup_after_all=True,location=None,scale=None,auto_frame=True,camera_location=None,camera_rotation=None,camera_fov=None,user_parameter_overrides=None,preserve_override_parameters=True,allow_current_map_preview=False):
    return _named("sample_niagara_system_in_preview_lab",{"system_path":system_path,"output_dir":output_dir,"label":label,"warmup_times":warmup_times,"views":views,"warmup_tick_delta":warmup_tick_delta,"cleanup_before":cleanup_before,"cleanup_after_each":cleanup_after_each,"cleanup_after_all":cleanup_after_all,"location":location,"scale":scale,"auto_frame":auto_frame,"camera_location":camera_location,"camera_rotation":camera_rotation,"camera_fov":camera_fov,"user_parameter_overrides":user_parameter_overrides,"preserve_override_parameters":preserve_override_parameters,"allow_current_map_preview":allow_current_map_preview})

def sample_niagara_system_in_preview_lab_action(result,system_path,output_dir="",label="",warmup_times=None,views=None,warmup_tick_delta=0.03333333333333333,cleanup_before=True,cleanup_after_each=True,cleanup_after_all=True,location=None,scale=None,auto_frame=True,camera_location=None,camera_rotation=None,camera_fov=None,user_parameter_overrides=None,preserve_override_parameters=True,allow_current_map_preview=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"sample_niagara_system_in_preview_lab",{"system_path":system_path,"output_dir":output_dir,"label":label,"warmup_times":warmup_times,"views":views,"warmup_tick_delta":warmup_tick_delta,"cleanup_before":cleanup_before,"cleanup_after_each":cleanup_after_each,"cleanup_after_all":cleanup_after_all,"location":location,"scale":scale,"auto_frame":auto_frame,"camera_location":camera_location,"camera_rotation":camera_rotation,"camera_fov":camera_fov,"user_parameter_overrides":user_parameter_overrides,"preserve_override_parameters":preserve_override_parameters,"allow_current_map_preview":allow_current_map_preview},checkpoint_callback,_legacy_command_params,_public_input)

# Domain-specific Named lifecycle: the direct/raw functions above remain synchronous dict APIs.
import hashlib
import os
import time
import uuid
import unreal
from cubeless_mcp_ue58.registration.async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.registration.results import reflected_result
ASYNC_METHODS=["open_niagara_preview_player", "preview_niagara_component_asset_swap", "sample_niagara_system_in_preview_lab", "preview_niagara_system_in_preview_lab"]
_ACTIVE = False
_TICKER_HANDLE = None
_SHUTDOWN_HANDLE = None
_PENDING = {}
_QUARANTINED = {}
_LAST_TERMINAL = {}
_LIFECYCLE_ERRORS = []

def _job_arguments(method,public):
    if method not in ASYNC_METHODS:raise state.OperationError('niagara_preview_workflows_async_method_invalid','Only the dedicated Preview workflow construction job is admitted')
    if method=="preview_niagara_system_in_preview_lab":from . import niagara_preview_lab as niagara_preview_workflows
    else:from . import niagara_preview_workflows
    args,original,legacy=niagara_preview_workflows._arguments(method,public,None,True)
    return dict(arguments=copy.deepcopy(public),args=args,original=original)

def _job_context(result,method,public):
    if method=="preview_niagara_system_in_preview_lab":from . import niagara_preview_lab as niagara_preview_workflows
    else:from . import niagara_preview_workflows
    niagara_preview_workflows._context(result,method,public['args'],public['original'],None)
    result['data'].update(async_native_job=True,async_entry='Named_official_MCP_only',direct_Python_dict_return_shape_preserved=True,direct_RawBatch_execution="Native_pre_writer_unsupported_requires_Root_forwarder",Raw_Batch_async_continuation_implemented=False,warmup_not_bypassed=True,
        no_Python_asset_mutation_compile_or_save=True,shared_compiler_or_render_cancel_attempted=False,actual_official_async_wire_verified=False)
    if method=='preview_niagara_system_in_preview_lab':result['data'].update(single_preview_lab_async_route=True,warmup_positive_prewrite_unsupported_until_real_continuation=False)

class _PreviewWorkflowJournal:
    def __init__(self, method, public, run_id):
        self.result = state.new_result(run_id)
        _job_context(self.result, method, public)
        self.result["stage"] = "before_native_niagara_preview_workflows_async_start"
        self.result["data"].update(native_start_dispatched=False, native_start_returned=False,
            native_snapshot_returned=False, mutation_performed=False, mutation_state_unknown=False,
            pending_native_intent={"method": method, "arguments": copy.deepcopy(public),
                "scope": "future native phases unverified until returned snapshots"})
        self.path = state._journal_path(run_id)
        self.started = time.perf_counter()
        self.owned = False
        if self.path.exists():
            raise state.OperationError("operation_incomplete", "Preview workflow IDs are not replayed; inspect the journal")
        if state._active_operation is not None:
            raise state.OperationError("mutation_busy", "Another operation owns the project slot")
        fingerprint = hashlib.sha256(json.dumps({"name": method, "arguments": public},
            sort_keys=True, allow_nan=False).encode("utf-8")).hexdigest()
        record = {"schema_version": 1, "operation_id": run_id, "operation": method,
            "arguments": copy.deepcopy(public), "fingerprint": fingerprint, "state": "started",
            "result": self.result, "engine_version": unreal.SystemLibrary.get_engine_version(), "process_id": os.getpid()}
        if method=="preview_niagara_system_in_preview_lab" and (state._active_operation is not None or not _ACTIVE or _QUARANTINED):
            raise state.OperationError("niagara_preview_lab_async_bootstrap_changed", "Preview operation slot/lifecycle changed during bootstrap; existing owner preserved, no Native start")
        encoded = json.dumps(record, ensure_ascii=False, sort_keys=True, allow_nan=False)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("x", encoding="utf-8") as stream:
            stream.write(encoded); stream.flush(); os.fsync(stream.fileno())
        state._active_operation = run_id
        self.owned = True

    def checkpoint(self):
        if not self.owned or state._active_operation != self.result["operation_id"]:
            raise state.OperationError("journal_checkpoint_not_owned", "Preview workflow ownership changed; no replay")
        state.checkpoint(self.result)

    def finish(self, recovery):
        if not self.owned or state._active_operation != self.result["operation_id"]:
            raise state.OperationError("journal_finalize_not_owned", "Preview workflow ownership changed before finalization")
        with self.path.open("r", encoding="utf-8") as stream:
            record = json.load(stream)
        if (record.get("operation_id") != self.result["operation_id"] or record.get("state") != "started"
                or record.get("process_id") != os.getpid()):
            raise state.OperationError("journal_finalize_not_owned", "Preview workflow started journal ownership differs")
        record["result"] = copy.deepcopy(self.result)
        record["state"] = "started" if recovery else "complete"
        state._write_record(self.path, record)
        if not recovery:
            state._active_operation = None
            self.owned = False


def _job_adopt(entry,native):
    if entry["method"]=="preview_niagara_system_in_preview_lab":from . import niagara_preview_lab as niagara_preview_workflows
    else:from . import niagara_preview_workflows
    result=entry['journal'].result;was_saved=result['save'].get('saved') is True
    job_id=native['data'].get('native_job_id');prior_id=entry.get('native_job_id')
    if prior_id is not None and job_id!=prior_id:raise state.OperationError('niagara_preview_workflows_job_identity_changed','Snapshot belongs to another job; do not repeat start')
    entry['native_job_id']=job_id
    niagara_preview_workflows._adopt(result,entry['method'],entry['public']['args'],entry['public']['original'],None,native)
    result['save']['saved']=was_saved or native['saved']
    if entry['method']=='preview_niagara_system_in_preview_lab':
        result['compile']['status']=('not_requested' if not native['compile_requested'] else 'not_attempted' if not native['compile_attempted'] else 'compiled' if native['compiled'] is True else 'unknown' if native['compiled'] is None else 'failed')
        result['save']['status']=('saved' if result['save']['saved'] else 'not_requested' if not native['save_requested'] else 'not_attempted' if not native['save_attempted'] else 'save_returned_false')
        result['data']['compile_save_receipt_source']='Native single Preview Lab actual asset-work receipt; PNG file capture is separate'
    _job_context(result,entry['method'],entry['public'])
    result['data'].update(native_snapshot_returned=True,native_terminal_observed=native['data']['native_job_terminal'],adapter_snapshot_calls=entry['snapshot_calls'],adapter_tick_callbacks=entry['tick_callbacks'],command_may_still_be_in_flight=not native['data']['native_job_terminal'],native_job_id=job_id)
    result['data']['deferred_terminal_wait_completion_unverified']=not (native['data']['native_job_terminal'] and native['success'] and native['postcondition_passed'])
    result['stage']=native['stage']
    result['validation'].update(native_terminal_observed=native['data']['native_job_terminal'],preview_restore_file_proof_native_authoritative=True)
    result['retry'].update(automatic_retry=False,duplicate_risk='held_preview_workflow_job_and_journal_no_replay')
    entry['last_native']=copy.deepcopy(native)
    if native['data'].get('unsafe_job_publication_quarantined') is True:
        raise state.OperationError('niagara_preview_workflows_unsafe_publication','Native stopped writers without safe terminal publication; actual prefix retained, no replay')

def _job_checkpoint_native(entry, native, force=False):
    revision = native["data"].get("native_checkpoint_revision")
    if type(revision) not in (int,float) or revision<0 or revision!=int(revision):
        raise state.OperationError("niagara_preview_workflows_async_revision_invalid", "Native meaningful snapshot revision required")
    previous = entry.get("last_checkpoint_revision")
    if previous is not None and revision < previous:
        raise state.OperationError("niagara_preview_workflows_async_revision_regressed", "Native snapshot revision regressed; no replay")
    if force or previous != revision:
        entry["native_checkpoint_attempts"] = entry.get("native_checkpoint_attempts", 0) + 1
        entry["journal"].result["data"]["adapter_native_checkpoint_attempts"] = entry["native_checkpoint_attempts"]
        entry["journal"].checkpoint()
        entry["last_checkpoint_revision"] = revision


def _job_delivery_error(entry, message):
    return (str(message)[:700] + "; operation_id=" + entry["run_id"]
        + "; stage=" + str(entry["journal"].result["stage"])
        + "; journal=" + str(entry["journal"].path))


def _job_cancel_once(entry, reason):
    result = entry["journal"].result
    if not entry.get("job_reference_verified"):
        result["data"]["owned_cancel_not_attempted_unverified_job_reference"] = True
        return None
    if not entry.get("cancel_attempted"):
        entry["cancel_attempted"] = True
        result["data"]["adapter_owned_cancel_attempt_count"] = 1
        try:
            acknowledgement = entry["job"].cancel_job(str(reason)[:1024])
            result["data"].update(cancel_call_returned=True,
                cancel_acknowledgement=acknowledgement if type(acknowledgement) is bool else None,
                cancel_acknowledgement_is_not_rollback_or_global_render_stop=True)
        except Exception as error:
            result["data"]["owned_cancel_error"] = str(error)[:700]
    # One best-effort read remains safe even if cancellation acknowledgement failed.
    entry["snapshot_calls"] += 1
    try:
        native = api.snapshot(entry["job"],entry["method"])
        _job_adopt(entry, native)
        return native
    except Exception as error:
        if getattr(error, "native_snapshot_returned", False):
            result["data"]["cancel_snapshot_undecoded_raw_result"] = getattr(error, "native_raw_result", None)
        _job_preserve_snapshot_error(entry,error)
        result["data"]["cancel_snapshot_error"] = str(error)[:700]
        return None


def _job_seal(entry, recovery):
    """Prepare all result data before the one durable finalization attempt."""
    if entry.get("finalization_attempted"):
        raise state.OperationError("niagara_preview_workflows_async_finalize_already_attempted", "Finalization is not replayed")
    result = entry["journal"].result
    result["data"]["elapsed_seconds"] = round(time.perf_counter() - entry["journal"].started, 6)
    entry["sealed_result"] = copy.deepcopy(result)
    entry["finalization_attempted"] = True
    entry["journal"].finish(recovery)
    entry["journal_finalized"] = True


def _job_delivery_failed(entry, error):
    # A finalization/write may already have committed. Do not mutate the sealed
    # result/journal or observe/cancel/restart the Job after this boundary.
    entry["delivery_error"] = str(error)[:1024]
    entry["observation_only_quarantine"] = False
    _QUARANTINED[entry["run_id"]] = entry
    _PENDING.pop(entry["run_id"], None)
    if not entry.get("cu_completed") and not entry.get("cu_error_attempted") and not (entry.get("method")=="preview_niagara_system_in_preview_lab" and entry.get("single_preview_completion_entered")):
        entry["cu_error_attempted"] = True
        try:
            entry["result_object"].set_error(_job_delivery_error(entry, "Final receipt/durable delivery is uncertain; no replay: " + str(error)))
        except Exception as delivery_error:
            entry["delivery_error"] = str(delivery_error)[:1024]


def _job_deliver(entry):
    if entry.get("cu_delivery_attempted"):
        raise state.OperationError("niagara_preview_workflows_async_delivery_already_attempted", "CU delivery already attempted; no second final")
    if not entry.get("journal_finalized") or type(entry.get("sealed_result")) is not dict:
        raise state.OperationError("niagara_preview_workflows_async_final_not_sealed", "Durable final result is required before typed final delivery")
    entry["cu_delivery_attempted"] = True
    entry["result_object"].set_editor_property("value", reflected_result(copy.deepcopy(entry["sealed_result"])))
    if entry.get("method")=="preview_niagara_system_in_preview_lab":entry["single_preview_completion_entered"]=True
    if entry["result_object"].complete_value() is not True:
        raise state.OperationError("niagara_preview_workflows_async_delivery_failed", "CU completion refused; no retry or result mutation")
    entry["cu_completed"] = True


def _job_terminal(entry, native):
    global _LAST_TERMINAL
    if entry.get("finalization_attempted"):
        return
    result = entry["journal"].result
    _job_adopt(entry, native)
    _job_checkpoint_native(entry, native, force=True)
    result.update(success=native["success"], error_code=native.get("error_code", ""), error=native.get("error", ""))
    if native["success"] and (not native["change_state_known"] or not native["postcondition_passed"] or not native["data"]["operation_complete"]):
        state.fail(result, state.OperationError("niagara_preview_workflows_async_result_unverified", "Native success lacks terminal known preview/restore/file postconditions"))
    if native["success"] and (native["compile_requested"] or native["compile_attempted"] or native["compiled"] is not False or native["save_requested"] or native["save_attempted"] or native["saved"]):
        state.fail(result,state.OperationError("niagara_preview_workflows_unexpected_asset_work","Preview has no explicit asset compile/save contract; actual history retained"))
    recovery = not native["change_state_known"] or native["data"].get("workflow_recovery_required") is True
    result["data"].update(native_terminal_observed=True, workflow_recovery_required=recovery,
        cancel_acknowledgement_is_not_rollback_or_global_render_stop=True,
        final_receipt_sealed_before_delivery=True)
    try:
        _job_seal(entry, recovery)
        if recovery:
            _QUARANTINED[entry["run_id"]] = entry
        _job_deliver(entry)
    except Exception as error:
        _job_delivery_failed(entry, error)
    _LAST_TERMINAL = {"operation_id": entry["run_id"], "success": result["success"],
        "native_terminal_observed": True, "recovery_required": recovery,
        "delivery_error": entry.get("delivery_error", "")}
    _PENDING.pop(entry["run_id"], None)

def _job_preserve_snapshot_error(entry,error):
    result=entry['journal'].result
    if getattr(error,'native_snapshot_returned',False):result['data'].update(native_snapshot_returned=True,undecoded_native_raw_result=getattr(error,'native_raw_result',None))
    native=getattr(error,'native_decoded_result',None)
    if type(native) is dict and native.get('operation')==entry['method'] and native.get('data',{}).get('native_job_id')==entry.get('native_job_id'):
        # A typed same-job receipt may prove a save prefix even if its public terminal publication failed.
        # Keep the receipt as evidence, without treating it as a verified terminal or clearing quarantine.
        result['data']['same_job_snapshot_at_adapter_failure']=copy.deepcopy(native)
        result['save']['saved']=result['save'].get('saved') is True or native.get('saved') is True
        result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'])
        result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'])


def _job_uncertain(entry, error):
    if entry.get("finalization_attempted"):
        _job_delivery_failed(entry, error)
        return
    result = entry["journal"].result
    failure_stage = result["stage"]
    from . import niagara_preview_workflows
    result["data"]["legacy_Named_result"] = niagara_preview_workflows._legacy_exception(entry['method'],error)
    _job_preserve_snapshot_error(entry,error)
    if getattr(error, "native_snapshot_returned", False):
        result["data"].update(native_snapshot_returned=True, undecoded_native_raw_result=getattr(error, "native_raw_result", None))
    last = entry.get("last_native")
    terminal = last is not None and last["data"]["native_job_terminal"]
    if not terminal and entry.get("job_reference_verified"):
        native = _job_cancel_once(entry, "Stop owned next writers after observer/journal failure: " + str(error))
        if native is not None:
            last = native
            terminal = native["data"]["native_job_terminal"]
    state.fail(result, error)  # Preserve the triggering failure; never promote late success.
    no_dispatch = not entry.get("native_start_call_entered")
    recovery = not no_dispatch and (not terminal or not last["change_state_known"])
    result["data"].update(adapter_failure_stage_before_cancel=failure_stage,
        native_terminal_observed=terminal, workflow_recovery_required=recovery,
        last_returned_native_prefix_retained=True)
    if no_dispatch:
        result["data"].update(native_start_dispatched=False, mutation_state_unknown=False, mutation_performed=False)
        result["compile"].update(requested=False, attempted=False, compiled=None)
        result["save"].update(requested=False, attempted=False, saved=False)
    elif not terminal:
        result["data"].update(mutation_state_unknown=True, mutation_performed=None)
        for change in result["changes"]:
            change.update(change_state_known=False, changed=None)
    try:
        entry["journal"].checkpoint()
        if no_dispatch or terminal:
            _job_seal(entry, recovery)
    except Exception as checkpoint_error:
        if entry.get("finalization_attempted"):
            _job_delivery_failed(entry, checkpoint_error)
            return
        result["data"].update(checkpoint_error=str(checkpoint_error)[:700], workflow_recovery_required=True)
        recovery = True
    if recovery:
        entry["observation_only_quarantine"] = entry.get("job_reference_verified") and not terminal
        _QUARANTINED[entry["run_id"]] = entry
    _PENDING.pop(entry["run_id"], None)
    try:
        if no_dispatch or terminal:_job_deliver(entry)
        else:
            entry["cu_delivery_attempted"]=True
            entry["cu_error_attempted"]=True
            entry["result_object"].set_error(_job_delivery_error(entry,"Native terminal is unverified; started journal and ownership retained, no retry"))
    except Exception as delivery_error:
        _job_delivery_failed(entry, delivery_error)


def _job_observe_quarantined(entry):
    if entry.get("finalization_attempted"):
        return
    # If the one owned cancellation could not certify terminal, retain only read
    # observation. No second cancel, writers, saves or second CU delivery occurs.
    try:
        entry["snapshot_calls"] += 1
        native = api.snapshot(entry["job"],entry["method"])
        failure = {k: entry["journal"].result[k] for k in ("success", "error_code", "error")}
        _job_adopt(entry, native)
        entry["journal"].result.update(failure)
        entry["journal"].result["data"].update(workflow_recovery_required=True,
            quarantine_later_native_terminal=native["data"]["native_job_terminal"])
        revision = native["data"].get("native_checkpoint_revision")
        if ("quarantine_checkpoint_attempt_revision" not in entry
                or entry["quarantine_checkpoint_attempt_revision"] != revision):
            # A failed durable write is not retried each frame for the same
            # observed revision. A later actual phase/terminal prefix may be recorded.
            entry["quarantine_checkpoint_attempt_revision"] = revision
            _job_checkpoint_native(entry, native)
        if native["data"]["native_job_terminal"]:
            entry["observation_only_quarantine"] = False
    except Exception as error:
        _job_preserve_snapshot_error(entry,error)
        entry["last_quarantine_observation_error"] = str(error)[:700]


def _job_tick_all(delta_seconds):
    if not _ACTIVE:
        return False
    for entry in tuple(_PENDING.values()):
        try:
            entry["tick_callbacks"] += 1
            entry["snapshot_calls"] += 1
            native = api.snapshot(entry["job"],entry["method"])
            if native["data"]["native_job_terminal"]:
                _job_terminal(entry, native)
            else:
                _job_adopt(entry, native)
                _job_checkpoint_native(entry, native)
        except Exception as error:
            _job_uncertain(entry, error)
    for entry in tuple(_QUARANTINED.values()):
        if entry.get("observation_only_quarantine"):
            _job_observe_quarantined(entry)
    return True


def _job_shutdown():
    # Native module owns teardown. No late asset/job/save/cleanup method here.
    global _ACTIVE
    _ACTIVE = False
    for entry in tuple(_PENDING.values()):
        if entry.get("finalization_attempted"):
            continue
        result = entry["journal"].result
        state.fail(result, state.OperationError("niagara_preview_workflows_async_shutdown_unknown", "Shutdown before verified Native terminal; no late work attempted"))
        result["data"].update(mutation_state_unknown=True, mutation_performed=None,
            native_terminal_observed=False, workflow_recovery_required=True)
        try:
            entry["journal"].checkpoint()
        except Exception:
            pass  # Started journal remains; no repeated write or Engine work.
        _QUARANTINED[entry["run_id"]] = entry
        _PENDING.pop(entry["run_id"], None)


def async_binding_status():
    return {"active": _ACTIVE, "pending_count": len(_PENDING), "quarantined_count": len(_QUARANTINED),
        "ticker_handle_present": _TICKER_HANDLE is not None, "shutdown_handle_present": _SHUTDOWN_HANDLE is not None,
        "last_terminal": copy.deepcopy(_LAST_TERMINAL), "lifecycle_errors": list(_LIFECYCLE_ERRORS),
        "quarantined_operation_ids": sorted(_QUARANTINED),
        "quarantined_journal_paths": {run_id: str(entry["journal"].path) for run_id, entry in _QUARANTINED.items()},
        "observation_only_quarantine_count": sum(bool(entry.get("observation_only_quarantine")) for entry in _QUARANTINED.values()),
        "native_handle_membership_verified": False, "ordinary_async_Batch_integration_verified": False, "Native_bindings": api.job_binding_status()}


def activate():
    global _ACTIVE, _TICKER_HANDLE, _SHUTDOWN_HANDLE
    status=api.job_binding_status()
    if not all(value is True for value in status.values()):raise state.OperationError("niagara_preview_workflows_async_native_jobs_unavailable","Exact dedicated Native start/Job bindings are required")
    if not _ACTIVE:
        if _PENDING or _QUARANTINED or _TICKER_HANDLE is not None or _SHUTDOWN_HANDLE is not None:
            raise state.OperationError("niagara_preview_workflows_async_ownership_retained", "Prior retained ownership requires Root reconciliation")
        _SHUTDOWN_HANDLE = unreal.register_python_shutdown_callback(_job_shutdown)
        try:
            _TICKER_HANDLE = unreal.register_ticker_callback(_job_tick_all, delay=0.0)
        except Exception:
            unreal.unregister_python_shutdown_callback(_SHUTDOWN_HANDLE)
            _SHUTDOWN_HANDLE = None
            raise
        _ACTIVE = True
    return async_binding_status()


def deactivate(reason="Preview workflow toolset deactivated"):
    global _ACTIVE, _TICKER_HANDLE, _SHUTDOWN_HANDLE
    _ACTIVE = False
    for name, handle, remover in (("ticker", _TICKER_HANDLE, unreal.unregister_ticker_callback),
            ("shutdown", _SHUTDOWN_HANDLE, unreal.unregister_python_shutdown_callback)):
        if handle is not None:
            try:
                remover(handle)
                if name == "ticker":
                    _TICKER_HANDLE = None
                else:
                    _SHUTDOWN_HANDLE = None
            except Exception as error:
                _LIFECYCLE_ERRORS.append({"handle": name, "error": str(error)[:700]})
    for entry in tuple(_PENDING.values()):
        native = _job_cancel_once(entry, reason)
        if native is not None and native["data"]["native_job_terminal"]:
            try:
                _job_terminal(entry, native)
            except Exception as error:
                _job_uncertain(entry, error)
        else:
            _job_uncertain(entry, state.OperationError("niagara_preview_workflows_async_cancel_terminal_unverified", "Owned cancellation not terminal verified; no repeat"))
    return async_binding_status()


def begin(method,public):
    result_object = CubelessPythonAsyncOperationResult()
    run_id = uuid.uuid4().hex
    journal = None
    entry = None
    try:
        arguments = _job_arguments(method, public)
        if not _ACTIVE:
            raise state.OperationError("niagara_preview_workflows_async_domain_inactive", "Root registration has not activated the Preview workflow domain")
        if _QUARANTINED:
            raise state.OperationError("niagara_preview_workflows_async_workflow_quarantined", "Retained prior Preview workflow outcome requires Root reconciliation")
        journal = _PreviewWorkflowJournal(method, arguments, run_id)
        entry = {"run_id": run_id, "method": method, "public": arguments, "journal": journal,
            "result_object": result_object, "job": None, "tick_callbacks": 0, "snapshot_calls": 0,
            "native_start_call_entered": False, "job_reference_verified": False,
            "cancel_attempted": False, "last_checkpoint_revision": None}
        _PENDING[run_id] = entry
        journal.result["data"].update(native_start_dispatched=True, native_mutation_attempted=None,
            mutation_performed=None, mutation_state_unknown=True)
        journal.result["compile"].update(requested=None, attempted=None, compiled=None, status="native_start_outcome_unverified")
        journal.result["save"].update(requested=None, attempted=None, saved=None, status="native_start_outcome_unverified")
        journal.checkpoint()  # Covers all future Native phases BEFORE start.
        entry["native_start_call_entered"] = True
        entry["job"] = api.start_job(method,arguments["args"]["params"])
        entry["job_reference_verified"] = True
        journal.result["data"].update(native_start_returned=True)
        entry["snapshot_calls"] += 1
        native = api.snapshot(entry["job"],entry["method"])
        if native["data"]["native_job_terminal"]:
            _job_terminal(entry, native)
        else:
            _job_adopt(entry, native)
            _job_checkpoint_native(entry, native)
    except Exception as error:
        if entry is not None:
            if getattr(error, "native_start_returned", False):
                entry["job"] = getattr(error, "native_returned_job", None)
                journal.result["data"]["native_start_returned"] = True
            if getattr(error, "native_start_dispatched", None) is False:
                entry["native_start_call_entered"] = False
                journal.result["data"].update(native_start_dispatched=False, mutation_state_unknown=False, mutation_performed=False)
                journal.result["compile"].update(requested=False, attempted=False, compiled=None)
                journal.result["save"].update(requested=False, attempted=False, saved=False)
                if method=="preview_niagara_system_in_preview_lab":from . import niagara_preview_lab as niagara_preview_workflows
                else:from . import niagara_preview_workflows
                journal.result["data"]["legacy_Named_result"] = niagara_preview_workflows._legacy_exception(method,error)
                state.fail(journal.result, error)
                try:
                    _job_seal(entry, False)
                    _job_deliver(entry)
                except Exception as delivery_error:
                    _job_uncertain(entry, delivery_error)
                _PENDING.pop(run_id, None)
            else:
                _job_uncertain(entry, error)
        else:
            if method=="preview_niagara_system_in_preview_lab":from . import niagara_preview_lab as niagara_preview_workflows
            else:from . import niagara_preview_workflows
            record = niagara_preview_workflows._rejected(method,error);record["operation_id"]=run_id
            record["data"]["legacy_Named_result"] = copy.deepcopy(getattr(error,"legacy_result",None if hasattr(error,"public_validation_errors") else niagara_preview_workflows._legacy_exception(method,error)))
            record["data"].update(operation=method, admission_rejected=True, native_start_dispatched=False,
                native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result_object.set_editor_property("value", reflected_result(record))
            if result_object.complete_value() is not True:
                result_object.set_error("Preview workflow admission failed and CU completion refused; operation_id=" + run_id
                    + "; stage=" + str(record["stage"]) + "; journal=" + str(state._journal_path(run_id)))
    return result_object


def completed_result(record):
    """Typed async final for pre-dispatch admission rejection only; no journal or Native job."""
    value=CubelessPythonAsyncOperationResult();value.set_editor_property('value',reflected_result(copy.deepcopy(record)))
    if value.complete_value() is not True:value.set_error('GZ pre-dispatch completion refused; no Native job was dispatched')
    return value
