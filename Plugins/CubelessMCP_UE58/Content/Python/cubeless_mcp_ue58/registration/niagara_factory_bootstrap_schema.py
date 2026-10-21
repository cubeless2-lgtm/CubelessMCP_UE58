"""GP exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import niagara_factory_bootstrap as api
from cubeless_mcp_ue58.operations import niagara_factory_bootstrap as domain
TOOLSET = 'cubeless_mcp_ue58.registration.niagara_factory_bootstrap_toolset.CubelessNiagaraFactoryBootstrapTools'
BINDINGS = ["inspect_niagara_factory_bootstrap_defaults", "create_empty_niagara_system", "create_empty_niagara_emitter", "create_basic_sprite_niagara_system"]
PUBLIC_FIELDS_BY_METHOD = {"inspect_niagara_factory_bootstrap_defaults": ["kind", "create_default_system_nodes", "add_default_modules_and_renderers", "include_pins", "max_function_calls"], "create_empty_niagara_system": ["target_system_path", "allow_non_temp_path", "overwrite_existing", "save", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"], "create_empty_niagara_emitter": ["target_emitter_path", "add_default_modules_and_renderers", "allow_non_temp_path", "overwrite_existing", "save"], "create_basic_sprite_niagara_system": ["target_system_path", "emitter_name", "material_path", "allow_non_temp_path", "overwrite_existing", "save", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"]}
LOGICAL_INPUT_SCHEMAS = {"inspect_niagara_factory_bootstrap_defaults": {"properties": {"kind": {"default": "all", "title": "Kind", "type": "string"}, "create_default_system_nodes": {"default": True, "title": "Create Default System Nodes", "type": "boolean"}, "add_default_modules_and_renderers": {"default": False, "title": "Add Default Modules And Renderers", "type": "boolean"}, "include_pins": {"default": False, "title": "Include Pins", "type": "boolean"}, "max_function_calls": {"default": 80, "title": "Max Function Calls", "type": "integer"}}, "title": "inspect_niagara_factory_bootstrap_defaultsArguments", "type": "object"}, "create_empty_niagara_system": {"properties": {"target_system_path": {"title": "Target System Path", "type": "string"}, "allow_non_temp_path": {"default": False, "title": "Allow Non Temp Path", "type": "boolean"}, "overwrite_existing": {"default": False, "title": "Overwrite Existing", "type": "boolean"}, "save": {"default": True, "title": "Save", "type": "boolean"}, "wait_for_completion": {"default": True, "title": "Wait For Completion", "type": "boolean"}, "timeout_seconds": {"default": 10, "title": "Timeout Seconds", "type": "number"}, "poll_interval_seconds": {"default": 0.1, "title": "Poll Interval Seconds", "type": "number"}}, "required": ["target_system_path"], "title": "create_empty_niagara_systemArguments", "type": "object"}, "create_empty_niagara_emitter": {"properties": {"target_emitter_path": {"title": "Target Emitter Path", "type": "string"}, "add_default_modules_and_renderers": {"default": False, "title": "Add Default Modules And Renderers", "type": "boolean"}, "allow_non_temp_path": {"default": False, "title": "Allow Non Temp Path", "type": "boolean"}, "overwrite_existing": {"default": False, "title": "Overwrite Existing", "type": "boolean"}, "save": {"default": True, "title": "Save", "type": "boolean"}}, "required": ["target_emitter_path"], "title": "create_empty_niagara_emitterArguments", "type": "object"}, "create_basic_sprite_niagara_system": {"properties": {"target_system_path": {"title": "Target System Path", "type": "string"}, "emitter_name": {"default": "Emitter", "title": "Emitter Name", "type": "string"}, "material_path": {"default": "", "title": "Material Path", "type": "string"}, "allow_non_temp_path": {"default": False, "title": "Allow Non Temp Path", "type": "boolean"}, "overwrite_existing": {"default": False, "title": "Overwrite Existing", "type": "boolean"}, "save": {"default": True, "title": "Save", "type": "boolean"}, "wait_for_completion": {"default": True, "title": "Wait For Completion", "type": "boolean"}, "timeout_seconds": {"default": 10, "title": "Timeout Seconds", "type": "number"}, "poll_interval_seconds": {"default": 0.1, "title": "Poll Interval Seconds", "type": "number"}}, "required": ["target_system_path"], "title": "create_basic_sprite_niagara_systemArguments", "type": "object"}}
PHYSICAL_DEFAULTS_BY_METHOD = {"inspect_niagara_factory_bootstrap_defaults": {"kind": "\"all\"", "create_default_system_nodes": "true", "add_default_modules_and_renderers": "false", "include_pins": "false", "max_function_calls": "80"}, "create_empty_niagara_system": {"allow_non_temp_path": "false", "overwrite_existing": "false", "save": "true", "wait_for_completion": "true", "timeout_seconds": "10.0", "poll_interval_seconds": "0.1"}, "create_empty_niagara_emitter": {"add_default_modules_and_renderers": "false", "allow_non_temp_path": "false", "overwrite_existing": "false", "save": "true"}, "create_basic_sprite_niagara_system": {"emitter_name": "\"Emitter\"", "material_path": "\"\"", "allow_non_temp_path": "false", "overwrite_existing": "false", "save": "true", "wait_for_completion": "true", "timeout_seconds": "10.0", "poll_interval_seconds": "0.1"}}
REQUIRED_BY_METHOD = {"inspect_niagara_factory_bootstrap_defaults": [], "create_empty_niagara_system": ["target_system_path"], "create_empty_niagara_emitter": ["target_emitter_path"], "create_basic_sprite_niagara_system": ["target_system_path"]}
ATTACHMENT_LIBRARY = 'CubelessNiagaraFactoryBootstrapAttachmentLibrary'
ATTACHMENT_METHODS = ('install_niagara_factory_bootstrap_toolset', 'verify_niagara_factory_bootstrap_toolset', 'remove_niagara_factory_bootstrap_toolset')
ATTACHMENT_READY_FIELDS = ["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "niagara_factory_public5_7_5_9_schema_verified", "required0_1_1_1_default5_6_4_8_verified", "json_value_carrier26_schema_verified", "scalar_original_model_direct_body_passthrough_preserved", "raw_hidden0_and_all_public_dict_fields_preserved", "transient_factory_and_wait_incomplete_receipt_owned", "niagara_factory_bootstrap4_native_contract_owned"]
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();async_status=domain.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()) and all(value is True for value in async_status["Native_bindings"].values()),factory_system_async=async_status,library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=4,
        public_input_counts=dict(zip(BINDINGS,(5,7,5,9))),physical_input_counts=dict(zip(BINDINGS,(5,7,5,9))),native_input_counts=dict(zip(BINDINGS,(1,1,1,1))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=4:raise RuntimeError('Exact NiagaraFactoryBootstrap Toolset4 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original Niagara runtime inspection methods required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(5,7,5,9))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered NiagaraFactoryBootstrap wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered NiagaraFactoryBootstrap wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
