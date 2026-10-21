"""GO Niagara runtime observation4 with original Named/direct/raw and exact Native prefixes. No transport, simulation creation, synthetic readiness or replay."""
import copy
import json
import math
import re
from cubeless_mcp_ue58.compatibility.ue58 import niagara_runtime_inspection as api
from . import state
from .state import OperationError
METHODS=api.BINDINGS
PARAM_FIELDS=api.PARAM_FIELDS
PUBLIC_FIELDS={"inspect_niagara_component_runtime_state": ["actor_name", "actor_label", "component_name", "system_path", "parameter_name_contains", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_asset_exposed_parameters", "include_instance_parameters", "include_parameter_object_properties", "include_emitter_runtime", "max_emitter_instances", "max_object_properties", "max_array_elements", "max_property_value_chars"], "inspect_niagara_component_parameter_store": ["actor_name", "actor_label", "component_name", "system_path", "parameter_name_contains", "max_components", "max_parameters", "include_raw_bytes", "max_raw_bytes"], "inspect_niagara_component_grid2d_runtime": ["actor_name", "actor_label", "component_name", "system_path", "max_components", "max_grid_interfaces", "include_internal"], "sample_render_target2d_region_stats": ["render_target_path", "x0", "x1", "y0", "y1", "channel", "threshold", "max_first_hits", "probe_x", "probe_y", "probe_radius"]}
DEFAULTS={"inspect_niagara_component_runtime_state": {"actor_name": "", "actor_label": "", "component_name": "", "system_path": "", "parameter_name_contains": "", "max_components": 32, "max_parameters": 256, "include_parameter_values": True, "include_raw_bytes": False, "max_raw_bytes": 64, "include_asset_exposed_parameters": False, "include_instance_parameters": False, "include_parameter_object_properties": False, "include_emitter_runtime": True, "max_emitter_instances": 128, "max_object_properties": 80, "max_array_elements": 16, "max_property_value_chars": 512}, "inspect_niagara_component_parameter_store": {"actor_name": "", "actor_label": "", "component_name": "", "system_path": "", "parameter_name_contains": "", "max_components": 8, "max_parameters": 64, "include_raw_bytes": False, "max_raw_bytes": 64}, "inspect_niagara_component_grid2d_runtime": {"actor_name": "", "actor_label": "", "component_name": "", "system_path": "", "max_components": 16, "max_grid_interfaces": 16, "include_internal": True}, "sample_render_target2d_region_stats": {"x0": 0, "x1": -1, "y0": 0, "y1": -1, "channel": "max", "threshold": 0.01, "max_first_hits": 8, "probe_x": -1, "probe_y": -1, "probe_radius": 8}}
REQUIRED={"inspect_niagara_component_runtime_state": [], "inspect_niagara_component_parameter_store": [], "inspect_niagara_component_grid2d_runtime": [], "sample_render_target2d_region_stats": ["render_target_path"]}
FIELD_TYPES={"inspect_niagara_component_runtime_state": {"actor_name": "string", "actor_label": "string", "component_name": "string", "system_path": "string", "parameter_name_contains": "string", "max_components": "integer", "max_parameters": "integer", "include_parameter_values": "boolean", "include_raw_bytes": "boolean", "max_raw_bytes": "integer", "include_asset_exposed_parameters": "boolean", "include_instance_parameters": "boolean", "include_parameter_object_properties": "boolean", "include_emitter_runtime": "boolean", "max_emitter_instances": "integer", "max_object_properties": "integer", "max_array_elements": "integer", "max_property_value_chars": "integer"}, "inspect_niagara_component_parameter_store": {"actor_name": "string", "actor_label": "string", "component_name": "string", "system_path": "string", "parameter_name_contains": "string", "max_components": "integer", "max_parameters": "integer", "include_raw_bytes": "boolean", "max_raw_bytes": "integer"}, "inspect_niagara_component_grid2d_runtime": {"actor_name": "string", "actor_label": "string", "component_name": "string", "system_path": "string", "max_components": "integer", "max_grid_interfaces": "integer", "include_internal": "boolean"}, "sample_render_target2d_region_stats": {"render_target_path": "string", "x0": "integer", "x1": "integer", "y0": "integer", "y1": "integer", "channel": "string", "threshold": "number", "max_first_hits": "integer", "probe_x": "integer", "probe_y": "integer", "probe_radius": "integer"}}
NULLABLE={"inspect_niagara_component_runtime_state": [], "inspect_niagara_component_parameter_store": [], "inspect_niagara_component_grid2d_runtime": [], "sample_render_target2d_region_stats": []}
ITEMS={"inspect_niagara_component_runtime_state": {}, "inspect_niagara_component_parameter_store": {}, "inspect_niagara_component_grid2d_runtime": {}, "sample_render_target2d_region_stats": {}}
OBSERVERS=["inspect_niagara_component_runtime_state", "inspect_niagara_component_parameter_store", "inspect_niagara_component_grid2d_runtime", "sample_render_target2d_region_stats"]
NO_JOURNAL_METHODS=["inspect_niagara_component_runtime_state", "inspect_niagara_component_parameter_store", "inspect_niagara_component_grid2d_runtime", "sample_render_target2d_region_stats"]
WRITERS=[]

_FORBIDDEN_MCP_REFERENCE_TOKENS=["/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor", "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry", "/engine/pythontypes"]

def _dependency(value):
    if not isinstance(value,str):return False
    v=value.strip().strip('\"\'').replace('\\','/').lower()
    return bool(v) and any(token in v for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)

def _reject_legacy_command_dependency_refs(params):
    """Original Batch recursively guards the complete source params before known-field filtering; Named scalar guards are separate."""
    def visit(field,value):
        if isinstance(value,str) and _dependency(value):
            error=OperationError('niagara_runtime_inspection_dependency_reference','Refusing MCP-only reference in '+field);error.field_name=field;error.reason='mcp_only_reference';raise error
        if isinstance(value,dict):
            for key,row in value.items():visit(field+'.'+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+'['+str(i)+']',row)
    visit('params',params)


def _public_error(field,kind,message):
    error=OperationError('niagara_runtime_inspection_public_argument_invalid',field+': '+message)
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
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_runtime_inspection_public_argument_invalid','Known method and public argument object required')
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
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_runtime_inspection_public_argument_invalid','Unexpected frozen item type')
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
        else:raise OperationError('niagara_runtime_inspection_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

def _legacy_exception(method,error):
    message=str(error)
    return dict(success=False,message=message,error=message,error_code='editor_tool_failed',details=dict(exception_type=type(error).__name__))

def _arguments(method,arguments,legacy,public_input=False):
    original=copy.deepcopy(arguments)
    if type(public_input) is not bool:raise OperationError('niagara_runtime_inspection_entry_invalid','Internal public selector must be Boolean')
    if legacy is not None:
        if type(legacy) is not dict or public_input:raise OperationError('niagara_runtime_inspection_entry_invalid','Raw Native and public model are separate entries')
        params={field:copy.deepcopy(legacy[field]) for field in PARAM_FIELDS[method] if field in legacy}
        api._carrier(params);return dict(params=params,entry='raw_legacy',public_input=False),original,copy.deepcopy(legacy)
    values=normalize_public_mcp_arguments(method,arguments) if public_input else copy.deepcopy(arguments)
    if method=='sample_render_target2d_region_stats':
        params=dict(render_target_path=values['render_target_path'],x0=int(values['x0']),y0=int(values['y0']),channel=values['channel'],threshold=float(values['threshold']),max_first_hits=int(values['max_first_hits']))
        if int(values['probe_x'])>=0 and int(values['probe_y'])>=0:
            params.update(probe_x=int(values['probe_x']),probe_y=int(values['probe_y']),probe_radius=int(values['probe_radius']))
        if int(values['x1'])>=0:params['x1']=int(values['x1'])
        if int(values['y1'])>=0:params['y1']=int(values['y1'])
    else:
        params={}
        for field in PUBLIC_FIELDS[method]:
            value=values[field];kind=FIELD_TYPES[method][field]
            if kind=='integer':params[field]=int(value)
            elif kind=='boolean':params[field]=bool(value)
            elif value:params[field]=value
    api._carrier(params)
    return dict(params=params,entry='named_mcp' if public_input else 'direct_python',public_input=public_input,public_normalized_arguments=copy.deepcopy(values) if public_input else None),original,None

def _context(result,method,args,original,legacy):
    result['data'].update(public_method=method,requested_arguments=copy.deepcopy(original),native_params=copy.deepcopy(args['params']),entry_route=args['entry'],public_argument_validation_applied=args['public_input'],public_normalized_arguments=copy.deepcopy(args.get('public_normalized_arguments')),legacy_command_params=copy.deepcopy(legacy),raw_Native_entry=legacy is not None,
        original_named_body_omission_preserved=True,raw_locale_boolean_integer_and_presence_Native_owned=True,hidden_fields=len(PARAM_FIELDS[method])-len(PUBLIC_FIELDS[method]),Batch_recursive_guard_owned_by_Batch=True,adapter_engine_queries=0,adapter_explicit_modify_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        operational_effect='read_world' if method in NO_JOURNAL_METHODS else 'write',original_inferred_risk='low' if method in NO_JOURNAL_METHODS else 'medium',original_native_observer=True,live_instance_safety_Native_authoritative=True,include_internal_UE58_scope_Native_authoritative=True,GPU_particle_count_may_be_latent_estimate=True,render_readback_internal_enqueue_flush_scope_Native_owned=True,unavailable_observation_not_replaced_with_numeric_zero=True,registration_dependency='ToolsetAPI',operation_dependency='Dedicated Editor-only EngineAPI',server_RPC_dependency=False,
        Engine_runtime_types_only=True,Cubeless_runtime_asset_dependency_allowed=False,automatic_action_retry=False,atomic_rollback=False,actual_named_wire_verified=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _adopt(result,method,args,original,legacy,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']))
    result['data'].update(native_result_returned=True,native_raw_result=native['_raw_native_json'],native_result_data=copy.deepcopy(native['data']),
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],
        execution_mutation_attempted=native['mutation_attempted'],change_state_known=known,mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,method,args,original,legacy)
    if legacy is None:result['data']['legacy_Named_result']=copy.deepcopy(native['data'].get('legacy_result'))
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_niagara_runtime_inspection_explicit_compiler_receipt')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_niagara_runtime_inspection_explicit_save_receipt')
    result['changes']=[] if known and native['changed'] is False and not native['mutation_attempted'] else [dict(kind=method,attempted=native['mutation_attempted'],change_state_known=known,changed=native['changed'] if known else None,postcondition_passed=native['postcondition_passed'])]
    result['validation'].update(status='native_niagara_runtime_inspection_public_postconditions',native_postcondition_passed=native['postcondition_passed'],total_change_state_known=known,
        original_runtime_store_RT_projection_and_limit_prefix_preserved=True,disk_reload='not_run',unique_external_pipeline='not_run')
    result['retry'].update(automatic_retry=False,duplicate_risk='unknown_niagara_runtime_inspection_load_getter_no_replay' if not known else 'observed_changes_inspect_before_new_call' if native['mutation_attempted'] else 'none_observed')
    result["stage"]="returned_niagara_runtime_inspection_operation"

def _run(result,method,args,original,legacy,checkpoint_callback=None):
    callback=checkpoint_callback
    _context(result,method,args,original,legacy)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False,status='native_not_dispatched');result['save'].update(requested=False,attempted=False,saved=False,status='native_not_dispatched')
    result['stage']='before_niagara_runtime_inspection_checkpoint'
    if callback:callback(result)
    result['data'].update(native_dispatched=True,native_mutation_attempted=None,execution_mutation_attempted=None,change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    result['changes']=[dict(kind=method,attempted=None,changed=None,change_state_known=False)];result['stage']='native_niagara_runtime_inspection_operation'
    try:
        native=getattr(api,method)(args['params'])
    except Exception as error:
        if legacy is None:result['data']['legacy_Named_result']=_legacy_exception(method,error)
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'native_call_dispatched',None) is False:
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False);result['changes']=[]
        result['stage']='returned_niagara_runtime_inspection_operation'
        if callback:callback(result)
        raise
    _adopt(result,method,args,original,legacy,native)
    if callback:callback(result)
    if native.get('operation_complete') is False or native['data'].get('operation_complete') is False:
        result['data'].update(operation_complete=False,in_flight=True,automatic_action_retry=False)
        raise OperationError('niagara_runtime_inspection_incomplete','Native observation is incomplete; actual partial prefix retained without synthetic completion or replay')
    if not native['success']:raise OperationError(native.get('error_code') or 'niagara_runtime_inspection_native_failed',native.get('error') or 'Native niagara_runtime_inspection failed; actual partial prefix retained')
    if not native['postcondition_passed']:raise OperationError('niagara_runtime_inspection_postcondition_unverified','Native niagara_runtime_inspection postcondition did not pass; no replay')
    # Native assertion/postconditions own actual observations; load/registry/file/cache global effects can remain unknown.
    if method in OBSERVERS and (native['mutation_attempted'] or native['compile_requested'] or native['compile_attempted'] or native['save_requested'] or native['save_attempted'] or native['compiled'] is not False or native['saved']):
        raise OperationError('niagara_runtime_inspection_observer_unverified','Observer returned unexpected explicit work; actual prefix retained')
    result.update(success=True,stage='complete');return result

def _rejection(result,method,error):
    state.fail(result,error);result['stage']='niagara_runtime_inspection_input_validation';result['changes']=[]
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

def inspect_niagara_component_runtime_state(actor_name="",actor_label="",component_name="",system_path="",parameter_name_contains="",max_components=32,max_parameters=256,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_asset_exposed_parameters=False,include_instance_parameters=False,include_parameter_object_properties=False,include_emitter_runtime=True,max_emitter_instances=128,max_object_properties=80,max_array_elements=16,max_property_value_chars=512):
    return _named("inspect_niagara_component_runtime_state",dict(actor_name=actor_name,actor_label=actor_label,component_name=component_name,system_path=system_path,parameter_name_contains=parameter_name_contains,max_components=max_components,max_parameters=max_parameters,include_parameter_values=include_parameter_values,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes,include_asset_exposed_parameters=include_asset_exposed_parameters,include_instance_parameters=include_instance_parameters,include_parameter_object_properties=include_parameter_object_properties,include_emitter_runtime=include_emitter_runtime,max_emitter_instances=max_emitter_instances,max_object_properties=max_object_properties,max_array_elements=max_array_elements,max_property_value_chars=max_property_value_chars))

def inspect_niagara_component_runtime_state_action(result,actor_name="",actor_label="",component_name="",system_path="",parameter_name_contains="",max_components=32,max_parameters=256,include_parameter_values=True,include_raw_bytes=False,max_raw_bytes=64,include_asset_exposed_parameters=False,include_instance_parameters=False,include_parameter_object_properties=False,include_emitter_runtime=True,max_emitter_instances=128,max_object_properties=80,max_array_elements=16,max_property_value_chars=512,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"inspect_niagara_component_runtime_state",dict(actor_name=actor_name,actor_label=actor_label,component_name=component_name,system_path=system_path,parameter_name_contains=parameter_name_contains,max_components=max_components,max_parameters=max_parameters,include_parameter_values=include_parameter_values,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes,include_asset_exposed_parameters=include_asset_exposed_parameters,include_instance_parameters=include_instance_parameters,include_parameter_object_properties=include_parameter_object_properties,include_emitter_runtime=include_emitter_runtime,max_emitter_instances=max_emitter_instances,max_object_properties=max_object_properties,max_array_elements=max_array_elements,max_property_value_chars=max_property_value_chars),checkpoint_callback,_legacy_command_params,_public_input)

def inspect_niagara_component_parameter_store(actor_name="",actor_label="",component_name="",system_path="",parameter_name_contains="",max_components=8,max_parameters=64,include_raw_bytes=False,max_raw_bytes=64):
    return _named("inspect_niagara_component_parameter_store",dict(actor_name=actor_name,actor_label=actor_label,component_name=component_name,system_path=system_path,parameter_name_contains=parameter_name_contains,max_components=max_components,max_parameters=max_parameters,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes))

def inspect_niagara_component_parameter_store_action(result,actor_name="",actor_label="",component_name="",system_path="",parameter_name_contains="",max_components=8,max_parameters=64,include_raw_bytes=False,max_raw_bytes=64,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"inspect_niagara_component_parameter_store",dict(actor_name=actor_name,actor_label=actor_label,component_name=component_name,system_path=system_path,parameter_name_contains=parameter_name_contains,max_components=max_components,max_parameters=max_parameters,include_raw_bytes=include_raw_bytes,max_raw_bytes=max_raw_bytes),checkpoint_callback,_legacy_command_params,_public_input)

def inspect_niagara_component_grid2d_runtime(actor_name="",actor_label="",component_name="",system_path="",max_components=16,max_grid_interfaces=16,include_internal=True):
    return _named("inspect_niagara_component_grid2d_runtime",dict(actor_name=actor_name,actor_label=actor_label,component_name=component_name,system_path=system_path,max_components=max_components,max_grid_interfaces=max_grid_interfaces,include_internal=include_internal))

def inspect_niagara_component_grid2d_runtime_action(result,actor_name="",actor_label="",component_name="",system_path="",max_components=16,max_grid_interfaces=16,include_internal=True,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"inspect_niagara_component_grid2d_runtime",dict(actor_name=actor_name,actor_label=actor_label,component_name=component_name,system_path=system_path,max_components=max_components,max_grid_interfaces=max_grid_interfaces,include_internal=include_internal),checkpoint_callback,_legacy_command_params,_public_input)

def sample_render_target2d_region_stats(render_target_path,x0=0,x1=-1,y0=0,y1=-1,channel="max",threshold=0.01,max_first_hits=8,probe_x=-1,probe_y=-1,probe_radius=8):
    return _named("sample_render_target2d_region_stats",dict(render_target_path=render_target_path,x0=x0,x1=x1,y0=y0,y1=y1,channel=channel,threshold=threshold,max_first_hits=max_first_hits,probe_x=probe_x,probe_y=probe_y,probe_radius=probe_radius))

def sample_render_target2d_region_stats_action(result,render_target_path,x0=0,x1=-1,y0=0,y1=-1,channel="max",threshold=0.01,max_first_hits=8,probe_x=-1,probe_y=-1,probe_radius=8,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _action(result,"sample_render_target2d_region_stats",dict(render_target_path=render_target_path,x0=x0,x1=x1,y0=y0,y1=y1,channel=channel,threshold=threshold,max_first_hits=max_first_hits,probe_x=probe_x,probe_y=probe_y,probe_radius=probe_radius),checkpoint_callback,_legacy_command_params,_public_input)

