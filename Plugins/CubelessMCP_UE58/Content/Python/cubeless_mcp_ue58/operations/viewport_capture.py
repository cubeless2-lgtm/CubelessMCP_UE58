"""Viewport public0/2/8/1. Original direct Python, public MCP and raw C++ routes stay distinct.
Native owns live Editor viewport/camera/bookmark/clean/PNG work and actual partial receipts.
No asset compiler/save, transport, unproven async completion or uncertain retry is added.
"""
import copy
import json
import math
import re
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import viewport_capture as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={method:tuple(k for k in fields if k!='fov') for method,fields in PARAM_FIELDS.items()}
OBSERVERS=('list_viewport_bookmarks',)
WRITERS=('set_active_viewport_realtime','capture_viewport_bookmark_screenshot','take_screenshot')
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    """Original public Batch recursively guards values; these Named methods have no dependency guard."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('viewport_capture_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


DEFAULTS={'list_viewport_bookmarks':{},'set_active_viewport_realtime':dict(realtime=True,realtime_frame_count=300),'capture_viewport_bookmark_screenshot':dict(bookmark_index=-1,redraw_count=2,clean_game_view=False,view_location=None,view_rotation=None,view_fov=None,restore_view_after_capture=True),'take_screenshot':{}}


def _legacy_exception(method,error):return dict(success=False,error=str(error),error_code='editor_tool_failed',details={'exception_type':type(error).__name__})


def _public_error(field,kind,message):
    error=OperationError('viewport_capture_public_argument_invalid',field+': '+message)
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
    """Only FN's proven FastMCP1.5/Pydantic2.10 JSON fields, not a framework clone."""
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('viewport_capture_public_argument_invalid','Known method and public argument object required')
    values=copy.deepcopy(DEFAULTS[method])
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:continue
        value=copy.deepcopy(arguments[field])
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        if field=='filepath':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif field in ('realtime','clean_game_view','restore_view_after_capture'):value=_public_bool(field,value)
        elif field in ('realtime_frame_count','bookmark_index','redraw_count'):value=_public_int(field,value)
        elif field in ('view_location','view_rotation') and value is not None:
            if not isinstance(value,(list,tuple)):raise _public_error(field,'list_type','Input should be a valid list')
            rows=[]
            for i,v in enumerate(value):
                try:rows.append(_public_float(field,v))
                except OperationError as error:error.public_validation_errors[0]['field'].append(i);raise
            value=rows
        elif field=='view_fov' and value is not None:value=_public_float(field,value)
        values[field]=value
    if method in ('capture_viewport_bookmark_screenshot','take_screenshot') and 'filepath' not in values:raise _public_error('filepath','missing','Field required')
    return values


def _dedicated_error(code,message):
    error=OperationError(code,message);error.legacy_result=dict(success=False,error=message,error_code=code);return error


