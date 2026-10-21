"""Landscape public0/1/11/2; direct Native partial/unknown receipts.
Loaded Editor-world proxy scope. Native owns synchronous Import/material updates.
No compiler/save/retry or asynchronous completion claim is added in Python.
"""
import copy
from typing import Any,Dict
from cubeless_mcp_ue58.compatibility.ue58 import landscape as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PUBLIC_FIELDS=api.PUBLIC_FIELDS
OBSERVERS=('list_landscapes','get_landscape_info')
WRITERS=('create_landscape','set_landscape_material')
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')


def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def _reject_legacy_command_dependency_refs(params):
    """Original public Batch recursively guards values; Named guards path only."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('landscape_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _legacy_exception(method,error):
    label={'list_landscapes':'listing landscapes','get_landscape_info':'reading landscape info','create_landscape':'creating landscape','set_landscape_material':'setting landscape material'}[method]
    return dict(success=False,error='Error '+label+': '+str(error),error_code='landscape_tool_failed',details={'exception_type':type(error).__name__})


def _named_field_guard(field,value):
    if _dependency(value):
        error=ValueError("Refusing to persist MCP-only reference in '"+field+"': "+value+'. UnrealMCP is an editor authoring bridge; finished assets must not depend on it.')
        error.field_name=field;error.reason='mcp_only_reference';raise error


def _arguments(method,arguments,legacy):
    original=copy.deepcopy(arguments);context=api._private_context(method,legacy)
    if context is not None:
        api._carrier(context)
        return copy.deepcopy(arguments),original,context
    values=copy.deepcopy(arguments)
    try:
        for field in ('actor_label','material_path','required_root'):
            if field in values and type(values[field]) is not str:raise TypeError(field+' requires a string on the typed Named path; raw entry retains JSON null/coercion')
        if method=='create_landscape':
            _named_field_guard('actor_label',values['actor_label'])
            if values['material_path']:_named_field_guard('material_path',values['material_path'])
            values['location']=values['location'] or [0.0,0.0,0.0];values['scale']=values['scale'] or [100.0,100.0,100.0]
            for field in api.INTEGER_FIELDS:
                values[field]=int(values[field])
                if not -(1<<63)<=values[field]<(1<<63):raise OverflowError(field+' exceeds exact Native int64 carrier')
            values['allow_non_temp_level']=bool(values['allow_non_temp_level']);api._carrier(values['location']);api._carrier(values['scale'])
        elif method=='set_landscape_material':_named_field_guard('material_path',values['material_path'])
    except Exception as error:
        wrapped=OperationError('landscape_tool_failed',str(error));wrapped.legacy_result=_legacy_exception(method,error);raise wrapped from error
    return values,original,None


def _context(result,method,args,original,legacy):
    result['data'].update(operation=method,requested_arguments=copy.deepcopy(original),normalized_arguments=copy.deepcopy(args),legacy_command_params=copy.deepcopy(legacy),
        raw_Native_entry=legacy is not None,legacy_hidden_field_count=0,internal_raw_compatibility_input_count=0 if method=='list_landscapes' else 1,
        raw_context_preserves_original_presence=True,raw_locale_conversion_owned_by_actual_Native_Json_API=True,
        Named_dependency_fields=['actor_label','material_path_if_truthy'] if method=='create_landscape' else ['material_path'] if method=='set_landscape_material' else [],Batch_recursive_string_value_guard_owned_by_Batch=True,
        Named_observer_mutation_journal_created=False,read_only_direct_API=method in OBSERVERS,
        loaded_Editor_world_proxy_scope=True,global_WorldPartition_cells_enumerated=False,Foliage_LevelPartition_guard_copied=False,
        original_first_match_IgnoreCase_label_or_name=True,sorted_or_unique_proxy_selector=False,layer_order_and_duplicates_preserved=True,
        adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        registration_dependency='ToolsetAPI',operation_dependency='EngineAPI Native leaf',server_RPC_dependency=False,
        explicit_compile_save_receipt_is_not_implicit_callback_purity=True,Import_is_synchronous_public_SDK=True,Import_internal_slow_dialog_and_texture_cache_work='No whole-operation async/no-wait guarantee',
        actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_landscape_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_landscape_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_landscape_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        complete_partial_legacy_actor_material_component_dirty_prefix_preserved=True,render_completion='not_waited',disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_landscape_spawn_Import_material_prefix_inspect_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result['stage']='returned_landscape_operation'


def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_landscape_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_landscape_operation'
    try:
        if method=='list_landscapes':native=api.list_landscapes()
        else:native=getattr(api,method)(**args,_legacy_command_params=legacy)
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_landscape_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'landscape_native_failed',native.get('error') or 'Native landscape failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('landscape_postcondition_unverified','Native landscape postcondition did not pass; no replay')
    # Native postconditions own observed projection; the material getter can leave global caches unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted']):
        raise OperationError('landscape_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result


def _rejection(result,method,error):
    state.fail(result,error);result['stage']='landscape_input_validation';result['changes']=[]
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['data'].update(operation=method,admission_rejected=True,native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False)
    if hasattr(error,'legacy_result'):result['data']['legacy_result']=copy.deepcopy(error.legacy_result)
    return result


def _rejected(method,error):return _rejection(state.new_result(),method,error)


def _named(method,arguments):
    try:args,original,legacy=_arguments(method,arguments,None)
    except Exception as error:
        rejected=_rejected(method,error);rejected['data']['legacy_Named_result']=copy.deepcopy(getattr(error,'legacy_result',_legacy_exception(method,error)));return rejected
    if method in OBSERVERS:
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


def list_landscapes()->Dict[str,Any]:return _named('list_landscapes',{})


def get_landscape_info(actor_label:str='')->Dict[str,Any]:return _named('get_landscape_info',dict(actor_label=actor_label))


def create_landscape(actor_label:str='MCP_Landscape',material_path:str='',location:list[float]|None=None,scale:list[float]|None=None,component_count_x:int=1,component_count_y:int=1,sections_per_component:int=1,quads_per_section:int=7,height_value:int=32768,allow_non_temp_level:bool=False,required_root:str='/Game/_MCP_Temp')->Dict[str,Any]:
    return _named('create_landscape',dict(actor_label=actor_label,material_path=material_path,location=location,scale=scale,component_count_x=component_count_x,component_count_y=component_count_y,sections_per_component=sections_per_component,quads_per_section=quads_per_section,height_value=height_value,allow_non_temp_level=allow_non_temp_level,required_root=required_root))


def set_landscape_material(material_path:str,actor_label:str='')->Dict[str,Any]:return _named('set_landscape_material',dict(material_path=material_path,actor_label=actor_label))


def list_landscapes_action(result,checkpoint_callback=None,*,_legacy_command_params=None):return _action(result,'list_landscapes',{},checkpoint_callback,_legacy_command_params)


def get_landscape_info_action(result,actor_label:str='',checkpoint_callback=None,*,_legacy_command_params=None):return _action(result,'get_landscape_info',dict(actor_label=actor_label),checkpoint_callback,_legacy_command_params)


def create_landscape_action(result,actor_label:str='MCP_Landscape',material_path:str='',location:list[float]|None=None,scale:list[float]|None=None,component_count_x:int=1,component_count_y:int=1,sections_per_component:int=1,quads_per_section:int=7,height_value:int=32768,allow_non_temp_level:bool=False,required_root:str='/Game/_MCP_Temp',checkpoint_callback=None,*,_legacy_command_params=None):
    return _action(result,'create_landscape',dict(actor_label=actor_label,material_path=material_path,location=location,scale=scale,component_count_x=component_count_x,component_count_y=component_count_y,sections_per_component=sections_per_component,quads_per_section=quads_per_section,height_value=height_value,allow_non_temp_level=allow_non_temp_level,required_root=required_root),checkpoint_callback,_legacy_command_params)


def set_landscape_material_action(result,material_path:str,actor_label:str='',checkpoint_callback=None,*,_legacy_command_params=None):return _action(result,'set_landscape_material',dict(material_path=material_path,actor_label=actor_label),checkpoint_callback,_legacy_command_params)
