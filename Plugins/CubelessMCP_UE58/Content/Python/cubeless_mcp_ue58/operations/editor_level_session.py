"""FO Editor level session public6/1/6/8. Scheduled load and actual completion are separate.
Engine work is Native-owned; no journal updates are invented after the call returns.
"""
import copy
import json
import math
import re
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import editor_level_session as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={method:tuple(k for k in fields if k not in (('map_path',) if method=='open_editor_level' else ('level_path',) if method=='safe_new_preview_map' else ())) for method,fields in PARAM_FIELDS.items()}
OBSERVERS=('get_editor_level_open_status',)
WRITERS=('open_editor_level','clear_dirty_package_flags','safe_new_preview_map')
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    """Original public Batch recursively guards values; these Named methods have no dependency guard."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('editor_level_session_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


DEFAULTS={'open_editor_level':dict(dry_run=True,allow_dirty_packages=False,load_as_template=False,show_progress=True,deferred_load_delay_seconds=1.0),'get_editor_level_open_status':dict(request_id=''),'clear_dirty_package_flags':dict(package_paths=None,package_path='',clear_all_dirty=False,dry_run=True,allow_non_temp_path=False,required_root='/Game/_MCP_Temp'),'safe_new_preview_map':dict(target_path='',map_path='',dry_run=True,allow_dirty_packages=False,overwrite_existing=False,allow_non_temp_path=False,required_root='/Game/_MCP_Temp',is_partitioned_world=False)}


def _legacy_exception(method,error):return dict(success=False,error=str(error),error_code='editor_tool_failed',details={'exception_type':type(error).__name__})


def _public_error(field,kind,message):
    error=OperationError('editor_level_session_public_argument_invalid',field+': '+message)
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
    """Only these FO fields' actual original FastMCP preparse/arg-model coercion."""
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('editor_level_session_public_argument_invalid','Known method and public argument object required')
    values=copy.deepcopy(DEFAULTS[method])
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:continue
        value=copy.deepcopy(arguments[field])
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        if field in ('level_path','request_id','package_path','required_root','target_path','map_path'):
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif field=='deferred_load_delay_seconds':value=_public_float(field,value)
        elif field=='package_paths' and value is None:pass
        elif field=='package_paths' and value is not None:
            if not isinstance(value,(list,tuple)):raise _public_error(field,'list_type','Input should be a valid list')
            rows=[]
            for i,v in enumerate(value):
                if type(v) is not str:
                    error=_public_error(field,'string_type','Input should be a valid string');error.public_validation_errors[0]['field'].append(i);raise error
                rows.append(v)
            value=rows
        else:value=_public_bool(field,value)
        values[field]=value
    if method=='open_editor_level' and 'level_path' not in values:raise _public_error('level_path','missing','Field required')
    return values


