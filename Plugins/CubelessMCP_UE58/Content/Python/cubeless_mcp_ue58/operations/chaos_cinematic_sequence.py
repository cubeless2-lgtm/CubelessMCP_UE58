"""FV Chaos cinematic public7/7/10/13. Native owns all Engine work and exact save prefixes.
The inspector is an observer; no physics/MRQ/cold or pipeline completion is inferred.
"""
import copy
import json
import math
import re
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import chaos_cinematic_sequence as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={'create_chaos_level_sequence': ('output_sequence_path', 'camera_actor_name', 'display_rate', 'tick_resolution', 'start_frame', 'end_frame', 'overwrite_existing'), 'bind_chaos_cache_to_sequence': ('sequence_path', 'cache_manager_actor_name', 'cache_collection_path', 'section_start_frame', 'section_end_frame', 'cache_play_rate', 'cache_start_offset_seconds'), 'configure_chaos_sequence_timing': ('sequence_path', 'display_rate', 'tick_resolution', 'start_frame', 'end_frame', 'section_start_frame', 'section_end_frame', 'cache_play_rate', 'cache_start_offset_seconds', 'reverse'), 'inspect_chaos_cinematic_pipeline': ('geometry_collection_path', 'sequence_path', 'cache_manager_actor_name', 'map_path', 'chaos_state_sha256', 'field_actor_name', 'require_movie_render_pipeline', 'mrq_output_directory', 'mrq_manifest_path', 'expected_display_rate', 'expected_tick_resolution', 'expected_playback_start_frame', 'expected_playback_end_frame')}
DEFAULTS={'create_chaos_level_sequence': {'camera_actor_name': '', 'display_rate': 30.0, 'tick_resolution': 24000, 'start_frame': 0, 'end_frame': 60, 'overwrite_existing': False}, 'bind_chaos_cache_to_sequence': {'section_start_frame': 0, 'section_end_frame': 60, 'cache_play_rate': 1.0, 'cache_start_offset_seconds': 0.0}, 'configure_chaos_sequence_timing': {'display_rate': 30.0, 'tick_resolution': 24000, 'start_frame': 0, 'end_frame': 60, 'section_start_frame': 0, 'section_end_frame': 60, 'cache_play_rate': 1.0, 'cache_start_offset_seconds': 0.0, 'reverse': False}, 'inspect_chaos_cinematic_pipeline': {'field_actor_name': 'Chaos_AnchorField', 'require_movie_render_pipeline': True, 'mrq_output_directory': '', 'mrq_manifest_path': '', 'expected_display_rate': 30, 'expected_tick_resolution': 24000, 'expected_playback_start_frame': 0, 'expected_playback_end_frame': 60}}
REQUIRED={'create_chaos_level_sequence': ('output_sequence_path',), 'bind_chaos_cache_to_sequence': ('sequence_path', 'cache_manager_actor_name', 'cache_collection_path'), 'configure_chaos_sequence_timing': ('sequence_path',), 'inspect_chaos_cinematic_pipeline': ('geometry_collection_path', 'sequence_path', 'cache_manager_actor_name', 'map_path', 'chaos_state_sha256')}
FIELD_TYPES={'create_chaos_level_sequence': {'output_sequence_path': 'string', 'camera_actor_name': 'string', 'display_rate': 'number', 'tick_resolution': 'integer', 'start_frame': 'integer', 'end_frame': 'integer', 'overwrite_existing': 'boolean'}, 'bind_chaos_cache_to_sequence': {'sequence_path': 'string', 'cache_manager_actor_name': 'string', 'cache_collection_path': 'string', 'section_start_frame': 'integer', 'section_end_frame': 'integer', 'cache_play_rate': 'number', 'cache_start_offset_seconds': 'number'}, 'configure_chaos_sequence_timing': {'sequence_path': 'string', 'display_rate': 'number', 'tick_resolution': 'integer', 'start_frame': 'integer', 'end_frame': 'integer', 'section_start_frame': 'integer', 'section_end_frame': 'integer', 'cache_play_rate': 'number', 'cache_start_offset_seconds': 'number', 'reverse': 'boolean'}, 'inspect_chaos_cinematic_pipeline': {'geometry_collection_path': 'string', 'sequence_path': 'string', 'cache_manager_actor_name': 'string', 'map_path': 'string', 'chaos_state_sha256': 'string', 'field_actor_name': 'string', 'require_movie_render_pipeline': 'boolean', 'mrq_output_directory': 'string', 'mrq_manifest_path': 'string', 'expected_display_rate': 'integer', 'expected_tick_resolution': 'integer', 'expected_playback_start_frame': 'integer', 'expected_playback_end_frame': 'integer'}}
OBSERVERS=("inspect_chaos_cinematic_pipeline",)
WRITERS=tuple(m for m in METHODS if m not in OBSERVERS)
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('chaos_cinematic_sequence_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


DEPENDENCY_FIELDS={"create_chaos_level_sequence":("output_sequence_path",),"bind_chaos_cache_to_sequence":("sequence_path","cache_collection_path"),"configure_chaos_sequence_timing":("sequence_path",),"inspect_chaos_cinematic_pipeline":("geometry_collection_path","sequence_path")}
LEGACY_ERRORS={"create_chaos_level_sequence":("Chaos Level Sequence creation failed","chaos_cinematic_sequence_create_failed"),"bind_chaos_cache_to_sequence":("Chaos Cache sequence binding failed","chaos_cinematic_sequence_binding_failed"),"configure_chaos_sequence_timing":("Chaos sequence timing configuration failed","chaos_cinematic_sequence_timing_failed"),"inspect_chaos_cinematic_pipeline":("Chaos cinematic pipeline inspection failed","chaos_cinematic_inspection_failed")}

def _legacy_exception(method,error):
    message,code=LEGACY_ERRORS[method]
    return dict(success=False,message=message,error=message,error_code=code,details={"exception_type":type(error).__name__})

def _public_error(field,kind,message):
    error=OperationError('chaos_cinematic_sequence_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('chaos_cinematic_sequence_public_argument_invalid','Known method and public argument object required')
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
        if kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('chaos_cinematic_sequence_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values


def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('chaos_cinematic_sequence_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('chaos_cinematic_sequence_entry_invalid','Raw dict and public normalization are separate entries')
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
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method]}
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None


def _context(result,method,args,original,legacy):
    result['data'].update(operation=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=2 if method=='bind_chaos_cache_to_sequence' else 0,
        Named_dependency_guard_count=len(DEPENDENCY_FIELDS[method]),Batch_recursive_string_value_guard_owned_by_Batch=True,
        Named_inspect_mutation_journal_created=False,read_only_direct_API=method in OBSERVERS,
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        raw_timing_omission_preserved=True,create_overwrite_existing_does_not_allow_overwrite=True,
        native_legacy_envelope_preserved=True,transport_success_is_assertion_success=False,
        Native_PIE_world_selection_preserved=True,managed_writer_idle_gate_differs_from_original_Native_PIE=True,
        SDK_EndEvaluate_internal_wait_flush='Synchronous native solver/cache work; wrapper extra wait0',
        inspector_checks_manifest_claims_and_file_hashes=True,inspector_does_not_decode_PNG_or_recompute_black_perceptual=True,
        actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_chaos_cinematic_sequence_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_chaos_cinematic_sequence_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_chaos_cinematic_sequence_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_legacy_sequence_manager_component_track_section_dirty_save_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_sequence_manager_component_track_section_save_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result['stage']='returned_chaos_cinematic_sequence_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_chaos_cinematic_sequence_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_chaos_cinematic_sequence_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_chaos_cinematic_sequence_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'chaos_cinematic_sequence_native_failed',native.get('error') or 'Native chaos_cinematic_sequence failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('chaos_cinematic_sequence_postcondition_unverified','Native chaos_cinematic_sequence postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted'] or native['compiled'] is True or native['saved']):
        raise OperationError('chaos_cinematic_sequence_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='chaos_cinematic_sequence_input_validation';result['changes']=[]
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
    result['data']['parent_journal_only']=True
    try:args,original,legacy=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        _rejection(result,method,error);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,args,original,legacy,callback)


def _named_mcp(method,arguments):return _named(method,arguments,_public_input=True)


def create_chaos_level_sequence(output_sequence_path:str,camera_actor_name:str='',display_rate:float=30.0,tick_resolution:int=24000,start_frame:int=0,end_frame:int=60,overwrite_existing:bool=False)->Dict[str,Any]:
    return _named('create_chaos_level_sequence',dict(output_sequence_path=output_sequence_path,camera_actor_name=camera_actor_name,display_rate=display_rate,tick_resolution=tick_resolution,start_frame=start_frame,end_frame=end_frame,overwrite_existing=overwrite_existing))


def create_chaos_level_sequence_action(result,output_sequence_path=None,camera_actor_name='',display_rate=30.0,tick_resolution=24000,start_frame=0,end_frame=60,overwrite_existing=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'create_chaos_level_sequence',dict(output_sequence_path=output_sequence_path,camera_actor_name=camera_actor_name,display_rate=display_rate,tick_resolution=tick_resolution,start_frame=start_frame,end_frame=end_frame,overwrite_existing=overwrite_existing),checkpoint_callback,_legacy_command_params,_public_input)


def bind_chaos_cache_to_sequence(sequence_path:str,cache_manager_actor_name:str,cache_collection_path:str,section_start_frame:int=0,section_end_frame:int=60,cache_play_rate:float=1.0,cache_start_offset_seconds:float=0.0)->Dict[str,Any]:
    return _named('bind_chaos_cache_to_sequence',dict(sequence_path=sequence_path,cache_manager_actor_name=cache_manager_actor_name,cache_collection_path=cache_collection_path,section_start_frame=section_start_frame,section_end_frame=section_end_frame,cache_play_rate=cache_play_rate,cache_start_offset_seconds=cache_start_offset_seconds))


def bind_chaos_cache_to_sequence_action(result,sequence_path=None,cache_manager_actor_name=None,cache_collection_path=None,section_start_frame=0,section_end_frame=60,cache_play_rate=1.0,cache_start_offset_seconds=0.0,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'bind_chaos_cache_to_sequence',dict(sequence_path=sequence_path,cache_manager_actor_name=cache_manager_actor_name,cache_collection_path=cache_collection_path,section_start_frame=section_start_frame,section_end_frame=section_end_frame,cache_play_rate=cache_play_rate,cache_start_offset_seconds=cache_start_offset_seconds),checkpoint_callback,_legacy_command_params,_public_input)


def configure_chaos_sequence_timing(sequence_path:str,display_rate:float=30.0,tick_resolution:int=24000,start_frame:int=0,end_frame:int=60,section_start_frame:int=0,section_end_frame:int=60,cache_play_rate:float=1.0,cache_start_offset_seconds:float=0.0,reverse:bool=False)->Dict[str,Any]:
    return _named('configure_chaos_sequence_timing',dict(sequence_path=sequence_path,display_rate=display_rate,tick_resolution=tick_resolution,start_frame=start_frame,end_frame=end_frame,section_start_frame=section_start_frame,section_end_frame=section_end_frame,cache_play_rate=cache_play_rate,cache_start_offset_seconds=cache_start_offset_seconds,reverse=reverse))


def configure_chaos_sequence_timing_action(result,sequence_path=None,display_rate=30.0,tick_resolution=24000,start_frame=0,end_frame=60,section_start_frame=0,section_end_frame=60,cache_play_rate=1.0,cache_start_offset_seconds=0.0,reverse=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'configure_chaos_sequence_timing',dict(sequence_path=sequence_path,display_rate=display_rate,tick_resolution=tick_resolution,start_frame=start_frame,end_frame=end_frame,section_start_frame=section_start_frame,section_end_frame=section_end_frame,cache_play_rate=cache_play_rate,cache_start_offset_seconds=cache_start_offset_seconds,reverse=reverse),checkpoint_callback,_legacy_command_params,_public_input)


def inspect_chaos_cinematic_pipeline(geometry_collection_path:str,sequence_path:str,cache_manager_actor_name:str,map_path:str,chaos_state_sha256:str,field_actor_name:str='Chaos_AnchorField',require_movie_render_pipeline:bool=True,mrq_output_directory:str='',mrq_manifest_path:str='',expected_display_rate:int=30,expected_tick_resolution:int=24000,expected_playback_start_frame:int=0,expected_playback_end_frame:int=60)->Dict[str,Any]:
    return _named('inspect_chaos_cinematic_pipeline',dict(geometry_collection_path=geometry_collection_path,sequence_path=sequence_path,cache_manager_actor_name=cache_manager_actor_name,map_path=map_path,chaos_state_sha256=chaos_state_sha256,field_actor_name=field_actor_name,require_movie_render_pipeline=require_movie_render_pipeline,mrq_output_directory=mrq_output_directory,mrq_manifest_path=mrq_manifest_path,expected_display_rate=expected_display_rate,expected_tick_resolution=expected_tick_resolution,expected_playback_start_frame=expected_playback_start_frame,expected_playback_end_frame=expected_playback_end_frame))


def inspect_chaos_cinematic_pipeline_action(result,geometry_collection_path=None,sequence_path=None,cache_manager_actor_name=None,map_path=None,chaos_state_sha256=None,field_actor_name='Chaos_AnchorField',require_movie_render_pipeline=True,mrq_output_directory='',mrq_manifest_path='',expected_display_rate=30,expected_tick_resolution=24000,expected_playback_start_frame=0,expected_playback_end_frame=60,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'inspect_chaos_cinematic_pipeline',dict(geometry_collection_path=geometry_collection_path,sequence_path=sequence_path,cache_manager_actor_name=cache_manager_actor_name,map_path=map_path,chaos_state_sha256=chaos_state_sha256,field_actor_name=field_actor_name,require_movie_render_pipeline=require_movie_render_pipeline,mrq_output_directory=mrq_output_directory,mrq_manifest_path=mrq_manifest_path,expected_display_rate=expected_display_rate,expected_tick_resolution=expected_tick_resolution,expected_playback_start_frame=expected_playback_start_frame,expected_playback_end_frame=expected_playback_end_frame),checkpoint_callback,_legacy_command_params,_public_input)


