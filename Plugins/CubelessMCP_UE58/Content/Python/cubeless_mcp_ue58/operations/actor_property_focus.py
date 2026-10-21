"""FP property setter and focus contracts: exact raw presence and actual original Any profile."""
import copy
import json
import math
import re
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import actor_property_focus as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={method:tuple(fields) for method,fields in PARAM_FIELDS.items()}
OBSERVERS=()
WRITERS=('set_actor_property','focus_viewport')
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    """Original public Batch recursively guards values; these Named methods have no dependency guard."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('actor_property_focus_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


DEFAULTS={'set_actor_property':{},'focus_viewport':dict(target=None,location=None,distance=1000.0,orientation=None)}


def _public_error(field,kind,message):
    error=OperationError('actor_property_focus_public_argument_invalid',field+': '+message)
    error.public_validation_errors=[dict(field=[field],type=kind,message=message)];return error


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


def _legacy_exception(method,error):
    message=('Error setting actor property: '+str(error)) if method=='set_actor_property' else str(error)
    return dict(success=False,error=message,error_code='editor_tool_failed',details={'exception_type':type(error).__name__})


def normalize_public_mcp_arguments(method,arguments):
    """Preserve original unannotated Any runtime value plus its string-only schema."""
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('actor_property_focus_public_argument_invalid','Known method and public argument object required')
    values=copy.deepcopy(DEFAULTS[method])
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:continue
        value=copy.deepcopy(arguments[field])
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        if method=='set_actor_property':
            if field!='property_value' and type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
            # Actual original annotation is typing.Any with WithJsonSchema(type=string).
            # property_value therefore accepts each original JSON type after preparse.
        elif field=='target':
            if value is not None and type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif field in ('location','orientation'):
            if value is not None:
                if not isinstance(value,(list,tuple)):raise _public_error(field,'list_type','Input should be a valid list')
                rows=[]
                for i,v in enumerate(value):
                    try:rows.append(_public_float(field,v))
                    except OperationError as error:error.public_validation_errors[0]['field'].append(i);raise
                value=rows
        else:value=_public_float(field,value)
        values[field]=value
    if method=='set_actor_property':
        for field in PUBLIC_FIELDS[method]:
            if field not in values:raise _public_error(field,'missing','Field required')
    return values


def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('actor_property_focus_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('actor_property_focus_entry_invalid','Raw dict and public normalization are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    try:
        if method=='set_actor_property':params=copy.deepcopy(values)
        else:
            params={}
            if values['target']:params['target']=copy.deepcopy(values['target'])
            elif values['location']:params['location']=copy.deepcopy(values['location'])
            if values['distance']:params['distance']=values['distance']
            if values['orientation']:params['orientation']=copy.deepcopy(values['orientation'])
    except Exception as error:
        wrapped=OperationError('editor_tool_failed',str(error));wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None


def _context(result,method,args,original,legacy):
    result['data'].update(operation=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=0,Named_dependency_guard_count=0,Batch_recursive_string_value_guard_owned_by_Batch=True,
        property_value_original_schema_type='string',property_value_original_arg_model_annotation='typing.Any',original_pre_parse_json_preserved=True,
        Native_reference_guard_scope='Class/Object reference branches only; ordinary FString property is not guarded by Named adapter',
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_notify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        original_focus_truthiness_and_raw_presence_are_separate=True,actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_actor_property_focus_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_actor_property_focus_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_actor_property_focus_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_legacy_actor_property_reference_load_camera_dirty_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_actor_property_reference_load_camera_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result['stage']='returned_actor_property_focus_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_actor_property_focus_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_actor_property_focus_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_actor_property_focus_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'actor_property_focus_native_failed',native.get('error') or 'Native actor_property_focus failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('actor_property_focus_postcondition_unverified','Native actor_property_focus postcondition did not pass; no replay')
    # Native postconditions own actual actor/property/camera observations; singleton/virtual getter global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted']):
        raise OperationError('actor_property_focus_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='actor_property_focus_input_validation';result['changes']=[]
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
    try:args,original,legacy=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        _rejection(result,method,error);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,args,original,legacy,callback)


def _named_mcp(method,arguments):return _named(method,arguments,_public_input=True)


def set_actor_property(name:str,property_name:str,property_value)->Dict[str,Any]:
    return _named('set_actor_property',dict(name=name,property_name=property_name,property_value=property_value))


def focus_viewport(target:str|None=None,location:list[float]|None=None,distance:float=1000.0,orientation:list[float]|None=None)->Dict[str,Any]:
    return _named('focus_viewport',dict(target=target,location=location,distance=distance,orientation=orientation))


def set_actor_property_action(result,name=None,property_name=None,property_value=None,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'set_actor_property',dict(name=name,property_name=property_name,property_value=property_value),checkpoint_callback,_legacy_command_params,_public_input)


def focus_viewport_action(result,target=None,location=None,distance=1000.0,orientation=None,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'focus_viewport',dict(target=target,location=location,distance=distance,orientation=orientation),checkpoint_callback,_legacy_command_params,_public_input)
