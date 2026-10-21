"""GQ exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import niagara_event_stage_authoring as api
from cubeless_mcp_ue58.operations import niagara_event_stage_authoring as domain
TOOLSET = 'cubeless_mcp_ue58.registration.niagara_event_stage_authoring_toolset.CubelessNiagaraEventStageTools'
BINDINGS = ["inspect_niagara_simulation_stages", "inspect_niagara_event_handlers", "add_niagara_event_handler"]
PUBLIC_FIELDS_BY_METHOD = {"inspect_niagara_simulation_stages": ["system_path", "include_compile_data", "include_script_compile_status", "max_stages"], "inspect_niagara_event_handlers": ["system_path", "emitter_index", "emitter_name", "max_handlers"], "add_niagara_event_handler": ["system_path", "emitter_index", "emitter_name", "source_emitter_index", "source_emitter_name", "source_emitter_id", "source_event_name", "execution_mode", "spawn_number", "max_events_per_frame", "random_spawn_number", "min_spawn_number", "update_attribute_initial_values", "skip_if_duplicate", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"]}
LOGICAL_INPUT_SCHEMAS = {"inspect_niagara_simulation_stages": {"properties": {"system_path": {"title": "System Path", "type": "string"}, "include_compile_data": {"default": True, "title": "Include Compile Data", "type": "boolean"}, "include_script_compile_status": {"default": True, "title": "Include Script Compile Status", "type": "boolean"}, "max_stages": {"default": 128, "title": "Max Stages", "type": "integer"}}, "required": ["system_path"], "title": "inspect_niagara_simulation_stagesArguments", "type": "object"}, "inspect_niagara_event_handlers": {"properties": {"system_path": {"title": "System Path", "type": "string"}, "emitter_index": {"anyOf": [{"type": "integer"}, {"type": "null"}], "default": None, "title": "Emitter Index"}, "emitter_name": {"default": "", "title": "Emitter Name", "type": "string"}, "max_handlers": {"default": 256, "title": "Max Handlers", "type": "integer"}}, "required": ["system_path"], "title": "inspect_niagara_event_handlersArguments", "type": "object"}, "add_niagara_event_handler": {"properties": {"system_path": {"title": "System Path", "type": "string"}, "emitter_index": {"anyOf": [{"type": "integer"}, {"type": "null"}], "default": None, "title": "Emitter Index"}, "emitter_name": {"default": "", "title": "Emitter Name", "type": "string"}, "source_emitter_index": {"anyOf": [{"type": "integer"}, {"type": "null"}], "default": None, "title": "Source Emitter Index"}, "source_emitter_name": {"default": "", "title": "Source Emitter Name", "type": "string"}, "source_emitter_id": {"default": "", "title": "Source Emitter Id", "type": "string"}, "source_event_name": {"default": "CollisionEvent", "title": "Source Event Name", "type": "string"}, "execution_mode": {"default": "SpawnedParticles", "title": "Execution Mode", "type": "string"}, "spawn_number": {"default": 1, "title": "Spawn Number", "type": "integer"}, "max_events_per_frame": {"default": 64, "title": "Max Events Per Frame", "type": "integer"}, "random_spawn_number": {"default": False, "title": "Random Spawn Number", "type": "boolean"}, "min_spawn_number": {"default": 0, "title": "Min Spawn Number", "type": "integer"}, "update_attribute_initial_values": {"default": True, "title": "Update Attribute Initial Values", "type": "boolean"}, "skip_if_duplicate": {"default": True, "title": "Skip If Duplicate", "type": "boolean"}, "allow_source_edit": {"default": False, "title": "Allow Source Edit", "type": "boolean"}, "save": {"default": True, "title": "Save", "type": "boolean"}, "request_compile": {"default": True, "title": "Request Compile", "type": "boolean"}, "wait_for_completion": {"default": True, "title": "Wait For Completion", "type": "boolean"}, "timeout_seconds": {"default": 60, "title": "Timeout Seconds", "type": "number"}, "poll_interval_seconds": {"default": 0.1, "title": "Poll Interval Seconds", "type": "number"}}, "required": ["system_path"], "title": "add_niagara_event_handlerArguments", "type": "object"}}
PHYSICAL_DEFAULTS_BY_METHOD = {"inspect_niagara_simulation_stages": {"include_compile_data": "true", "include_script_compile_status": "true", "max_stages": "128"}, "inspect_niagara_event_handlers": {"emitter_index": "null", "emitter_name": "\"\"", "max_handlers": "256"}, "add_niagara_event_handler": {"emitter_index": "null", "emitter_name": "\"\"", "source_emitter_index": "null", "source_emitter_name": "\"\"", "source_emitter_id": "\"\"", "source_event_name": "\"CollisionEvent\"", "execution_mode": "\"SpawnedParticles\"", "spawn_number": "1", "max_events_per_frame": "64", "random_spawn_number": "false", "min_spawn_number": "0", "update_attribute_initial_values": "true", "skip_if_duplicate": "true", "allow_source_edit": "false", "save": "true", "request_compile": "true", "wait_for_completion": "true", "timeout_seconds": "60.0", "poll_interval_seconds": "0.1"}}
REQUIRED_BY_METHOD = {"inspect_niagara_simulation_stages": ["system_path"], "inspect_niagara_event_handlers": ["system_path"], "add_niagara_event_handler": ["system_path"]}
ATTACHMENT_LIBRARY = 'CubelessNiagaraEventStageAttachmentLibrary'
ATTACHMENT_METHODS = ('install_niagara_event_stage_authoring_toolset', 'verify_niagara_event_stage_authoring_toolset', 'remove_niagara_event_stage_authoring_toolset')
ATTACHMENT_READY_FIELDS = ["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "niagara_event_stage_public4_4_20_schema_verified", "required1_1_1_default3_3_19_verified", "json_value_carrier28_schema_verified", "scalar_nullable_original_model_and_body_omission_preserved", "raw_hidden0_and_Named_raw_wait_defaults_preserved", "observer2_writer_required_wait_save_partial_receipt_owned", "niagara_event_stage_authoring3_native_contract_owned"]
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();async_status=domain.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()) and all(value is True for value in async_status["Native_bindings"].values()),finite_Named_continuation=async_status,library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=3,
        public_input_counts=dict(zip(BINDINGS,(4,4,20))),physical_input_counts=dict(zip(BINDINGS,(4,4,20))),native_input_counts=dict(zip(BINDINGS,(1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0))),
        schema_verified=False,default_verified=False,raw_guard_required=True,raw_guard_ready=False,nullable_raw_guard_ready=False,boolean_raw_guard_ready=False,number_raw_guard_ready=False,
        actual_official_wire_verified=False,nullable_Named_wire_verified=False,boolean_Named_wire_verified=False,number_Named_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _required(expected,actual):return type(actual) is list and len(actual)==len(expected) and all(type(v) is str for v in actual) and set(actual)==set(expected)

def _exact(a,b):
    # Unreal JSON serializes integral finite floats as integers; JSON has one number kind.
    if type(a) in (int,float) and type(b) in (int,float):
        return (type(a) is int or math.isfinite(a)) and (type(b) is int or math.isfinite(b)) and a==b
    if type(a) is not type(b):return False
    if type(a) is dict:return set(a)==set(b) and all(_exact(a[k],b[k]) for k in a)
    if type(a) is list:return len(a)==len(b) and all(_exact(x,y) for x,y in zip(a,b))
    return a==b

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=3:raise RuntimeError('Exact NiagaraEventStage Toolset3 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original Niagara event-stage authoring methods required')
        method=names[0];seen.add(method)
        if not callable(getattr(toolset_class,method,None)):raise RuntimeError('Original method unavailable')
        inputs=row.get('inputSchema')
        if not reflected_carriers:
            if not _exact(inputs,LOGICAL_INPUT_SCHEMAS[method]):raise RuntimeError('Exact original eager input schema required; no new length/range/extra-field policy')
        else:
            if type(inputs) is not dict or inputs.get('type')!='object':raise RuntimeError('Physical object required')
            props=inputs.get('properties',{});required=inputs.get('required',[])
            if type(props) is not dict or set(props)!=set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(REQUIRED_BY_METHOD[method],required):raise RuntimeError('Exact physical fields/required required')
            for field in PUBLIC_FIELDS_BY_METHOD[method]:
                item=props[field]
                if type(item) is not dict or item.get('type')!='string':raise RuntimeError('Physical JSON FString carrier required')
                if field in REQUIRED_BY_METHOD[method]:
                    if 'default' in item:raise RuntimeError('Required public field has physical default')
                elif type(item.get('default')) is not str or item['default']!=PHYSICAL_DEFAULTS_BY_METHOD[method][field]:raise RuntimeError('Exact physical JSON default required')
        output=row.get('outputSchema');outer=output.get('properties') if type(output) is dict else None
        if type(output) is not dict or output.get('type')!='object' or type(outer) is not dict or set(outer)!={'returnValue'} or not _required(('returnValue',),output.get('required')):raise RuntimeError('Exact returnValue outer required')
        result=outer['returnValue'];props=result.get('properties') if type(result) is dict else None
        if type(result) is not dict or result.get('type')!='object' or type(props) is not dict or set(props)!=set(RESULT_FIELDS) or not _required(RESULT_FIELDS,result.get('required')) or any(type(props[k]) is not dict or props[k].get('type')!=('boolean' if k=='success' else 'string') for k in RESULT_FIELDS):raise RuntimeError('Exact standard11 output required')
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(4,4,20))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

def validate_schema(schema,toolset_class):return _validate_schema(schema,toolset_class)

def validate_reflected_carrier_schema(schema,toolset_class):return _validate_schema(schema,toolset_class,True)

def validate_registered_metadata(class_schema, attachment_report, toolset_class):
    reflected = validate_reflected_carrier_schema(class_schema, toolset_class)
    if (type(attachment_report) is not dict or attachment_report.get('operation') not in ATTACHMENT_METHODS[:2]
            or not all(attachment_report.get(key) is True for key in ATTACHMENT_READY_FIELDS)
            or attachment_report.get('asset_mutation') is not False or attachment_report.get('metadata_mutation') is not False
            or attachment_report.get('actual_named_wire_verified') is not False
            or attachment_report.get('logical_schema_source') != 'actual_registered_wrapper_GetJsonSchema'
            or attachment_report.get('logical_schema_registry_identity_verified') is not True):
        raise RuntimeError('Exact ready owner receipt with actual registered NiagaraEventStage wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered NiagaraEventStage wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
