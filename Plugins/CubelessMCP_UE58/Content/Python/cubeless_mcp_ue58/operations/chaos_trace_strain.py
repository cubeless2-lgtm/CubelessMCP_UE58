"""FZ Chaos trace strain public11. Native owns all Engine work and exact save prefixes.
Current request receipts and cumulative ticker snapshots are separate; no completed physics/render/pipeline claim.
"""
import copy
import json
import math
import re
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import chaos_trace_strain as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={'trigger_trace_external_strain': ('operation', 'actor_name', 'start', 'end', 'expected_hit', 'trace_complex', 'radius', 'strain', 'propagation_depth', 'propagation_factor', 'breaking_velocity')}
DEFAULTS={'trigger_trace_external_strain': {'operation': 'begin', 'actor_name': '', 'start': None, 'end': None, 'expected_hit': True, 'trace_complex': True, 'radius': 100, 'strain': 100000, 'propagation_depth': 1, 'propagation_factor': 0.5, 'breaking_velocity': 800}}
REQUIRED={'trigger_trace_external_strain': ()}
FIELD_TYPES={'trigger_trace_external_strain': {'operation': 'string', 'actor_name': 'string', 'start': 'array', 'end': 'array', 'expected_hit': 'boolean', 'trace_complex': 'boolean', 'radius': 'number', 'strain': 'number', 'propagation_depth': 'integer', 'propagation_factor': 'number', 'breaking_velocity': 'number'}}
OBSERVERS=()
WRITERS=tuple(METHODS)
NULLABLE={'trigger_trace_external_strain': {'start', 'end'}}
ITEMS={'trigger_trace_external_strain': {'start': 'number', 'end': 'number'}}
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('chaos_trace_strain_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


DEPENDENCY_FIELDS={'trigger_trace_external_strain':()}
LEGACY_ERRORS={'trigger_trace_external_strain':("Trace + External Strain failed","chaos_cinematic_trace_strain_failed")}

def _legacy_exception(method,error):
    message,code=LEGACY_ERRORS[method]
    return dict(success=False,message=message,error=message,error_code=code,details={"exception_type":type(error).__name__})

def _public_error(field,kind,message):
    error=OperationError('chaos_trace_strain_public_argument_invalid',field+': '+message)
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
        if not -(1<<63)<value<(1<<63):raise _public_error(field,'int_parsing_size','Unable to parse input string as an integer, exceeded maximum size')
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('chaos_trace_strain_public_argument_invalid','Known method and public argument object required')
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
                    else:item=_public_float(field,item)
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('chaos_trace_strain_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values


def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('chaos_trace_strain_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('chaos_trace_strain_entry_invalid','Raw dict and public normalization are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params)
        return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    try:
        params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method]}
        if params['start'] is None:params['start']=[]
        if params['end'] is None:params['end']=[]
    except Exception as error:
        wrapped=OperationError(LEGACY_ERRORS[method][1],LEGACY_ERRORS[method][0]);wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None


def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_session_operation=copy.deepcopy(args['params'].get('operation')),requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=1,Named_dependency_guard_count=0,Batch_recursive_string_value_guard_owned_by_Batch=True,
        operational_effect='runtime_write',original_inferred_effect='write',original_inferred_risk='medium',physics_effect_risk='high',adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        raw_prepared_trace_id_presence_preserved=True,Named_extra_fields_dropped=True,Named_vector_length_admission_deferred_to_Native=True,unknown_operation_begin_behavior_Native_owned=True,
        current_receipt_and_cumulative_session_execution_are_separate=True,returned_journal_scope='Current request receipt and observed cumulative session snapshot only',Native_tick_updates_Python_durable_journal=False,Native_session_id_links_later_observations=True,
        session_control_is_not_equivalent_to_physics_dispatch=True,abort_is_not_queued_physics_rollback=True,Engine_Editor_PIE_admission_Native_owned=True,
        actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_chaos_trace_strain_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_chaos_trace_strain_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_chaos_trace_strain_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_trace_session_collision_item_ordered_physics_sampling_cleanup_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_trace_session_collision_item_ordered_physics_sampling_cleanup_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    session=native['data'].get('session_execution')
    execution=session if type(session) is dict else {}
    result['data'].update(current_request_receipt={k:copy.deepcopy(native[k]) for k in ('mutation_attempted','change_state_known','changed','compile_attempted','compiled','save_attempted','saved','postcondition_passed')},
        session_actual_saved_observed=execution.get('saved') is True,session_mutation_state_unknown=execution.get('change_state_known') is False,
        session_in_flight=native['data'].get('operation_complete') is False or execution.get('operation_complete') is False,
        session_operation_complete=native['data'].get('operation_complete'),session_verification_passed=execution.get('postcondition_passed'),
        session_id_observed=execution.get('session_id',native['data'].get('session_id')),
        session_saved_history_promoted_to_current_request_save=False)
    result['validation'].update(completion_scope='Current request only; begin acceptance does not establish whole session completion',session_operation_complete=native['data'].get('operation_complete'),session_verification_passed=execution.get('postcondition_passed'))
    if result['data']['session_in_flight'] or result['data']['session_mutation_state_unknown']:result['retry']['duplicate_risk']='session_in_flight_or_cumulative_unknown_inspect_snapshot_no_replay'
    result['stage']='returned_chaos_trace_strain_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_chaos_trace_strain_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_chaos_trace_strain_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_chaos_trace_strain_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'chaos_trace_strain_native_failed',native.get('error') or 'Native chaos_trace_strain failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('chaos_trace_strain_postcondition_unverified','Native chaos_trace_strain postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted'] or native['compiled'] is True or native['saved']):
        raise OperationError('chaos_trace_strain_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='accepted_chaos_trace_session' if result['data'].get('session_in_flight') else 'complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='chaos_trace_strain_input_validation';result['changes']=[]
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


def trigger_trace_external_strain(operation:str='begin',actor_name:str='',start:list=None,end:list=None,expected_hit:bool=True,trace_complex:bool=True,radius:float=100,strain:float=100000,propagation_depth:int=1,propagation_factor:float=0.5,breaking_velocity:float=800)->Dict[str,Any]:
    return _named('trigger_trace_external_strain',dict(operation=operation,actor_name=actor_name,start=start,end=end,expected_hit=expected_hit,trace_complex=trace_complex,radius=radius,strain=strain,propagation_depth=propagation_depth,propagation_factor=propagation_factor,breaking_velocity=breaking_velocity))


def trigger_trace_external_strain_action(result,operation='begin',actor_name='',start=None,end=None,expected_hit=True,trace_complex=True,radius=100,strain=100000,propagation_depth=1,propagation_factor=0.5,breaking_velocity=800,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'trigger_trace_external_strain',dict(operation=operation,actor_name=actor_name,start=start,end=end,expected_hit=expected_hit,trace_complex=trace_complex,radius=radius,strain=strain,propagation_depth=propagation_depth,propagation_factor=propagation_factor,breaking_velocity=breaking_velocity),checkpoint_callback,_legacy_command_params,_public_input)
