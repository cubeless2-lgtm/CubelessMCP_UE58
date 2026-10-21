"""Dedicated Chaos Cloth observers; Named/direct Python/raw contracts remain distinct."""
import copy
import json
import math
import re
from typing import Any, Dict
from cubeless_mcp_ue58.compatibility.ue58 import chaos_cloth_inspection as api
from . import state
from .state import OperationError

METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={method:tuple(fields) for method,fields in PARAM_FIELDS.items()}
OBSERVERS=tuple(METHODS)
WRITERS=()
_FORBIDDEN_MCP_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')

def _dependency(value):
    if not isinstance(value,str):return False
    value=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(value) and any(token in value for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursive values guard is separate from the Named path guard."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('chaos_cloth_inspection_dependency_reference','Refusing MCP-only reference in '+field)
            error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for index,row in enumerate(value):visit(field+'['+str(index)+']',row)
    visit('params',params)

DEFAULTS={
    'inspect_chaos_cloth_asset':dict(lod_index=-1,property_name_filter='',include_property_values=True,include_property_flags=True,include_dataflow_nodes=True,max_properties=512,max_nodes=256),
    'inspect_chaos_cloth_simulation_mesh':dict(lod_index=0,weight_map_name='',vertex_start=0,max_vertices=4096,face_start=0,max_faces=4096,include_positions=True,include_normals=False,include_indices=True,include_weight_values=True),
}

def _public_error(field,kind,message):
    error=OperationError('chaos_cloth_inspection_public_argument_invalid',field+': '+message)
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

def _legacy_exception(method,error):
    asset=method=='inspect_chaos_cloth_asset'
    return dict(success=False,error='Chaos Cloth asset inspection failed' if asset else 'Chaos Cloth simulation mesh inspection failed',
        error_code='chaos_cloth_asset_inspection_failed' if asset else 'chaos_cloth_simulation_mesh_inspection_failed',details={'exception_type':type(error).__name__})

def normalize_public_mcp_arguments(method,arguments):
    """Only the confirmed original FastMCP preparse/model; raw bypasses this."""
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('chaos_cloth_inspection_public_argument_invalid','Known method and public argument object required')
    if 'cloth_asset_path' not in arguments:raise _public_error('cloth_asset_path','missing','Field required')
    values=copy.deepcopy(DEFAULTS[method])
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:continue
        value=copy.deepcopy(arguments[field])
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        if field in ('cloth_asset_path','property_name_filter','weight_map_name'):
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif field.startswith('include_'):value=_public_bool(field,value)
        else:value=_public_int(field,value)
        values[field]=value
    return values

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('chaos_cloth_inspection_entry_invalid','Internal public-input selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('chaos_cloth_inspection_entry_invalid','Raw dict and public normalization are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params)
        return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    if _dependency(values.get('cloth_asset_path')):
        error=OperationError('mcp_dependency_reference_rejected','MCP-only dependency reference rejected')
        error.legacy_result=dict(success=False,error='MCP-only dependency reference rejected',error_code='mcp_dependency_reference_rejected',
            details={'field':'cloth_asset_path','reason':'mcp_only_reference'})
        raise error
    api._carrier(values)
    return dict(params=values,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,
        public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(operation=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),
        entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),
        legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,legacy_hidden_field_count=0,
        Named_dependency_guard_fields=['cloth_asset_path'],Batch_recursive_string_value_guard_owned_by_Batch=True,
        original_pre_parse_json_preserved=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_notify_calls=0,
        adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,registration_dependency='ToolsetAPI',
        operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,SDK_load_getter_evaluation_may_have_effects=True,
        full_hash_and_paging_contract_owned_by_Native=True,current_snapshot_state_equivalence_owned_by_Native=True,
        actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={key:copy.deepcopy(value) for key,value in native.items() if key not in ('data','_raw_native_json')},
        native_mutation_attempted=native['mutation_attempted'],execution_mutation_attempted=native['mutation_attempted'],
        change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_chaos_cloth_inspection_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_chaos_cloth_inspection_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_chaos_cloth_inspection_public_postconditions',native_postcondition_passed=native['postcondition_passed'],
        total_change_state_known=known,complete_Native_load_read_evaluation_partial_prefix_preserved=True,current_snapshot_equivalence='see_native_receipt',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_load_getter_evaluation_prefix_inspect_no_replay' if not known else 'observed_explicit_work_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result['stage']='returned_chaos_cloth_inspection_operation'

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,
        change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched')
    result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_chaos_cloth_inspection_checkpoint'
    if checkpoint_callback:checkpoint_callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,
        change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)]
    result['stage']='native_chaos_cloth_inspection_operation'
    try:native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,
                change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_chaos_cloth_inspection_operation'
        if checkpoint_callback:checkpoint_callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if checkpoint_callback:checkpoint_callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'chaos_cloth_inspection_native_failed',native.get('error') or 'Native inspection failed; actual prefix retained')
    if not native['postcondition_passed']:raise OperationError('chaos_cloth_inspection_postcondition_unverified','Native inspection postcondition did not pass; no replay')
    if native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted']:
        raise OperationError('chaos_cloth_inspection_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='chaos_cloth_inspection_input_validation';result['changes']=[]
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched')
    result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['data'].update(operation=method,admission_rejected=True,native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,
        execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False,journal_created=False)
    if hasattr(error,'legacy_result'):result['data']['legacy_result']=copy.deepcopy(error.legacy_result)
    if hasattr(error,'public_validation_errors'):result['data'].update(public_validation_errors=copy.deepcopy(error.public_validation_errors),original_framework_error_text_reproduced=False,tool_body_invoked=False)
    return result

def _rejected(method,error):return _rejection(state.new_result(),method,error)

def _named(method,arguments,_public_input=False):
    try:args,original,legacy=_arguments(method,arguments,None,_public_input)
    except Exception as error:
        rejected=_rejected(method,error)
        rejected['data']['legacy_Named_result']=copy.deepcopy(getattr(error,'legacy_result',None if hasattr(error,'public_validation_errors') else _legacy_exception(method,error)))
        return rejected
    result=state.new_result()
    try:return _run(result,method,args,original,legacy)
    except Exception as error:return state.fail(result,error)

def _named_mcp(method,arguments):return _named(method,arguments,True)

def _action(result,method,arguments,callback,legacy,public_input=False):
    try:args,original,legacy=_arguments(method,arguments,legacy,public_input)
    except Exception as error:
        _rejection(result,method,error);result['data']['parent_journal_only']=True
        if callback:callback(result)
        raise
    return _run(result,method,args,original,legacy,callback)

def inspect_chaos_cloth_asset(cloth_asset_path:str,lod_index:int=-1,property_name_filter:str='',include_property_values:bool=True,include_property_flags:bool=True,include_dataflow_nodes:bool=True,max_properties:int=512,max_nodes:int=256)->Dict[str,Any]:
    return _named('inspect_chaos_cloth_asset',dict(cloth_asset_path=cloth_asset_path,lod_index=lod_index,property_name_filter=property_name_filter,include_property_values=include_property_values,include_property_flags=include_property_flags,include_dataflow_nodes=include_dataflow_nodes,max_properties=max_properties,max_nodes=max_nodes))

def inspect_chaos_cloth_simulation_mesh(cloth_asset_path:str,lod_index:int=0,weight_map_name:str='',vertex_start:int=0,max_vertices:int=4096,face_start:int=0,max_faces:int=4096,include_positions:bool=True,include_normals:bool=False,include_indices:bool=True,include_weight_values:bool=True)->Dict[str,Any]:
    return _named('inspect_chaos_cloth_simulation_mesh',dict(cloth_asset_path=cloth_asset_path,lod_index=lod_index,weight_map_name=weight_map_name,vertex_start=vertex_start,max_vertices=max_vertices,face_start=face_start,max_faces=max_faces,include_positions=include_positions,include_normals=include_normals,include_indices=include_indices,include_weight_values=include_weight_values))

def inspect_chaos_cloth_asset_action(result,cloth_asset_path=None,lod_index=-1,property_name_filter='',include_property_values=True,include_property_flags=True,include_dataflow_nodes=True,max_properties=512,max_nodes=256,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'inspect_chaos_cloth_asset',dict(cloth_asset_path=cloth_asset_path,lod_index=lod_index,property_name_filter=property_name_filter,include_property_values=include_property_values,include_property_flags=include_property_flags,include_dataflow_nodes=include_dataflow_nodes,max_properties=max_properties,max_nodes=max_nodes),checkpoint_callback,_legacy_command_params,_public_input)

def inspect_chaos_cloth_simulation_mesh_action(result,cloth_asset_path=None,lod_index=0,weight_map_name='',vertex_start=0,max_vertices=4096,face_start=0,max_faces=4096,include_positions=True,include_normals=False,include_indices=True,include_weight_values=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'inspect_chaos_cloth_simulation_mesh',dict(cloth_asset_path=cloth_asset_path,lod_index=lod_index,weight_map_name=weight_map_name,vertex_start=vertex_start,max_vertices=max_vertices,face_start=face_start,max_faces=max_faces,include_positions=include_positions,include_normals=include_normals,include_indices=include_indices,include_weight_values=include_weight_values),checkpoint_callback,_legacy_command_params,_public_input)
