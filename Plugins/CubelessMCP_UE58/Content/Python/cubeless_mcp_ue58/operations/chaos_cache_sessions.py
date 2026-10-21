"""FW Chaos cache sessions public15/9. Native owns all Engine work and exact save prefixes.
Current request receipts and cumulative ticker snapshots are separate; no completed physics/render/pipeline claim.
"""
import copy
import json
import math
import re
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import chaos_cache_sessions as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={'record_chaos_cache': ('operation', 'cache_manager_actor_name', 'geometry_collection_actor_names', 'cache_collection_path', 'cache_names', 'solver_actor_name', 'duration_seconds', 'frame_rate', 'timeout_seconds', 'initial_trigger_mode', 'motion_velocity', 'per_advance_breaks_allowed', 'per_advance_breaks_reschedule_limit', 'particle_release_throttling_min_count', 'particle_release_throttling_max_count'), 'validate_chaos_cache': ('operation', 'cache_manager_actor_name', 'cache_collection_path', 'sequence_path', 'geometry_collection_actor_names', 'sample_times', 'scrub_frames', 'repeat_count', 'timeout_seconds')}
DEFAULTS={'record_chaos_cache': {'cache_names': None, 'solver_actor_name': '', 'duration_seconds': 2.0, 'frame_rate': 30.0, 'timeout_seconds': 10.0, 'initial_trigger_mode': 'strain_velocity', 'motion_velocity': None, 'per_advance_breaks_allowed': 8, 'per_advance_breaks_reschedule_limit': 64, 'particle_release_throttling_min_count': 64, 'particle_release_throttling_max_count': 128}, 'validate_chaos_cache': {'geometry_collection_actor_names': None, 'sample_times': None, 'scrub_frames': None, 'repeat_count': 2, 'timeout_seconds': 10.0}}
REQUIRED={'record_chaos_cache': ('operation', 'cache_manager_actor_name', 'geometry_collection_actor_names', 'cache_collection_path'), 'validate_chaos_cache': ('operation', 'cache_manager_actor_name', 'cache_collection_path', 'sequence_path')}
FIELD_TYPES={'record_chaos_cache': {'operation': 'string', 'cache_manager_actor_name': 'string', 'geometry_collection_actor_names': 'array', 'cache_collection_path': 'string', 'cache_names': 'array', 'solver_actor_name': 'string', 'duration_seconds': 'number', 'frame_rate': 'number', 'timeout_seconds': 'number', 'initial_trigger_mode': 'string', 'motion_velocity': 'array', 'per_advance_breaks_allowed': 'integer', 'per_advance_breaks_reschedule_limit': 'integer', 'particle_release_throttling_min_count': 'integer', 'particle_release_throttling_max_count': 'integer'}, 'validate_chaos_cache': {'operation': 'string', 'cache_manager_actor_name': 'string', 'cache_collection_path': 'string', 'sequence_path': 'string', 'geometry_collection_actor_names': 'array', 'sample_times': 'array', 'scrub_frames': 'array', 'repeat_count': 'integer', 'timeout_seconds': 'number'}}
OBSERVERS=()
WRITERS=tuple(METHODS)
NULLABLE={'record_chaos_cache': {'motion_velocity', 'cache_names'}, 'validate_chaos_cache': {'scrub_frames', 'sample_times', 'geometry_collection_actor_names'}}
ITEMS={'record_chaos_cache': {'geometry_collection_actor_names': 'string', 'cache_names': 'string', 'motion_velocity': 'number'}, 'validate_chaos_cache': {'geometry_collection_actor_names': 'string', 'sample_times': 'number', 'scrub_frames': 'integer'}}
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('chaos_cache_sessions_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


DEPENDENCY_FIELDS={"record_chaos_cache":("cache_collection_path",),"validate_chaos_cache":("cache_collection_path","sequence_path")}
LEGACY_ERRORS={"record_chaos_cache":("Chaos Cache recording failed","chaos_cinematic_cache_record_failed"),"validate_chaos_cache":("Chaos Cache validation failed","chaos_cinematic_cache_validation_failed")}

def _legacy_exception(method,error):
    message,code=LEGACY_ERRORS[method]
    return dict(success=False,message=message,error=message,error_code=code,details={"exception_type":type(error).__name__})

def _public_error(field,kind,message):
    error=OperationError('chaos_cache_sessions_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('chaos_cache_sessions_public_argument_invalid','Known method and public argument object required')
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
        else:raise OperationError('chaos_cache_sessions_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values


def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('chaos_cache_sessions_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('chaos_cache_sessions_entry_invalid','Raw dict and public normalization are separate entries')
        # Outer Batch owns its recursive source-extra guard. No public defaults or coercion here.
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params)
        return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    try:
        for field in DEPENDENCY_FIELDS[method]:
            if _dependency(values.get(field)):raise ValueError('MCP-only dependency reference rejected')
    except Exception as error:
        wrapped=OperationError(LEGACY_ERRORS[method][1],LEGACY_ERRORS[method][0]);wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    try:
        params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method]}
        if method=='record_chaos_cache':
            if params['cache_names'] is None:params['cache_names']=[]
            if params['motion_velocity'] is None:params['motion_velocity']=[300.0,0.0,600.0]
        else:
            if params['geometry_collection_actor_names'] is None:params['geometry_collection_actor_names']=[]
            if params['sample_times'] is None:params['sample_times']=[0.25,0.5,1.0]
            if params['scrub_frames'] is None:params['scrub_frames']=[0,15,30,45,60]
            params['validate_forward_reverse']=True
        removed_transport_timeout_seconds=max(30,int(values['timeout_seconds'])+10)
    except Exception as error:
        wrapped=OperationError(LEGACY_ERRORS[method][1],LEGACY_ERRORS[method][0]);wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None,removed_transport_timeout_seconds=removed_transport_timeout_seconds),original,None


