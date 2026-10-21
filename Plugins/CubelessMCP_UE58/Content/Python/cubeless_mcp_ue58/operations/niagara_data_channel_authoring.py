"""GN Data Channel layout, reader stages and typed DI authoring. No transport, replay or claimed atomic rollback."""
import copy
import json
import math
import re
import hashlib, os, time, uuid
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import niagara_data_channel_authoring as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"configure_niagara_data_channel_asset": ["asset_path", "variables", "create_if_missing", "keep_previous_frame_data", "save"], "create_niagara_ndc_reader_scratch_pad_module": ["system_path", "scratch_pad_name", "data_channel_asset_path", "stage", "position_variable", "velocity_variable", "spawn_count_variable", "event_type_variable", "position_target", "velocity_target", "spawn_count_target", "event_type_target", "read_success_target", "read_current_frame", "dry_run", "allow_source_edit", "save", "request_compile"], "add_niagara_scratch_pad_di_variadic_input": ["system_path", "scratch_pad_name", "function_node_guid", "expected_function_name", "expected_di_class", "expected_existing_inputs", "pin_name", "module_input_name", "expected_referencing_module_node_guid", "default_value", "input_type", "scratch_pad_script_path", "expected_input_map_node_guid", "expected_output_node_guid", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"]}
DEFAULTS={"configure_niagara_data_channel_asset": {"variables": None, "create_if_missing": True, "keep_previous_frame_data": True, "save": True}, "create_niagara_ndc_reader_scratch_pad_module": {"stage": "particle_spawn_read", "position_variable": "Position", "velocity_variable": "Velocity", "spawn_count_variable": "SpawnCount", "event_type_variable": "EventType", "position_target": "Particles.Position", "velocity_target": "Particles.Velocity", "spawn_count_target": "Particles.NDCSpawnCount", "event_type_target": "Particles.NDCEventType", "read_success_target": "Particles.NDCReadSuccess", "read_current_frame": False, "dry_run": False, "allow_source_edit": False, "save": True, "request_compile": True}, "add_niagara_scratch_pad_di_variadic_input": {"default_value": 0, "input_type": "int", "scratch_pad_script_path": "", "expected_input_map_node_guid": "", "expected_output_node_guid": "", "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": True, "timeout_seconds": 60.0, "poll_interval_seconds": 0.1}}
REQUIRED={"configure_niagara_data_channel_asset": ["asset_path"], "create_niagara_ndc_reader_scratch_pad_module": ["system_path", "scratch_pad_name", "data_channel_asset_path"], "add_niagara_scratch_pad_di_variadic_input": ["system_path", "scratch_pad_name", "function_node_guid", "expected_function_name", "expected_di_class", "expected_existing_inputs", "pin_name", "module_input_name", "expected_referencing_module_node_guid"]}
FIELD_TYPES={"configure_niagara_data_channel_asset": {"asset_path": "string", "variables": "array", "create_if_missing": "boolean", "keep_previous_frame_data": "boolean", "save": "boolean"}, "create_niagara_ndc_reader_scratch_pad_module": {"system_path": "string", "scratch_pad_name": "string", "data_channel_asset_path": "string", "stage": "string", "position_variable": "string", "velocity_variable": "string", "spawn_count_variable": "string", "event_type_variable": "string", "position_target": "string", "velocity_target": "string", "spawn_count_target": "string", "event_type_target": "string", "read_success_target": "string", "read_current_frame": "boolean", "dry_run": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean"}, "add_niagara_scratch_pad_di_variadic_input": {"system_path": "string", "scratch_pad_name": "string", "function_node_guid": "string", "expected_function_name": "string", "expected_di_class": "string", "expected_existing_inputs": "array", "pin_name": "string", "module_input_name": "string", "expected_referencing_module_node_guid": "string", "default_value": "any", "input_type": "string", "scratch_pad_script_path": "string", "expected_input_map_node_guid": "string", "expected_output_node_guid": "string", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}}
NULLABLE={"configure_niagara_data_channel_asset": ["variables"], "create_niagara_ndc_reader_scratch_pad_module": [], "add_niagara_scratch_pad_di_variadic_input": []}
ITEMS={"configure_niagara_data_channel_asset": {"variables": "object"}, "create_niagara_ndc_reader_scratch_pad_module": {}, "add_niagara_scratch_pad_di_variadic_input": {"expected_existing_inputs": "object"}}
OBSERVERS=[]
NO_JOURNAL_METHODS=[]
WRITERS=["configure_niagara_data_channel_asset", "create_niagara_ndc_reader_scratch_pad_module", "add_niagara_scratch_pad_di_function_call", "add_niagara_scratch_pad_di_variadic_input"]
RAW_ONLY_METHOD="add_niagara_scratch_pad_di_function_call"

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_data_channel_authoring_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_data_channel_authoring_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_data_channel_authoring_public_argument_invalid','Known method and public argument object required')
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
                        if any(type(key) is not str for key in item):raise _public_error(field,'string_type','Dictionary keys should be valid strings')
                        if method=='configure_niagara_data_channel_asset' and any(type(value) is not str for value in item.values()):raise _public_error(field,'string_type','Dictionary values should be valid strings')
                    elif ITEMS[method][field]=='number':item=_public_float(field,item)
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_data_channel_authoring_public_argument_invalid','Unexpected frozen item type')
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='any':pass
        elif kind=='object':
            if type(value) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
            if any(type(key) is not str for key in value):raise _public_error(field,'string_type','Dictionary keys should be valid strings')
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('niagara_data_channel_authoring_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

ORIGINAL_POLICY={"configure_niagara_data_channel_asset": {"effect": "write", "risk": "medium", "supports_dry_run": False, "dry_run_default": None, "transactional": False, "postcondition": "structured_result_and_save_status", "confirmation_required": False, "confirmation_mode": "none", "result_contract": "mutation_result_v1", "verification_level": "legacy", "strict_postcondition": False, "rationale": "Conservative policy inferred from niagara:configure_niagara_data_channel_asset public contract."}, "create_niagara_ndc_reader_scratch_pad_module": {"effect": "write", "risk": "medium", "supports_dry_run": True, "dry_run_default": False, "transactional": False, "postcondition": "compile_and_structured_readback", "confirmation_required": False, "confirmation_mode": "none", "result_contract": "mutation_result_v1", "verification_level": "legacy", "strict_postcondition": False, "rationale": "Conservative policy inferred from niagara:create_niagara_ndc_reader_scratch_pad_module public contract."}, "add_niagara_scratch_pad_di_variadic_input": {"effect": "write", "risk": "medium", "supports_dry_run": False, "dry_run_default": None, "transactional": False, "postcondition": "compile_and_structured_readback", "confirmation_required": False, "confirmation_mode": "none", "result_contract": "mutation_result_v1", "verification_level": "legacy", "strict_postcondition": False, "rationale": "Conservative policy inferred from niagara:add_niagara_scratch_pad_di_variadic_input public contract."}}

LEGACY_MESSAGES={"configure_niagara_data_channel_asset": "Error configuring Niagara data channel asset: ", "create_niagara_ndc_reader_scratch_pad_module": "Error creating Niagara NDC reader Scratch Pad module: ", "add_niagara_scratch_pad_di_variadic_input": "Error adding Niagara Scratch Pad DI variadic input: "}
def _legacy_exception(method,error):
    message=LEGACY_MESSAGES[method]+str(error)
    return dict(success=False,message=message,error=message,error_code="niagara_tool_failed")

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if method not in METHODS or type(public_input) is not bool:raise OperationError('niagara_data_channel_authoring_entry_invalid','Exact dedicated method and Boolean public selector required')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_data_channel_authoring_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    if method==RAW_ONLY_METHOD:raise OperationError('niagara_data_channel_authoring_raw_only','DI function-call has no original public Python contract; finite raw Batch entry required')
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method]}
    if method=='configure_niagara_data_channel_asset' and not values['variables']:params.pop('variables',None)
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,
        original_named_body_omission_preserved=True,raw_locale_boolean_integer_and_presence_Native_owned=True,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]) if method in PUBLIC_FIELDS else None,native_only_command=method==RAW_ONLY_METHOD,Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        operational_effect='read_world' if method in OBSERVERS else 'write',original_inferred_effect=ORIGINAL_POLICY.get(method,{}).get('effect'),original_inferred_risk=ORIGINAL_POLICY.get(method,{}).get('risk'),original_policy_preserved=True,mandatory_wait_contract_preserved=True,deferred_terminal_wait_completion_unverified=True,registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_niagara_data_channel_authoring_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_niagara_data_channel_authoring_explicit_save_receipt')
    if method==api.VARIADIC_METHOD and type(native['data'].get('native_job_terminal')) is bool:
        result['compile']['status']=('not_requested' if not native['compile_requested'] else 'not_attempted' if not native['compile_attempted'] else 'compiled' if native['compiled'] is True else 'unknown' if native['compiled'] is None else 'failed')
        result['save']['status']=('not_requested' if not native['save_requested'] else 'not_attempted' if not native['save_attempted'] else 'saved' if native['saved'] else 'save_returned_false')
        result['data']['compile_save_receipt_source']='Native DataChannel variadic continuation actual receipt'
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_niagara_data_channel_authoring_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        original_layout_graph_signature_and_actual_save_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['data']['original_public_policy']=copy.deepcopy(ORIGINAL_POLICY.get(method))
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_niagara_data_channel_authoring_partial_compile_save_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_niagara_data_channel_authoring_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_niagara_data_channel_authoring_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_niagara_data_channel_authoring_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_niagara_data_channel_authoring_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if native['data'].get('operation_complete') is False:
        result['data'].update(deferred_final_receipt_pending=True,command_may_still_be_in_flight=True,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        result['changes']=[dict(kind=method,attempted=native['mutation_attempted'],changed=None,change_state_known=False)]
        result['retry'].update(automatic_retry=False,duplicate_risk='deferred_native_completion_in_flight_no_replay')
        if callback:callback(result)
        raise OperationError('niagara_data_channel_authoring_deferred_completion_pending','Native scheduling is not final input/compile/readback/save completion; receipt retained and no replay')
    if not native['success']:raise OperationError(native.get('error_code') or 'niagara_data_channel_authoring_native_failed',native.get('error') or 'Native niagara_data_channel_authoring failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('niagara_data_channel_authoring_postcondition_unverified','Native niagara_data_channel_authoring postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_requested'] or native['compile_attempted'] or native['save_requested'] or native['save_attempted'] or native['compiled'] is not False or native['saved']):
        raise OperationError('niagara_data_channel_authoring_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_data_channel_authoring_input_validation';result['changes']=[]
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

def _named_mcp(method,arguments):
    if method==api.VARIADIC_METHOD:return _named_variadic_async(arguments)
    return _named(method,arguments,_public_input=True)

def configure_niagara_data_channel_asset(asset_path,variables=None,create_if_missing=True,keep_previous_frame_data=True,save=True):
    return _named("configure_niagara_data_channel_asset",dict(asset_path=asset_path,variables=variables,create_if_missing=create_if_missing,keep_previous_frame_data=keep_previous_frame_data,save=save))

def configure_niagara_data_channel_asset_action(result,asset_path,variables=None,create_if_missing=True,keep_previous_frame_data=True,save=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"configure_niagara_data_channel_asset",dict(asset_path=asset_path,variables=variables,create_if_missing=create_if_missing,keep_previous_frame_data=keep_previous_frame_data,save=save),checkpoint_callback,_legacy_command_params,_public_input)

def create_niagara_ndc_reader_scratch_pad_module(system_path,scratch_pad_name,data_channel_asset_path,stage="particle_spawn_read",position_variable="Position",velocity_variable="Velocity",spawn_count_variable="SpawnCount",event_type_variable="EventType",position_target="Particles.Position",velocity_target="Particles.Velocity",spawn_count_target="Particles.NDCSpawnCount",event_type_target="Particles.NDCEventType",read_success_target="Particles.NDCReadSuccess",read_current_frame=False,dry_run=False,allow_source_edit=False,save=True,request_compile=True):
    return _named("create_niagara_ndc_reader_scratch_pad_module",dict(system_path=system_path,scratch_pad_name=scratch_pad_name,data_channel_asset_path=data_channel_asset_path,stage=stage,position_variable=position_variable,velocity_variable=velocity_variable,spawn_count_variable=spawn_count_variable,event_type_variable=event_type_variable,position_target=position_target,velocity_target=velocity_target,spawn_count_target=spawn_count_target,event_type_target=event_type_target,read_success_target=read_success_target,read_current_frame=read_current_frame,dry_run=dry_run,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile))

def create_niagara_ndc_reader_scratch_pad_module_action(result,system_path,scratch_pad_name,data_channel_asset_path,stage="particle_spawn_read",position_variable="Position",velocity_variable="Velocity",spawn_count_variable="SpawnCount",event_type_variable="EventType",position_target="Particles.Position",velocity_target="Particles.Velocity",spawn_count_target="Particles.NDCSpawnCount",event_type_target="Particles.NDCEventType",read_success_target="Particles.NDCReadSuccess",read_current_frame=False,dry_run=False,allow_source_edit=False,save=True,request_compile=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"create_niagara_ndc_reader_scratch_pad_module",dict(system_path=system_path,scratch_pad_name=scratch_pad_name,data_channel_asset_path=data_channel_asset_path,stage=stage,position_variable=position_variable,velocity_variable=velocity_variable,spawn_count_variable=spawn_count_variable,event_type_variable=event_type_variable,position_target=position_target,velocity_target=velocity_target,spawn_count_target=spawn_count_target,event_type_target=event_type_target,read_success_target=read_success_target,read_current_frame=read_current_frame,dry_run=dry_run,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile),checkpoint_callback,_legacy_command_params,_public_input)

def add_niagara_scratch_pad_di_variadic_input(system_path,scratch_pad_name,function_node_guid,expected_function_name,expected_di_class,expected_existing_inputs,pin_name,module_input_name,expected_referencing_module_node_guid,default_value=0,input_type="int",scratch_pad_script_path="",expected_input_map_node_guid="",expected_output_node_guid="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=60.0,poll_interval_seconds=0.1):
    return _named("add_niagara_scratch_pad_di_variadic_input",dict(system_path=system_path,scratch_pad_name=scratch_pad_name,function_node_guid=function_node_guid,expected_function_name=expected_function_name,expected_di_class=expected_di_class,expected_existing_inputs=expected_existing_inputs,pin_name=pin_name,module_input_name=module_input_name,expected_referencing_module_node_guid=expected_referencing_module_node_guid,default_value=default_value,input_type=input_type,scratch_pad_script_path=scratch_pad_script_path,expected_input_map_node_guid=expected_input_map_node_guid,expected_output_node_guid=expected_output_node_guid,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def add_niagara_scratch_pad_di_variadic_input_action(result,system_path,scratch_pad_name,function_node_guid,expected_function_name,expected_di_class,expected_existing_inputs,pin_name,module_input_name,expected_referencing_module_node_guid,default_value=0,input_type="int",scratch_pad_script_path="",expected_input_map_node_guid="",expected_output_node_guid="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=60.0,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"add_niagara_scratch_pad_di_variadic_input",dict(system_path=system_path,scratch_pad_name=scratch_pad_name,function_node_guid=function_node_guid,expected_function_name=expected_function_name,expected_di_class=expected_di_class,expected_existing_inputs=expected_existing_inputs,pin_name=pin_name,module_input_name=module_input_name,expected_referencing_module_node_guid=expected_referencing_module_node_guid,default_value=default_value,input_type=input_type,scratch_pad_script_path=scratch_pad_script_path,expected_input_map_node_guid=expected_input_map_node_guid,expected_output_node_guid=expected_output_node_guid,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)

def add_niagara_scratch_pad_di_function_call_action(result,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    """Native-only finite raw action; no invented public parameters or Named tool."""
    if type(_legacy_command_params) is not dict or _public_input:
        error=OperationError("niagara_data_channel_authoring_raw_only","Exact raw params object required for original Native-only DI call")
        _rejection(result,RAW_ONLY_METHOD,error)
        if checkpoint_callback:checkpoint_callback(result)
        raise error
    return _action(result,RAW_ONLY_METHOD,{},checkpoint_callback,_legacy_command_params,False)

# MT: one concrete variadic job, using the existing async Value and journal pattern.
# No Batch driver, transport, blocking wait, cancellation-as-rollback or action replay.
_VARIADIC_PENDING={}
_VARIADIC_QUARANTINED={}
_VARIADIC_SHUTDOWN_HANDLE=None
_VARIADIC_SHUTDOWN_REGISTERED=False
_VARIADIC_STOPPING=False
_VARIADIC_LIFECYCLE_ERRORS=[]

def _variadic_requires_async(params):
    # This helper is used only after the existing Named public Boolean normalization.
    return params.get('request_compile',True) is not False or params.get('save',True) is not False

def completed_result(record,_value=None):
    from cubeless_mcp_ue58.registration.async_interop_toolset import CubelessPythonAsyncOperationResult
    from cubeless_mcp_ue58.registration.results import reflected_result
    value=_value if _value is not None else CubelessPythonAsyncOperationResult()
    value.set_editor_property('value',reflected_result(record))
    if value.complete_value() is not True:raise OperationError('niagara_data_channel_variadic_delivery_failed','Async result completion was not confirmed; no replay')
    return value

def async_binding_status():
    return dict(api.async_binding_status(),pending_count=len(_VARIADIC_PENDING),quarantined_count=len(_VARIADIC_QUARANTINED),shutdown_handle_present=_VARIADIC_SHUTDOWN_HANDLE is not None,shutdown_callback_registered=_VARIADIC_SHUTDOWN_REGISTERED,stopping=_VARIADIC_STOPPING,lifecycle_errors=copy.deepcopy(_VARIADIC_LIFECYCLE_ERRORS))

def _variadic_checkpoint(entry):
    if state._active_operation!=entry['result']['operation_id']:raise OperationError('journal_checkpoint_not_owned','Variadic parent lease changed; no later writer')
    state.checkpoint(entry['result'])

def _variadic_remove_ticker(entry):
    handle=entry.get('ticker')
    if handle is not None:
        unreal.unregister_ticker_callback(handle);entry['ticker']=None

def _variadic_release_shutdown():
    global _VARIADIC_SHUTDOWN_HANDLE,_VARIADIC_SHUTDOWN_REGISTERED
    if _VARIADIC_PENDING or not _VARIADIC_SHUTDOWN_REGISTERED:return
    try:unreal.unregister_python_shutdown_callback(_VARIADIC_SHUTDOWN_HANDLE)
    except Exception as error:
        _VARIADIC_LIFECYCLE_ERRORS.append(dict(handle='shutdown',error=str(error)[:700]));raise
    _VARIADIC_SHUTDOWN_HANDLE=None;_VARIADIC_SHUTDOWN_REGISTERED=False

def _variadic_finish(entry):
    from cubeless_mcp_ue58.registration.results import reflected_result
    if entry.get('finalization_entered'):return
    entry['finalization_entered']=True;result=entry['result'];run_id=result['operation_id']
    recovery=result['data'].get('mutation_state_unknown') is True
    result['data'].update(operation_complete=entry.get('terminal') is True,workflow_recovery_required=recovery,
        journal_state='started' if recovery else 'complete',elapsed_seconds=round(time.perf_counter()-entry['started'],6))
    try:
        if not entry.get('journal_owned'):
            result['data'].update(journal_created=False,journal_creation_failed=True,journal_path=str(entry['path']),journal_state='unverified')
            _variadic_release_shutdown()
            entry['delivery_value_entered']=True;entry['result_object'].set_editor_property('value',reflected_result(result))
            entry['completion_entered']=True
            if entry['result_object'].complete_value() is not True:raise OperationError('niagara_data_channel_variadic_delivery_failed','Pre-dispatch error delivery was not confirmed')
            return
        _variadic_remove_ticker(entry);_variadic_checkpoint(entry)
        with entry['path'].open('r',encoding='utf-8') as stream:record=json.load(stream)
        if (state._active_operation!=run_id or record.get('operation_id')!=run_id or record.get('state')!='started'
                or record.get('process_id')!=os.getpid() or record.get('fingerprint')!=entry['fingerprint']):
            raise OperationError('journal_finalize_not_owned','Variadic journal identity/fingerprint changed; no replay')
        record['result']=copy.deepcopy(result);record['state']='started' if recovery else 'complete';state._write_record(entry['path'],record)
        _VARIADIC_PENDING.pop(run_id,None)
        _variadic_release_shutdown()
        if recovery:_VARIADIC_QUARANTINED[run_id]=entry
        else:state._active_operation=None
        if entry.get('shutdown_interrupted'):return
        entry['delivery_value_entered']=True;entry['result_object'].set_editor_property('value',reflected_result(result))
        entry['completion_entered']=True
        if entry['result_object'].complete_value() is not True:
            raise OperationError('niagara_data_channel_variadic_delivery_failed','Async completion was not confirmed; journal and Native prefix retained')
    except Exception as error:
        state.fail(result,error);result['data'].update(workflow_recovery_required=True,adapter_delivery_or_journal_failed=True)
        _VARIADIC_PENDING.pop(run_id,None);_VARIADIC_QUARANTINED[run_id]=entry
        if not _VARIADIC_LIFECYCLE_ERRORS:
            try:_variadic_release_shutdown()
            except Exception as cleanup_error:result['data']['shutdown_callback_release_error']=str(cleanup_error)[:700]
        if not entry.get('shutdown_interrupted') and not entry.get('completion_entered') and not entry.get('error_delivery_entered'):
            entry['error_delivery_entered']=True
            try:entry['result_object'].set_error('Variadic delivery uncertain; inspect journal; no replay; operation_id='+run_id+'; '+str(error)[:700])
            except Exception as delivery_error:entry['delivery_error']=str(delivery_error)[:700]

def _variadic_problem(entry,error):
    if entry.get('finalization_entered'):return
    result=entry['result'];entry['stopped']=True
    if getattr(error,'native_snapshot_returned',False):result['data']['unverified_native_snapshot_raw']=getattr(error,'native_raw_result',None)
    if getattr(error,'native_start_returned',False):
        entry['unverified_returned_job']=getattr(error,'native_returned_job',None);result['data']['native_start_returned']=True
    dispatched=entry.get('start_entered',False) and getattr(error,'native_start_dispatched',None) is not False
    if entry.get('job') is not None and not entry.get('terminal') and not entry.get('cancel_entered') and not entry.get('shutdown_interrupted'):
        entry['cancel_entered']=True
        try:result['data']['native_cancel_returned']=api.cancel_variadic_job(entry['job'],str(error)[:700])
        except Exception as cancel_error:result['data']['native_cancel_error']=str(cancel_error)[:700]
        result['data']['compiler_cessation_verified']=False
    state.fail(result,error)
    result['data'].update(change_state_known=not dispatched,mutation_state_unknown=dispatched,mutation_performed=None if dispatched else False,
        command_may_still_be_in_flight=dispatched,automatic_action_retry=False)
    if not dispatched:
        result['changes']=[];result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False)
        entry['terminal']=True
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_variadic_job_no_replay' if dispatched else 'none_observed')
    _variadic_finish(entry)

def _variadic_update(entry,native):
    data=native['data'];job_id=data['native_job_id'];revision=data['native_checkpoint_revision'];job_path=data['native_job_path']
    if ((entry.get('job_id') is not None and entry['job_id']!=job_id) or (entry.get('job_path') is not None and entry['job_path']!=job_path) or revision<entry.get('revision',0)):
        entry['result']['data']['unverified_native_snapshot_raw']=native['_raw_native_json']
        raise OperationError('niagara_data_channel_variadic_snapshot_changed','Job identity/revision differs; no later snapshot or replay')
    entry['job_id']=job_id;entry['job_path']=job_path;entry['revision']=revision;entry['terminal']=data['native_job_terminal']
    result=entry['result'];_adopt(result,api.VARIADIC_METHOD,entry['args'],entry['original'],None,native)
    result['data'].update(compound_request_fingerprint=entry['fingerprint'],adapter_engine_queries=2,
        adapter_query_scope='returned transient job class/path only; cached Native snapshots and lifecycle APIs',
        operation_complete=entry['terminal'],deferred_terminal_wait_completion_unverified=not (entry['terminal'] and native['success'] and native['postcondition_passed'] and native['change_state_known']),
        deferred_final_receipt_pending=not entry['terminal'],command_may_still_be_in_flight=not entry['terminal'] or data.get('compiler_may_continue_after_job') is True)
    if not entry['terminal']:
        result['success']=False;result['stage']='native_niagara_data_channel_variadic_pending'
        result['data'].update(change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        result['changes']=[dict(kind=api.VARIADIC_METHOD,attempted=native['mutation_attempted'],changed=None,change_state_known=False)]
        result['retry'].update(automatic_retry=False,duplicate_risk='deferred_native_completion_in_flight_no_replay')
        _variadic_checkpoint(entry);return
    if native['success'] and native['postcondition_passed'] and native['change_state_known']:result.update(success=True,stage='complete')
    else:state.fail(result,OperationError(native.get('error_code') or 'niagara_data_channel_variadic_native_failed',native.get('error') or 'Native final postcondition failed; actual prefix retained'))
    _variadic_finish(entry)

def _variadic_tick(run_id,delta):
    entry=_VARIADIC_PENDING.get(run_id)
    if entry is None:return False
    if entry.get('executing'):return True
    if entry.get('stopped') or entry.get('finalization_entered'):return False
    entry['executing']=True
    try:
        _variadic_checkpoint(entry)
        native=api.snapshot_variadic_job(entry['job'])
        if entry.get('shutdown_interrupted'):raise OperationError('niagara_data_channel_variadic_shutdown','Shutdown interrupted snapshot delivery; no later Native call')
        _variadic_update(entry,native)
    except Exception as error:_variadic_problem(entry,error)
    finally:entry['executing']=False
    return run_id in _VARIADIC_PENDING

def _variadic_shutdown():
    global _VARIADIC_STOPPING
    _VARIADIC_STOPPING=True
    for entry in tuple(_VARIADIC_PENDING.values()):
        entry['shutdown_interrupted']=True
        if not entry.get('executing'):
            _variadic_problem(entry,OperationError('niagara_data_channel_variadic_shutdown','Python shutdown interrupted completion; compiler cessation is not established; no replay'))
    if not _VARIADIC_PENDING:
        try:_variadic_release_shutdown()
        except Exception:pass # Actual handle and diagnostic remain available; never retry a writer.


def _named_variadic_async(arguments):
    from cubeless_mcp_ue58.registration.async_interop_toolset import CubelessPythonAsyncOperationResult
    global _VARIADIC_SHUTDOWN_HANDLE,_VARIADIC_SHUTDOWN_REGISTERED
    method=api.VARIADIC_METHOD;entry=None;value=None
    try:
        args,original,legacy=_arguments(method,arguments,None,True)
        if not _variadic_requires_async(args['params']):return completed_result(_named(method,arguments,_public_input=True))
        status=api.async_binding_status()
        if not all(v is True for v in status.values()):raise OperationError('niagara_data_channel_variadic_job_unavailable','Exact Native job starts/snapshot/cancel/type are required; no fallback dispatched')
        if _VARIADIC_STOPPING or _VARIADIC_LIFECYCLE_ERRORS or state._active_operation is not None or _VARIADIC_PENDING or _VARIADIC_QUARANTINED:raise OperationError('mutation_busy','A mutation or unreconciled variadic job owns the operation slot')
        if not _VARIADIC_SHUTDOWN_REGISTERED:
            _VARIADIC_SHUTDOWN_HANDLE=unreal.register_python_shutdown_callback(_variadic_shutdown)
            _VARIADIC_SHUTDOWN_REGISTERED=True
        shutdown_handle=_VARIADIC_SHUTDOWN_HANDLE
        engine_version=unreal.SystemLibrary.get_engine_version()
        value=CubelessPythonAsyncOperationResult()
        result=state.new_result();run_id=result['operation_id'];path=state._journal_path(run_id)
        if path.exists():raise OperationError('operation_incomplete','Variadic operation IDs are not replayed')
        _context(result,method,args,original,None)
        result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
        fingerprint=hashlib.sha256(json.dumps(dict(name=method,arguments=original),sort_keys=True,allow_nan=False).encode('utf-8')).hexdigest()
        record=dict(schema_version=1,operation_id=run_id,operation=method,arguments=original,fingerprint=fingerprint,state='started',result=result,engine_version=engine_version,process_id=os.getpid())
        if (_VARIADIC_STOPPING or _VARIADIC_LIFECYCLE_ERRORS or state._active_operation is not None or _VARIADIC_PENDING or _VARIADIC_QUARANTINED
                or not _VARIADIC_SHUTDOWN_REGISTERED or _VARIADIC_SHUTDOWN_HANDLE is not shutdown_handle):
            raise OperationError('niagara_data_channel_variadic_bootstrap_changed','Operation slot/lifecycle changed during async result bootstrap; existing owner is preserved; no Native job dispatched')
        encoded=json.dumps(record,ensure_ascii=False,allow_nan=False,sort_keys=True);path.parent.mkdir(parents=True,exist_ok=True)
        entry=dict(result=result,result_object=value,args=args,original=original,path=path,fingerprint=fingerprint,started=time.perf_counter(),job=None,ticker=None,executing=True,terminal=False,journal_owned=False)
        with path.open('x',encoding='utf-8') as stream:stream.write(encoded);stream.flush();os.fsync(stream.fileno())
        entry['journal_owned']=True;state._active_operation=run_id
        _VARIADIC_PENDING[run_id]=entry;_variadic_checkpoint(entry)
        entry['ticker']=unreal.register_ticker_callback(lambda delta:_variadic_tick(run_id,delta),delay=0.0)
        if (_VARIADIC_STOPPING or entry.get('shutdown_interrupted') or state._active_operation!=run_id
                or _VARIADIC_PENDING.get(run_id) is not entry or _VARIADIC_SHUTDOWN_HANDLE is not shutdown_handle):
            raise OperationError('niagara_data_channel_variadic_bootstrap_changed','Owned entry/lifecycle changed during ticker registration; no Native job dispatched')
        result['data'].update(native_dispatched=True,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        result['stage']='before_native_niagara_data_channel_variadic_start';_variadic_checkpoint(entry)
        entry['start_entered']=True;entry['job']=api.start_variadic_job(args['params'])
        result['data'].update(native_start_returned=True,native_job_reference_verified=True)
        if entry.get('shutdown_interrupted'):raise OperationError('niagara_data_channel_variadic_shutdown','Shutdown interrupted start return; no later Native call')
        native=api.snapshot_variadic_job(entry['job'])
        if entry.get('shutdown_interrupted'):raise OperationError('niagara_data_channel_variadic_shutdown','Shutdown interrupted initial snapshot; no later Native call')
        _variadic_update(entry,native)
    except Exception as error:
        if entry is not None:_variadic_problem(entry,error)
        else:
            try:_variadic_release_shutdown()
            except Exception as cleanup_error:error.shutdown_callback_release_error=str(cleanup_error)[:700]
            rejected=_rejected(method,error);rejected['data']['legacy_Named_result']=copy.deepcopy(getattr(error,'legacy_result',None if hasattr(error,'public_validation_errors') else _legacy_exception(method,error)))
            rejected['data']['async_result_object_returned']=value is not None
            if _VARIADIC_STOPPING:
                rejected['data']['delivery_deferred_during_shutdown']=True
                _VARIADIC_QUARANTINED[rejected['operation_id']]=dict(result=rejected,result_object=value,shutdown_interrupted=True)
                return value
            return completed_result(rejected,value)
    finally:
        if entry is not None:entry['executing']=False
    return value
