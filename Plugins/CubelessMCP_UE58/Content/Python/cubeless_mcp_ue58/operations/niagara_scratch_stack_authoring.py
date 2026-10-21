"""GQ dedicated resource DI/scratch authoring4. Preserve original model, direct body, raw presence and actual Native prefixes; no transport or replay."""
import copy
import hashlib
import os
import time
import uuid
import unreal
from cubeless_mcp_ue58.registration.async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.registration.results import reflected_result
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import niagara_scratch_stack_authoring as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"add_niagara_scratch_pad_module_input": ["system_path", "input_name", "default_value", "input_type", "consumer_pin_name", "scratch_pad_script_path", "scratch_pad_owner_kind", "scratch_pad_script_index", "scratch_pad_name", "scratch_pad_emitter_index", "scratch_pad_emitter_name", "require_consumer_link", "overwrite_existing_link", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"], "add_niagara_scratch_pad_assignment_module_to_stack": ["target_system_path", "parameter_name", "parameter_type", "default_value", "value", "target_usage", "target_usage_id", "target_emitter_index", "target_emitter_name", "target_index", "skip_if_duplicate", "allow_assignment_module_route", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"], "wrap_niagara_scratch_pad_output_with_stack_context": ["system_path", "target_pin_name", "scratch_pad_script_path", "scratch_pad_owner_kind", "scratch_pad_script_index", "scratch_pad_name", "scratch_pad_emitter_index", "scratch_pad_emitter_name", "target_node_guid", "target_node_index", "parameter_map_set_index", "previous_stack_value_pin_name", "local_value_pin_name", "expression", "skip_if_already_wrapped", "replace_existing_custom", "allow_source_edit", "save", "request_compile"]}
DEFAULTS={"add_niagara_scratch_pad_module_input": {"input_type": "bool", "consumer_pin_name": "", "scratch_pad_script_path": "", "scratch_pad_owner_kind": "system", "scratch_pad_script_index": None, "scratch_pad_name": "", "scratch_pad_emitter_index": None, "scratch_pad_emitter_name": "", "require_consumer_link": True, "overwrite_existing_link": False, "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": False, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1}, "add_niagara_scratch_pad_assignment_module_to_stack": {"parameter_name": "Particles.SpriteSize", "parameter_type": "vec2", "default_value": "50.000000,50.000000", "value": None, "target_usage": "ParticleUpdateScript", "target_usage_id": "", "target_emitter_index": None, "target_emitter_name": "", "target_index": None, "skip_if_duplicate": True, "allow_assignment_module_route": "", "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": True, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1}, "wrap_niagara_scratch_pad_output_with_stack_context": {"target_pin_name": "StackContext.RGBA", "scratch_pad_script_path": "", "scratch_pad_owner_kind": "system", "scratch_pad_script_index": None, "scratch_pad_name": "RenderCircleToGrid", "scratch_pad_emitter_index": None, "scratch_pad_emitter_name": "", "target_node_guid": "", "target_node_index": None, "parameter_map_set_index": None, "previous_stack_value_pin_name": "PreviousStackValue", "local_value_pin_name": "LocalStampValue", "expression": "max(PreviousStackValue, LocalStampValue)", "skip_if_already_wrapped": True, "replace_existing_custom": True, "allow_source_edit": False, "save": True, "request_compile": True}}
REQUIRED={"add_niagara_scratch_pad_module_input": ["system_path", "input_name", "default_value"], "add_niagara_scratch_pad_assignment_module_to_stack": ["target_system_path"], "wrap_niagara_scratch_pad_output_with_stack_context": ["system_path"]}
FIELD_TYPES={"add_niagara_scratch_pad_module_input": {"system_path": "string", "input_name": "string", "default_value": "any", "input_type": "string", "consumer_pin_name": "string", "scratch_pad_script_path": "string", "scratch_pad_owner_kind": "string", "scratch_pad_script_index": "integer", "scratch_pad_name": "string", "scratch_pad_emitter_index": "integer", "scratch_pad_emitter_name": "string", "require_consumer_link": "boolean", "overwrite_existing_link": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}, "add_niagara_scratch_pad_assignment_module_to_stack": {"target_system_path": "string", "parameter_name": "string", "parameter_type": "string", "default_value": "string", "value": "any", "target_usage": "string", "target_usage_id": "string", "target_emitter_index": "integer", "target_emitter_name": "string", "target_index": "integer", "skip_if_duplicate": "boolean", "allow_assignment_module_route": "string", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}, "wrap_niagara_scratch_pad_output_with_stack_context": {"system_path": "string", "target_pin_name": "string", "scratch_pad_script_path": "string", "scratch_pad_owner_kind": "string", "scratch_pad_script_index": "integer", "scratch_pad_name": "string", "scratch_pad_emitter_index": "integer", "scratch_pad_emitter_name": "string", "target_node_guid": "string", "target_node_index": "integer", "parameter_map_set_index": "integer", "previous_stack_value_pin_name": "string", "local_value_pin_name": "string", "expression": "string", "skip_if_already_wrapped": "boolean", "replace_existing_custom": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean"}}
NULLABLE={"add_niagara_scratch_pad_module_input": ["scratch_pad_script_index", "scratch_pad_emitter_index"], "add_niagara_scratch_pad_assignment_module_to_stack": ["value", "target_emitter_index", "target_index"], "wrap_niagara_scratch_pad_output_with_stack_context": ["scratch_pad_script_index", "scratch_pad_emitter_index", "target_node_index", "parameter_map_set_index"]}
ITEMS={"add_niagara_scratch_pad_module_input": {}, "add_niagara_scratch_pad_assignment_module_to_stack": {}, "wrap_niagara_scratch_pad_output_with_stack_context": {}}
OBSERVERS=[]
NO_JOURNAL_METHODS=[]
WRITERS=["add_niagara_scratch_pad_module_input", "add_niagara_scratch_pad_assignment_module_to_stack", "wrap_niagara_scratch_pad_output_with_stack_context"]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_scratch_stack_authoring_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_scratch_stack_authoring_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_scratch_stack_authoring_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]=='object':
                        if type(item) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
                    elif ITEMS[method][field]=='number':item=_public_float(field,item)
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_scratch_stack_authoring_public_argument_invalid','Unexpected frozen item type')
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='object':
            if type(value) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
            if any(type(key) is not str for key in value):raise _public_error(field,'string_type','Dictionary keys should be valid strings')
        elif kind=='any':pass
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('niagara_scratch_stack_authoring_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

LEGACY_MESSAGES={"add_niagara_scratch_pad_module_input": "Error adding Niagara Scratch Pad module input: ", "add_niagara_scratch_pad_assignment_module_to_stack": "Error adding Niagara Scratch Pad assignment module to stack: ", "wrap_niagara_scratch_pad_output_with_stack_context": "Error wrapping Niagara Scratch Pad output with StackContext accumulator: "}
def _legacy_exception(method,error):
    message=LEGACY_MESSAGES[method]+str(error)
    return dict(success=False,message=message,error=message,error_code='niagara_tool_failed')

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('niagara_scratch_stack_authoring_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_scratch_stack_authoring_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method]}
    # Preserve original public body omission after model coercion; raw presence bypasses this branch.
    for field in PUBLIC_FIELDS[method]:
        if FIELD_TYPES[method][field]=='integer' and field in NULLABLE[method] and params[field] is None:params.pop(field)
    if method=='add_niagara_scratch_pad_assignment_module_to_stack':
        for field in ('wait_for_completion','timeout_seconds','poll_interval_seconds'):params.pop(field)
        if values['default_value']:params.pop('value')
        else:params.pop('default_value')
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None,public_arguments=copy.deepcopy(values)),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,
        original_named_body_omission_and_value_passthrough_preserved=True,raw_locale_boolean_integer_and_presence_Native_owned=True,hidden_fields=len(set(PARAM_FIELDS[method])-set(PUBLIC_FIELDS[method])),public_only_fields=sorted(set(PUBLIC_FIELDS[method])-set(PARAM_FIELDS[method])),Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        operational_effect='read_world' if method in NO_JOURNAL_METHODS else 'write',original_inferred_risk='low' if method in NO_JOURNAL_METHODS else 'medium',original_native_observer=method in OBSERVERS,scratch_stack_graph_authoring_Native_owned=True,explicit_wait_True_not_faked_or_overridden=True,module_input_default_wait_False_preserved=True,assignment_public_default_wait_True_preserved=True,module_input_compile_wait_save_order_Native_owned=True,assignment_save_then_Python_status_owned=True,actual_save_history_preserved_before_post_guard=True,registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_niagara_scratch_stack_authoring_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_niagara_scratch_stack_authoring_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_niagara_scratch_stack_authoring_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        original_resource_DI_and_scratch_projection_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_niagara_scratch_stack_authoring_load_getter_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_niagara_scratch_stack_authoring_operation"

def _assignment_compile_status(result,args,checkpoint_callback=None):
    """Original Python composite after mutation/save, using the existing dedicated GH Engine API without child journal/RPC/replay."""
    values=args['public_arguments'];method="add_niagara_scratch_pad_assignment_module_to_stack";legacy=result['data'].get('legacy_result')
    payload=legacy if type(legacy) is dict else {}
    skipped=payload.get('skipped_duplicate') is True
    if not (values['request_compile'] and values['wait_for_completion'] and bool(payload.get('compile_requested',values['request_compile'] and not skipped))):return
    params=dict(system_path=values['target_system_path'],request_compile=False,force=False,allow_source_compile=values['allow_source_edit'],wait_for_completion=True,timeout_seconds=values['timeout_seconds'],poll_interval_seconds=values['poll_interval_seconds'])
    result['stage']='assignment_compile_status';result['data'].update(assignment_compile_status_requested=True,assignment_compile_status_params=copy.deepcopy(params),assignment_compile_status_dispatched=False)
    if checkpoint_callback:checkpoint_callback(result)
    try:
        from cubeless_mcp_ue58.compatibility.ue58 import niagara_authoring_inspection as compile_api
        result['data']['assignment_compile_status_dispatched']=True
        status=compile_api.inspect_niagara_compile_status(params)
    except Exception as error:
        result['data'].update(assignment_compile_status_returned=getattr(error,'native_call_returned',False),assignment_compile_status_raw=getattr(error,'native_raw_result',None),assignment_compile_status_error=str(error),niagara_compile_may_still_be_in_flight=True,command_may_still_be_in_flight=True,automatic_action_retry=False)
        if getattr(error,'native_call_dispatched',None) is False:result['data']['assignment_compile_status_dispatched']=False
        result['retry'].update(automatic_retry=False,duplicate_risk='assignment_already_applied_compile_status_unknown_no_replay')
        if checkpoint_callback:checkpoint_callback(result)
        raise OperationError('niagara_compile_status_read_failed','Assignment Native result retained; dedicated compile-status read failed') from error
    # Adopt every actual followup prefix before interpreting health or raising. This is current-request work, not past session save history.
    result['data'].update(assignment_compile_status_returned=True,assignment_compile_status_native_result=copy.deepcopy(status),assignment_compile_status_raw=status.get('_raw_native_json'),assignment_mutation_native_result=copy.deepcopy(result['data'].get('native_outcome')),assignment_mutation_saved=result['save']['saved'])
    result['compile']['requested']=result['compile']['requested'] or status['compile_requested']
    result['compile']['attempted']=result['compile']['attempted'] or status['compile_attempted']
    result['save']['requested']=result['save']['requested'] or status['save_requested']
    result['save']['attempted']=result['save']['attempted'] or status['save_attempted']
    result['save']['saved']=result['save']['saved'] or status['saved']
    if status['mutation_attempted']:
        result['data']['execution_mutation_attempted']=True
        result['changes'].append(dict(kind='assignment_compile_status',attempted=True,changed=status['changed'] if status['change_state_known'] else None,change_state_known=status['change_state_known']))
    if not status['change_state_known']:
        result['data'].update(change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    raw_status=copy.deepcopy(status['data'].get('legacy_result'))
    result['data']['assignment_compile_status_legacy_result']=raw_status
    if type(result['data'].get('legacy_Named_result')) is dict:result['data']['legacy_Named_result']['compile_status']=copy.deepcopy(status)
    invalid=False
    compile_payload=raw_status if type(raw_status) is dict else {}
    if type(raw_status) is not dict:invalid=True
    timed_out=compile_payload.get('wait_timed_out',False);has_errors=compile_payload.get('has_errors',False)
    if type(timed_out) is not bool:invalid=True;timed_out=False
    if type(has_errors) is not bool:invalid=True;has_errors=False
    pending={}
    for field in ('outstanding_compilation_requests_after','active_compilations_after'):
        value=compile_payload.get(field,0)
        # UE58 compatibility: original Native produces bool states while original Python expected counts. Never manufacture a numeric count.
        if type(value) is bool:pending[field]=value
        elif type(value) is int and value>=0:pending[field]=value>0
        else:invalid=True;pending[field]=False
    count=compile_payload.get('error_count',0)
    if type(count) is not int or count<0:invalid=True;count=0
    in_flight=bool(timed_out or any(pending.values()))
    pending_unsupported=status.get('error_code')=='niagara_inspection_async_wait_unsupported'
    incomplete=status.get('operation_complete') is False or status['data'].get('operation_complete') is False
    if pending_unsupported or incomplete:in_flight=True
    errors=bool(has_errors or count>0)
    observer_violation=any(status[k] is not False for k in ('mutation_attempted','compile_requested','compile_attempted','save_requested','save_attempted','saved')) or status['compiled'] is not False
    failed=not status['success'] or not status['postcondition_passed'] or compile_payload.get('success') is False or observer_violation
    result['data'].update(niagara_compile_may_still_be_in_flight=in_flight,niagara_compile_has_errors=errors,niagara_compile_status_invalid=invalid,assignment_compile_pending_predicates=pending,assignment_compile_boolean_states_are_not_counts=True,assignment_followup_observer_contract_violated=observer_violation)
    result['validation']['assignment_compile_status_checked']=True
    result['validation']['assignment_compile_status_passed']=not (failed or in_flight or errors or invalid)
    if failed or in_flight or errors or invalid:
        if pending_unsupported:code='niagara_inspection_async_wait_unsupported';message='Assignment completed; actual pending compile wait is not implemented by the dedicated synchronous status API'
        elif in_flight:code='niagara_compile_completion_timeout';message='Assignment completed, but compilation may still be in flight'
        elif errors:code='niagara_compile_postcondition_failed';message='Assignment completed, but compile readback reported errors'
        elif invalid:code='niagara_compile_status_invalid';message='Assignment completed, but compile readback fields were malformed'
        else:code='niagara_compile_status_read_failed';message='Assignment was sent, but compile-status readback failed'
        result['data'].update(command_may_still_be_in_flight=in_flight,automatic_action_retry=False,assignment_compile_followup_failed=True,completed_mutation_stages=[] if skipped else [method])
        mutation=copy.deepcopy(result['data'].get('legacy_Named_result'))
        result['data']['legacy_Named_result']=dict(success=False,message=message,error=message,error_code=code,mutation_response=mutation,compile_status=copy.deepcopy(status),niagara_compile_may_still_be_in_flight=in_flight,niagara_compile_has_errors=errors,niagara_compile_status_invalid=invalid)
        result['retry'].update(automatic_retry=False,duplicate_risk='assignment_already_applied_compile_followup_failed_no_replay')
        if checkpoint_callback:checkpoint_callback(result)
        raise OperationError(code,message)
    if checkpoint_callback:checkpoint_callback(result)

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_niagara_scratch_stack_authoring_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_niagara_scratch_stack_authoring_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_niagara_scratch_stack_authoring_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if native.get('operation_complete') is False or native['data'].get('operation_complete') is False:
        result['data'].update(operation_complete=False,in_flight=True,command_may_still_be_in_flight=True,automatic_action_retry=False,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        result['changes']=[dict(kind=method,attempted=native['mutation_attempted'],changed=None,change_state_known=False)]
        result['retry'].update(automatic_retry=False,duplicate_risk='deferred_resource_binding_completion_in_flight_no_replay')
        if callback:callback(result)
        raise OperationError('niagara_scratch_stack_authoring_incomplete','Native observation is incomplete; actual partial prefix retained without synthetic completion or replay')
    if not native['success']:raise OperationError(native.get('error_code') or 'niagara_scratch_stack_authoring_native_failed',native.get('error') or 'Native niagara_scratch_stack_authoring failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('niagara_scratch_stack_authoring_postcondition_unverified','Native niagara_scratch_stack_authoring postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method=="add_niagara_scratch_pad_assignment_module_to_stack" and legacy is None:_assignment_compile_status(result,args,callback)
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_scratch_stack_authoring_input_validation';result['changes']=[]
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
    if method in NO_JOURNAL_METHODS:
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

def add_niagara_scratch_pad_module_input(system_path,input_name,default_value,input_type="bool",consumer_pin_name="",scratch_pad_script_path="",scratch_pad_owner_kind="system",scratch_pad_script_index=None,scratch_pad_name="",scratch_pad_emitter_index=None,scratch_pad_emitter_name="",require_consumer_link=True,overwrite_existing_link=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1):
    return _named("add_niagara_scratch_pad_module_input",dict(system_path=system_path,input_name=input_name,default_value=default_value,input_type=input_type,consumer_pin_name=consumer_pin_name,scratch_pad_script_path=scratch_pad_script_path,scratch_pad_owner_kind=scratch_pad_owner_kind,scratch_pad_script_index=scratch_pad_script_index,scratch_pad_name=scratch_pad_name,scratch_pad_emitter_index=scratch_pad_emitter_index,scratch_pad_emitter_name=scratch_pad_emitter_name,require_consumer_link=require_consumer_link,overwrite_existing_link=overwrite_existing_link,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def add_niagara_scratch_pad_module_input_action(result,system_path,input_name,default_value,input_type="bool",consumer_pin_name="",scratch_pad_script_path="",scratch_pad_owner_kind="system",scratch_pad_script_index=None,scratch_pad_name="",scratch_pad_emitter_index=None,scratch_pad_emitter_name="",require_consumer_link=True,overwrite_existing_link=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"add_niagara_scratch_pad_module_input",dict(system_path=system_path,input_name=input_name,default_value=default_value,input_type=input_type,consumer_pin_name=consumer_pin_name,scratch_pad_script_path=scratch_pad_script_path,scratch_pad_owner_kind=scratch_pad_owner_kind,scratch_pad_script_index=scratch_pad_script_index,scratch_pad_name=scratch_pad_name,scratch_pad_emitter_index=scratch_pad_emitter_index,scratch_pad_emitter_name=scratch_pad_emitter_name,require_consumer_link=require_consumer_link,overwrite_existing_link=overwrite_existing_link,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)

def add_niagara_scratch_pad_assignment_module_to_stack(target_system_path,parameter_name="Particles.SpriteSize",parameter_type="vec2",default_value="50.000000,50.000000",value=None,target_usage="ParticleUpdateScript",target_usage_id="",target_emitter_index=None,target_emitter_name="",target_index=None,skip_if_duplicate=True,allow_assignment_module_route="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=10.0,poll_interval_seconds=0.1):
    return _named("add_niagara_scratch_pad_assignment_module_to_stack",dict(target_system_path=target_system_path,parameter_name=parameter_name,parameter_type=parameter_type,default_value=default_value,value=value,target_usage=target_usage,target_usage_id=target_usage_id,target_emitter_index=target_emitter_index,target_emitter_name=target_emitter_name,target_index=target_index,skip_if_duplicate=skip_if_duplicate,allow_assignment_module_route=allow_assignment_module_route,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def add_niagara_scratch_pad_assignment_module_to_stack_action(result,target_system_path,parameter_name="Particles.SpriteSize",parameter_type="vec2",default_value="50.000000,50.000000",value=None,target_usage="ParticleUpdateScript",target_usage_id="",target_emitter_index=None,target_emitter_name="",target_index=None,skip_if_duplicate=True,allow_assignment_module_route="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=10.0,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"add_niagara_scratch_pad_assignment_module_to_stack",dict(target_system_path=target_system_path,parameter_name=parameter_name,parameter_type=parameter_type,default_value=default_value,value=value,target_usage=target_usage,target_usage_id=target_usage_id,target_emitter_index=target_emitter_index,target_emitter_name=target_emitter_name,target_index=target_index,skip_if_duplicate=skip_if_duplicate,allow_assignment_module_route=allow_assignment_module_route,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)

def wrap_niagara_scratch_pad_output_with_stack_context(system_path,target_pin_name="StackContext.RGBA",scratch_pad_script_path="",scratch_pad_owner_kind="system",scratch_pad_script_index=None,scratch_pad_name="RenderCircleToGrid",scratch_pad_emitter_index=None,scratch_pad_emitter_name="",target_node_guid="",target_node_index=None,parameter_map_set_index=None,previous_stack_value_pin_name="PreviousStackValue",local_value_pin_name="LocalStampValue",expression="max(PreviousStackValue, LocalStampValue)",skip_if_already_wrapped=True,replace_existing_custom=True,allow_source_edit=False,save=True,request_compile=True):
    return _named("wrap_niagara_scratch_pad_output_with_stack_context",dict(system_path=system_path,target_pin_name=target_pin_name,scratch_pad_script_path=scratch_pad_script_path,scratch_pad_owner_kind=scratch_pad_owner_kind,scratch_pad_script_index=scratch_pad_script_index,scratch_pad_name=scratch_pad_name,scratch_pad_emitter_index=scratch_pad_emitter_index,scratch_pad_emitter_name=scratch_pad_emitter_name,target_node_guid=target_node_guid,target_node_index=target_node_index,parameter_map_set_index=parameter_map_set_index,previous_stack_value_pin_name=previous_stack_value_pin_name,local_value_pin_name=local_value_pin_name,expression=expression,skip_if_already_wrapped=skip_if_already_wrapped,replace_existing_custom=replace_existing_custom,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile))

def wrap_niagara_scratch_pad_output_with_stack_context_action(result,system_path,target_pin_name="StackContext.RGBA",scratch_pad_script_path="",scratch_pad_owner_kind="system",scratch_pad_script_index=None,scratch_pad_name="RenderCircleToGrid",scratch_pad_emitter_index=None,scratch_pad_emitter_name="",target_node_guid="",target_node_index=None,parameter_map_set_index=None,previous_stack_value_pin_name="PreviousStackValue",local_value_pin_name="LocalStampValue",expression="max(PreviousStackValue, LocalStampValue)",skip_if_already_wrapped=True,replace_existing_custom=True,allow_source_edit=False,save=True,request_compile=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"wrap_niagara_scratch_pad_output_with_stack_context",dict(system_path=system_path,target_pin_name=target_pin_name,scratch_pad_script_path=scratch_pad_script_path,scratch_pad_owner_kind=scratch_pad_owner_kind,scratch_pad_script_index=scratch_pad_script_index,scratch_pad_name=scratch_pad_name,scratch_pad_emitter_index=scratch_pad_emitter_index,scratch_pad_emitter_name=scratch_pad_emitter_name,target_node_guid=target_node_guid,target_node_index=target_node_index,parameter_map_set_index=parameter_map_set_index,previous_stack_value_pin_name=previous_stack_value_pin_name,local_value_pin_name=local_value_pin_name,expression=expression,skip_if_already_wrapped=skip_if_already_wrapped,replace_existing_custom=replace_existing_custom,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile),checkpoint_callback,_legacy_command_params,_public_input)

# HU: finite Named ModuleInput and Assignment follow-up only; synchronous entries above remain unchanged.
ASYNC_METHODS=["add_niagara_scratch_pad_module_input","add_niagara_scratch_pad_assignment_module_to_stack"]
_ACTIVE=False
_TICKER_HANDLE=None
_SHUTDOWN_HANDLE=None
_PENDING={}
_QUARANTINED={}
_LAST_TERMINAL={}
_LIFECYCLE_ERRORS=[]

def _async_arguments(method,public):
    if method not in ASYNC_METHODS:raise OperationError("niagara_scratch_stack_authoring_async_method_invalid","Only the original Scratch ModuleInput and Assignment Named continuations are admitted")
    args,original,legacy=_arguments(method,public,None,True)
    return dict(arguments=copy.deepcopy(public),args=args,original=original)

def _async_context(result,method,public):
    _context(result,method,public["args"],public["original"],None)
    result["data"].update(async_native_job=True,async_entry="Named_official_MCP_only",direct_Python_dict_contract_preserved=True,Raw_Batch_async_continuation_implemented=False,mandatory_wait_not_bypassed=True,
        no_Python_asset_mutation_compile_or_save=True,global_compiler_cancel_attempted=False,actual_official_async_wire_verified=False)

class _ScratchStackJournal:
    def __init__(self, method, public, run_id):
        self.result = state.new_result(run_id)
        _async_context(self.result, method, public)
        self.result["stage"] = "before_native_niagara_scratch_stack_authoring_async_start"
        self.result["data"].update(native_start_dispatched=False, native_start_returned=False,
            native_snapshot_returned=False, mutation_performed=False, mutation_state_unknown=False,
            pending_native_intent={"method": method, "arguments": copy.deepcopy(public),
                "scope": "future native phases unverified until returned snapshots"})
        self.path = state._journal_path(run_id)
        self.started = time.perf_counter()
        self.owned = False
        if self.path.exists():
            raise state.OperationError("operation_incomplete", "Niagara scratch stack IDs are not replayed; inspect the journal")
        if state._active_operation is not None:
            raise state.OperationError("mutation_busy", "Another operation owns the project slot")
        fingerprint = hashlib.sha256(json.dumps({"name": method, "arguments": public},
            sort_keys=True, allow_nan=False).encode("utf-8")).hexdigest()
        record = {"schema_version": 1, "operation_id": run_id, "operation": method,
            "arguments": copy.deepcopy(public), "fingerprint": fingerprint, "state": "started",
            "result": self.result, "engine_version": unreal.SystemLibrary.get_engine_version(), "process_id": os.getpid()}
        encoded = json.dumps(record, ensure_ascii=False, sort_keys=True, allow_nan=False)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("x", encoding="utf-8") as stream:
            stream.write(encoded); stream.flush(); os.fsync(stream.fileno())
        state._active_operation = run_id
        self.owned = True

    def checkpoint(self):
        if not self.owned or state._active_operation != self.result["operation_id"]:
            raise state.OperationError("journal_checkpoint_not_owned", "Niagara scratch stack ownership changed; no replay")
        state.checkpoint(self.result)

    def finish(self, recovery):
        if not self.owned or state._active_operation != self.result["operation_id"]:
            raise state.OperationError("journal_finalize_not_owned", "Niagara scratch stack ownership changed before finalization")
        with self.path.open("r", encoding="utf-8") as stream:
            record = json.load(stream)
        if (record.get("operation_id") != self.result["operation_id"] or record.get("state") != "started"
                or record.get("process_id") != os.getpid()):
            raise state.OperationError("journal_finalize_not_owned", "Niagara scratch stack started journal ownership differs")
        record["result"] = copy.deepcopy(self.result)
        record["state"] = "started" if recovery else "complete"
        state._write_record(self.path, record)
        if not recovery:
            state._active_operation = None
            self.owned = False


def _async_adopt(entry,native):
    result=entry['journal'].result
    job_id=native['data'].get('native_job_id');prior_id=entry.get('native_job_id')
    if prior_id is not None and job_id!=prior_id:raise state.OperationError('niagara_scratch_stack_authoring_job_identity_changed','Snapshot belongs to another job; do not repeat start')
    entry['native_job_id']=job_id
    if entry['method']==ASYNC_METHODS[0]:
        was_saved=result['save'].get('saved') is True
        _adopt(result,entry['method'],entry['public']['args'],entry['public']['original'],None,native)
        result['save']['saved']=was_saved or native['saved']
    else:
        _assignment_adopt_status(result,native)
    _async_context(result,entry['method'],entry['public'])
    result['data'].update(native_snapshot_returned=True,native_terminal_observed=native['data']['native_job_terminal'],adapter_snapshot_calls=entry['snapshot_calls'],adapter_tick_callbacks=entry['tick_callbacks'],command_may_still_be_in_flight=not native['data']['native_job_terminal'],native_job_id=job_id)
    result['data']['deferred_terminal_wait_completion_unverified']=not (native['data']['native_job_terminal'] and native['success'] and native['postcondition_passed'])
    result['stage']=native['stage']
    result['validation'].update(native_terminal_observed=native['data']['native_job_terminal'],scratch_stack_wait_proof_native_authoritative=True)
    result['retry'].update(automatic_retry=False,duplicate_risk='held_scratch_stack_job_and_journal_no_replay')
    entry['last_native']=copy.deepcopy(native)


def _assignment_wait_params(result,args):
    values=args['public_arguments'];payload=result['data'].get('legacy_result')
    payload=payload if type(payload) is dict else {}
    skipped=payload.get('skipped_duplicate') is True
    if not (values['request_compile'] and values['wait_for_completion'] and bool(payload.get('compile_requested',values['request_compile'] and not skipped))):return None
    return dict(system_path=values['target_system_path'],request_compile=False,force=False,allow_source_compile=values['allow_source_edit'],wait_for_completion=True,timeout_seconds=values['timeout_seconds'],poll_interval_seconds=values['poll_interval_seconds'])


def _assignment_adopt_status(result,status):
    # The insertion/save receipt stays canonical; GH is a separate observation within this same request.
    result['data'].update(assignment_compile_status_returned=True,assignment_compile_status_native_result=copy.deepcopy(status),assignment_compile_status_raw=status.get('_raw_native_json'))
    for key,field in (('requested','compile_requested'),('attempted','compile_attempted')):result['compile'][key]=result['compile'].get(key) is True or status[field]
    for key,field in (('requested','save_requested'),('attempted','save_attempted'),('saved','saved')):result['save'][key]=result['save'].get(key) is True or status[field]
    # A status-only follow-up does not overwrite nullable compilation or actual mutation/save history.
    if status['mutation_attempted']:
        result['data']['execution_mutation_attempted']=True
        result['changes'].append(dict(kind='assignment_compile_status',attempted=True,changed=status['changed'] if status['change_state_known'] else None,change_state_known=status['change_state_known']))
    source=result['data']['assignment_mutation_native_result'];known=source['change_state_known'] and status['change_state_known']
    result['data'].update(change_state_known=known,mutation_state_unknown=not known,mutation_performed=bool(source['changed'] or status['changed']) if known else None,execution_mutation_attempted=source['mutation_attempted'] or status['mutation_attempted'],SDK_global_purity_unverified=result['data'].get('SDK_global_purity_unverified') is True or status['data'].get('SDK_global_purity_unverified') is True)
    result['validation']['total_change_state_known']=known
    result['data']['assignment_compile_status_legacy_result']=copy.deepcopy(status['data'].get('legacy_result'))
    if type(result['data'].get('legacy_Named_result')) is dict:result['data']['legacy_Named_result']['compile_status']=copy.deepcopy(status)


def _assignment_finish_status(result,args,status):
    values=args['public_arguments'];payload=result['data'].get('legacy_result');payload=payload if type(payload) is dict else {}
    skipped=payload.get('skipped_duplicate') is True
    raw_status=copy.deepcopy(status['data'].get('legacy_result'));invalid=type(raw_status) is not dict
    compile_payload=raw_status if type(raw_status) is dict else {}
    timed_out=compile_payload.get('wait_timed_out',False);has_errors=compile_payload.get('has_errors',False)
    if type(timed_out) is not bool:invalid=True;timed_out=False
    if type(has_errors) is not bool:invalid=True;has_errors=False
    pending={}
    for field in ('outstanding_compilation_requests_after','active_compilations_after'):
        value=compile_payload.get(field,0)
        if type(value) is bool:pending[field]=value
        elif type(value) is int and value>=0:pending[field]=value>0
        else:invalid=True;pending[field]=False
    count=compile_payload.get('error_count',0)
    if type(count) is not int or count<0:invalid=True;count=0
    in_flight=bool(timed_out or any(pending.values()))
    incomplete=status.get('operation_complete') is False or status['data'].get('operation_complete') is False
    if incomplete:in_flight=True
    errors=bool(has_errors or count>0)
    observer_violation=any(status[k] is not False for k in ('mutation_attempted','compile_requested','compile_attempted','save_requested','save_attempted','saved')) or status['compiled'] is not False
    failed=not status['success'] or not status['postcondition_passed'] or compile_payload.get('success') is False or observer_violation
    result['data'].update(niagara_compile_may_still_be_in_flight=in_flight,niagara_compile_has_errors=errors,niagara_compile_status_invalid=invalid,assignment_compile_pending_predicates=pending,assignment_compile_boolean_states_are_not_counts=True,assignment_followup_observer_contract_violated=observer_violation)
    result['validation'].update(assignment_compile_status_checked=True,assignment_compile_status_passed=not(failed or in_flight or errors or invalid))
    if failed or in_flight or errors or invalid:
        if in_flight:code='niagara_compile_completion_timeout';message='Assignment completed, but compilation may still be in flight'
        elif errors:code='niagara_compile_postcondition_failed';message='Assignment completed, but compile readback reported errors'
        elif invalid:code='niagara_compile_status_invalid';message='Assignment completed, but compile readback fields were malformed'
        else:code='niagara_compile_status_read_failed';message='Assignment was sent, but compile-status readback failed'
        result['data'].update(command_may_still_be_in_flight=in_flight,automatic_action_retry=False,assignment_compile_followup_failed=True,completed_mutation_stages=[] if skipped else [ASYNC_METHODS[1]])
        mutation=copy.deepcopy(result['data'].get('legacy_Named_result'))
        result['data']['legacy_Named_result']=dict(success=False,message=message,error=message,error_code=code,mutation_response=mutation,compile_status=copy.deepcopy(status),niagara_compile_may_still_be_in_flight=in_flight,niagara_compile_has_errors=errors,niagara_compile_status_invalid=invalid)
        result['retry'].update(automatic_retry=False,duplicate_risk='assignment_already_applied_compile_followup_failed_no_replay')
        raise OperationError(code,message)


def _entry_terminal(entry,native):
    return bool(native and ((entry.get('job_reference_verified') and native['data'].get('native_job_terminal') is True) or (not entry.get('job_reference_verified') and not entry.get('native_start_call_entered') and entry.get('source_response_verified'))))


def _finish_parent(entry):
    global _LAST_TERMINAL
    result=entry['journal'].result;recovery=result['data'].get('change_state_known') is not True
    result['data'].update(native_terminal_observed=True,workflow_recovery_required=recovery,cancel_acknowledgement_is_not_shared_compiler_stop=True,final_receipt_sealed_before_delivery=True)
    try:
        _seal(entry,recovery)
        if recovery:_QUARANTINED[entry['run_id']]=entry
        _deliver(entry)
    except Exception as error:_delivery_failed(entry,error)
    _LAST_TERMINAL=dict(operation_id=entry['run_id'],success=result['success'],native_terminal_observed=True,recovery_required=recovery,delivery_error=entry.get('delivery_error',''))
    _PENDING.pop(entry['run_id'],None)


def _checkpoint_native(entry, native, force=False):
    revision = native["data"].get("native_checkpoint_revision")
    if type(revision) not in (int,float) or revision<0 or revision!=int(revision):
        raise state.OperationError("niagara_scratch_stack_authoring_async_revision_invalid", "Native meaningful snapshot revision required")
    previous = entry.get("last_checkpoint_revision")
    if previous is not None and revision < previous:
        raise state.OperationError("niagara_scratch_stack_authoring_async_revision_regressed", "Native snapshot revision regressed; no replay")
    if force or previous != revision:
        entry["native_checkpoint_attempts"] = entry.get("native_checkpoint_attempts", 0) + 1
        entry["journal"].result["data"]["adapter_native_checkpoint_attempts"] = entry["native_checkpoint_attempts"]
        entry["journal"].checkpoint()
        entry["last_checkpoint_revision"] = revision


def _delivery_error(entry, message):
    return (str(message)[:700] + "; operation_id=" + entry["run_id"]
        + "; stage=" + str(entry["journal"].result["stage"])
        + "; journal=" + str(entry["journal"].path))


def _cancel_once(entry, reason):
    if entry.get("finalization_attempted"):
        return None
    result = entry["journal"].result
    if not entry.get("job_reference_verified"):
        result["data"]["owned_cancel_not_attempted_unverified_job_reference"] = True
        return None
    if not entry.get("cancel_attempted"):
        entry["cancel_attempted"] = True
        result["data"]["adapter_owned_cancel_attempt_count"] = 1
        try:
            acknowledgement = entry["job"].cancel_job(str(reason)[:1024])
            result["data"].update(cancel_call_returned=True,
                cancel_acknowledgement=acknowledgement if type(acknowledgement) is bool else None,
                cancel_acknowledgement_is_not_shared_compiler_stop=True)
        except Exception as error:
            result["data"]["owned_cancel_error"] = str(error)[:700]
    # One best-effort read remains safe even if cancellation acknowledgement failed.
    entry["snapshot_calls"] += 1
    try:
        native = api.snapshot(entry["job"],entry["method"])
        _async_adopt(entry, native)
        return native
    except Exception as error:
        if getattr(error, "native_snapshot_returned", False):
            result["data"]["cancel_snapshot_undecoded_raw_result"] = getattr(error, "native_raw_result", None)
        _preserve_snapshot_error(entry,error)
        result["data"]["cancel_snapshot_error"] = str(error)[:700]
        return None


def _seal(entry, recovery):
    """Prepare all result data before the one durable finalization attempt."""
    if entry.get("finalization_attempted"):
        raise state.OperationError("niagara_scratch_stack_authoring_async_finalize_already_attempted", "Finalization is not replayed")
    result = entry["journal"].result
    result["data"]["elapsed_seconds"] = round(time.perf_counter() - entry["journal"].started, 6)
    entry["sealed_result"] = copy.deepcopy(result)
    entry["finalization_attempted"] = True
    entry["journal"].finish(recovery)
    entry["journal_finalized"] = True


def _delivery_failed(entry, error):
    # A finalization/write may already have committed. Do not mutate the sealed
    # result/journal or observe/cancel/restart the Job after this boundary.
    entry["delivery_error"] = str(error)[:1024]
    entry["observation_only_quarantine"] = False
    _QUARANTINED[entry["run_id"]] = entry
    _PENDING.pop(entry["run_id"], None)
    if (not entry.get("cu_completed") and not entry.get("cu_error_attempted")
            and not entry.get("cu_completion_call_entered")):
        entry["cu_error_attempted"] = True
        try:
            entry["result_object"].set_error(_delivery_error(entry, "Final receipt/durable delivery is uncertain; no replay: " + str(error)))
        except Exception as delivery_error:
            entry["delivery_error"] = str(delivery_error)[:1024]


def _deliver(entry):
    if entry.get("cu_delivery_attempted"):
        raise state.OperationError("niagara_scratch_stack_authoring_async_delivery_already_attempted", "CU delivery already attempted; no second final")
    if not entry.get("journal_finalized") or type(entry.get("sealed_result")) is not dict:
        raise state.OperationError("niagara_scratch_stack_authoring_async_final_not_sealed", "Durable final result is required before typed final delivery")
    entry["cu_delivery_attempted"] = True
    entry["result_object"].set_editor_property("value", reflected_result(copy.deepcopy(entry["sealed_result"])))
    # CompleteValue may itself call SetError and return False. Once entered,
    # no fallback completion is safe, even when its return/publication is uncertain.
    entry["cu_completion_call_entered"] = True
    if entry["result_object"].complete_value() is not True:
        raise state.OperationError("niagara_scratch_stack_authoring_async_delivery_failed", "CU completion refused; no retry or result mutation")
    entry["cu_completed"] = True


def _terminal(entry,native):
    if entry.get('finalization_attempted'):return
    result=entry['journal'].result
    _async_adopt(entry,native)
    _checkpoint_native(entry,native,force=True)
    if entry['method']==ASYNC_METHODS[1]:
        result.update(success=True,error_code='',error='')
        try:_assignment_finish_status(result,entry['public']['args'],native)
        except OperationError as error:state.fail(result,error)
    else:
        result.update(success=native['success'],error_code=native.get('error_code',''),error=native.get('error',''))
        if native['success'] and (not native['change_state_known'] or not native['postcondition_passed'] or not native['data']['operation_complete']):state.fail(result,OperationError('niagara_scratch_stack_authoring_async_result_unverified','Native success lacks terminal scoped postconditions'))
    _finish_parent(entry)


def _preserve_snapshot_error(entry,error):
    result=entry['journal'].result
    if getattr(error,'native_snapshot_returned',False):result['data'].update(native_snapshot_returned=True,undecoded_native_raw_result=getattr(error,'native_raw_result',None))
    if getattr(error,'native_unsafe_publication_quarantined',False):
        entry['native_publication_quarantined']=True
        result['data'].update(unsafe_job_publication_quarantined=True,native_writers_stopped=True)
    native=getattr(error,'native_decoded_result',None)
    if type(native) is not dict or native.get('operation')!=api.native_job_operation(entry['method']) or type(native.get('data')) is not dict:return
    job_id=native['data'].get('native_job_id');prior=entry.get('native_job_id')
    if (not entry.get('job_reference_verified') or type(job_id) is not str or not job_id or prior is not None and prior!=job_id):return
    # Even the first typed same-UObject receipt can prove a save before safe
    # terminal publication fails. Adopt its actual prefix, never its finality.
    entry['native_job_id']=job_id
    result['data']['same_job_snapshot_at_adapter_failure']=copy.deepcopy(native)
    result['save']['saved']=result['save'].get('saved') is True or native.get('saved') is True
    result['data'].update(native_mutation_attempted=result['data'].get('native_mutation_attempted') is True or native['mutation_attempted'],execution_mutation_attempted=result['data'].get('execution_mutation_attempted') is True or native['mutation_attempted'])
    result['compile'].update(requested=result['compile'].get('requested') is True or native['compile_requested'],attempted=result['compile'].get('attempted') is True or native['compile_attempted'])
    if entry['method']==ASYNC_METHODS[0]:result['compile']['compiled']=native['compiled']
    result['save'].update(requested=result['save'].get('requested') is True or native['save_requested'],attempted=result['save'].get('attempted') is True or native['save_attempted'])


def _uncertain(entry, error):
    if entry.get("finalization_attempted"):
        _delivery_failed(entry, error)
        return
    result = entry["journal"].result
    failure_stage = result["stage"]
    prior_legacy=copy.deepcopy(result['data'].get('legacy_Named_result'))
    result['data']['legacy_Named_result']=_legacy_exception(entry['method'],error)
    if entry['method']==ASYNC_METHODS[1] and entry.get('source_native_call_entered'):result['data']['legacy_Named_result'].update(mutation_response=prior_legacy,compile_status=copy.deepcopy(result['data'].get('assignment_compile_status_native_result')))
    _preserve_snapshot_error(entry,error)
    if getattr(error, "native_snapshot_returned", False):
        result["data"].update(native_snapshot_returned=True, undecoded_native_raw_result=getattr(error, "native_raw_result", None))
    last = entry.get("last_native")
    terminal = _entry_terminal(entry,last)
    if not terminal and entry.get("job_reference_verified") and not entry.get("native_publication_quarantined"):
        native = _cancel_once(entry, "Stop owned next writers after observer/journal failure: " + str(error))
        if native is not None:
            last = native
            terminal = _entry_terminal(entry,native)
    state.fail(result, error)  # Preserve the triggering failure; never promote late success.
    no_dispatch = not entry.get("native_start_call_entered") and not entry.get("source_native_call_entered")
    recovery = not no_dispatch and (not terminal or not last["change_state_known"] or entry.get("lifecycle_interrupted") and entry.get("cu_delivery_attempted"))
    result["data"].update(adapter_failure_stage_before_cancel=failure_stage,
        native_terminal_observed=terminal, workflow_recovery_required=recovery,
        last_returned_native_prefix_retained=True)
    if no_dispatch:
        result["data"].update(native_start_dispatched=False, mutation_state_unknown=False, mutation_performed=False)
        result["compile"].update(requested=False, attempted=False, compiled=None)
        result["save"].update(requested=False, attempted=False, saved=False)
    elif not terminal:
        result["data"].update(mutation_state_unknown=True, mutation_performed=None)
        for change in result["changes"]:
            change.update(change_state_known=False, changed=None)
    try:
        entry["journal"].checkpoint()
        if no_dispatch or terminal:
            _seal(entry, recovery)
    except Exception as checkpoint_error:
        if entry.get("finalization_attempted"):
            _delivery_failed(entry, checkpoint_error)
            return
        result["data"].update(checkpoint_error=str(checkpoint_error)[:700], workflow_recovery_required=True)
        recovery = True
    if recovery:
        entry["observation_only_quarantine"] = bool(entry.get("job_reference_verified") and not terminal and not entry.get("native_publication_quarantined"))
        _QUARANTINED[entry["run_id"]] = entry
    _PENDING.pop(entry["run_id"], None)
    if entry.get("cu_delivery_attempted"):
        entry["late_native_observation_after_delivery_attempt_retained"] = True
        return
    try:
        if no_dispatch or terminal:_deliver(entry)
        else:
            entry["cu_delivery_attempted"]=True
            entry["cu_error_attempted"]=True
            entry["result_object"].set_error(_delivery_error(entry,"Native terminal is unverified; started journal and ownership retained, no retry"))
    except Exception as delivery_error:
        _delivery_failed(entry, delivery_error)


def _observe_quarantined(entry):
    # If the one owned cancellation could not certify terminal, retain only read
    # observation. No second cancel, writers, saves or second CU delivery occurs.
    try:
        entry["snapshot_calls"] += 1
        native = api.snapshot(entry["job"],entry["method"])
        failure = {k: entry["journal"].result[k] for k in ("success", "error_code", "error")}
        _async_adopt(entry, native)
        entry["journal"].result.update(failure)
        entry["journal"].result["data"].update(workflow_recovery_required=True,
            quarantine_later_native_terminal=native["data"]["native_job_terminal"])
        revision = native["data"].get("native_checkpoint_revision")
        if ("quarantine_checkpoint_attempt_revision" not in entry
                or entry["quarantine_checkpoint_attempt_revision"] != revision):
            # A failed durable write is not retried each frame for the same
            # observed revision. A later actual phase/terminal prefix may be recorded.
            entry["quarantine_checkpoint_attempt_revision"] = revision
            _checkpoint_native(entry, native)
        if native["data"]["native_job_terminal"]:
            entry["observation_only_quarantine"] = False
    except Exception as error:
        _preserve_snapshot_error(entry,error)
        entry["last_quarantine_observation_error"] = str(error)[:700]


def _tick_all(delta_seconds):
    if not _ACTIVE:
        return False
    for entry in tuple(_PENDING.values()):
        if entry.get("finalization_attempted"):
            continue
        try:
            entry["tick_callbacks"] += 1
            entry["snapshot_calls"] += 1
            native = api.snapshot(entry["job"],entry["method"])
            if native["data"]["native_job_terminal"]:
                _terminal(entry, native)
            else:
                _async_adopt(entry, native)
                _checkpoint_native(entry, native)
        except Exception as error:
            _uncertain(entry, error)
    for entry in tuple(_QUARANTINED.values()):
        if entry.get("observation_only_quarantine"):
            _observe_quarantined(entry)
    return True


def _shutdown():
    # Native module owns teardown. No late asset/job/save/cleanup method here.
    global _ACTIVE
    _ACTIVE = False
    for entry in tuple(_PENDING.values()):
        if entry.get("finalization_attempted"):
            continue
        entry["lifecycle_interrupted"] = True
        result = entry["journal"].result
        state.fail(result, state.OperationError("niagara_scratch_stack_authoring_async_shutdown_unknown", "Shutdown before verified Native terminal; no late work attempted"))
        result["data"].update(mutation_state_unknown=True, mutation_performed=None,
            native_terminal_observed=False, workflow_recovery_required=True)
        try:
            entry["journal"].checkpoint()
        except Exception:
            pass  # Started journal remains; no repeated write or Engine work.
        _QUARANTINED[entry["run_id"]] = entry
        _PENDING.pop(entry["run_id"], None)


def binding_status():
    return {"active": _ACTIVE, "pending_count": len(_PENDING), "quarantined_count": len(_QUARANTINED),
        "ticker_handle_present": _TICKER_HANDLE is not None, "shutdown_handle_present": _SHUTDOWN_HANDLE is not None,
        "last_terminal": copy.deepcopy(_LAST_TERMINAL), "lifecycle_errors": list(_LIFECYCLE_ERRORS),
        "quarantined_operation_ids": sorted(_QUARANTINED),
        "quarantined_journal_paths": {run_id: str(entry["journal"].path) for run_id, entry in _QUARANTINED.items()},
        "observation_only_quarantine_count": sum(bool(entry.get("observation_only_quarantine")) for entry in _QUARANTINED.values()),
        "native_handle_membership_verified": False, "ordinary_async_Batch_integration_verified": False, "Native_bindings": api.async_binding_status()}


def activate():
    global _ACTIVE, _TICKER_HANDLE, _SHUTDOWN_HANDLE
    status=api.async_binding_status()
    if not all(value is True for value in status.values()):raise state.OperationError("niagara_scratch_stack_authoring_async_native_jobs_unavailable","Exact dedicated Native start/Job bindings are required")
    if not _ACTIVE:
        if _PENDING or _QUARANTINED or _TICKER_HANDLE is not None or _SHUTDOWN_HANDLE is not None:
            raise state.OperationError("niagara_scratch_stack_authoring_async_ownership_retained", "Prior retained ownership requires Root reconciliation")
        _SHUTDOWN_HANDLE = unreal.register_python_shutdown_callback(_shutdown)
        try:
            _TICKER_HANDLE = unreal.register_ticker_callback(_tick_all, delay=0.0)
        except Exception:
            unreal.unregister_python_shutdown_callback(_SHUTDOWN_HANDLE)
            _SHUTDOWN_HANDLE = None
            raise
        _ACTIVE = True
    return binding_status()


def deactivate(reason="Niagara scratch stack toolset deactivated"):
    global _ACTIVE, _TICKER_HANDLE, _SHUTDOWN_HANDLE
    _ACTIVE = False
    for name, handle, remover in (("ticker", _TICKER_HANDLE, unreal.unregister_ticker_callback),
            ("shutdown", _SHUTDOWN_HANDLE, unreal.unregister_python_shutdown_callback)):
        if handle is not None:
            try:
                remover(handle)
                if name == "ticker":
                    _TICKER_HANDLE = None
                else:
                    _SHUTDOWN_HANDLE = None
            except Exception as error:
                _LIFECYCLE_ERRORS.append({"handle": name, "error": str(error)[:700]})
    for entry in tuple(_PENDING.values()):
        if entry.get("finalization_attempted"):
            continue
        entry["lifecycle_interrupted"] = True
        native = _cancel_once(entry, reason)
        if native is not None and native["data"]["native_job_terminal"]:
            try:
                _terminal(entry, native)
            except Exception as error:
                _uncertain(entry, error)
        else:
            _uncertain(entry, state.OperationError("niagara_scratch_stack_authoring_async_cancel_terminal_unverified", "Owned cancellation not terminal verified; no repeat"))
    return binding_status()


def _begin(method,public):
    result_object = CubelessPythonAsyncOperationResult()
    run_id = uuid.uuid4().hex
    journal = None
    entry = None
    try:
        arguments = _async_arguments(method, public)
        if not _ACTIVE:
            raise state.OperationError("niagara_scratch_stack_authoring_async_domain_inactive", "Root registration has not activated the Niagara scratch stack domain")
        if _QUARANTINED:
            raise state.OperationError("niagara_scratch_stack_authoring_async_workflow_quarantined", "Retained prior Niagara scratch stack outcome requires Root reconciliation")
        journal = _ScratchStackJournal(method, arguments, run_id)
        entry = {"run_id": run_id, "method": method, "public": arguments, "journal": journal,
            "result_object": result_object, "job": None, "tick_callbacks": 0, "snapshot_calls": 0,
            "native_start_call_entered": False, "job_reference_verified": False,
            "cancel_attempted": False, "last_checkpoint_revision": None}
        _PENDING[run_id] = entry
        journal.result["data"].update(native_start_dispatched=True, native_mutation_attempted=None,
            mutation_performed=None, mutation_state_unknown=True)
        journal.result["compile"].update(requested=None, attempted=None, compiled=None, status="native_start_outcome_unverified")
        journal.result["save"].update(requested=None, attempted=None, saved=None, status="native_start_outcome_unverified")
        journal.checkpoint()  # Covers all future Native phases BEFORE start.
        start_params=arguments['args']['params']
        if method==ASYNC_METHODS[1]:
            entry['source_native_call_entered']=True
            journal.result['data'].update(assignment_mutation_native_dispatched=True)
            try:source_native=api.add_niagara_scratch_pad_assignment_module_to_stack(start_params)
            except Exception as error:
                if getattr(error,'native_call_dispatched',None) is False:entry['source_native_call_entered']=False
                if getattr(error,'native_call_returned',False):journal.result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
                raise
            _adopt(journal.result,method,arguments['args'],arguments['original'],None,source_native)
            _async_context(journal.result,method,arguments)
            entry['source_response_verified']=source_native.get('operation_complete') is not False and source_native['data'].get('operation_complete') is not False
            entry['last_native']=copy.deepcopy(source_native)
            journal.result['data'].update(assignment_mutation_native_result=copy.deepcopy(source_native),assignment_mutation_saved=source_native['saved'])
            journal.checkpoint() # Actual insertion/compile/save is durable before follow-up health or SDK.
            if not _ACTIVE or entry.get('lifecycle_interrupted') or _PENDING.get(run_id) is not entry:raise OperationError('niagara_scratch_stack_authoring_assignment_interrupted','Assignment returned across lifecycle interruption; no follow-up dispatch')
            if not entry['source_response_verified']:raise OperationError('niagara_scratch_stack_authoring_assignment_incomplete','Assignment prefix is incomplete; no follow-up dispatch or replay')
            if not source_native['success'] or not source_native['postcondition_passed']:
                state.fail(journal.result,OperationError(source_native.get('error_code') or 'niagara_scratch_stack_authoring_native_failed',source_native.get('error') or 'Native assignment failed; actual prefix retained'))
                _finish_parent(entry);return result_object
            start_params=_assignment_wait_params(journal.result,arguments['args'])
            if start_params is None:
                journal.result.update(success=True,stage='complete')
                journal.result['data']['assignment_followup_skipped_by_original_condition']=True
                _finish_parent(entry);return result_object
            journal.result['stage']='assignment_compile_status'
            journal.result['data'].update(assignment_compile_status_requested=True,assignment_compile_status_params=copy.deepcopy(start_params),assignment_compile_status_dispatched=False)
            journal.checkpoint()
        entry['native_start_call_entered']=True
        entry['job']=api.start_job(method,start_params)
        if method==ASYNC_METHODS[1]:journal.result['data']['assignment_compile_status_dispatched']=True
        entry["job_reference_verified"] = True
        journal.result["data"].update(native_start_returned=True)
        if not _ACTIVE or entry.get("lifecycle_interrupted") or _PENDING.get(run_id) is not entry:
            raise state.OperationError("niagara_scratch_stack_authoring_async_start_interrupted","Native start returned after lifecycle interruption; stop only this returned Job and retain started ownership")
        entry["snapshot_calls"] += 1
        native = api.snapshot(entry["job"],entry["method"])
        if native["data"]["native_job_terminal"]:
            _terminal(entry, native)
        else:
            _async_adopt(entry, native)
            _checkpoint_native(entry, native)
    except Exception as error:
        if entry is not None:
            if getattr(error,"native_start_dispatched",None) is False:entry["native_start_call_entered"]=False
            if getattr(error, "native_start_returned", False):
                entry["job"] = getattr(error, "native_returned_job", None)
                journal.result["data"]["native_start_returned"] = True
            if getattr(error, "native_start_dispatched", None) is False and not entry.get("source_native_call_entered"):
                entry["native_start_call_entered"] = False
                journal.result["data"].update(native_start_dispatched=False, mutation_state_unknown=False, mutation_performed=False)
                journal.result["compile"].update(requested=False, attempted=False, compiled=None)
                journal.result["save"].update(requested=False, attempted=False, saved=False)
                journal.result["data"]["legacy_Named_result"] = _legacy_exception(method,error)
                state.fail(journal.result, error)
                try:
                    _seal(entry, False)
                    _deliver(entry)
                except Exception as delivery_error:
                    _uncertain(entry, delivery_error)
                _PENDING.pop(run_id, None)
            else:
                _uncertain(entry, error)
        else:
            record = _rejected(method,error);record["operation_id"]=run_id
            record["data"]["legacy_Named_result"] = copy.deepcopy(getattr(error,"legacy_result",None if hasattr(error,"public_validation_errors") else _legacy_exception(method,error)))
            record["data"].update(operation=method, admission_rejected=True, native_start_dispatched=False,
                native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result_object.set_editor_property("value", reflected_result(record))
            result_object.complete_value()  # Official CU Reject may already SetError; never complete twice.
    return result_object


def completed_result(record):
    """Typed async final for pre-dispatch admission rejection only; no journal or Native job."""
    value=CubelessPythonAsyncOperationResult();value.set_editor_property('value',reflected_result(record))
    value.complete_value()  # Reject already ends the official future; no second SetError.
    return value


def _named_mcp(method,arguments):
    if method in ASYNC_METHODS:return _begin(method,arguments)
    return _named(method,arguments,_public_input=True)