def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('viewport_capture_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('viewport_capture_entry_invalid','Raw dict and public normalization are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    try:
        if method=='list_viewport_bookmarks':params={}
        elif method=='set_active_viewport_realtime':params=dict(realtime=bool(values['realtime']),realtime_frame_count=max(0,int(values['realtime_frame_count'])))
        elif method=='take_screenshot':params=dict(filepath=values['filepath'])
        else:
            params={field:copy.deepcopy(values[field]) for field in ('filepath','redraw_count','clean_game_view','restore_view_after_capture')}
            if values['bookmark_index']>=0:params['bookmark_index']=values['bookmark_index']
            for field,code in (('view_location','editor_invalid_view_location'),('view_rotation','editor_invalid_view_rotation')):
                value=values[field]
                if value is not None:
                    if not isinstance(value,list) or len(value)!=3:raise _dedicated_error(code,field+' must be a list of 3 numbers')
                    params[field]=[float(v) for v in value]
            if values['view_fov'] is not None:
                value=float(values['view_fov'])
                if value<=0.0 or value>=180.0:raise _dedicated_error('editor_invalid_view_fov','view_fov must be greater than 0 and less than 180 degrees')
                params['view_fov']=value
    except Exception as error:
        if hasattr(error,'legacy_result'):raise
        wrapped=OperationError('editor_tool_failed',str(error));wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None


def _context(result,method,args,original,legacy):
    result['data'].update(operation=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=1 if method=='capture_viewport_bookmark_screenshot' else 0,
        Named_dependency_guard_count=0,Batch_recursive_string_value_guard_owned_by_Batch=True,
        Named_list_mutation_journal_created=False,read_only_direct_API=method in OBSERVERS,active_rendered_Editor_viewport_required=True,
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='EngineAPI dedicated Native leaf',server_RPC_dependency=False,
        PNG_file_save_is_not_asset_save=True,ReadPixels_internal_render_flush='Synchronous SDK work; no whole-operation async/no-wait claim',
        bookmark_jump_not_undone_by_temporary_camera_restore=True,clean_flag_restoration_separate_from_camera_restore=True,
        actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_viewport_capture_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_viewport_capture_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_viewport_capture_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_legacy_UI_camera_bookmark_clean_realtime_directory_file_dirty_prefix_preserved=True,render_completion='Native_ReadPixels_receipt_only_no_additional_wrapper_wait',disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_viewport_camera_bookmark_directory_PNG_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result['stage']='returned_viewport_capture_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_viewport_capture_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_viewport_capture_operation'
    try:
        if method=='list_viewport_bookmarks':native=api.list_viewport_bookmarks()
        else:native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_viewport_capture_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'viewport_capture_native_failed',native.get('error') or 'Native viewport_capture failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('viewport_capture_postcondition_unverified','Native viewport_capture postcondition did not pass; no replay')
    # Native postconditions own actual viewport projection; singleton/virtual getter global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted']):
        raise OperationError('viewport_capture_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='viewport_capture_input_validation';result['changes']=[]
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


def list_viewport_bookmarks()->Dict[str,Any]:return _named('list_viewport_bookmarks',{})


def set_active_viewport_realtime(realtime:bool=True,realtime_frame_count:int=300)->Dict[str,Any]:return _named('set_active_viewport_realtime',dict(realtime=realtime,realtime_frame_count=realtime_frame_count))


def capture_viewport_bookmark_screenshot(filepath:str,bookmark_index:int=-1,redraw_count:int=2,clean_game_view:bool=False,view_location:list[float]|None=None,view_rotation:list[float]|None=None,view_fov:float|None=None,restore_view_after_capture:bool=True)->Dict[str,Any]:
    return _named('capture_viewport_bookmark_screenshot',dict(filepath=filepath,bookmark_index=bookmark_index,redraw_count=redraw_count,clean_game_view=clean_game_view,view_location=view_location,view_rotation=view_rotation,view_fov=view_fov,restore_view_after_capture=restore_view_after_capture))


def take_screenshot(filepath:str)->Dict[str,Any]:return _named('take_screenshot',dict(filepath=filepath))


def list_viewport_bookmarks_action(result,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):return _action(result,'list_viewport_bookmarks',{},checkpoint_callback,_legacy_command_params,_public_input)


def set_active_viewport_realtime_action(result,realtime:bool=True,realtime_frame_count:int=300,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):return _action(result,'set_active_viewport_realtime',dict(realtime=realtime,realtime_frame_count=realtime_frame_count),checkpoint_callback,_legacy_command_params,_public_input)


def capture_viewport_bookmark_screenshot_action(result,filepath=None,bookmark_index:int=-1,redraw_count:int=2,clean_game_view:bool=False,view_location:list[float]|None=None,view_rotation:list[float]|None=None,view_fov:float|None=None,restore_view_after_capture:bool=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'capture_viewport_bookmark_screenshot',dict(filepath=filepath,bookmark_index=bookmark_index,redraw_count=redraw_count,clean_game_view=clean_game_view,view_location=view_location,view_rotation=view_rotation,view_fov=view_fov,restore_view_after_capture=restore_view_after_capture),checkpoint_callback,_legacy_command_params,_public_input)


def take_screenshot_action(result,filepath=None,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):return _action(result,'take_screenshot',dict(filepath=filepath),checkpoint_callback,_legacy_command_params,_public_input)
