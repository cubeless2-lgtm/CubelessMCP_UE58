"""GX Blueprint Niagara template4: original inspect/dryrun/canary/durable contracts and actual Native receipt. No transport, synthetic verification, replay or shared-job framework."""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_niagara_template_swap as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"inspect_blueprint_niagara_component_template": ["blueprint_path", "component_name", "expected_system_path", "parameter_name_contains", "include_inherited_scs_templates", "require_exactly_one", "require_expected_system_match", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_component_object_properties", "max_object_properties", "max_array_elements", "max_property_value_chars"], "dry_run_blueprint_niagara_component_template_asset_swap": ["blueprint_path", "component_name", "source_system_path", "replacement_system_path", "confirmation_token", "operation", "dry_run", "preserve_override_parameters", "allow_unrealmcp_dependency", "parameter_name_contains", "include_inherited_scs_templates", "require_exactly_one", "require_source_system_match", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_component_object_properties", "max_object_properties", "max_array_elements", "max_property_value_chars"], "execute_blueprint_niagara_component_template_asset_swap_canary": ["source_blueprint_path", "target_blueprint_path", "component_name", "source_system_path", "replacement_system_path", "confirmation_token", "operation", "dry_run", "preserve_override_parameters", "allow_unrealmcp_dependency", "replace_existing_target", "allow_dirty_source_blueprint", "compile_blueprint", "save_blueprint", "parameter_name_contains", "include_inherited_scs_templates", "require_exactly_one", "require_source_system_match", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_component_object_properties", "max_object_properties", "max_array_elements", "max_property_value_chars"], "execute_blueprint_niagara_component_template_asset_swap_durable": ["blueprint_path", "component_name", "source_system_path", "replacement_system_path", "confirmation_token", "operation", "dry_run", "preserve_override_parameters", "allow_unrealmcp_dependency", "allow_dirty_blueprint", "compile_blueprint", "save_blueprint", "parameter_name_contains", "include_inherited_scs_templates", "require_exactly_one", "require_source_system_match", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_component_object_properties", "max_object_properties", "max_array_elements", "max_property_value_chars"]}
DEFAULTS={"inspect_blueprint_niagara_component_template": {"component_name": "", "expected_system_path": "", "parameter_name_contains": "", "include_inherited_scs_templates": True, "require_exactly_one": False, "require_expected_system_match": False, "max_components": 64, "max_parameters": 256, "include_parameter_values": True, "include_raw_bytes": False, "max_raw_bytes": 64, "include_parameter_object_properties": False, "include_component_object_properties": False, "max_object_properties": 80, "max_array_elements": 16, "max_property_value_chars": 512}, "dry_run_blueprint_niagara_component_template_asset_swap": {"operation": "durable_bp_component_template_asset_swap", "dry_run": True, "preserve_override_parameters": True, "allow_unrealmcp_dependency": False, "parameter_name_contains": "", "include_inherited_scs_templates": True, "require_exactly_one": True, "require_source_system_match": True, "max_components": 8, "max_parameters": 256, "include_parameter_values": True, "include_raw_bytes": False, "max_raw_bytes": 64, "include_parameter_object_properties": False, "include_component_object_properties": False, "max_object_properties": 80, "max_array_elements": 16, "max_property_value_chars": 512}, "execute_blueprint_niagara_component_template_asset_swap_canary": {"operation": "temp_bp_component_template_asset_swap_canary", "dry_run": True, "preserve_override_parameters": True, "allow_unrealmcp_dependency": False, "replace_existing_target": False, "allow_dirty_source_blueprint": False, "compile_blueprint": False, "save_blueprint": False, "parameter_name_contains": "", "include_inherited_scs_templates": False, "require_exactly_one": True, "require_source_system_match": True, "max_components": 8, "max_parameters": 256, "include_parameter_values": True, "include_raw_bytes": False, "max_raw_bytes": 64, "include_parameter_object_properties": False, "include_component_object_properties": False, "max_object_properties": 80, "max_array_elements": 16, "max_property_value_chars": 512}, "execute_blueprint_niagara_component_template_asset_swap_durable": {"operation": "durable_bp_component_template_asset_swap", "dry_run": True, "preserve_override_parameters": True, "allow_unrealmcp_dependency": False, "allow_dirty_blueprint": False, "compile_blueprint": False, "save_blueprint": False, "parameter_name_contains": "", "include_inherited_scs_templates": False, "require_exactly_one": True, "require_source_system_match": True, "max_components": 8, "max_parameters": 256, "include_parameter_values": True, "include_raw_bytes": False, "max_raw_bytes": 64, "include_parameter_object_properties": False, "include_component_object_properties": False, "max_object_properties": 80, "max_array_elements": 16, "max_property_value_chars": 512}}
REQUIRED={"inspect_blueprint_niagara_component_template": ["blueprint_path"], "dry_run_blueprint_niagara_component_template_asset_swap": ["blueprint_path", "component_name", "source_system_path", "replacement_system_path", "confirmation_token"], "execute_blueprint_niagara_component_template_asset_swap_canary": ["source_blueprint_path", "target_blueprint_path", "component_name", "source_system_path", "replacement_system_path", "confirmation_token"], "execute_blueprint_niagara_component_template_asset_swap_durable": ["blueprint_path", "component_name", "source_system_path", "replacement_system_path", "confirmation_token"]}
FIELD_TYPES={"inspect_blueprint_niagara_component_template": {"blueprint_path": "string", "component_name": "string", "expected_system_path": "string", "parameter_name_contains": "string", "include_inherited_scs_templates": "boolean", "require_exactly_one": "boolean", "require_expected_system_match": "boolean", "max_components": "integer", "max_parameters": "integer", "include_parameter_values": "boolean", "include_raw_bytes": "boolean", "max_raw_bytes": "integer", "include_parameter_object_properties": "boolean", "include_component_object_properties": "boolean", "max_object_properties": "integer", "max_array_elements": "integer", "max_property_value_chars": "integer"}, "dry_run_blueprint_niagara_component_template_asset_swap": {"blueprint_path": "string", "component_name": "string", "source_system_path": "string", "replacement_system_path": "string", "confirmation_token": "string", "operation": "string", "dry_run": "boolean", "preserve_override_parameters": "boolean", "allow_unrealmcp_dependency": "boolean", "parameter_name_contains": "string", "include_inherited_scs_templates": "boolean", "require_exactly_one": "boolean", "require_source_system_match": "boolean", "max_components": "integer", "max_parameters": "integer", "include_parameter_values": "boolean", "include_raw_bytes": "boolean", "max_raw_bytes": "integer", "include_parameter_object_properties": "boolean", "include_component_object_properties": "boolean", "max_object_properties": "integer", "max_array_elements": "integer", "max_property_value_chars": "integer"}, "execute_blueprint_niagara_component_template_asset_swap_canary": {"source_blueprint_path": "string", "target_blueprint_path": "string", "component_name": "string", "source_system_path": "string", "replacement_system_path": "string", "confirmation_token": "string", "operation": "string", "dry_run": "boolean", "preserve_override_parameters": "boolean", "allow_unrealmcp_dependency": "boolean", "replace_existing_target": "boolean", "allow_dirty_source_blueprint": "boolean", "compile_blueprint": "boolean", "save_blueprint": "boolean", "parameter_name_contains": "string", "include_inherited_scs_templates": "boolean", "require_exactly_one": "boolean", "require_source_system_match": "boolean", "max_components": "integer", "max_parameters": "integer", "include_parameter_values": "boolean", "include_raw_bytes": "boolean", "max_raw_bytes": "integer", "include_parameter_object_properties": "boolean", "include_component_object_properties": "boolean", "max_object_properties": "integer", "max_array_elements": "integer", "max_property_value_chars": "integer"}, "execute_blueprint_niagara_component_template_asset_swap_durable": {"blueprint_path": "string", "component_name": "string", "source_system_path": "string", "replacement_system_path": "string", "confirmation_token": "string", "operation": "string", "dry_run": "boolean", "preserve_override_parameters": "boolean", "allow_unrealmcp_dependency": "boolean", "allow_dirty_blueprint": "boolean", "compile_blueprint": "boolean", "save_blueprint": "boolean", "parameter_name_contains": "string", "include_inherited_scs_templates": "boolean", "require_exactly_one": "boolean", "require_source_system_match": "boolean", "max_components": "integer", "max_parameters": "integer", "include_parameter_values": "boolean", "include_raw_bytes": "boolean", "max_raw_bytes": "integer", "include_parameter_object_properties": "boolean", "include_component_object_properties": "boolean", "max_object_properties": "integer", "max_array_elements": "integer", "max_property_value_chars": "integer"}}
NULLABLE={"inspect_blueprint_niagara_component_template": [], "dry_run_blueprint_niagara_component_template_asset_swap": [], "execute_blueprint_niagara_component_template_asset_swap_canary": [], "execute_blueprint_niagara_component_template_asset_swap_durable": []}
ITEMS={"inspect_blueprint_niagara_component_template": {}, "dry_run_blueprint_niagara_component_template_asset_swap": {}, "execute_blueprint_niagara_component_template_asset_swap_canary": {}, "execute_blueprint_niagara_component_template_asset_swap_durable": {}}
OBSERVERS=["inspect_blueprint_niagara_component_template", "dry_run_blueprint_niagara_component_template_asset_swap"]
NO_JOURNAL_METHODS=["inspect_blueprint_niagara_component_template", "dry_run_blueprint_niagara_component_template_asset_swap"]
WRITERS=["execute_blueprint_niagara_component_template_asset_swap_canary", "execute_blueprint_niagara_component_template_asset_swap_durable"]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('blueprint_niagara_template_swap_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('blueprint_niagara_template_swap_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('blueprint_niagara_template_swap_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('blueprint_niagara_template_swap_public_argument_invalid','Unexpected frozen item type')
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='object':
            if type(value) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
            if any(type(key) is not str for key in value):raise _public_error(field,'string_type','Dictionary keys should be valid strings')
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('blueprint_niagara_template_swap_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

def _legacy_exception(method,error):
    message=str(error)
    return dict(success=False,message=message,error=message,error_code='editor_tool_failed',details=dict(exception_type=type(error).__name__))

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('blueprint_niagara_template_swap_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('blueprint_niagara_template_swap_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={}
    for field in PUBLIC_FIELDS[method]:
        value=values[field];kind=FIELD_TYPES[method][field]
        if kind=='boolean':params[field]=bool(value)
        elif kind=='integer':params[field]=int(value)
        elif field=='parameter_name_contains' or method==METHODS[0] and field in ('component_name','expected_system_path'):
            if value:params[field]=copy.deepcopy(value)
        else:params[field]=copy.deepcopy(value)
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _is_observer(method,params):
    return method in OBSERVERS or method in WRITERS and params.get('dry_run',True) is True

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,hidden_fields=0,
        original_model_scalar_coercion_and_truthy_filter_omission_preserved=True,raw_known_presence_and_native_defaults_preserved=True,Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,operational_effect='read_world' if _is_observer(method,args['params']) else 'write',original_inferred_effect='read' if method in OBSERVERS else 'write',original_inferred_risk='low' if method in OBSERVERS else 'medium',original_native_observer=_is_observer(method,args['params']),
        SCS_inherited_actual_template_selection_Native_owned=True,preserve_overrides_is_SetAsset_reset_false_not_full_store_preservation_proof=True,source_dirty_target_duplicate_compile_save_marker_partial_receipt_Native_owned=True,inspect_preflight_false_is_diagnostic_not_synthetic_failure=True,canary_compile_save_defaults_false_preserved=True,durable_apply_compile_and_save_required_Native_owned=True,replace_existing_force_unsupported_prewrite_not_faked=True,registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_blueprint_niagara_template_swap_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_blueprint_niagara_template_swap_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_blueprint_niagara_template_swap_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        original_template_swap_projection_and_partial_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_blueprint_niagara_template_swap_load_getter_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_blueprint_niagara_template_swap_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_blueprint_niagara_template_swap_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_blueprint_niagara_template_swap_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_blueprint_niagara_template_swap_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if native.get('operation_complete') is False or native['data'].get('operation_complete') is False:
        result['data'].update(operation_complete=False,in_flight=True,command_may_still_be_in_flight=True,automatic_action_retry=False,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        result['changes']=[dict(kind=method,attempted=native['mutation_attempted'],changed=None,change_state_known=False)]
        result['retry'].update(automatic_retry=False,duplicate_risk='template_swap_completion_in_flight_no_replay')
        if callback:callback(result)
        raise OperationError('blueprint_niagara_template_swap_incomplete','Native observation is incomplete; actual partial prefix retained without synthetic completion or replay')
    if not native['success']:raise OperationError(native.get('error_code') or 'blueprint_niagara_template_swap_native_failed',native.get('error') or 'Native blueprint_niagara_template_swap failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('blueprint_niagara_template_swap_postcondition_unverified','Native blueprint_niagara_template_swap postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if _is_observer(method,args['params']) and (native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted'] or native['compiled'] is not False or native['saved'] or (method in OBSERVERS and (native['compile_requested'] or native['save_requested']))):
        raise OperationError('blueprint_niagara_template_swap_observer_unverified','Observer returned unexpected explicit work; actual returned prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='blueprint_niagara_template_swap_input_validation';result['changes']=[]
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

def inspect_blueprint_niagara_component_template(blueprint_path,component_name="",expected_system_path="",parameter_name_contains="",include_inherited_scs_templates=True,require_exactly_one=False,require_expected_system_match=False,max_components=64,max_parameters=256,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_parameter_object_properties=False,include_component_object_properties=False,max_object_properties=80,max_array_elements=16,max_property_value_chars=512):
    return _named("inspect_blueprint_niagara_component_template",dict(blueprint_path=blueprint_path,component_name=component_name,expected_system_path=expected_system_path,parameter_name_contains=parameter_name_contains,include_inherited_scs_templates=include_inherited_scs_templates,require_exactly_one=require_exactly_one,require_expected_system_match=require_expected_system_match,max_components=max_components,max_parameters=max_parameters,include_parameter_values=include_parameter_values,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes,include_parameter_object_properties=include_parameter_object_properties,include_component_object_properties=include_component_object_properties,max_object_properties=max_object_properties,max_array_elements=max_array_elements,max_property_value_chars=max_property_value_chars))

def inspect_blueprint_niagara_component_template_action(result,blueprint_path,component_name="",expected_system_path="",parameter_name_contains="",include_inherited_scs_templates=True,require_exactly_one=False,require_expected_system_match=False,max_components=64,max_parameters=256,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_parameter_object_properties=False,include_component_object_properties=False,max_object_properties=80,max_array_elements=16,max_property_value_chars=512,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"inspect_blueprint_niagara_component_template",dict(blueprint_path=blueprint_path,component_name=component_name,expected_system_path=expected_system_path,parameter_name_contains=parameter_name_contains,include_inherited_scs_templates=include_inherited_scs_templates,require_exactly_one=require_exactly_one,require_expected_system_match=require_expected_system_match,max_components=max_components,max_parameters=max_parameters,include_parameter_values=include_parameter_values,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes,include_parameter_object_properties=include_parameter_object_properties,include_component_object_properties=include_component_object_properties,max_object_properties=max_object_properties,max_array_elements=max_array_elements,max_property_value_chars=max_property_value_chars),checkpoint_callback,_legacy_command_params,_public_input)

def dry_run_blueprint_niagara_component_template_asset_swap(blueprint_path,component_name,source_system_path,replacement_system_path,confirmation_token,operation="durable_bp_component_template_asset_swap",dry_run=True,preserve_override_parameters=True,allow_unrealmcp_dependency=False,parameter_name_contains="",include_inherited_scs_templates=True,require_exactly_one=True,require_source_system_match=True,max_components=8,max_parameters=256,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_parameter_object_properties=False,include_component_object_properties=False,max_object_properties=80,max_array_elements=16,max_property_value_chars=512):
    return _named("dry_run_blueprint_niagara_component_template_asset_swap",dict(blueprint_path=blueprint_path,component_name=component_name,source_system_path=source_system_path,replacement_system_path=replacement_system_path,confirmation_token=confirmation_token,operation=operation,dry_run=dry_run,preserve_override_parameters=preserve_override_parameters,allow_unrealmcp_dependency=allow_unrealmcp_dependency,parameter_name_contains=parameter_name_contains,include_inherited_scs_templates=include_inherited_scs_templates,require_exactly_one=require_exactly_one,require_source_system_match=require_source_system_match,max_components=max_components,max_parameters=max_parameters,include_parameter_values=include_parameter_values,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes,include_parameter_object_properties=include_parameter_object_properties,include_component_object_properties=include_component_object_properties,max_object_properties=max_object_properties,max_array_elements=max_array_elements,max_property_value_chars=max_property_value_chars))

def dry_run_blueprint_niagara_component_template_asset_swap_action(result,blueprint_path,component_name,source_system_path,replacement_system_path,confirmation_token,operation="durable_bp_component_template_asset_swap",dry_run=True,preserve_override_parameters=True,allow_unrealmcp_dependency=False,parameter_name_contains="",include_inherited_scs_templates=True,require_exactly_one=True,require_source_system_match=True,max_components=8,max_parameters=256,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_parameter_object_properties=False,include_component_object_properties=False,max_object_properties=80,max_array_elements=16,max_property_value_chars=512,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"dry_run_blueprint_niagara_component_template_asset_swap",dict(blueprint_path=blueprint_path,component_name=component_name,source_system_path=source_system_path,replacement_system_path=replacement_system_path,confirmation_token=confirmation_token,operation=operation,dry_run=dry_run,preserve_override_parameters=preserve_override_parameters,allow_unrealmcp_dependency=allow_unrealmcp_dependency,parameter_name_contains=parameter_name_contains,include_inherited_scs_templates=include_inherited_scs_templates,require_exactly_one=require_exactly_one,require_source_system_match=require_source_system_match,max_components=max_components,max_parameters=max_parameters,include_parameter_values=include_parameter_values,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes,include_parameter_object_properties=include_parameter_object_properties,include_component_object_properties=include_component_object_properties,max_object_properties=max_object_properties,max_array_elements=max_array_elements,max_property_value_chars=max_property_value_chars),checkpoint_callback,_legacy_command_params,_public_input)

def execute_blueprint_niagara_component_template_asset_swap_canary(source_blueprint_path,target_blueprint_path,component_name,source_system_path,replacement_system_path,confirmation_token,operation="temp_bp_component_template_asset_swap_canary",dry_run=True,preserve_override_parameters=True,allow_unrealmcp_dependency=False,replace_existing_target=False,allow_dirty_source_blueprint=False,compile_blueprint=False,save_blueprint=False,parameter_name_contains="",include_inherited_scs_templates=False,require_exactly_one=True,require_source_system_match=True,max_components=8,max_parameters=256,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_parameter_object_properties=False,include_component_object_properties=False,max_object_properties=80,max_array_elements=16,max_property_value_chars=512):
    return _named("execute_blueprint_niagara_component_template_asset_swap_canary",dict(source_blueprint_path=source_blueprint_path,target_blueprint_path=target_blueprint_path,component_name=component_name,source_system_path=source_system_path,replacement_system_path=replacement_system_path,confirmation_token=confirmation_token,operation=operation,dry_run=dry_run,preserve_override_parameters=preserve_override_parameters,allow_unrealmcp_dependency=allow_unrealmcp_dependency,replace_existing_target=replace_existing_target,allow_dirty_source_blueprint=allow_dirty_source_blueprint,compile_blueprint=compile_blueprint,save_blueprint=save_blueprint,parameter_name_contains=parameter_name_contains,include_inherited_scs_templates=include_inherited_scs_templates,require_exactly_one=require_exactly_one,require_source_system_match=require_source_system_match,max_components=max_components,max_parameters=max_parameters,include_parameter_values=include_parameter_values,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes,include_parameter_object_properties=include_parameter_object_properties,include_component_object_properties=include_component_object_properties,max_object_properties=max_object_properties,max_array_elements=max_array_elements,max_property_value_chars=max_property_value_chars))

def execute_blueprint_niagara_component_template_asset_swap_canary_action(result,source_blueprint_path,target_blueprint_path,component_name,source_system_path,replacement_system_path,confirmation_token,operation="temp_bp_component_template_asset_swap_canary",dry_run=True,preserve_override_parameters=True,allow_unrealmcp_dependency=False,replace_existing_target=False,allow_dirty_source_blueprint=False,compile_blueprint=False,save_blueprint=False,parameter_name_contains="",include_inherited_scs_templates=False,require_exactly_one=True,require_source_system_match=True,max_components=8,max_parameters=256,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_parameter_object_properties=False,include_component_object_properties=False,max_object_properties=80,max_array_elements=16,max_property_value_chars=512,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"execute_blueprint_niagara_component_template_asset_swap_canary",dict(source_blueprint_path=source_blueprint_path,target_blueprint_path=target_blueprint_path,component_name=component_name,source_system_path=source_system_path,replacement_system_path=replacement_system_path,confirmation_token=confirmation_token,operation=operation,dry_run=dry_run,preserve_override_parameters=preserve_override_parameters,allow_unrealmcp_dependency=allow_unrealmcp_dependency,replace_existing_target=replace_existing_target,allow_dirty_source_blueprint=allow_dirty_source_blueprint,compile_blueprint=compile_blueprint,save_blueprint=save_blueprint,parameter_name_contains=parameter_name_contains,include_inherited_scs_templates=include_inherited_scs_templates,require_exactly_one=require_exactly_one,require_source_system_match=require_source_system_match,max_components=max_components,max_parameters=max_parameters,include_parameter_values=include_parameter_values,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes,include_parameter_object_properties=include_parameter_object_properties,include_component_object_properties=include_component_object_properties,max_object_properties=max_object_properties,max_array_elements=max_array_elements,max_property_value_chars=max_property_value_chars),checkpoint_callback,_legacy_command_params,_public_input)

def execute_blueprint_niagara_component_template_asset_swap_durable(blueprint_path,component_name,source_system_path,replacement_system_path,confirmation_token,operation="durable_bp_component_template_asset_swap",dry_run=True,preserve_override_parameters=True,allow_unrealmcp_dependency=False,allow_dirty_blueprint=False,compile_blueprint=False,save_blueprint=False,parameter_name_contains="",include_inherited_scs_templates=False,require_exactly_one=True,require_source_system_match=True,max_components=8,max_parameters=256,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_parameter_object_properties=False,include_component_object_properties=False,max_object_properties=80,max_array_elements=16,max_property_value_chars=512):
    return _named("execute_blueprint_niagara_component_template_asset_swap_durable",dict(blueprint_path=blueprint_path,component_name=component_name,source_system_path=source_system_path,replacement_system_path=replacement_system_path,confirmation_token=confirmation_token,operation=operation,dry_run=dry_run,preserve_override_parameters=preserve_override_parameters,allow_unrealmcp_dependency=allow_unrealmcp_dependency,allow_dirty_blueprint=allow_dirty_blueprint,compile_blueprint=compile_blueprint,save_blueprint=save_blueprint,parameter_name_contains=parameter_name_contains,include_inherited_scs_templates=include_inherited_scs_templates,require_exactly_one=require_exactly_one,require_source_system_match=require_source_system_match,max_components=max_components,max_parameters=max_parameters,include_parameter_values=include_parameter_values,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes,include_parameter_object_properties=include_parameter_object_properties,include_component_object_properties=include_component_object_properties,max_object_properties=max_object_properties,max_array_elements=max_array_elements,max_property_value_chars=max_property_value_chars))

def execute_blueprint_niagara_component_template_asset_swap_durable_action(result,blueprint_path,component_name,source_system_path,replacement_system_path,confirmation_token,operation="durable_bp_component_template_asset_swap",dry_run=True,preserve_override_parameters=True,allow_unrealmcp_dependency=False,allow_dirty_blueprint=False,compile_blueprint=False,save_blueprint=False,parameter_name_contains="",include_inherited_scs_templates=False,require_exactly_one=True,require_source_system_match=True,max_components=8,max_parameters=256,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_parameter_object_properties=False,include_component_object_properties=False,max_object_properties=80,max_array_elements=16,max_property_value_chars=512,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"execute_blueprint_niagara_component_template_asset_swap_durable",dict(blueprint_path=blueprint_path,component_name=component_name,source_system_path=source_system_path,replacement_system_path=replacement_system_path,confirmation_token=confirmation_token,operation=operation,dry_run=dry_run,preserve_override_parameters=preserve_override_parameters,allow_unrealmcp_dependency=allow_unrealmcp_dependency,allow_dirty_blueprint=allow_dirty_blueprint,compile_blueprint=compile_blueprint,save_blueprint=save_blueprint,parameter_name_contains=parameter_name_contains,include_inherited_scs_templates=include_inherited_scs_templates,require_exactly_one=require_exactly_one,require_source_system_match=require_source_system_match,max_components=max_components,max_parameters=max_parameters,include_parameter_values=include_parameter_values,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes,include_parameter_object_properties=include_parameter_object_properties,include_component_object_properties=include_component_object_properties,max_object_properties=max_object_properties,max_array_elements=max_array_elements,max_property_value_chars=max_property_value_chars),checkpoint_callback,_legacy_command_params,_public_input)

