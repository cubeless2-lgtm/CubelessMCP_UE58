"""GJ Niagara cleanup6; original selectors/omission/raw aliases and actual partial receipt. No transport or replay."""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import niagara_cleanup as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"remove_niagara_modules": ["system_path", "module_names", "module_name_prefixes", "module_name_contains", "module_name", "emitter_index", "emitter_name", "include_system_modules", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"], "remove_niagara_parameter_map_set_nodes": ["system_path", "parameter_pin_names", "parameter_pin_name", "node_guid", "emitter_index", "emitter_name", "dry_run", "allow_multiple", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"], "remove_niagara_orphan_parameter_map_get_nodes": ["system_path", "parameter_pin_names", "parameter_pin_name", "node_guid", "emitter_index", "emitter_name", "dry_run", "allow_multiple", "only_if_unlinked", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"], "remove_niagara_user_parameters": ["system_path", "parameter_names", "parameter_name", "dry_run", "remove_graph_references", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds", "continue_on_error"], "remove_niagara_simulation_stage": ["system_path", "emitter_index", "emitter_name", "stage_index", "stage_name", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"], "remove_niagara_scratch_pad_script": ["system_path", "scratch_pad_name", "scratch_pad_owner_kind", "scratch_pad_script_index", "scratch_pad_emitter_index", "scratch_pad_emitter_name", "force", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"]}
DEFAULTS={"remove_niagara_modules": {"module_names": None, "module_name_prefixes": None, "module_name_contains": None, "module_name": "", "emitter_index": None, "emitter_name": "", "include_system_modules": False, "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": False, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1}, "remove_niagara_parameter_map_set_nodes": {"parameter_pin_names": None, "parameter_pin_name": "", "node_guid": "", "emitter_index": None, "emitter_name": "", "dry_run": True, "allow_multiple": False, "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": False, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1}, "remove_niagara_orphan_parameter_map_get_nodes": {"parameter_pin_names": None, "parameter_pin_name": "", "node_guid": "", "emitter_index": None, "emitter_name": "", "dry_run": True, "allow_multiple": False, "only_if_unlinked": True, "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": False, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1}, "remove_niagara_user_parameters": {"parameter_names": None, "parameter_name": "", "dry_run": True, "remove_graph_references": True, "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": True, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1, "continue_on_error": False}, "remove_niagara_simulation_stage": {"emitter_index": None, "emitter_name": "", "stage_index": None, "stage_name": "", "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": False, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1}, "remove_niagara_scratch_pad_script": {"scratch_pad_name": "", "scratch_pad_owner_kind": "any", "scratch_pad_script_index": None, "scratch_pad_emitter_index": None, "scratch_pad_emitter_name": "", "force": False, "allow_source_edit": False, "save": True, "request_compile": True, "wait_for_completion": False, "timeout_seconds": 10.0, "poll_interval_seconds": 0.1}}
REQUIRED={"remove_niagara_modules": ["system_path"], "remove_niagara_parameter_map_set_nodes": ["system_path"], "remove_niagara_orphan_parameter_map_get_nodes": ["system_path"], "remove_niagara_user_parameters": ["system_path"], "remove_niagara_simulation_stage": ["system_path"], "remove_niagara_scratch_pad_script": ["system_path"]}
FIELD_TYPES={"remove_niagara_modules": {"system_path": "string", "module_names": "array", "module_name_prefixes": "array", "module_name_contains": "array", "module_name": "string", "emitter_index": "integer", "emitter_name": "string", "include_system_modules": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}, "remove_niagara_parameter_map_set_nodes": {"system_path": "string", "parameter_pin_names": "array", "parameter_pin_name": "string", "node_guid": "string", "emitter_index": "integer", "emitter_name": "string", "dry_run": "boolean", "allow_multiple": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}, "remove_niagara_orphan_parameter_map_get_nodes": {"system_path": "string", "parameter_pin_names": "array", "parameter_pin_name": "string", "node_guid": "string", "emitter_index": "integer", "emitter_name": "string", "dry_run": "boolean", "allow_multiple": "boolean", "only_if_unlinked": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}, "remove_niagara_user_parameters": {"system_path": "string", "parameter_names": "array", "parameter_name": "string", "dry_run": "boolean", "remove_graph_references": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number", "continue_on_error": "boolean"}, "remove_niagara_simulation_stage": {"system_path": "string", "emitter_index": "integer", "emitter_name": "string", "stage_index": "integer", "stage_name": "string", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}, "remove_niagara_scratch_pad_script": {"system_path": "string", "scratch_pad_name": "string", "scratch_pad_owner_kind": "string", "scratch_pad_script_index": "integer", "scratch_pad_emitter_index": "integer", "scratch_pad_emitter_name": "string", "force": "boolean", "allow_source_edit": "boolean", "save": "boolean", "request_compile": "boolean", "wait_for_completion": "boolean", "timeout_seconds": "number", "poll_interval_seconds": "number"}}
NULLABLE={"remove_niagara_modules": ["module_names", "module_name_prefixes", "module_name_contains", "emitter_index"], "remove_niagara_parameter_map_set_nodes": ["parameter_pin_names", "emitter_index"], "remove_niagara_orphan_parameter_map_get_nodes": ["parameter_pin_names", "emitter_index"], "remove_niagara_user_parameters": ["parameter_names"], "remove_niagara_simulation_stage": ["emitter_index", "stage_index"], "remove_niagara_scratch_pad_script": ["scratch_pad_script_index", "scratch_pad_emitter_index"]}
ITEMS={"remove_niagara_modules": {"module_names": "string", "module_name_prefixes": "string", "module_name_contains": "string"}, "remove_niagara_parameter_map_set_nodes": {"parameter_pin_names": "string"}, "remove_niagara_orphan_parameter_map_get_nodes": {"parameter_pin_names": "string"}, "remove_niagara_user_parameters": {"parameter_names": "string"}, "remove_niagara_simulation_stage": {}, "remove_niagara_scratch_pad_script": {}}

# TB original public6 local generated-package cleanup; existing Native6 untouched.
PUBLIC_FIELDS['cleanup_generated_niagara_assets_offline']=['package_paths', 'name_prefix', 'older_than_minutes', 'dry_run', 'require_editor_closed', 'allow_all']
DEFAULTS['cleanup_generated_niagara_assets_offline']={'package_paths': None, 'name_prefix': '', 'older_than_minutes': None, 'dry_run': True, 'require_editor_closed': True, 'allow_all': False}
REQUIRED['cleanup_generated_niagara_assets_offline']=[]
FIELD_TYPES['cleanup_generated_niagara_assets_offline']={'package_paths':'array','name_prefix':'string','older_than_minutes':'number','dry_run':'boolean','require_editor_closed':'boolean','allow_all':'boolean'}
NULLABLE['cleanup_generated_niagara_assets_offline']=['package_paths','older_than_minutes']
ITEMS['cleanup_generated_niagara_assets_offline']={'package_paths':'string'}
OBSERVERS=[]
NO_JOURNAL_METHODS=[]
WRITERS=["remove_niagara_modules", "remove_niagara_parameter_map_set_nodes", "remove_niagara_orphan_parameter_map_get_nodes", "remove_niagara_user_parameters", "remove_niagara_simulation_stage", "remove_niagara_scratch_pad_script"]

WRITERS.append('cleanup_generated_niagara_assets_offline')
_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_cleanup_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_cleanup_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_cleanup_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_cleanup_public_argument_invalid','Unexpected frozen item type')
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
        else:raise OperationError('niagara_cleanup_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

LEGACY_MESSAGES={"remove_niagara_modules":"Error removing Niagara modules: ","remove_niagara_parameter_map_set_nodes":"Error removing Niagara ParameterMapSet nodes: ","remove_niagara_orphan_parameter_map_get_nodes":"Error removing Niagara orphan ParameterMapGet nodes: ","remove_niagara_user_parameters":"Error removing Niagara user parameters: ","remove_niagara_simulation_stage":"Error removing Niagara simulation stage: ","remove_niagara_scratch_pad_script":"Error removing Niagara Scratch Pad script: "}
LEGACY_MESSAGES['cleanup_generated_niagara_assets_offline']='Error cleaning generated Niagara assets offline: '
def _legacy_exception(method,error):
    message=LEGACY_MESSAGES[method]+str(error)
    return dict(success=False,message=message,error=message,error_code="niagara_tool_failed")

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('niagara_cleanup_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_cleanup_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    params={field:copy.deepcopy(values[field]) for field in PUBLIC_FIELDS[method]}
    if method=='cleanup_generated_niagara_assets_offline':
        api._carrier(params);return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None
    if method=='remove_niagara_modules':
        for field in ('module_names','module_name_prefixes','module_name_contains','module_name','emitter_name'):
            if not values[field]:params.pop(field,None)
        if values['emitter_index'] is None:params.pop('emitter_index',None)
    elif method in ('remove_niagara_parameter_map_set_nodes','remove_niagara_orphan_parameter_map_get_nodes'):
        for field in ('parameter_pin_names','parameter_pin_name'):
            if not values[field]:params.pop(field,None)
        if values['emitter_index'] is None:params.pop('emitter_index',None)
    elif method=='remove_niagara_user_parameters':params['parameter_names']=copy.deepcopy(values['parameter_names'] or [])
    else:
        for field in NULLABLE[method]:
            if values[field] is None:params.pop(field,None)
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,
        original_named_body_omission_preserved=True,raw_locale_boolean_integer_and_presence_Native_owned=True,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        operational_effect='write',original_inferred_effect='destructive',original_inferred_risk='high',original_destructive_high_policy=True,mandatory_wait_contract_preserved=True,deferred_terminal_wait_completion_unverified=True,registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_niagara_cleanup_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_niagara_cleanup_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_niagara_cleanup_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        original_removal_reference_rewire_and_actual_save_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_niagara_cleanup_partial_compile_save_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_niagara_cleanup_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    if method=='cleanup_generated_niagara_assets_offline':return _run_offline_cleanup(result,args,original,legacy,checkpoint_callback)
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_niagara_cleanup_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_niagara_cleanup_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_niagara_cleanup_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if native['data'].get('operation_complete') is False:
        result['data'].update(deferred_final_receipt_pending=True,command_may_still_be_in_flight=True,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
        result['changes']=[dict(kind=method,attempted=native['mutation_attempted'],changed=None,change_state_known=False)]
        result['retry'].update(automatic_retry=False,duplicate_risk='deferred_native_completion_in_flight_no_replay')
        if callback:callback(result)
        raise OperationError('niagara_cleanup_deferred_completion_pending','Native scheduling is not final removal/compile/absence/save completion; receipt retained and no replay')
    if not native['success']:raise OperationError(native.get('error_code') or 'niagara_cleanup_native_failed',native.get('error') or 'Native niagara_cleanup failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('niagara_cleanup_postcondition_unverified','Native niagara_cleanup postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_requested'] or native['compile_attempted'] or native['save_requested'] or native['save_attempted'] or native['compiled'] is not False or native['saved']):
        raise OperationError('niagara_cleanup_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_cleanup_input_validation';result['changes']=[]
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

def remove_niagara_modules(system_path,module_names=None,module_name_prefixes=None,module_name_contains=None,module_name="",emitter_index=None,emitter_name="",include_system_modules=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1):
    return _named("remove_niagara_modules",dict(system_path=system_path,module_names=module_names,module_name_prefixes=module_name_prefixes,module_name_contains=module_name_contains,module_name=module_name,emitter_index=emitter_index,emitter_name=emitter_name,include_system_modules=include_system_modules,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def remove_niagara_modules_action(result,system_path,module_names=None,module_name_prefixes=None,module_name_contains=None,module_name="",emitter_index=None,emitter_name="",include_system_modules=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"remove_niagara_modules",dict(system_path=system_path,module_names=module_names,module_name_prefixes=module_name_prefixes,module_name_contains=module_name_contains,module_name=module_name,emitter_index=emitter_index,emitter_name=emitter_name,include_system_modules=include_system_modules,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)

def remove_niagara_parameter_map_set_nodes(system_path,parameter_pin_names=None,parameter_pin_name="",node_guid="",emitter_index=None,emitter_name="",dry_run=True,allow_multiple=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1):
    return _named("remove_niagara_parameter_map_set_nodes",dict(system_path=system_path,parameter_pin_names=parameter_pin_names,parameter_pin_name=parameter_pin_name,node_guid=node_guid,emitter_index=emitter_index,emitter_name=emitter_name,dry_run=dry_run,allow_multiple=allow_multiple,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def remove_niagara_parameter_map_set_nodes_action(result,system_path,parameter_pin_names=None,parameter_pin_name="",node_guid="",emitter_index=None,emitter_name="",dry_run=True,allow_multiple=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"remove_niagara_parameter_map_set_nodes",dict(system_path=system_path,parameter_pin_names=parameter_pin_names,parameter_pin_name=parameter_pin_name,node_guid=node_guid,emitter_index=emitter_index,emitter_name=emitter_name,dry_run=dry_run,allow_multiple=allow_multiple,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)

def remove_niagara_orphan_parameter_map_get_nodes(system_path,parameter_pin_names=None,parameter_pin_name="",node_guid="",emitter_index=None,emitter_name="",dry_run=True,allow_multiple=False,only_if_unlinked=True,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1):
    return _named("remove_niagara_orphan_parameter_map_get_nodes",dict(system_path=system_path,parameter_pin_names=parameter_pin_names,parameter_pin_name=parameter_pin_name,node_guid=node_guid,emitter_index=emitter_index,emitter_name=emitter_name,dry_run=dry_run,allow_multiple=allow_multiple,only_if_unlinked=only_if_unlinked,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def remove_niagara_orphan_parameter_map_get_nodes_action(result,system_path,parameter_pin_names=None,parameter_pin_name="",node_guid="",emitter_index=None,emitter_name="",dry_run=True,allow_multiple=False,only_if_unlinked=True,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"remove_niagara_orphan_parameter_map_get_nodes",dict(system_path=system_path,parameter_pin_names=parameter_pin_names,parameter_pin_name=parameter_pin_name,node_guid=node_guid,emitter_index=emitter_index,emitter_name=emitter_name,dry_run=dry_run,allow_multiple=allow_multiple,only_if_unlinked=only_if_unlinked,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)

def remove_niagara_user_parameters(system_path,parameter_names=None,parameter_name="",dry_run=True,remove_graph_references=True,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=10.0,poll_interval_seconds=0.1,continue_on_error=False):
    return _named("remove_niagara_user_parameters",dict(system_path=system_path,parameter_names=parameter_names,parameter_name=parameter_name,dry_run=dry_run,remove_graph_references=remove_graph_references,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds,continue_on_error=continue_on_error))

def remove_niagara_user_parameters_action(result,system_path,parameter_names=None,parameter_name="",dry_run=True,remove_graph_references=True,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=10.0,poll_interval_seconds=0.1,continue_on_error=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"remove_niagara_user_parameters",dict(system_path=system_path,parameter_names=parameter_names,parameter_name=parameter_name,dry_run=dry_run,remove_graph_references=remove_graph_references,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds,continue_on_error=continue_on_error),checkpoint_callback,_legacy_command_params,_public_input)

def remove_niagara_simulation_stage(system_path,emitter_index=None,emitter_name="",stage_index=None,stage_name="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1):
    return _named("remove_niagara_simulation_stage",dict(system_path=system_path,emitter_index=emitter_index,emitter_name=emitter_name,stage_index=stage_index,stage_name=stage_name,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def remove_niagara_simulation_stage_action(result,system_path,emitter_index=None,emitter_name="",stage_index=None,stage_name="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"remove_niagara_simulation_stage",dict(system_path=system_path,emitter_index=emitter_index,emitter_name=emitter_name,stage_index=stage_index,stage_name=stage_name,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)

def remove_niagara_scratch_pad_script(system_path,scratch_pad_name="",scratch_pad_owner_kind="any",scratch_pad_script_index=None,scratch_pad_emitter_index=None,scratch_pad_emitter_name="",force=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1):
    return _named("remove_niagara_scratch_pad_script",dict(system_path=system_path,scratch_pad_name=scratch_pad_name,scratch_pad_owner_kind=scratch_pad_owner_kind,scratch_pad_script_index=scratch_pad_script_index,scratch_pad_emitter_index=scratch_pad_emitter_index,scratch_pad_emitter_name=scratch_pad_emitter_name,force=force,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

def remove_niagara_scratch_pad_script_action(result,system_path,scratch_pad_name="",scratch_pad_owner_kind="any",scratch_pad_script_index=None,scratch_pad_emitter_index=None,scratch_pad_emitter_name="",force=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=False,timeout_seconds=10.0,poll_interval_seconds=0.1,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"remove_niagara_scratch_pad_script",dict(system_path=system_path,scratch_pad_name=scratch_pad_name,scratch_pad_owner_kind=scratch_pad_owner_kind,scratch_pad_script_index=scratch_pad_script_index,scratch_pad_emitter_index=scratch_pad_emitter_index,scratch_pad_emitter_name=scratch_pad_emitter_name,force=force,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds),checkpoint_callback,_legacy_command_params,_public_input)


def _run_offline_cleanup(result,args,original,legacy,callback):
    receipt={}
    def adopt(current):
        known=current.get('change_state_known') is True;attempted=current.get('mutation_attempted') is True;changed=current.get('observed_changed') is True
        result['data'].update(public_method='cleanup_generated_niagara_assets_offline',requested_arguments=copy.deepcopy(original),
            entry_route=args['entry'],public_argument_validation_applied=args['public_input'],raw_Native_entry=False,
            legacy_command_params=copy.deepcopy(legacy),native_dispatched=False,server_RPC_dependency=False,
            local_filesystem_receipt={k:copy.deepcopy(v) for k,v in current.items() if k!='legacy_result'},
            execution_mutation_attempted=attempted,mutation_performed=changed if known else None,
            change_state_known=known,mutation_state_unknown=not known,automatic_action_retry=False,
            atomic_rollback=False,operational_effect='local_file_delete_or_dry_run',original_destructive_high_policy=True)
        if 'legacy_result' in current:
            result['data']['legacy_result']=copy.deepcopy(current['legacy_result'])
            result['data']['legacy_Named_result']=copy.deepcopy(current['legacy_result'])
        result['compile'].update(requested=False,attempted=False,compiled=False,status='filesystem_only')
        result['save'].update(requested=False,attempted=False,saved=False,status='filesystem_only_no_asset_save')
        result['changes']=[] if known and not attempted and not changed else [dict(kind='generated_niagara_package_files',attempted=attempted,changed=changed if known else None,change_state_known=known)]
        result['stage']=current.get('stage','preflight')
        result['retry'].update(automatic_retry=False,duplicate_risk='partial_unlink_inspect_before_new_call' if attempted else 'none_observed')
    def checkpoint(current):
        adopt(current)
        if callback:callback(result)
    try:api.execute_offline_cleanup(args['params'],receipt,checkpoint)
    finally:adopt(receipt)
    if callback:callback(result)
    if receipt.get('error_code'):raise OperationError(receipt['error_code'],receipt.get('error','Offline cleanup interrupted'))
    leaf=receipt.get('legacy_result',{})
    if leaf.get('success') is not True:raise OperationError(leaf.get('error_code','niagara_cleanup_offline_failed'),leaf.get('message','Offline cleanup reported per-file errors'))
    if receipt.get('change_state_known') is not True:raise OperationError('niagara_cleanup_offline_unknown','Unexpected file-call outcome; inspect retained prefix before a new request')
    result['validation'].update(status='original_generated_package_files_scope',compile_save='not_requested',asset_registry_or_loaded_object_cleanup='not_performed',global_filesystem_atomicity='not_proved')
    result.update(success=True,stage='complete');return result


def cleanup_generated_niagara_assets_offline(package_paths=None,name_prefix='',older_than_minutes=None,dry_run=True,require_editor_closed=True,allow_all=False):
    return _named('cleanup_generated_niagara_assets_offline',dict(package_paths=package_paths,name_prefix=name_prefix,older_than_minutes=older_than_minutes,dry_run=dry_run,require_editor_closed=require_editor_closed,allow_all=allow_all))


def cleanup_generated_niagara_assets_offline_action(result,package_paths=None,name_prefix='',older_than_minutes=None,dry_run=True,require_editor_closed=True,allow_all=False,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,'cleanup_generated_niagara_assets_offline',dict(package_paths=package_paths,name_prefix=name_prefix,older_than_minutes=older_than_minutes,dry_run=dry_run,require_editor_closed=require_editor_closed,allow_all=allow_all),checkpoint_callback,_legacy_command_params,_public_input)
