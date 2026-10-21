"""GP Niagara factory bootstrap4 with original Named/direct/raw and exact Native prefixes. No transport, simulation creation, synthetic readiness or replay."""
import copy
import hashlib
import os
import time
import uuid
import unreal
from cubeless_mcp_ue58.registration.async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.registration.results import reflected_result
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import niagara_factory_bootstrap as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"inspect_niagara_factory_bootstrap_defaults": ["kind", "create_default_system_nodes", "add_default_modules_and_renderers", "include_pins", "max_function_calls"], "create_empty_niagara_system": ["target_system_path", "allow_non_temp_path", "overwrite_existing", "save", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"], "create_empty_niagara_emitter": ["target_emitter_path", "add_default_modules_and_renderers", "allow_non_temp_path", "overwrite_existing", "save"], "create_basic_sprite_niagara_system": ["target_system_path", "emitter_name", "material_path", "allow_non_temp_path", "overwrite_existing", "save", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"]}
DEFAULTS={"inspect_niagara_factory_bootstrap_defaults": {"kind": "all", "create_default_system_nodes": True, "add_default_modules_and_renderers": False, "include_pins": False, "max_function_calls": 80}, "create_empty_niagara_system": {"allow_non_temp_path": False, "overwrite_existing": False, "save": True, "wait_for_completion": True, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1}, "create_empty_niagara_emitter": {"add_default_modules_and_renderers": False, "allow_non_temp_path": False, "overwrite_existing": False, "save": True}, "create_basic_sprite_niagara_system": {"emitter_name": "Emitter", "material_path": "", "allow_non_temp_path": False, "overwrite_existing": False, "save": True, "wait_for_completion": True, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1}}
REQUIRED={"inspect_niagara_factory_bootstrap_defaults": [], "create_empty_niagara_system": ["target_system_path"], "create_empty_niagara_emitter": ["target_emitter_path"], "create_basic_sprite_niagara_system": ["target_system_path"]}
FIELD_TYPES={"inspect_niagara_factory_bootstrap_defaults": {"kind": "string", "create_default_system_nodes": "boolean", "add_default_modules_and_renderers": "boolean", "include_pins": "boolean", "max_function_calls": "integer"}, "create_empty_niagara_system": {"target_system_path": "string", "allow_non_temp_path": "boolean", "overwrite_existing": "boolean", "save": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}, "create_empty_niagara_emitter": {"target_emitter_path": "string", "add_default_modules_and_renderers": "boolean", "allow_non_temp_path": "boolean", "overwrite_existing": "boolean", "save": "boolean"}, "create_basic_sprite_niagara_system": {"target_system_path": "string", "emitter_name": "string", "material_path": "string", "allow_non_temp_path": "boolean", "overwrite_existing": "boolean", "save": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}}
NULLABLE={"inspect_niagara_factory_bootstrap_defaults": [], "create_empty_niagara_system": [], "create_empty_niagara_emitter": [], "create_basic_sprite_niagara_system": []}
ITEMS={"inspect_niagara_factory_bootstrap_defaults": {}, "create_empty_niagara_system": {}, "create_empty_niagara_emitter": {}, "create_basic_sprite_niagara_system": {}}
OBSERVERS=["inspect_niagara_factory_bootstrap_defaults"]
NO_JOURNAL_METHODS=["inspect_niagara_factory_bootstrap_defaults"]
WRITERS=["create_empty_niagara_system", "create_empty_niagara_emitter", "create_basic_sprite_niagara_system"]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_factory_bootstrap_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_factory_bootstrap_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_factory_bootstrap_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_factory_bootstrap_public_argument_invalid','Unexpected frozen item type')
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
        else:raise OperationError('niagara_factory_bootstrap_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

LEGACY_MESSAGES={"inspect_niagara_factory_bootstrap_defaults": "Error inspecting Niagara factory bootstrap defaults: ", "create_empty_niagara_system": "Error creating empty Niagara system: ", "create_empty_niagara_emitter": "Error creating empty Niagara emitter: ", "create_basic_sprite_niagara_system": "Error creating basic sprite Niagara system: "}
def _legacy_exception(method,error):
    message=LEGACY_MESSAGES[method]+str(error)
    return dict(success=False,message=message,error=message,error_code='niagara_tool_failed')

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('niagara_factory_bootstrap_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_factory_bootstrap_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method]}
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,
        original_named_body_all_fields_passthrough_preserved=True,raw_locale_boolean_integer_and_presence_Native_owned=True,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        operational_effect='read_world' if method in NO_JOURNAL_METHODS else 'write',original_inferred_risk='low' if method in NO_JOURNAL_METHODS else 'medium',original_native_observer=method in OBSERVERS,explicit_transient_factory_creation_Native_owned=True,factory_wait_True_not_faked_or_overridden=True,wait_False_still_factory_compile_enqueue_not_completion=True,actual_save_history_preserved_before_post_guard=True,registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_niagara_factory_bootstrap_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_niagara_factory_bootstrap_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_niagara_factory_bootstrap_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        original_factory_bootstrap_projection_and_limit_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_niagara_factory_bootstrap_load_getter_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_niagara_factory_bootstrap_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_niagara_factory_bootstrap_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_niagara_factory_bootstrap_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_niagara_factory_bootstrap_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if native.get('operation_complete') is False or native['data'].get('operation_complete') is False:
        result['data'].update(operation_complete=False,in_flight=True,command_may_still_be_in_flight=True,automatic_action_retry=False,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        result['changes']=[dict(kind=method,attempted=native['mutation_attempted'],changed=None,change_state_known=False)]
        result['retry'].update(automatic_retry=False,duplicate_risk='deferred_factory_completion_in_flight_no_replay')
        if callback:callback(result)
        raise OperationError('niagara_factory_bootstrap_incomplete','Native observation is incomplete; actual partial prefix retained without synthetic completion or replay')
    if not native['success']:raise OperationError(native.get('error_code') or 'niagara_factory_bootstrap_native_failed',native.get('error') or 'Native niagara_factory_bootstrap failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('niagara_factory_bootstrap_postcondition_unverified','Native niagara_factory_bootstrap postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['data'].get('explicit_persistent_asset_mutation_attempted') is not False or native['data'].get('transient_creation_only') is not True or native['compile_requested'] or native['compile_attempted'] or native['save_requested'] or native['save_attempted'] or native['compiled'] is not False or native['saved']):
        raise OperationError('niagara_factory_bootstrap_observer_unverified','Transient factory observation lacks exact persistent-writer-zero scope or returned compile/save work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_factory_bootstrap_input_validation';result['changes']=[]
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
    if method in ASYNC_METHODS:return _begin(method,arguments)
    return _named(method,arguments,_public_input=True)

def inspect_niagara_factory_bootstrap_defaults(kind="all",create_default_system_nodes=True,add_default_modules_and_renderers=False,include_pins=False,max_function_calls=80):
    return _named("inspect_niagara_factory_bootstrap_defaults",dict(kind=kind,create_default_system_nodes=create_default_system_nodes,add_default_modules_and_renderers=add_default_modules_and_renderers,include_pins=include_pins,max_function_calls=max_function_calls))

def inspect_niagara_factory_bootstrap_defaults_action(result,kind="all",create_default_system_nodes=True,add_default_modules_and_renderers=False,include_pins=False,max_function_calls=80,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"inspect_niagara_factory_bootstrap_defaults",dict(kind=kind,create_default_system_nodes=create_default_system_nodes,add_default_modules_and_renderers=add_default_modules_and_renderers,include_pins=include_pins,max_function_calls=max_function_calls),checkpoint_callback,_legacy_command_params,_public_input)

def create_empty_niagara_system(target_system_path,allow_non_temp_path=False,overwrite_existing=False,save=True,wait_for_completion=True,timeout_seconds=10.0,poll_interval_seconds=0.1):
    return _named("create_empty_niagara_system",dict(target_system_path=target_system_path,allow_non_temp_path=allow_non_temp_path,overwrite_existing=overwrite_existing,save=save,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def create_empty_niagara_system_action(result,target_system_path,allow_non_temp_path=False,overwrite_existing=False,save=True,wait_for_completion=True,timeout_seconds=10.0,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"create_empty_niagara_system",dict(target_system_path=target_system_path,allow_non_temp_path=allow_non_temp_path,overwrite_existing=overwrite_existing,save=save,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)

def create_empty_niagara_emitter(target_emitter_path,add_default_modules_and_renderers=False,allow_non_temp_path=False,overwrite_existing=False,save=True):
    return _named("create_empty_niagara_emitter",dict(target_emitter_path=target_emitter_path,add_default_modules_and_renderers=add_default_modules_and_renderers,allow_non_temp_path=allow_non_temp_path,overwrite_existing=overwrite_existing,save=save))

def create_empty_niagara_emitter_action(result,target_emitter_path,add_default_modules_and_renderers=False,allow_non_temp_path=False,overwrite_existing=False,save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"create_empty_niagara_emitter",dict(target_emitter_path=target_emitter_path,add_default_modules_and_renderers=add_default_modules_and_renderers,allow_non_temp_path=allow_non_temp_path,overwrite_existing=overwrite_existing,save=save),checkpoint_callback,_legacy_command_params,_public_input)

def create_basic_sprite_niagara_system(target_system_path,emitter_name="Emitter",material_path="",allow_non_temp_path=False,overwrite_existing=False,save=True,wait_for_completion=True,timeout_seconds=10.0,poll_interval_seconds=0.1):
    return _named("create_basic_sprite_niagara_system",dict(target_system_path=target_system_path,emitter_name=emitter_name,material_path=material_path,allow_non_temp_path=allow_non_temp_path,overwrite_existing=overwrite_existing,save=save,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def create_basic_sprite_niagara_system_action(result,target_system_path,emitter_name="Emitter",material_path="",allow_non_temp_path=False,overwrite_existing=False,save=True,wait_for_completion=True,timeout_seconds=10.0,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"create_basic_sprite_niagara_system",dict(target_system_path=target_system_path,emitter_name=emitter_name,material_path=material_path,allow_non_temp_path=allow_non_temp_path,overwrite_existing=overwrite_existing,save=save,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)



# HP: two concrete factory continuations; direct Python/action paths above stay synchronous.
ASYNC_METHODS=["create_empty_niagara_system","create_basic_sprite_niagara_system"]
_ACTIVE=False
_TICKER_HANDLE=None
_SHUTDOWN_HANDLE=None
_PENDING={}
_QUARANTINED={}
_LAST_TERMINAL={}
_LIFECYCLE_ERRORS=[]

def _async_arguments(method,public):
    if method not in ASYNC_METHODS:raise OperationError("niagara_factory_bootstrap_async_method_invalid","Only the original System factory2 Named jobs are admitted")
    args,original,legacy=_arguments(method,public,None,True)
    return dict(arguments=copy.deepcopy(public),args=args,original=original)

def _async_context(result,method,public,legacy=None,batch_entry=False):
    _context(result,method,public["args"],public["original"],legacy)
    result["data"].update(async_native_job=True,async_entry="Raw_or_public_Batch_parent_only" if batch_entry else "Named_official_MCP_only",direct_Python_dict_contract_preserved=True,Raw_Batch_async_continuation_implemented=batch_entry,mandatory_wait_not_bypassed=True,
        no_Python_asset_mutation_compile_or_save=True,global_compiler_cancel_attempted=False,actual_official_async_wire_verified=False)

class _FactoryBootstrapJournal:
    def __init__(self, method, public, run_id):
        self.result = state.new_result(run_id)
        _async_context(self.result, method, public)
        self.result["stage"] = "before_native_niagara_factory_bootstrap_async_start"
        self.result["data"].update(native_start_dispatched=False, native_start_returned=False,
            native_snapshot_returned=False, mutation_performed=False, mutation_state_unknown=False,
            pending_native_intent={"method": method, "arguments": copy.deepcopy(public),
                "scope": "future native phases unverified until returned snapshots"})
        self.path = state._journal_path(run_id)
        self.started = time.perf_counter()
        self.owned = False
        if self.path.exists():
            raise state.OperationError("operation_incomplete", "Factory bootstrap IDs are not replayed; inspect the journal")
        if state._active_operation is not None:
            raise state.OperationError("mutation_busy", "Another operation owns the project slot")
        fingerprint = hashlib.sha256(json.dumps({"name": method, "arguments": public},
            sort_keys=True, allow_nan=False).encode("utf-8")).hexdigest()
        record = {"schema_version": 1, "operation_id": run_id, "operation": method,
            "arguments": copy.deepcopy(public), "fingerprint": fingerprint, "state": "started",
            "result": self.result, "engine_version": unreal.SystemLibrary.get_engine_version(), "process_id": os.getpid()}
        encoded = json.dumps(record, ensure_ascii=False, sort_keys=True, allow_nan=False)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("x", encoding="utf-8") as stream:
            stream.write(encoded); stream.flush(); os.fsync(stream.fileno())
        state._active_operation = run_id
        self.owned = True

    def checkpoint(self):
        if not self.owned or state._active_operation != self.result["operation_id"]:
            raise state.OperationError("journal_checkpoint_not_owned", "Factory bootstrap ownership changed; no replay")
        state.checkpoint(self.result)

    def finish(self, recovery):
        if not self.owned or state._active_operation != self.result["operation_id"]:
            raise state.OperationError("journal_finalize_not_owned", "Factory bootstrap ownership changed before finalization")
        with self.path.open("r", encoding="utf-8") as stream:
            record = json.load(stream)
        if (record.get("operation_id") != self.result["operation_id"] or record.get("state") != "started"
                or record.get("process_id") != os.getpid()):
            raise state.OperationError("journal_finalize_not_owned", "Factory bootstrap started journal ownership differs")
        record["result"] = copy.deepcopy(self.result)
        record["state"] = "started" if recovery else "complete"
        state._write_record(self.path, record)
        if not recovery:
            state._active_operation = None
            self.owned = False


def _async_adopt(entry,native):
    result=entry['journal'].result;was_saved=result['save'].get('saved') is True
    job_id=native['data'].get('native_job_id');prior_id=entry.get('native_job_id')
    if prior_id is not None and job_id!=prior_id:raise state.OperationError('niagara_factory_bootstrap_job_identity_changed','Snapshot belongs to another job; do not repeat start')
    entry['native_job_id']=job_id
    _adopt(result,entry['method'],entry['public']['args'],entry['public']['original'],entry.get('legacy'),native)
    result['save']['saved']=was_saved or native['saved']
    _async_context(result,entry['method'],entry['public'],entry.get('legacy'),entry.get('batch_parent_only',False))
    result['data'].update(native_snapshot_returned=True,native_terminal_observed=native['data']['native_job_terminal'],adapter_snapshot_calls=entry['snapshot_calls'],adapter_tick_callbacks=entry['tick_callbacks'],command_may_still_be_in_flight=not native['data']['native_job_terminal'],native_job_id=job_id)
    result['data']['deferred_terminal_wait_completion_unverified']=not (native['data']['native_job_terminal'] and native['success'] and native['postcondition_passed'])
    result['stage']=native['stage']
    result['validation'].update(native_terminal_observed=native['data']['native_job_terminal'],factory_wait_save_audit_proof_native_authoritative=True)
    result['retry'].update(automatic_retry=False,duplicate_risk='held_factory_bootstrap_job_and_journal_no_replay')
    entry['last_native']=copy.deepcopy(native)

def _checkpoint_native(entry, native, force=False):
    revision = native["data"].get("native_checkpoint_revision")
    if type(revision) not in (int,float) or revision<0 or revision!=int(revision):
        raise state.OperationError("niagara_factory_bootstrap_async_revision_invalid", "Native meaningful snapshot revision required")
    previous = entry.get("last_checkpoint_revision")
    if previous is not None and revision < previous:
        raise state.OperationError("niagara_factory_bootstrap_async_revision_regressed", "Native snapshot revision regressed; no replay")
    if force or previous != revision:
        entry["native_checkpoint_attempts"] = entry.get("native_checkpoint_attempts", 0) + 1
        entry["journal"].result["data"]["adapter_native_checkpoint_attempts"] = entry["native_checkpoint_attempts"]
        entry["journal"].checkpoint()
        entry["last_checkpoint_revision"] = revision


def _delivery_error(entry, message):
    return (str(message)[:700] + "; operation_id=" + entry["run_id"]
        + "; stage=" + str(entry["journal"].result["stage"])
        + "; journal=" + str(entry["journal"].path))


def _cancel_once(entry, reason):
    if entry.get("finalization_attempted"):
        return None
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
                cancel_acknowledgement_is_not_shared_compiler_stop=True)
        except Exception as error:
            result["data"]["owned_cancel_error"] = str(error)[:700]
    # One best-effort read remains safe even if cancellation acknowledgement failed.
    entry["snapshot_calls"] += 1
    try:
        native = api.snapshot(entry["job"],entry["method"])
        _async_adopt(entry, native)
        return native
    except Exception as error:
        if getattr(error, "native_snapshot_returned", False):
            result["data"]["cancel_snapshot_undecoded_raw_result"] = getattr(error, "native_raw_result", None)
        _preserve_snapshot_error(entry,error)
        result["data"]["cancel_snapshot_error"] = str(error)[:700]
        return None


def _seal(entry, recovery):
    """Prepare all result data before the one durable finalization attempt."""
    if entry.get("finalization_attempted"):
        raise state.OperationError("niagara_factory_bootstrap_async_finalize_already_attempted", "Finalization is not replayed")
    result = entry["journal"].result
    result["data"]["elapsed_seconds"] = round(time.perf_counter() - entry["journal"].started, 6)
    entry["sealed_result"] = copy.deepcopy(result)
    entry["finalization_attempted"] = True
    entry["journal"].finish(recovery)
    entry["journal_finalized"] = True


def _delivery_failed(entry, error):
    # A finalization/write may already have committed. Do not mutate the sealed
    # result/journal or observe/cancel/restart the Job after this boundary.
    entry["delivery_error"] = str(error)[:1024]
    entry["observation_only_quarantine"] = False
    _QUARANTINED[entry["run_id"]] = entry
    _PENDING.pop(entry["run_id"], None)
    if (not entry.get("cu_completed") and not entry.get("cu_error_attempted")
            and not entry.get("cu_completion_call_entered")):
        entry["cu_error_attempted"] = True
        try:
            entry["result_object"].set_error(_delivery_error(entry, "Final receipt/durable delivery is uncertain; no replay: " + str(error)))
        except Exception as delivery_error:
            entry["delivery_error"] = str(delivery_error)[:1024]


def _deliver(entry):
    if entry.get("cu_delivery_attempted"):
        raise state.OperationError("niagara_factory_bootstrap_async_delivery_already_attempted", "CU delivery already attempted; no second final")
    if not entry.get("journal_finalized") or type(entry.get("sealed_result")) is not dict:
        raise state.OperationError("niagara_factory_bootstrap_async_final_not_sealed", "Durable final result is required before typed final delivery")
    entry["cu_delivery_attempted"] = True
    entry["result_object"].set_editor_property("value", reflected_result(copy.deepcopy(entry["sealed_result"])))
    # CompleteValue may itself call SetError and return False. Once entered,
    # no fallback completion is safe, even when its return/publication is uncertain.
    entry["cu_completion_call_entered"] = True
    if entry["result_object"].complete_value() is not True:
        raise state.OperationError("niagara_factory_bootstrap_async_delivery_failed", "CU completion refused; no retry or result mutation")
    entry["cu_completed"] = True


def _terminal(entry, native):
    global _LAST_TERMINAL
    if entry.get("finalization_attempted"):
        return
    result = entry["journal"].result
    _async_adopt(entry, native)
    _checkpoint_native(entry, native, force=True)
    result.update(success=native["success"], error_code=native.get("error_code", ""), error=native.get("error", ""))
    if native["success"] and (not native["change_state_known"] or not native["postcondition_passed"] or not native["data"]["operation_complete"]):
        state.fail(result, state.OperationError("niagara_factory_bootstrap_async_result_unverified", "Native success lacks terminal known factory/wait/save/audit postconditions"))
    recovery = not native["change_state_known"]
    result["data"].update(native_terminal_observed=True, workflow_recovery_required=recovery,
        cancel_acknowledgement_is_not_shared_compiler_stop=True,
        final_receipt_sealed_before_delivery=True)
    try:
        _seal(entry, recovery)
        if recovery:
            _QUARANTINED[entry["run_id"]] = entry
        _deliver(entry)
    except Exception as error:
        _delivery_failed(entry, error)
    _LAST_TERMINAL = {"operation_id": entry["run_id"], "success": result["success"],
        "native_terminal_observed": True, "recovery_required": recovery,
        "delivery_error": entry.get("delivery_error", "")}
    _PENDING.pop(entry["run_id"], None)


def _preserve_snapshot_error(entry,error):
    result=entry['journal'].result
    if getattr(error,'native_snapshot_returned',False):result['data'].update(native_snapshot_returned=True,undecoded_native_raw_result=getattr(error,'native_raw_result',None))
    if getattr(error,'native_unsafe_publication_quarantined',False):
        entry['native_publication_quarantined']=True
        result['data'].update(unsafe_job_publication_quarantined=True,native_writers_stopped=True)
    native=getattr(error,'native_decoded_result',None)
    if type(native) is not dict or native.get('operation')!=entry['method'] or type(native.get('data')) is not dict:return
    job_id=native['data'].get('native_job_id');prior=entry.get('native_job_id')
    if (not entry.get('job_reference_verified') or type(job_id) is not str or not job_id or prior is not None and prior!=job_id):return
    # Even the first typed same-UObject receipt can prove a save before safe
    # terminal publication fails. Adopt its actual prefix, never its finality.
    entry['native_job_id']=job_id
    result['data']['same_job_snapshot_at_adapter_failure']=copy.deepcopy(native)
    result['save']['saved']=result['save'].get('saved') is True or native.get('saved') is True
    result['data'].update(native_mutation_attempted=native['mutation_attempted'],execution_mutation_attempted=native['mutation_attempted'])
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'])
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'])


def _uncertain(entry, error):
    if entry.get("finalization_attempted"):
        _delivery_failed(entry, error)
        return
    result = entry["journal"].result
    failure_stage = result["stage"]
    result["data"]["legacy_Named_result"] = _legacy_exception(entry['method'],error)
    _preserve_snapshot_error(entry,error)
    if getattr(error, "native_snapshot_returned", False):
        result["data"].update(native_snapshot_returned=True, undecoded_native_raw_result=getattr(error, "native_raw_result", None))
    last = entry.get("last_native")
    terminal = last is not None and last["data"]["native_job_terminal"]
    if not terminal and entry.get("job_reference_verified") and not entry.get("native_publication_quarantined"):
        native = _cancel_once(entry, "Stop owned next writers after observer/journal failure: " + str(error))
        if native is not None:
            last = native
            terminal = native["data"]["native_job_terminal"]
    state.fail(result, error)  # Preserve the triggering failure; never promote late success.
    no_dispatch = not entry.get("native_start_call_entered")
    recovery = not no_dispatch and (not terminal or not last["change_state_known"] or entry.get("lifecycle_interrupted") and entry.get("cu_delivery_attempted"))
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
            _seal(entry, recovery)
    except Exception as checkpoint_error:
        if entry.get("finalization_attempted"):
            _delivery_failed(entry, checkpoint_error)
            return
        result["data"].update(checkpoint_error=str(checkpoint_error)[:700], workflow_recovery_required=True)
        recovery = True
    if recovery:
        entry["observation_only_quarantine"] = bool(entry.get("job_reference_verified") and not terminal and not entry.get("native_publication_quarantined"))
        _QUARANTINED[entry["run_id"]] = entry
    _PENDING.pop(entry["run_id"], None)
    if entry.get("cu_delivery_attempted"):
        entry["late_native_observation_after_delivery_attempt_retained"] = True
        return
    try:
        if no_dispatch or terminal:_deliver(entry)
        else:
            entry["cu_delivery_attempted"]=True
            entry["cu_error_attempted"]=True
            entry["result_object"].set_error(_delivery_error(entry,"Native terminal is unverified; started journal and ownership retained, no retry"))
    except Exception as delivery_error:
        _delivery_failed(entry, delivery_error)


def _observe_quarantined(entry):
    # If the one owned cancellation could not certify terminal, retain only read
    # observation. No second cancel, writers, saves or second CU delivery occurs.
    try:
        entry["snapshot_calls"] += 1
        native = api.snapshot(entry["job"],entry["method"])
        failure = {k: entry["journal"].result[k] for k in ("success", "error_code", "error")}
        _async_adopt(entry, native)
        entry["journal"].result.update(failure)
        entry["journal"].result["data"].update(workflow_recovery_required=True,
            quarantine_later_native_terminal=native["data"]["native_job_terminal"])
        revision = native["data"].get("native_checkpoint_revision")
        if ("quarantine_checkpoint_attempt_revision" not in entry
                or entry["quarantine_checkpoint_attempt_revision"] != revision):
            # A failed durable write is not retried each frame for the same
            # observed revision. A later actual phase/terminal prefix may be recorded.
            entry["quarantine_checkpoint_attempt_revision"] = revision
            _checkpoint_native(entry, native)
        if native["data"]["native_job_terminal"]:
            entry["observation_only_quarantine"] = False
    except Exception as error:
        _preserve_snapshot_error(entry,error)
        entry["last_quarantine_observation_error"] = str(error)[:700]


def _tick_all(delta_seconds):
    if not _ACTIVE:
        return False
    from . import batch
    batch.tick_compile_status_batch(delta_seconds, factory=True)
    for entry in tuple(_PENDING.values()):
        if entry.get("finalization_attempted"):
            continue
        try:
            entry["tick_callbacks"] += 1
            entry["snapshot_calls"] += 1
            native = api.snapshot(entry["job"],entry["method"])
            if native["data"]["native_job_terminal"]:
                _terminal(entry, native)
            else:
                _async_adopt(entry, native)
                _checkpoint_native(entry, native)
        except Exception as error:
            _uncertain(entry, error)
    for entry in tuple(_QUARANTINED.values()):
        if entry.get("observation_only_quarantine"):
            _observe_quarantined(entry)
    return True


def _shutdown():
    # Native module owns teardown. No late asset/job/save/cleanup method here.
    global _ACTIVE
    _ACTIVE = False
    from . import batch
    batch.shutdown_compile_status_batches(factory=True)
    for entry in tuple(_PENDING.values()):
        if entry.get("finalization_attempted"):
            continue
        entry["lifecycle_interrupted"] = True
        result = entry["journal"].result
        state.fail(result, state.OperationError("niagara_factory_bootstrap_async_shutdown_unknown", "Shutdown before verified Native terminal; no late work attempted"))
        result["data"].update(mutation_state_unknown=True, mutation_performed=None,
            native_terminal_observed=False, workflow_recovery_required=True)
        try:
            entry["journal"].checkpoint()
        except Exception:
            pass  # Started journal remains; no repeated write or Engine work.
        _QUARANTINED[entry["run_id"]] = entry
        _PENDING.pop(entry["run_id"], None)


def binding_status():
    from . import batch
    parent = batch.compile_status_batch_status(factory=True)
    return {"active": _ACTIVE, "pending_count": len(_PENDING)+parent["pending_count"], "quarantined_count": len(_QUARANTINED)+parent["quarantined_count"],
        "batch_factory_bootstrap": parent,
        "ticker_handle_present": _TICKER_HANDLE is not None, "shutdown_handle_present": _SHUTDOWN_HANDLE is not None,
        "last_terminal": copy.deepcopy(_LAST_TERMINAL), "lifecycle_errors": list(_LIFECYCLE_ERRORS),
        "quarantined_operation_ids": sorted(_QUARANTINED),
        "quarantined_journal_paths": {run_id: str(entry["journal"].path) for run_id, entry in _QUARANTINED.items()},
        "observation_only_quarantine_count": sum(bool(entry.get("observation_only_quarantine")) for entry in _QUARANTINED.values()),
        "native_handle_membership_verified": False, "ordinary_async_Batch_integration_verified": False, "Native_bindings": api.async_binding_status()}


def activate():
    global _ACTIVE, _TICKER_HANDLE, _SHUTDOWN_HANDLE
    status=api.async_binding_status()
    if not all(value is True for value in status.values()):raise state.OperationError("niagara_factory_bootstrap_async_native_jobs_unavailable","Exact dedicated Native start/Job bindings are required")
    if not _ACTIVE:
        if _PENDING or _QUARANTINED or _TICKER_HANDLE is not None or _SHUTDOWN_HANDLE is not None:
            raise state.OperationError("niagara_factory_bootstrap_async_ownership_retained", "Prior retained ownership requires Root reconciliation")
        from . import batch
        batch.activate_compile_status_batches(factory=True)
        try:
            _SHUTDOWN_HANDLE = unreal.register_python_shutdown_callback(_shutdown)
            try:
                _TICKER_HANDLE = unreal.register_ticker_callback(_tick_all, delay=0.0)
            except Exception:
                unreal.unregister_python_shutdown_callback(_SHUTDOWN_HANDLE)
                _SHUTDOWN_HANDLE = None
                raise
        except Exception:
            batch.deactivate_compile_status_batches("Factory ticker activation failed", factory=True)
            raise
        _ACTIVE = True
    return binding_status()


def deactivate(reason="Factory bootstrap toolset deactivated"):
    global _ACTIVE, _TICKER_HANDLE, _SHUTDOWN_HANDLE
    _ACTIVE = False
    from . import batch
    batch.deactivate_compile_status_batches(reason, factory=True)
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
        if entry.get("finalization_attempted"):
            continue
        entry["lifecycle_interrupted"] = True
        native = _cancel_once(entry, reason)
        if native is not None and native["data"]["native_job_terminal"]:
            try:
                _terminal(entry, native)
            except Exception as error:
                _uncertain(entry, error)
        else:
            _uncertain(entry, state.OperationError("niagara_factory_bootstrap_async_cancel_terminal_unverified", "Owned cancellation not terminal verified; no repeat"))
    return binding_status()


def _begin(method,public):
    result_object = CubelessPythonAsyncOperationResult()
    run_id = uuid.uuid4().hex
    journal = None
    entry = None
    try:
        arguments = _async_arguments(method, public)
        if not _ACTIVE:
            raise state.OperationError("niagara_factory_bootstrap_async_domain_inactive", "Root registration has not activated the Factory bootstrap domain")
        if _QUARANTINED:
            raise state.OperationError("niagara_factory_bootstrap_async_workflow_quarantined", "Retained prior Factory bootstrap outcome requires Root reconciliation")
        journal = _FactoryBootstrapJournal(method, arguments, run_id)
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
        if not _ACTIVE or entry.get("lifecycle_interrupted") or _PENDING.get(run_id) is not entry:
            raise state.OperationError("niagara_factory_bootstrap_async_start_interrupted","Native start returned after lifecycle interruption; stop only this returned Job and retain started ownership")
        entry["snapshot_calls"] += 1
        native = api.snapshot(entry["job"],entry["method"])
        if native["data"]["native_job_terminal"]:
            _terminal(entry, native)
        else:
            _async_adopt(entry, native)
            _checkpoint_native(entry, native)
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
                journal.result["data"]["legacy_Named_result"] = _legacy_exception(method,error)
                state.fail(journal.result, error)
                try:
                    _seal(entry, False)
                    _deliver(entry)
                except Exception as delivery_error:
                    _uncertain(entry, delivery_error)
                _PENDING.pop(run_id, None)
            else:
                _uncertain(entry, error)
        else:
            record = _rejected(method,error);record["operation_id"]=run_id
            record["data"]["legacy_Named_result"] = copy.deepcopy(getattr(error,"legacy_result",None if hasattr(error,"public_validation_errors") else _legacy_exception(method,error)))
            record["data"].update(operation=method, admission_rejected=True, native_start_dispatched=False,
                native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result_object.set_editor_property("value", reflected_result(record))
            result_object.complete_value()  # Official CU Reject may already SetError; never complete twice.
    return result_object


def completed_result(record):
    """Typed async final for pre-dispatch admission rejection only; no journal or Native job."""
    value=CubelessPythonAsyncOperationResult();value.set_editor_property('value',reflected_result(record))
    value.complete_value()  # Reject already ends the official future; no second SetError.
    return value


class _BatchFactoryBootstrapFlow:
    """One existing HN Job under execute_batch's journal; no child journal."""
    def __init__(self, result, arguments, checkpoint_callback, method, parent_current):
        self.r = self.result = result
        self.callback = checkpoint_callback
        self.parent_current = parent_current
        self.job = None
        self.in_flight = self.lost = self.cancel_entered = False
        self.checkpoint_failure_stop_permitted = False
        if method not in ("create_empty_niagara_system", "create_basic_sprite_niagara_system"):
            raise OperationError('niagara_factory_bootstrap_batch_method_invalid', 'Only the original exact factory System creation2 may use this continuation')
        self.method = method
        legacy = arguments.get("_legacy_command_params")
        original = {field: copy.deepcopy(arguments[field]) for field in PUBLIC_FIELDS[self.method] if field in arguments}
        args, original, legacy = _arguments(self.method, original, legacy, arguments.get("_public_input", False))
        public = dict(args=args, original=original)
        self.entry = dict(journal=self, method=self.method, public=public, legacy=legacy,
            batch_parent_only=True, snapshot_calls=0, tick_callbacks=0, job_reference_verified=False,
            last_checkpoint_revision=None)
        _async_context(result, self.method, public, legacy, True)
        result['data'].update(parent_journal_only=True, child_journals_created=0,
            native_start_dispatched=False, native_start_returned=False, native_snapshot_returned=False,
            native_mutation_attempted=False, execution_mutation_attempted=False,
            mutation_performed=False, mutation_state_unknown=False, change_state_known=True)
        result['compile'].update(requested=False, attempted=False, compiled=None)
        result['save'].update(requested=False, attempted=False, saved=False)

    def unsafe(self, error):
        self.lost = True
        self.r['data'].update(change_state_known=False, mutation_state_unknown=True,
            mutation_performed=None, do_not_retry=True, workflow_recovery_required=True)
        return error

    def checkpoint(self):
        # Reuse the existing parent callback/whole-child receipt adoption. The
        # meaningful native phase/revision remains in data/native_outcome.
        self.r['stage'] = 'returned_niagara_factory_bootstrap_operation'
        try:
            if self.callback:
                self.callback(self.r)
        except Exception as error:
            # Only the parent checkpoint callback failed after a same-job,
            # revision-validated receipt. Other observation loss never grants
            # permission to touch a Job again, even if the parent still lives.
            self.checkpoint_failure_stop_permitted = True
            raise self.unsafe(error)

    def start(self, asynchronous):
        if asynchronous is not True:
            raise OperationError('niagara_factory_bootstrap_batch_async_required', 'Only the official parent continuation admits this HN flow')
        self.r['stage'] = 'before_niagara_factory_bootstrap_checkpoint'
        if self.callback:
            self.callback(self.r)
        self.r['data'].update(native_start_dispatched=True, native_dispatched=True,
            native_mutation_attempted=None, execution_mutation_attempted=None,
            mutation_performed=None, mutation_state_unknown=True, change_state_known=False)
        self.r['compile'].update(requested=None, attempted=None, compiled=None)
        self.r['save'].update(requested=None, attempted=None, saved=False)
        self.in_flight = True
        try:
            self.job = api.start_job(self.method, self.entry['public']['args']['params'])
            self.entry.update(job=self.job, job_reference_verified=True)
            self.r['data']['native_start_returned'] = True
        except Exception as error:
            if getattr(error, 'native_start_returned', False):
                self.job = getattr(error, 'native_returned_job', None)
                self.entry['job'] = self.job
                self.r['data']['native_start_returned'] = True
            if getattr(error, 'native_start_dispatched', None) is False:
                self.in_flight = False
                self.r['data'].update(native_start_dispatched=False, native_dispatched=False,
                    native_mutation_attempted=False, execution_mutation_attempted=False,
                    mutation_performed=False, mutation_state_unknown=False, change_state_known=True)
                self.r['compile'].update(requested=False, attempted=False, compiled=None)
                self.r['save'].update(requested=False, attempted=False, saved=False)
            else:
                self.unsafe(error)
            raise
        return False  # Existing native ticker progresses; no immediate spin/sleep.

    def poll(self):
        if self.lost:
            raise OperationError('niagara_factory_bootstrap_batch_source_lost', 'Native publication/source loss is sticky; no next Job getter')
        self.checkpoint_failure_stop_permitted = False
        self.entry['tick_callbacks'] += 1
        self.entry['snapshot_calls'] += 1
        try:
            native = api.snapshot(self.job, self.method)
            _async_adopt(self.entry, native)
            terminal = native['data']['native_job_terminal']
            _checkpoint_native(self.entry, native, force=terminal)
        except Exception as error:
            _preserve_snapshot_error(self.entry, error)
            raise self.unsafe(error)
        if not terminal:
            return False
        self.in_flight = False
        self.r.update(success=native['success'], error_code=native.get('error_code', ''), error=native.get('error', ''))
        if native['success'] and (not native['change_state_known'] or not native['postcondition_passed']
                or not native['data']['operation_complete']):
            state.fail(self.r, OperationError('niagara_factory_bootstrap_async_result_unverified',
                'Native success lacks terminal known factory/wait/save/audit postconditions'))
        self.r['data'].update(native_terminal_observed=True,
            workflow_recovery_required=not native['change_state_known'],
            command_may_still_be_in_flight=False,
            cancel_acknowledgement_is_not_shared_compiler_stop=True)
        self.r['stage'] = 'complete' if self.r['success'] else 'returned_niagara_factory_bootstrap_operation'
        return True

    def fail(self, error):
        _preserve_snapshot_error(self.entry, error)
        # Future factory/save phases are still owned by this exact Job. A safe
        # parent lease permits one cooperative stop after observer/journal
        # failure; acknowledgement is not rollback or compiler cancellation.
        if self.checkpoint_failure_stop_permitted and self.in_flight and self.entry.get('job_reference_verified') and not self.entry.get('native_publication_quarantined') and not self.cancel_entered:
            try:
                self.parent_current()
                self.cancel_entered = True
                self.r['data']['owned_cancel_after_failure_attempts'] = 1
                acknowledged = self.job.cancel_job('Stop exact owned factory next writers after Batch failure: ' + str(error)[:700])
                self.r['data']['owned_cancel_after_failure_returned'] = True
                self.r['data']['owned_cancel_acknowledgement'] = acknowledged if type(acknowledged) is bool else None
                self.parent_current()
            except Exception as cancel_error:
                self.r['data']['owned_cancel_after_failure_error'] = str(cancel_error)[:700]
        state.fail(self.r, error)
        if self.in_flight or self.lost:
            self.r['data'].update(mutation_state_unknown=True, change_state_known=False,
                mutation_performed=None, workflow_recovery_required=True, do_not_retry=True)
        self.r['data']['command_may_still_be_in_flight'] = self.in_flight
        self.r['retry'].update(automatic_retry=False, duplicate_risk='owned_factory_bootstrap_Batch_no_replay')
        return self.r
