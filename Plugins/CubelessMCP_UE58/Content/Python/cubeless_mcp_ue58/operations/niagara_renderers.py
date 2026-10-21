"""GE renderer lifecycle7. Original public models/body omission and raw presence; Native owns Engine work. No transport or replay."""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import niagara_renderers as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"inspect_niagara_renderers": ["system_path"], "set_niagara_renderer_material": ["system_path", "material_path", "emitter_index", "emitter_name", "renderer_index", "material_slot_index", "allow_source_edit", "save"], "set_niagara_sprite_renderer_settings": ["system_path", "alignment", "facing_mode", "sub_image_size", "emitter_index", "emitter_name", "renderer_index", "allow_source_edit", "save", "request_compile"], "add_niagara_sprite_renderer": ["system_path", "material_path", "emitter_index", "emitter_name", "alignment", "facing_mode", "sub_image_size", "skip_if_existing", "allow_source_edit", "save", "request_compile"], "add_niagara_mesh_renderer": ["system_path", "emitter_index", "emitter_name", "meshes_binding_parameter", "mesh_index_binding", "mesh_paths", "skip_if_existing", "allow_source_edit", "save", "request_compile"], "set_niagara_renderer_enabled": ["system_path", "enabled", "emitter_index", "emitter_name", "renderer_index", "allow_source_edit", "save", "request_compile"], "cleanup_niagara_renderers": ["system_path", "remove_renderer_indices", "clear_meshes_binding_renderer_indices", "expected_renderers", "expected_renderer_count", "emitter_index", "emitter_name", "dry_run", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"]}
DEFAULTS={"inspect_niagara_renderers": {}, "set_niagara_renderer_material": {"emitter_index": None, "emitter_name": "", "renderer_index": None, "material_slot_index": 0, "allow_source_edit": False, "save": True}, "set_niagara_sprite_renderer_settings": {"alignment": "", "facing_mode": "", "sub_image_size": None, "emitter_index": None, "emitter_name": "", "renderer_index": None, "allow_source_edit": False, "save": True, "request_compile": True}, "add_niagara_sprite_renderer": {"emitter_index": None, "emitter_name": "", "alignment": "Automatic", "facing_mode": "FaceCamera", "sub_image_size": None, "skip_if_existing": True, "allow_source_edit": False, "save": True, "request_compile": True}, "add_niagara_mesh_renderer": {"emitter_index": None, "emitter_name": "", "meshes_binding_parameter": "User.MeshList", "mesh_index_binding": "Particles.MeshIndex", "mesh_paths": None, "skip_if_existing": True, "allow_source_edit": False, "save": True, "request_compile": True}, "set_niagara_renderer_enabled": {"emitter_index": None, "emitter_name": "", "renderer_index": None, "allow_source_edit": False, "save": True, "request_compile": True}, "cleanup_niagara_renderers": {"remove_renderer_indices": None, "clear_meshes_binding_renderer_indices": None, "expected_renderers": None, "expected_renderer_count": None, "emitter_index": None, "emitter_name": "", "dry_run": True, "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": False, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1}}
REQUIRED={"inspect_niagara_renderers": ["system_path"], "set_niagara_renderer_material": ["system_path", "material_path"], "set_niagara_sprite_renderer_settings": ["system_path"], "add_niagara_sprite_renderer": ["system_path", "material_path"], "add_niagara_mesh_renderer": ["system_path"], "set_niagara_renderer_enabled": ["system_path", "enabled"], "cleanup_niagara_renderers": ["system_path"]}
FIELD_TYPES={"inspect_niagara_renderers": {"system_path": "string"}, "set_niagara_renderer_material": {"system_path": "string", "material_path": "string", "emitter_index": "integer", "emitter_name": "string", "renderer_index": "integer", "material_slot_index": "integer", "allow_source_edit": "boolean", "save": "boolean"}, "set_niagara_sprite_renderer_settings": {"system_path": "string", "alignment": "string", "facing_mode": "string", "sub_image_size": "array", "emitter_index": "integer", "emitter_name": "string", "renderer_index": "integer", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean"}, "add_niagara_sprite_renderer": {"system_path": "string", "material_path": "string", "emitter_index": "integer", "emitter_name": "string", "alignment": "string", "facing_mode": "string", "sub_image_size": "array", "skip_if_existing": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean"}, "add_niagara_mesh_renderer": {"system_path": "string", "emitter_index": "integer", "emitter_name": "string", "meshes_binding_parameter": "string", "mesh_index_binding": "string", "mesh_paths": "array", "skip_if_existing": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean"}, "set_niagara_renderer_enabled": {"system_path": "string", "enabled": "boolean", "emitter_index": "integer", "emitter_name": "string", "renderer_index": "integer", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean"}, "cleanup_niagara_renderers": {"system_path": "string", "remove_renderer_indices": "array", "clear_meshes_binding_renderer_indices": "array", "expected_renderers": "array", "expected_renderer_count": "integer", "emitter_index": "integer", "emitter_name": "string", "dry_run": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}}
NULLABLE={"inspect_niagara_renderers": [], "set_niagara_renderer_material": ["emitter_index", "renderer_index"], "set_niagara_sprite_renderer_settings": ["sub_image_size", "emitter_index", "renderer_index"], "add_niagara_sprite_renderer": ["emitter_index", "sub_image_size"], "add_niagara_mesh_renderer": ["emitter_index", "mesh_paths"], "set_niagara_renderer_enabled": ["emitter_index", "renderer_index"], "cleanup_niagara_renderers": ["remove_renderer_indices", "clear_meshes_binding_renderer_indices", "expected_renderers", "expected_renderer_count", "emitter_index"]}
ITEMS={"inspect_niagara_renderers": {}, "set_niagara_renderer_material": {}, "set_niagara_sprite_renderer_settings": {"sub_image_size": "any"}, "add_niagara_sprite_renderer": {"sub_image_size": "any"}, "add_niagara_mesh_renderer": {"mesh_paths": "any"}, "set_niagara_renderer_enabled": {}, "cleanup_niagara_renderers": {"remove_renderer_indices": "integer", "clear_meshes_binding_renderer_indices": "integer", "expected_renderers": "object"}}
OBSERVERS=["inspect_niagara_renderers"]
WRITERS=["set_niagara_renderer_material", "set_niagara_sprite_renderer_settings", "add_niagara_sprite_renderer", "add_niagara_mesh_renderer", "set_niagara_renderer_enabled", "cleanup_niagara_renderers"]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_renderers_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_renderers_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_renderers_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_renderers_public_argument_invalid','Unexpected frozen item type')
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('niagara_renderers_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

LEGACY_MESSAGES={"inspect_niagara_renderers": "Error inspecting Niagara renderers: ", "set_niagara_renderer_material": "Error setting Niagara renderer material: ", "set_niagara_sprite_renderer_settings": "Error setting Niagara sprite renderer settings: ", "add_niagara_sprite_renderer": "Error adding Niagara sprite renderer: ", "add_niagara_mesh_renderer": "Error adding Niagara mesh renderer: ", "set_niagara_renderer_enabled": "Error setting Niagara renderer enabled state: ", "cleanup_niagara_renderers": "Error setting Niagara module static switch: "}
def _legacy_exception(method,error):
    message=LEGACY_MESSAGES[method]+str(error)
    return dict(success=False,message=message,error=message,error_code="niagara_tool_failed")

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('niagara_renderers_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_renderers_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method] if field not in NULLABLE[method] or values[field] is not None}
    if method=='set_niagara_sprite_renderer_settings':
        for field in ('alignment','facing_mode','emitter_name'):
            if not values[field]:params.pop(field,None)
    if method=='cleanup_niagara_renderers':
        for field in ('remove_renderer_indices','clear_meshes_binding_renderer_indices','expected_renderers'):params[field]=copy.deepcopy(values[field] or [])
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,
        original_named_body_omission_preserved=True,raw_locale_boolean_integer_and_presence_Native_owned=True,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        operational_effect='read_world' if method in OBSERVERS or method=='cleanup_niagara_renderers' and args['params'].get('dry_run',True) is True else 'write',original_inferred_risk='low' if method in OBSERVERS else 'high' if method=='cleanup_niagara_renderers' else 'medium',nonblocking_cleanup_wait_implementation_pending=not args.get('concrete_native_job',False),registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_niagara_renderers_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_niagara_renderers_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_niagara_renderers_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_compile_wait_save_renderer_topology_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_niagara_renderers_compile_save_report_cleanup_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_niagara_renderers_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_niagara_renderers_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_niagara_renderers_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_niagara_renderers_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'niagara_renderers_native_failed',native.get('error') or 'Native niagara_renderers failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('niagara_renderers_postcondition_unverified','Native niagara_renderers postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted'] or native['compiled'] is True or native['saved']):
        raise OperationError('niagara_renderers_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_renderers_input_validation';result['changes']=[]
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

def completed_result(record):
    from . import niagara_scratch_user_recipes as workflow
    return workflow.completed_result(record)

def _named_mcp(method,arguments):
    if method=='cleanup_niagara_renderers':
        from . import niagara_scratch_user_recipes as workflow
        return workflow._named_module_input_binding(method,arguments)
    return _named(method,arguments,_public_input=True)

def inspect_niagara_renderers(system_path):
    return _named("inspect_niagara_renderers",dict(system_path=system_path))

def inspect_niagara_renderers_action(result,system_path,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"inspect_niagara_renderers",dict(system_path=system_path),checkpoint_callback,_legacy_command_params,_public_input)

def set_niagara_renderer_material(system_path,material_path,emitter_index=None,emitter_name="",renderer_index=None,material_slot_index=0,allow_source_edit=False,save=True):
    return _named("set_niagara_renderer_material",dict(system_path=system_path,material_path=material_path,emitter_index=emitter_index,emitter_name=emitter_name,renderer_index=renderer_index,material_slot_index=material_slot_index,allow_source_edit=allow_source_edit,save=save))

def set_niagara_renderer_material_action(result,system_path,material_path,emitter_index=None,emitter_name="",renderer_index=None,material_slot_index=0,allow_source_edit=False,save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"set_niagara_renderer_material",dict(system_path=system_path,material_path=material_path,emitter_index=emitter_index,emitter_name=emitter_name,renderer_index=renderer_index,material_slot_index=material_slot_index,allow_source_edit=allow_source_edit,save=save),checkpoint_callback,_legacy_command_params,_public_input)

def set_niagara_sprite_renderer_settings(system_path,alignment="",facing_mode="",sub_image_size=None,emitter_index=None,emitter_name="",renderer_index=None,allow_source_edit=False,save=True,request_compile=True):
    return _named("set_niagara_sprite_renderer_settings",dict(system_path=system_path,alignment=alignment,facing_mode=facing_mode,sub_image_size=sub_image_size,emitter_index=emitter_index,emitter_name=emitter_name,renderer_index=renderer_index,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile))

def set_niagara_sprite_renderer_settings_action(result,system_path,alignment="",facing_mode="",sub_image_size=None,emitter_index=None,emitter_name="",renderer_index=None,allow_source_edit=False,save=True,request_compile=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"set_niagara_sprite_renderer_settings",dict(system_path=system_path,alignment=alignment,facing_mode=facing_mode,sub_image_size=sub_image_size,emitter_index=emitter_index,emitter_name=emitter_name,renderer_index=renderer_index,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile),checkpoint_callback,_legacy_command_params,_public_input)

def add_niagara_sprite_renderer(system_path,material_path,emitter_index=None,emitter_name="",alignment="Automatic",facing_mode="FaceCamera",sub_image_size=None,skip_if_existing=True,allow_source_edit=False,save=True,request_compile=True):
    return _named("add_niagara_sprite_renderer",dict(system_path=system_path,material_path=material_path,emitter_index=emitter_index,emitter_name=emitter_name,alignment=alignment,facing_mode=facing_mode,sub_image_size=sub_image_size,skip_if_existing=skip_if_existing,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile))

def add_niagara_sprite_renderer_action(result,system_path,material_path,emitter_index=None,emitter_name="",alignment="Automatic",facing_mode="FaceCamera",sub_image_size=None,skip_if_existing=True,allow_source_edit=False,save=True,request_compile=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"add_niagara_sprite_renderer",dict(system_path=system_path,material_path=material_path,emitter_index=emitter_index,emitter_name=emitter_name,alignment=alignment,facing_mode=facing_mode,sub_image_size=sub_image_size,skip_if_existing=skip_if_existing,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile),checkpoint_callback,_legacy_command_params,_public_input)

def add_niagara_mesh_renderer(system_path,emitter_index=None,emitter_name="",meshes_binding_parameter="User.MeshList",mesh_index_binding="Particles.MeshIndex",mesh_paths=None,skip_if_existing=True,allow_source_edit=False,save=True,request_compile=True):
    return _named("add_niagara_mesh_renderer",dict(system_path=system_path,emitter_index=emitter_index,emitter_name=emitter_name,meshes_binding_parameter=meshes_binding_parameter,mesh_index_binding=mesh_index_binding,mesh_paths=mesh_paths,skip_if_existing=skip_if_existing,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile))

def add_niagara_mesh_renderer_action(result,system_path,emitter_index=None,emitter_name="",meshes_binding_parameter="User.MeshList",mesh_index_binding="Particles.MeshIndex",mesh_paths=None,skip_if_existing=True,allow_source_edit=False,save=True,request_compile=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"add_niagara_mesh_renderer",dict(system_path=system_path,emitter_index=emitter_index,emitter_name=emitter_name,meshes_binding_parameter=meshes_binding_parameter,mesh_index_binding=mesh_index_binding,mesh_paths=mesh_paths,skip_if_existing=skip_if_existing,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile),checkpoint_callback,_legacy_command_params,_public_input)

def set_niagara_renderer_enabled(system_path,enabled,emitter_index=None,emitter_name="",renderer_index=None,allow_source_edit=False,save=True,request_compile=True):
    return _named("set_niagara_renderer_enabled",dict(system_path=system_path,enabled=enabled,emitter_index=emitter_index,emitter_name=emitter_name,renderer_index=renderer_index,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile))

def set_niagara_renderer_enabled_action(result,system_path,enabled,emitter_index=None,emitter_name="",renderer_index=None,allow_source_edit=False,save=True,request_compile=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"set_niagara_renderer_enabled",dict(system_path=system_path,enabled=enabled,emitter_index=emitter_index,emitter_name=emitter_name,renderer_index=renderer_index,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile),checkpoint_callback,_legacy_command_params,_public_input)

def cleanup_niagara_renderers(system_path,remove_renderer_indices=None,clear_meshes_binding_renderer_indices=None,expected_renderers=None,expected_renderer_count=None,emitter_index=None,emitter_name="",dry_run=True,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1):
    return _named("cleanup_niagara_renderers",dict(system_path=system_path,remove_renderer_indices=remove_renderer_indices,clear_meshes_binding_renderer_indices=clear_meshes_binding_renderer_indices,expected_renderers=expected_renderers,expected_renderer_count=expected_renderer_count,emitter_index=emitter_index,emitter_name=emitter_name,dry_run=dry_run,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def cleanup_niagara_renderers_action(result,system_path,remove_renderer_indices=None,clear_meshes_binding_renderer_indices=None,expected_renderers=None,expected_renderer_count=None,emitter_index=None,emitter_name="",dry_run=True,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"cleanup_niagara_renderers",dict(system_path=system_path,remove_renderer_indices=remove_renderer_indices,clear_meshes_binding_renderer_indices=clear_meshes_binding_renderer_indices,expected_renderers=expected_renderers,expected_renderer_count=expected_renderer_count,emitter_index=emitter_index,emitter_name=emitter_name,dry_run=dry_run,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)

