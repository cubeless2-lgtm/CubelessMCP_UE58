"""Original foliage public0/2/4 and native composite/failure accounting.

Current Editor world/current-level IFA is the scope. Native owns transactions,
per-item add prefix, count/placement verification, remove validation/dedup/cap,
owner reacquisition and asynchronous HISM observations. Python does not save,
compile, wait for render completion or retry uncertain writes.
"""
import copy
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import foliage_instances as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
RAW_REMOVE_FIELDS=api.RAW_REMOVE_FIELDS
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    """Original public Batch recursively guards values; Named guards path only."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('foliage_instances_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _legacy_exception(method,error):
    label={'add_foliage_instances':'adding foliage instances','remove_foliage_instances':'removing foliage instances','list_foliage_types':'listing foliage types'}[method]
    return dict(success=False,error='Error '+label+': '+str(error),error_code='landscape_tool_failed',details={'exception_type':type(error).__name__})


def _named_path_guard(path):
    if _dependency(path):
        error=ValueError("Refusing to persist MCP-only reference in 'foliage_type_path': "+path+'. UnrealMCP is an editor authoring bridge; finished assets must not depend on it.')
        error.field_name='foliage_type_path';error.reason='mcp_only_reference';raise error


def _arguments(method,arguments,legacy):
    original=copy.deepcopy(arguments)
    if legacy is not None and type(legacy) is not dict:raise OperationError('foliage_instances_raw_context_invalid','Raw entry marker must be a dict or None')
    raw=legacy is not None
    if method=='list_foliage_types':
        if arguments or raw and legacy:raise OperationError('foliage_instances_input_invalid','Original observer has no inputs or hidden fields')
        return {},original,legacy
    path=arguments['foliage_type_path']
    if type(path) is not str:raise OperationError('foliage_instances_input_invalid','foliage_type_path requires a string')
    if not raw:
        try:_named_path_guard(path)
        except Exception as error:
            wrapped=OperationError('landscape_tool_failed',str(error));wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    values=copy.deepcopy(arguments)
    if method=='add_foliage_instances':
        if raw and legacy:raise OperationError('foliage_instances_raw_context_invalid','Add has original hidden0; only empty raw entry marker is allowed')
        if not raw and not values['transforms']:
            error=OperationError('landscape_invalid_transforms','transforms must be a non-empty list');error.legacy_result=dict(success=False,error=str(error),error_code=error.code);raise error
        # Preserve Native per-item parse/write ordering: do not prevalidate all row shapes.
        api._carrier(values['transforms'])
    else:
        context=api._private_remove_context(legacy)
        if context is None:
            if not values['remove_all'] and not values['instance_indices']:
                error=OperationError('landscape_invalid_instance_indices','instance_indices must be provided unless remove_all=true');error.legacy_result=dict(success=False,error=str(error),error_code=error.code);raise error
            try:
                values['instance_indices']=[int(index) for index in (values['instance_indices'] or [])]
                values['remove_all']=bool(values['remove_all']);values['max_remove_count']=int(values['max_remove_count'])
                if not -(1<<63)<=values['max_remove_count']<(1<<63):raise OverflowError('max_remove_count must fit the specific Native int64 carrier')
            except Exception as error:
                wrapped=OperationError('landscape_tool_failed',str(error));wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
            api._carrier(values['instance_indices'])
        else:
            # Locale/number/string/default/ignored-index semantics stay in actual C++ Json APIs.
            api._carrier(context);values.update(instance_indices=[],remove_all=False,max_remove_count=5000)
    return values,original,copy.deepcopy(legacy)


def _context(result,method,args,original,legacy):
    result['data'].update(operation=method,requested_arguments=copy.deepcopy(original),normalized_arguments=copy.deepcopy(args),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=0,internal_remove_raw_compatibility_input_count=1 if method=='remove_foliage_instances' else 0,
        raw_remove_context_preserves_original_presence3=True,raw_locale_conversion_owned_by_actual_Native_Json_API=True,
        original_Named_path_guard_only=method!='list_foliage_types',Batch_recursive_string_value_guard_owned_by_Batch=True,
        list_named_mutation_journal_created=False,observer_no_IFA_creation=method=='list_foliage_types',
        add_per_item_parse_write_partial_order_preserved=True,remove_all_ignores_raw_indices_in_Native=True,
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='EngineAPI Native leaf',server_RPC_dependency=False,
        explicit_compile_save_receipt_is_not_implicit_callback_purity=True,async_render_completion_not_waited=True,
        actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_foliage_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_foliage_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_foliage_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_legacy_counts_transform_dirty_async_prefix_preserved=True,render_completion='not_waited',disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_foliage_prefix_or_swapped_indices_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result['stage']='returned_foliage_instances_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_foliage_instances_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_foliage_instances_operation'
    try:
        if method=='remove_foliage_instances':native=api.remove_foliage_instances(**args,_legacy_command_params=legacy)
        else:native=getattr(api,method)(**args)
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_foliage_instances_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'foliage_instances_native_failed',native.get('error') or 'Native foliage failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('foliage_instances_postcondition_unverified','Native foliage postcondition did not pass; no replay')
    if method=='list_foliage_types' and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted'] or not known_observer(native)):
        raise OperationError('foliage_instances_observer_unverified','Observer returned unexpected work or unknown change state; actual prefix retained')
    result.update(success=True,stage='complete');return result


def known_observer(native):return native['change_state_known'] and native['changed'] is False


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='foliage_instances_input_validation';result['changes']=[]
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['data'].update(operation=method,admission_rejected=True,native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False)
    if hasattr(error,'legacy_result'):result['data']['legacy_result']=copy.deepcopy(error.legacy_result)
    return result


def _rejected(method,error):return _rejection(state.new_result(),method,error)


def _named(method,arguments):
    try:args,original,legacy=_arguments(method,arguments,None)
    except Exception as error:
        rejected=_rejected(method,error);rejected['data']['legacy_Named_result']=copy.deepcopy(getattr(error,'legacy_result',_legacy_exception(method,error)));return rejected
    if method=='list_foliage_types':
        result=state.new_result()
        try:return _run(result,method,args,original,legacy)
        except Exception as error:return state.fail(result,error)
    return state.mutate(method,arguments,None,lambda result:_run(result,method,args,original,legacy,state.checkpoint))


def _action(result,method,arguments,callback,legacy):
    try:args,original,legacy=_arguments(method,arguments,legacy)
    except Exception as error:
        _rejection(result,method,error);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,args,original,legacy,callback)


def list_foliage_types()->Dict[str,Any]:return _named('list_foliage_types',{})

def add_foliage_instances(foliage_type_path:str,transforms:list[dict[str,Any]])->Dict[str,Any]:return _named('add_foliage_instances',dict(foliage_type_path=foliage_type_path,transforms=transforms))

def remove_foliage_instances(foliage_type_path:str,instance_indices:list[int]|None=None,remove_all:bool=False,max_remove_count:int=5000)->Dict[str,Any]:return _named('remove_foliage_instances',dict(foliage_type_path=foliage_type_path,instance_indices=instance_indices,remove_all=remove_all,max_remove_count=max_remove_count))

def list_foliage_types_action(result,checkpoint_callback=None,*,_legacy_command_params=None):return _action(result,'list_foliage_types',{},checkpoint_callback,_legacy_command_params)

def add_foliage_instances_action(result,foliage_type_path:str,transforms:list[dict[str,Any]],checkpoint_callback=None,*,_legacy_command_params=None):return _action(result,'add_foliage_instances',dict(foliage_type_path=foliage_type_path,transforms=transforms),checkpoint_callback,_legacy_command_params)

def remove_foliage_instances_action(result,foliage_type_path:str,instance_indices:list[int]|None=None,remove_all:bool=False,max_remove_count:int=5000,checkpoint_callback=None,*,_legacy_command_params=None):return _action(result,'remove_foliage_instances',dict(foliage_type_path=foliage_type_path,instance_indices=instance_indices,remove_all=remove_all,max_remove_count=max_remove_count),checkpoint_callback,_legacy_command_params)
