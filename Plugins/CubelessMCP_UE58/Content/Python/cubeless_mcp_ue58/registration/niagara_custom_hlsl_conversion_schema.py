"""GQ exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import niagara_custom_hlsl_conversion as api
from cubeless_mcp_ue58.operations import niagara_custom_hlsl_conversion as domain
TOOLSET = 'cubeless_mcp_ue58.registration.niagara_custom_hlsl_conversion_toolset.CubelessNiagaraCustomHlslConversionTools'
BINDINGS = ["replace_niagara_custom_hlsl", "apply_niagara_scratch_pad_graph_recipe"]
PUBLIC_FIELDS_BY_METHOD = {"apply_niagara_scratch_pad_graph_recipe": ["system_path", "custom_hlsl_node_guid", "nodes", "links", "output_mappings", "scratch_pad_name", "scratch_pad_script_path", "scratch_pad_script_index", "expected_hlsl_body_hash", "dry_run", "remove_custom_hlsl", "save", "request_compile", "timeout_seconds", "poll_interval_seconds"], "replace_niagara_custom_hlsl": ["system_path", "replacement_hlsl", "node_guid", "signature_name", "contains_text", "scratch_pad_name", "source_path_contains", "expected_contains", "dry_run", "allow_source_edit", "save", "request_compile", "timeout_seconds", "poll_interval_seconds", "max_hlsl_chars"]}
LOGICAL_INPUT_SCHEMAS = {"replace_niagara_custom_hlsl": {"properties": {"system_path": {"title": "System Path", "type": "string"}, "replacement_hlsl": {"title": "Replacement Hlsl", "type": "string"}, "node_guid": {"default": "", "title": "Node Guid", "type": "string"}, "signature_name": {"default": "", "title": "Signature Name", "type": "string"}, "contains_text": {"default": "", "title": "Contains Text", "type": "string"}, "scratch_pad_name": {"default": "", "title": "Scratch Pad Name", "type": "string"}, "source_path_contains": {"default": "", "title": "Source Path Contains", "type": "string"}, "expected_contains": {"default": "", "title": "Expected Contains", "type": "string"}, "dry_run": {"default": True, "title": "Dry Run", "type": "boolean"}, "allow_source_edit": {"default": False, "title": "Allow Source Edit", "type": "boolean"}, "save": {"default": True, "title": "Save", "type": "boolean"}, "request_compile": {"default": True, "title": "Request Compile", "type": "boolean"}, "timeout_seconds": {"default": 30, "title": "Timeout Seconds", "type": "number"}, "poll_interval_seconds": {"default": 0.05, "title": "Poll Interval Seconds", "type": "number"}, "max_hlsl_chars": {"default": 8000, "title": "Max Hlsl Chars", "type": "integer"}}, "required": ["system_path", "replacement_hlsl"], "title": "replace_niagara_custom_hlslArguments", "type": "object"}, "apply_niagara_scratch_pad_graph_recipe": {"properties": {"system_path": {"title": "System Path", "type": "string"}, "custom_hlsl_node_guid": {"title": "Custom Hlsl Node Guid", "type": "string"}, "nodes": {"items": {"type": "object"}, "title": "Nodes", "type": "array"}, "links": {"items": {"type": "object"}, "title": "Links", "type": "array"}, "output_mappings": {"items": {"type": "object"}, "title": "Output Mappings", "type": "array"}, "scratch_pad_name": {"default": "", "title": "Scratch Pad Name", "type": "string"}, "scratch_pad_script_path": {"default": "", "title": "Scratch Pad Script Path", "type": "string"}, "scratch_pad_script_index": {"anyOf": [{"type": "integer"}, {"type": "null"}], "default": None, "title": "Scratch Pad Script Index"}, "expected_hlsl_body_hash": {"default": "", "title": "Expected Hlsl Body Hash", "type": "string"}, "dry_run": {"default": True, "title": "Dry Run", "type": "boolean"}, "remove_custom_hlsl": {"default": True, "title": "Remove Custom Hlsl", "type": "boolean"}, "save": {"default": True, "title": "Save", "type": "boolean"}, "request_compile": {"default": True, "title": "Request Compile", "type": "boolean"}, "timeout_seconds": {"default": 30, "title": "Timeout Seconds", "type": "number"}, "poll_interval_seconds": {"default": 0.05, "title": "Poll Interval Seconds", "type": "number"}}, "required": ["system_path", "custom_hlsl_node_guid", "nodes", "links", "output_mappings"], "title": "apply_niagara_scratch_pad_graph_recipeArguments", "type": "object"}}
PHYSICAL_DEFAULTS_BY_METHOD = {"apply_niagara_scratch_pad_graph_recipe": {"scratch_pad_name": "\"\"", "scratch_pad_script_path": "\"\"", "scratch_pad_script_index": "null", "expected_hlsl_body_hash": "\"\"", "dry_run": "true", "remove_custom_hlsl": "true", "save": "true", "request_compile": "true", "timeout_seconds": "30.0", "poll_interval_seconds": "0.05"}, "replace_niagara_custom_hlsl": {"node_guid": "\"\"", "signature_name": "\"\"", "contains_text": "\"\"", "scratch_pad_name": "\"\"", "source_path_contains": "\"\"", "expected_contains": "\"\"", "dry_run": "true", "allow_source_edit": "false", "save": "true", "request_compile": "true", "timeout_seconds": "30.0", "poll_interval_seconds": "0.05", "max_hlsl_chars": "8000"}}
REQUIRED_BY_METHOD = {"apply_niagara_scratch_pad_graph_recipe": ["system_path", "custom_hlsl_node_guid", "nodes", "links", "output_mappings"], "replace_niagara_custom_hlsl": ["system_path", "replacement_hlsl"]}
ATTACHMENT_LIBRARY = 'CubelessNiagaraCustomHlslConversionAttachmentLibrary'
ATTACHMENT_METHODS = ('install_niagara_custom_hlsl_conversion_toolset', 'verify_niagara_custom_hlsl_conversion_toolset', 'remove_niagara_custom_hlsl_conversion_toolset')
ATTACHMENT_READY_FIELDS = ["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "niagara_HLSL_conversion_public15_15_schema_verified", "required2_5_default13_10_verified", "json_value_carrier30_schema_verified", "nested_Dict_Any_nullable_original_model_preserved", "raw_hidden0_and_phase_timeout_contract_preserved", "compile_ID_partial_restore_actual_receipt_owned", "niagara_custom_hlsl_conversion2_native_contract_owned"]
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();async_status=domain.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()) and all(value is True for value in async_status["Native_bindings"].values()),hlsl_conversion_async=async_status,library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=2,
        public_input_counts=dict(zip(BINDINGS,(15,15))),physical_input_counts=dict(zip(BINDINGS,(15,15))),native_input_counts=dict(zip(BINDINGS,(1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=2:raise RuntimeError('Exact NiagaraCustomHlslConversion Toolset2 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('Two exact original Niagara HLSL conversion methods required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(15,15))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered NiagaraCustomHlslConversion wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered NiagaraCustomHlslConversion wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