def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_session_operation=copy.deepcopy(args['params'].get('operation')),requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=8 if method=='record_chaos_cache' else 2,
        Named_dependency_guard_count=len(DEPENDENCY_FIELDS[method]),Batch_recursive_string_value_guard_owned_by_Batch=True,
        operational_effect='runtime_write',original_inferred_effect='write',adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        raw_operation_only_and_solver_defaults_preserved=True,Named_validate_forward_reverse_always_true=method=='validate_chaos_cache',
        removed_legacy_transport_timeout_seconds=args.get('removed_transport_timeout_seconds'),removed_timeout_is_new_network_timeout=False,
        current_receipt_and_cumulative_session_execution_are_separate=True,returned_journal_scope='Current request receipt and observed cumulative session snapshot only',
        Native_tick_updates_Python_durable_journal=False,Native_session_id_links_later_observations=True,
        record_requires_fresh_PIE_Native_admission=True,validate_allows_Editor_or_PIE_Native_admission=True,abort_precedes_world_gate_Native_owned=True,
        SDK_internal_waits='Synchronous Engine waits/flushes are not preempted by the wall deadline; no worker UObject access',
        actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_chaos_cache_sessions_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_chaos_cache_sessions_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_chaos_cache_sessions_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_cache_session_manager_solver_component_release_restore_save_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_cache_session_manager_solver_component_release_restore_save_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
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
    result['stage']='returned_chaos_cache_sessions_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_chaos_cache_sessions_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_chaos_cache_sessions_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_chaos_cache_sessions_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'chaos_cache_sessions_native_failed',native.get('error') or 'Native chaos_cache_sessions failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('chaos_cache_sessions_postcondition_unverified','Native chaos_cache_sessions postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted'] or native['compiled'] is True or native['saved']):
        raise OperationError('chaos_cache_sessions_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='accepted_chaos_cache_session' if result['data'].get('session_in_flight') else 'complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='chaos_cache_sessions_input_validation';result['changes']=[]
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


def record_chaos_cache(operation:str,cache_manager_actor_name:str,geometry_collection_actor_names:list,cache_collection_path:str,cache_names:list=None,solver_actor_name:str='',duration_seconds:float=2.0,frame_rate:float=30.0,timeout_seconds:float=10.0,initial_trigger_mode:str='strain_velocity',motion_velocity:list=None,per_advance_breaks_allowed:int=8,per_advance_breaks_reschedule_limit:int=64,particle_release_throttling_min_count:int=64,particle_release_throttling_max_count:int=128)->Dict[str,Any]:
    return _named('record_chaos_cache',dict(operation=operation,cache_manager_actor_name=cache_manager_actor_name,geometry_collection_actor_names=geometry_collection_actor_names,cache_collection_path=cache_collection_path,cache_names=cache_names,solver_actor_name=solver_actor_name,duration_seconds=duration_seconds,frame_rate=frame_rate,timeout_seconds=timeout_seconds,initial_trigger_mode=initial_trigger_mode,motion_velocity=motion_velocity,per_advance_breaks_allowed=per_advance_breaks_allowed,per_advance_breaks_reschedule_limit=per_advance_breaks_reschedule_limit,particle_release_throttling_min_count=particle_release_throttling_min_count,particle_release_throttling_max_count=particle_release_throttling_max_count))


def record_chaos_cache_action(result,operation=None,cache_manager_actor_name=None,geometry_collection_actor_names=None,cache_collection_path=None,cache_names=None,solver_actor_name='',duration_seconds=2.0,frame_rate=30.0,timeout_seconds=10.0,initial_trigger_mode='strain_velocity',motion_velocity=None,per_advance_breaks_allowed=8,per_advance_breaks_reschedule_limit=64,particle_release_throttling_min_count=64,particle_release_throttling_max_count=128,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'record_chaos_cache',dict(operation=operation,cache_manager_actor_name=cache_manager_actor_name,geometry_collection_actor_names=geometry_collection_actor_names,cache_collection_path=cache_collection_path,cache_names=cache_names,solver_actor_name=solver_actor_name,duration_seconds=duration_seconds,frame_rate=frame_rate,timeout_seconds=timeout_seconds,initial_trigger_mode=initial_trigger_mode,motion_velocity=motion_velocity,per_advance_breaks_allowed=per_advance_breaks_allowed,per_advance_breaks_reschedule_limit=per_advance_breaks_reschedule_limit,particle_release_throttling_min_count=particle_release_throttling_min_count,particle_release_throttling_max_count=particle_release_throttling_max_count),checkpoint_callback,_legacy_command_params,_public_input)


def validate_chaos_cache(operation:str,cache_manager_actor_name:str,cache_collection_path:str,sequence_path:str,geometry_collection_actor_names:list=None,sample_times:list=None,scrub_frames:list=None,repeat_count:int=2,timeout_seconds:float=10.0)->Dict[str,Any]:
    return _named('validate_chaos_cache',dict(operation=operation,cache_manager_actor_name=cache_manager_actor_name,cache_collection_path=cache_collection_path,sequence_path=sequence_path,geometry_collection_actor_names=geometry_collection_actor_names,sample_times=sample_times,scrub_frames=scrub_frames,repeat_count=repeat_count,timeout_seconds=timeout_seconds))


def validate_chaos_cache_action(result,operation=None,cache_manager_actor_name=None,cache_collection_path=None,sequence_path=None,geometry_collection_actor_names=None,sample_times=None,scrub_frames=None,repeat_count=2,timeout_seconds=10.0,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'validate_chaos_cache',dict(operation=operation,cache_manager_actor_name=cache_manager_actor_name,cache_collection_path=cache_collection_path,sequence_path=sequence_path,geometry_collection_actor_names=geometry_collection_actor_names,sample_times=sample_times,scrub_frames=scrub_frames,repeat_count=repeat_count,timeout_seconds=timeout_seconds),checkpoint_callback,_legacy_command_params,_public_input)


