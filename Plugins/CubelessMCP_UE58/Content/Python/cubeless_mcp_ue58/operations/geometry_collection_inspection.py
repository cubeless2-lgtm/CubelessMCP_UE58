"""FY Geometry Collection static and PIE observers. Native owns Engine work.
Explicit writes/compile/save are zero; SDK caches/read-lock effects can remain unknown.
"""
import copy,json,math,re
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import geometry_collection_inspection as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={'inspect_geometry_collection_performance_guarded': ('geometry_collection_path', 'actor_name', 'tiny_relative_threshold'), 'inspect_geometry_collection_runtime_state': ('component_name', 'actor_name', 'actor_label', 'include_pieces', 'max_pieces', 'pie_instance')}
DEFAULTS={'inspect_geometry_collection_performance_guarded': {'actor_name': '', 'tiny_relative_threshold': 0.04}, 'inspect_geometry_collection_runtime_state': {'actor_name': '', 'actor_label': '', 'include_pieces': True, 'max_pieces': 512, 'pie_instance': -1}}
REQUIRED={'inspect_geometry_collection_performance_guarded': ('geometry_collection_path',), 'inspect_geometry_collection_runtime_state': ('component_name',)}
FIELD_TYPES={'inspect_geometry_collection_performance_guarded': {'geometry_collection_path': 'string', 'actor_name': 'string', 'tiny_relative_threshold': 'number'}, 'inspect_geometry_collection_runtime_state': {'component_name': 'string', 'actor_name': 'string', 'actor_label': 'string', 'include_pieces': 'boolean', 'max_pieces': 'integer', 'pie_instance': 'integer'}}
OBSERVERS=tuple(METHODS)
WRITERS=()
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('geometry_collection_inspection_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


LEGACY_ERRORS={'inspect_geometry_collection_performance_guarded':('Geometry Collection inspection failed','geometry_collection_inspection_failed'),'inspect_geometry_collection_runtime_state':('Geometry Collection runtime-state inspection failed','geometry_collection_runtime_inspection_failed')}
def _legacy_exception(method,error):
    message,code=LEGACY_ERRORS[method]
    return dict(success=False,message=message,error=message,error_code=code,details={'exception_type':type(error).__name__})
def _selector_error(code,message,details=None):
    error=OperationError(code,message);error.legacy_result=dict(success=False,error=message,message=message,error_code=code)
    if details is not None:error.legacy_result['details']=details
    return error
def _runtime_selector(component_name,actor_name,actor_label):
    # Original direct Python strip calls precede its try block; callers preserve that boundary.
    normalized=component_name.strip()
    if not normalized:return _selector_error('chaos_component_name_required','component_name must be non-empty')
    if not (actor_name.strip() or actor_label.strip()):return _selector_error('chaos_actor_selector_required','Either actor_name or actor_label must be non-empty',{'fields':['actor_name','actor_label'],'reason':'at_least_one_non_empty'})
    return None

def _public_error(field,kind,message):
    error=OperationError('geometry_collection_inspection_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('geometry_collection_inspection_public_argument_invalid','Known method and public argument object required')
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
        else:raise OperationError('geometry_collection_inspection_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values


def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('geometry_collection_inspection_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('geometry_collection_inspection_entry_invalid','Raw and public entries are separate')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy};api._carrier(params)
        return dict(params=params,entry='raw_legacy',public_input=False,read_only=True),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method]}
    if method=='inspect_geometry_collection_runtime_state':
        selector=_runtime_selector(values['component_name'],values['actor_name'],values['actor_label'])
        if selector is not None:raise selector
        try:
            params['component_name']=values['component_name'].strip();params['include_pieces']=bool(values['include_pieces']);params['max_pieces']=max(1,min(int(values['max_pieces']),512))
            params.pop('pie_instance')
            if values['pie_instance']>=0:params['pie_instance']=int(values['pie_instance'])
        except Exception as error:
            wrapped=OperationError(LEGACY_ERRORS[method][1],LEGACY_ERRORS[method][0]);wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None,read_only=True),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=0,read_only_direct_API=True,Named_dependency_guard_count=0,Batch_recursive_string_value_guard_owned_by_Batch=True,
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Named_runtime_strip_bool_int_clamp_negative_PIE_omission_preserved=True,raw_pie_presence_RoundToInt_and_Blocked_semantics_Native_owned=True,
        static_operationPASS_assertionFAIL_learningBLOCKED_are_distinct=True,physics_effective_state_or_five_run_performance_proof=False,
        actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_geometry_collection_inspection_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_geometry_collection_inspection_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_geometry_collection_inspection_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_collection_projection_world_identity_physics_lock_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_collection_projection_world_identity_physics_lock_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result['stage']='returned_geometry_collection_inspection_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_geometry_collection_inspection_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_geometry_collection_inspection_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_geometry_collection_inspection_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'geometry_collection_inspection_native_failed',native.get('error') or 'Native geometry_collection_inspection failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('geometry_collection_inspection_postcondition_unverified','Native geometry_collection_inspection postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if args['read_only'] and any(native[key] is not False for key in ('mutation_attempted','compile_requested','compile_attempted','compiled','save_requested','save_attempted','saved')):
        raise OperationError('geometry_collection_inspection_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='geometry_collection_inspection_input_validation';result['changes']=[]
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


def inspect_geometry_collection_performance_guarded(geometry_collection_path:str,actor_name:str='',tiny_relative_threshold:float=0.04)->Dict[str,Any]:
    return _named('inspect_geometry_collection_performance_guarded',dict(geometry_collection_path=geometry_collection_path,actor_name=actor_name,tiny_relative_threshold=tiny_relative_threshold))


def inspect_geometry_collection_performance_guarded_action(result,geometry_collection_path=None,actor_name='',tiny_relative_threshold=0.04,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'inspect_geometry_collection_performance_guarded',dict(geometry_collection_path=geometry_collection_path,actor_name=actor_name,tiny_relative_threshold=tiny_relative_threshold),checkpoint_callback,_legacy_command_params,_public_input)


def inspect_geometry_collection_runtime_state(component_name:str,actor_name:str='',actor_label:str='',include_pieces:bool=True,max_pieces:int=512,pie_instance:int=-1)->Dict[str,Any]:
    selector=_runtime_selector(component_name,actor_name,actor_label)
    if selector is not None:return _rejected('inspect_geometry_collection_runtime_state',selector)
    return _named('inspect_geometry_collection_runtime_state',dict(component_name=component_name,actor_name=actor_name,actor_label=actor_label,include_pieces=include_pieces,max_pieces=max_pieces,pie_instance=pie_instance))


def inspect_geometry_collection_runtime_state_action(result,component_name=None,actor_name='',actor_label='',include_pieces=True,max_pieces=512,pie_instance=-1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'inspect_geometry_collection_runtime_state',dict(component_name=component_name,actor_name=actor_name,actor_label=actor_label,include_pieces=include_pieces,max_pieces=max_pieces,pie_instance=pie_instance),checkpoint_callback,_legacy_command_params,_public_input)


