"""GZ exact original eager schemas1/0/21/18 versus40 public JSON FString carriers; no hidden camera alias leakage."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import niagara_preview_workflows as api
TOOLSET = 'cubeless_mcp_ue58.registration.niagara_preview_workflows_toolset.CubelessNiagaraPreviewWorkflowTools'
BINDINGS=["open_niagara_preview_player", "get_niagara_preview_player_state", "preview_niagara_component_asset_swap", "sample_niagara_system_in_preview_lab"]
PUBLIC_FIELDS_BY_METHOD={"open_niagara_preview_player": ["system_path"], "get_niagara_preview_player_state": [], "preview_niagara_component_asset_swap": ["replacement_system_path", "actor_name", "actor_label", "component_name", "system_path", "parameter_name_contains", "restore_original_asset", "preserve_override_parameters", "activate_after_swap", "advance_time", "advance_tick_delta", "max_parameters", "max_emitter_instances", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_emitter_runtime", "max_object_properties", "max_array_elements", "max_property_value_chars"], "sample_niagara_system_in_preview_lab": ["system_path", "output_dir", "label", "warmup_times", "views", "warmup_tick_delta", "cleanup_before", "cleanup_after_each", "cleanup_after_all", "location", "scale", "auto_frame", "camera_location", "camera_rotation", "camera_fov", "user_parameter_overrides", "preserve_override_parameters", "allow_current_map_preview"]}
LOGICAL_INPUT_SCHEMAS={"open_niagara_preview_player": {"properties": {"system_path": {"type": "string", "default": "", "title": "System Path"}}, "title": "open_niagara_preview_playerArguments", "type": "object"}, "get_niagara_preview_player_state": {"properties": {}, "title": "get_niagara_preview_player_stateArguments", "type": "object"}, "preview_niagara_component_asset_swap": {"properties": {"replacement_system_path": {"type": "string", "title": "Replacement System Path"}, "actor_name": {"type": "string", "default": "", "title": "Actor Name"}, "actor_label": {"type": "string", "default": "", "title": "Actor Label"}, "component_name": {"type": "string", "default": "", "title": "Component Name"}, "system_path": {"type": "string", "default": "", "title": "System Path"}, "parameter_name_contains": {"type": "string", "default": "", "title": "Parameter Name Contains"}, "restore_original_asset": {"type": "boolean", "default": True, "title": "Restore Original Asset"}, "preserve_override_parameters": {"type": "boolean", "default": True, "title": "Preserve Override Parameters"}, "activate_after_swap": {"type": "boolean", "default": True, "title": "Activate After Swap"}, "advance_time": {"type": "number", "default": 0.35, "title": "Advance Time"}, "advance_tick_delta": {"type": "number", "default": 0.03333333333333333, "title": "Advance Tick Delta"}, "max_parameters": {"type": "integer", "default": 256, "title": "Max Parameters"}, "max_emitter_instances": {"type": "integer", "default": 128, "title": "Max Emitter Instances"}, "include_parameter_values": {"type": "boolean", "default": True, "title": "Include Parameter Values"}, "include_raw_bytes": {"type": "boolean", "default": False, "title": "Include Raw Bytes"}, "max_raw_bytes": {"type": "integer", "default": 64, "title": "Max Raw Bytes"}, "include_parameter_object_properties": {"type": "boolean", "default": False, "title": "Include Parameter Object Properties"}, "include_emitter_runtime": {"type": "boolean", "default": True, "title": "Include Emitter Runtime"}, "max_object_properties": {"type": "integer", "default": 80, "title": "Max Object Properties"}, "max_array_elements": {"type": "integer", "default": 16, "title": "Max Array Elements"}, "max_property_value_chars": {"type": "integer", "default": 512, "title": "Max Property Value Chars"}}, "title": "preview_niagara_component_asset_swapArguments", "type": "object", "required": ["replacement_system_path"]}, "sample_niagara_system_in_preview_lab": {"properties": {"system_path": {"type": "string", "title": "System Path"}, "output_dir": {"type": "string", "default": "", "title": "Output Dir"}, "label": {"type": "string", "default": "", "title": "Label"}, "warmup_times": {"anyOf": [{"type": "array", "items": {"type": "number"}}, {"type": "null"}], "default": None, "title": "Warmup Times"}, "views": {"anyOf": [{"type": "array", "items": {"type": "integer"}}, {"type": "null"}], "default": None, "title": "Views"}, "warmup_tick_delta": {"type": "number", "default": 0.03333333333333333, "title": "Warmup Tick Delta"}, "cleanup_before": {"type": "boolean", "default": True, "title": "Cleanup Before"}, "cleanup_after_each": {"type": "boolean", "default": True, "title": "Cleanup After Each"}, "cleanup_after_all": {"type": "boolean", "default": True, "title": "Cleanup After All"}, "location": {"anyOf": [{"type": "array", "items": {"type": "number"}}, {"type": "null"}], "default": None, "title": "Location"}, "scale": {"anyOf": [{"type": "array", "items": {"type": "number"}}, {"type": "null"}], "default": None, "title": "Scale"}, "auto_frame": {"type": "boolean", "default": True, "title": "Auto Frame"}, "camera_location": {"anyOf": [{"type": "array", "items": {"type": "number"}}, {"type": "null"}], "default": None, "title": "Camera Location"}, "camera_rotation": {"anyOf": [{"type": "array", "items": {"type": "number"}}, {"type": "null"}], "default": None, "title": "Camera Rotation"}, "camera_fov": {"anyOf": [{"type": "number"}, {"type": "null"}], "default": None, "title": "Camera Fov"}, "user_parameter_overrides": {"anyOf": [{"type": "object"}, {"type": "null"}], "default": None, "title": "User Parameter Overrides"}, "preserve_override_parameters": {"type": "boolean", "default": True, "title": "Preserve Override Parameters"}, "allow_current_map_preview": {"type": "boolean", "default": False, "title": "Allow Current Map Preview"}}, "title": "sample_niagara_system_in_preview_labArguments", "type": "object", "required": ["system_path"]}}
PHYSICAL_DEFAULTS_BY_METHOD={"open_niagara_preview_player": {"system_path": "\"\""}, "get_niagara_preview_player_state": {}, "preview_niagara_component_asset_swap": {"actor_name": "\"\"", "actor_label": "\"\"", "component_name": "\"\"", "system_path": "\"\"", "parameter_name_contains": "\"\"", "restore_original_asset": "true", "preserve_override_parameters": "true", "activate_after_swap": "true", "advance_time": "0.35", "advance_tick_delta": "0.03333333333333333", "max_parameters": "256", "max_emitter_instances": "128", "include_parameter_values": "true", "include_raw_bytes": "false", "max_raw_bytes": "64", "include_parameter_object_properties": "false", "include_emitter_runtime": "true", "max_object_properties": "80", "max_array_elements": "16", "max_property_value_chars": "512"}, "sample_niagara_system_in_preview_lab": {"output_dir": "\"\"", "label": "\"\"", "warmup_times": "null", "views": "null", "warmup_tick_delta": "0.03333333333333333", "cleanup_before": "true", "cleanup_after_each": "true", "cleanup_after_all": "true", "location": "null", "scale": "null", "auto_frame": "true", "camera_location": "null", "camera_rotation": "null", "camera_fov": "null", "user_parameter_overrides": "null", "preserve_override_parameters": "true", "allow_current_map_preview": "false"}}
REQUIRED_BY_METHOD={"open_niagara_preview_player": [], "get_niagara_preview_player_state": [], "preview_niagara_component_asset_swap": ["replacement_system_path"], "sample_niagara_system_in_preview_lab": ["system_path"]}
ATTACHMENT_LIBRARY = 'CubelessNiagaraPreviewWorkflowAttachmentLibrary'
ATTACHMENT_METHODS = ('install_niagara_preview_workflows_toolset', 'verify_niagara_preview_workflows_toolset', 'remove_niagara_preview_workflows_toolset')
ATTACHMENT_READY_FIELDS = ["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "niagara_preview_workflows_public1_0_21_18_schema_verified", "required0_0_1_1_default1_0_20_17_verified", "json_value_carrier40_schema_verified", "original_model_body_omission_and_camera_alias_preserved", "prefix_UI_borrowed_restore_sample_file_partial_receipt_Native_owned", "same_job_async_final_and_direct_raw_boundary_preserved", "niagara_preview_workflows4_native_contract_owned"]
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=4,
        public_input_counts=dict(zip(BINDINGS,(1,0,21,18))),physical_input_counts=dict(zip(BINDINGS,(1,0,21,18))),native_input_counts=dict(zip(BINDINGS,(1,1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0,3))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=4:raise RuntimeError('Exact NiagaraPreviewWorkflow Toolset4 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original Niagara Preview workflow method required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(1,0,21,18))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered NiagaraPreviewWorkflow wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered NiagaraPreviewWorkflow wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