def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('editor_level_session_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('editor_level_session_entry_invalid','Raw dict and public normalization are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    try:
        params=copy.deepcopy(values)
        if method=='clear_dirty_package_flags':params['package_paths']=values['package_paths'] or []
        elif method=='safe_new_preview_map':params['target_path']=values['target_path'] or values['map_path'];params['map_path']=values['map_path'] or values['target_path']
    except Exception as error:
        wrapped=OperationError('editor_tool_failed',str(error));wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None


def _context(result,method,args,original,legacy):
    result['data'].update(operation=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=1 if method in ('open_editor_level','safe_new_preview_map') else 0,
        Named_dependency_guard_count=0,Batch_recursive_string_value_guard_owned_by_Batch=True,
        Named_status_mutation_journal_created=False,read_only_direct_API=method in OBSERVERS,
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        delayed_load_is_completion=False,durable_journal_scope='Returned scheduling/action receipt only; callback does not automatically update this journal',
        status_request_id_links_in_memory_callback=True,status_restarts_preserve_job_history=False,clear_dirty_flag_is_save_or_rollback=False,
        SaveMap_is_intentional_only_for_safe_new_preview_map=True,weather_driver_reference_cleanup_parity='incomplete_no_game_asset_hard_dependency',
        synchronous_SDK_limit='LoadMap/NewMap/SaveMap are synchronous when dispatched; ticker scheduling is not whole-operation nonblocking completion',
        actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_editor_level_session_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_editor_level_session_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_editor_level_session_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_legacy_map_dirty_directory_load_create_save_callback_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_map_session_dirty_directory_save_callback_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    scheduled=method=='open_editor_level' and native['data'].get('legacy_result',{}).get('load_scheduled') is True
    result['data'].update(operation_complete=False if scheduled else None,load_completion_verified=False if scheduled else None,returned_receipt_scope='scheduled_request_only' if scheduled else 'Native_action_or_observation')
    result['stage']='returned_editor_level_session_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_editor_level_session_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_editor_level_session_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_editor_level_session_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'editor_level_session_native_failed',native.get('error') or 'Native editor_level_session failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('editor_level_session_postcondition_unverified','Native editor_level_session postcondition did not pass; no replay')
    # Native postconditions own actual map/session observations; singleton/virtual getter global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted']):
        raise OperationError('editor_level_session_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    scheduled=method=='open_editor_level' and native['data'].get('legacy_result',{}).get('load_scheduled') is True
    result['data'].update(operation_complete=not scheduled,load_completion_verified=False if scheduled else None,returned_receipt_scope='scheduled_request_only' if scheduled else 'Native_action_or_observation')
    result.update(success=True,stage='scheduled_editor_level_open' if scheduled else 'complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='editor_level_session_input_validation';result['changes']=[]
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


def open_editor_level(level_path:str,dry_run:bool=True,allow_dirty_packages:bool=False,load_as_template:bool=False,show_progress:bool=True,deferred_load_delay_seconds:float=1.0)->Dict[str,Any]:
    return _named('open_editor_level',dict(level_path=level_path,dry_run=dry_run,allow_dirty_packages=allow_dirty_packages,load_as_template=load_as_template,show_progress=show_progress,deferred_load_delay_seconds=deferred_load_delay_seconds))

def get_editor_level_open_status(request_id:str='')->Dict[str,Any]:return _named('get_editor_level_open_status',dict(request_id=request_id))

def clear_dirty_package_flags(package_paths:list[str]|None=None,package_path:str='',clear_all_dirty:bool=False,dry_run:bool=True,allow_non_temp_path:bool=False,required_root:str='/Game/_MCP_Temp')->Dict[str,Any]:
    return _named('clear_dirty_package_flags',dict(package_paths=package_paths,package_path=package_path,clear_all_dirty=clear_all_dirty,dry_run=dry_run,allow_non_temp_path=allow_non_temp_path,required_root=required_root))

def safe_new_preview_map(target_path:str='',map_path:str='',dry_run:bool=True,allow_dirty_packages:bool=False,overwrite_existing:bool=False,allow_non_temp_path:bool=False,required_root:str='/Game/_MCP_Temp',is_partitioned_world:bool=False)->Dict[str,Any]:
    return _named('safe_new_preview_map',dict(target_path=target_path,map_path=map_path,dry_run=dry_run,allow_dirty_packages=allow_dirty_packages,overwrite_existing=overwrite_existing,allow_non_temp_path=allow_non_temp_path,required_root=required_root,is_partitioned_world=is_partitioned_world))

def open_editor_level_action(result,level_path=None,dry_run=True,allow_dirty_packages=False,load_as_template=False,show_progress=True,deferred_load_delay_seconds=1.0,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'open_editor_level',dict(level_path=level_path,dry_run=dry_run,allow_dirty_packages=allow_dirty_packages,load_as_template=load_as_template,show_progress=show_progress,deferred_load_delay_seconds=deferred_load_delay_seconds),checkpoint_callback,_legacy_command_params,_public_input)

def get_editor_level_open_status_action(result,request_id='',checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):return _action(result,'get_editor_level_open_status',dict(request_id=request_id),checkpoint_callback,_legacy_command_params,_public_input)

def clear_dirty_package_flags_action(result,package_paths=None,package_path='',clear_all_dirty=False,dry_run=True,allow_non_temp_path=False,required_root='/Game/_MCP_Temp',checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'clear_dirty_package_flags',dict(package_paths=package_paths,package_path=package_path,clear_all_dirty=clear_all_dirty,dry_run=dry_run,allow_non_temp_path=allow_non_temp_path,required_root=required_root),checkpoint_callback,_legacy_command_params,_public_input)

def safe_new_preview_map_action(result,target_path='',map_path='',dry_run=True,allow_dirty_packages=False,overwrite_existing=False,allow_non_temp_path=False,required_root='/Game/_MCP_Temp',is_partitioned_world=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'safe_new_preview_map',dict(target_path=target_path,map_path=map_path,dry_run=dry_run,allow_dirty_packages=allow_dirty_packages,overwrite_existing=overwrite_existing,allow_non_temp_path=allow_non_temp_path,required_root=required_root,is_partitioned_world=is_partitioned_world),checkpoint_callback,_legacy_command_params,_public_input)
