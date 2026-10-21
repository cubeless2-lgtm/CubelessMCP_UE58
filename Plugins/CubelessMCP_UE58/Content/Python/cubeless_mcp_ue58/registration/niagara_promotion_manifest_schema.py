"""HC original plan14/execute9 public fields with exact nullable lists and Any manifest carriers; no new Native surface."""
import json
import math
from cubeless_mcp_ue58.operations import niagara_promotion_manifest as api
TOOLSET = 'cubeless_mcp_ue58.registration.niagara_promotion_manifest_toolset.CubelessNiagaraPromotionManifestTools'
BINDINGS=["plan_niagara_promotion_manifest", "execute_niagara_promotion_manifest"]
PUBLIC_FIELDS_BY_METHOD={"plan_niagara_promotion_manifest": ["source_system_path", "source_system_paths", "target_root", "target_system_paths", "asset_name_prefix", "asset_name_suffix", "forbidden_dependency_prefixes", "forbidden_source_roots", "allow_non_temp_source", "allow_temp_target", "allow_temp_dependencies", "write_manifest", "manifest_name", "timeout_seconds"], "execute_niagara_promotion_manifest": ["manifest", "manifest_path", "confirmation_token", "require_confirmation", "save", "request_compile", "write_execution_report", "execution_report_name", "timeout_seconds"]}
LOGICAL_INPUT_SCHEMAS={"plan_niagara_promotion_manifest": {"properties": {"source_system_path": {"default": "", "title": "Source System Path", "type": "string"}, "source_system_paths": {"anyOf": [{"items": {"type": "string"}, "type": "array"}, {"type": "null"}], "default": None, "title": "Source System Paths"}, "target_root": {"default": "/Game/MCPWeather/Generated", "title": "Target Root", "type": "string"}, "target_system_paths": {"anyOf": [{"items": {"type": "string"}, "type": "array"}, {"type": "null"}], "default": None, "title": "Target System Paths"}, "asset_name_prefix": {"default": "", "title": "Asset Name Prefix", "type": "string"}, "asset_name_suffix": {"default": "", "title": "Asset Name Suffix", "type": "string"}, "forbidden_dependency_prefixes": {"anyOf": [{"items": {"type": "string"}, "type": "array"}, {"type": "null"}], "default": None, "title": "Forbidden Dependency Prefixes"}, "forbidden_source_roots": {"anyOf": [{"items": {"type": "string"}, "type": "array"}, {"type": "null"}], "default": None, "title": "Forbidden Source Roots"}, "allow_non_temp_source": {"default": False, "title": "Allow Non Temp Source", "type": "boolean"}, "allow_temp_target": {"default": False, "title": "Allow Temp Target", "type": "boolean"}, "allow_temp_dependencies": {"default": False, "title": "Allow Temp Dependencies", "type": "boolean"}, "write_manifest": {"default": False, "title": "Write Manifest", "type": "boolean"}, "manifest_name": {"default": "", "title": "Manifest Name", "type": "string"}, "timeout_seconds": {"default": 120, "title": "Timeout Seconds", "type": "number"}}, "title": "plan_niagara_promotion_manifestArguments", "type": "object"}, "execute_niagara_promotion_manifest": {"properties": {"manifest": {"anyOf": [{}, {"type": "null"}], "default": None, "title": "Manifest"}, "manifest_path": {"default": "", "title": "Manifest Path", "type": "string"}, "confirmation_token": {"default": "", "title": "Confirmation Token", "type": "string"}, "require_confirmation": {"default": True, "title": "Require Confirmation", "type": "boolean"}, "save": {"default": True, "title": "Save", "type": "boolean"}, "request_compile": {"default": True, "title": "Request Compile", "type": "boolean"}, "write_execution_report": {"default": False, "title": "Write Execution Report", "type": "boolean"}, "execution_report_name": {"default": "", "title": "Execution Report Name", "type": "string"}, "timeout_seconds": {"default": 180, "title": "Timeout Seconds", "type": "number"}}, "title": "execute_niagara_promotion_manifestArguments", "type": "object"}}
PHYSICAL_DEFAULTS_BY_METHOD={"plan_niagara_promotion_manifest": {"source_system_path": "\"\"", "source_system_paths": "null", "target_root": "\"/Game/MCPWeather/Generated\"", "target_system_paths": "null", "asset_name_prefix": "\"\"", "asset_name_suffix": "\"\"", "forbidden_dependency_prefixes": "null", "forbidden_source_roots": "null", "allow_non_temp_source": "false", "allow_temp_target": "false", "allow_temp_dependencies": "false", "write_manifest": "false", "manifest_name": "\"\"", "timeout_seconds": "120.0"}, "execute_niagara_promotion_manifest": {"manifest": "null", "manifest_path": "\"\"", "confirmation_token": "\"\"", "require_confirmation": "true", "save": "true", "request_compile": "true", "write_execution_report": "false", "execution_report_name": "\"\"", "timeout_seconds": "180.0"}}
REQUIRED_BY_METHOD={"plan_niagara_promotion_manifest": [], "execute_niagara_promotion_manifest": []}
ATTACHMENT_LIBRARY = 'CubelessNiagaraPromotionManifestAttachmentLibrary'
ATTACHMENT_METHODS = ('install_niagara_promotion_manifest_toolset', 'verify_niagara_promotion_manifest_toolset', 'remove_niagara_promotion_manifest_toolset')
ATTACHMENT_READY_FIELDS=["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "niagara_promotion_manifest_public14_9_schema_verified", "required0_0_default14_9_verified", "json_value_carrier23_schema_verified", "original_model_lists_nullable_Any_and_raw_preserved", "manifest_plan_execute_contract_domain_owned", "confirmation_revalidation_partial_save_and_no_replay_preserved", "niagara_promotion_manifest2_python_contract_owned"]
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),library='CubelessNiagaraPromotionLibrary',methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=2,
        public_input_counts=dict(zip(BINDINGS,(14,9))),physical_input_counts=dict(zip(BINDINGS,(14,9))),native_input_counts=dict(zip(BINDINGS,(0,0))),hidden_input_counts=dict(zip(BINDINGS,(0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=2:raise RuntimeError('Exact NiagaraPromotionManifest Toolset2 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original promotion manifest method required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(14,9))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered NiagaraPromotionManifest wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered NiagaraPromotionManifest wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
