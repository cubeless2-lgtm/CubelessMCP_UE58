"""GQ exact original eager logical schemas versus public JSON FString carriers."""
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import niagara_promotion as api
TOOLSET = 'cubeless_mcp_ue58.registration.niagara_promotion_toolset.CubelessNiagaraPromotionTools'
BINDINGS = ["audit_niagara_promotion_candidate", "promote_niagara_system_candidate"]
PUBLIC_FIELDS_BY_METHOD = {"audit_niagara_promotion_candidate": ["system_path", "proposed_target_path", "forbidden_dependency_prefixes", "forbidden_source_roots", "allow_temp_target", "allow_temp_dependencies", "timeout_seconds"], "promote_niagara_system_candidate": ["source_system_path", "target_system_path", "dry_run", "forbidden_dependency_prefixes", "forbidden_source_roots", "allow_non_temp_source", "allow_temp_target", "allow_temp_dependencies", "overwrite_existing", "save", "request_compile", "timeout_seconds"]}
LOGICAL_INPUT_SCHEMAS = {"audit_niagara_promotion_candidate": {"properties": {"system_path": {"title": "System Path", "type": "string"}, "proposed_target_path": {"default": "", "title": "Proposed Target Path", "type": "string"}, "forbidden_dependency_prefixes": {"anyOf": [{"items": {"type": "string"}, "type": "array"}, {"type": "null"}], "default": None, "title": "Forbidden Dependency Prefixes"}, "forbidden_source_roots": {"anyOf": [{"items": {"type": "string"}, "type": "array"}, {"type": "null"}], "default": None, "title": "Forbidden Source Roots"}, "allow_temp_target": {"default": False, "title": "Allow Temp Target", "type": "boolean"}, "allow_temp_dependencies": {"default": False, "title": "Allow Temp Dependencies", "type": "boolean"}, "timeout_seconds": {"default": 60, "title": "Timeout Seconds", "type": "number"}}, "required": ["system_path"], "title": "audit_niagara_promotion_candidateArguments", "type": "object"}, "promote_niagara_system_candidate": {"properties": {"source_system_path": {"title": "Source System Path", "type": "string"}, "target_system_path": {"title": "Target System Path", "type": "string"}, "dry_run": {"default": True, "title": "Dry Run", "type": "boolean"}, "forbidden_dependency_prefixes": {"anyOf": [{"items": {"type": "string"}, "type": "array"}, {"type": "null"}], "default": None, "title": "Forbidden Dependency Prefixes"}, "forbidden_source_roots": {"anyOf": [{"items": {"type": "string"}, "type": "array"}, {"type": "null"}], "default": None, "title": "Forbidden Source Roots"}, "allow_non_temp_source": {"default": False, "title": "Allow Non Temp Source", "type": "boolean"}, "allow_temp_target": {"default": False, "title": "Allow Temp Target", "type": "boolean"}, "allow_temp_dependencies": {"default": False, "title": "Allow Temp Dependencies", "type": "boolean"}, "overwrite_existing": {"default": False, "title": "Overwrite Existing", "type": "boolean"}, "save": {"default": True, "title": "Save", "type": "boolean"}, "request_compile": {"default": True, "title": "Request Compile", "type": "boolean"}, "timeout_seconds": {"default": 120, "title": "Timeout Seconds", "type": "number"}}, "required": ["source_system_path", "target_system_path"], "title": "promote_niagara_system_candidateArguments", "type": "object"}}
PHYSICAL_DEFAULTS_BY_METHOD = {"audit_niagara_promotion_candidate": {"proposed_target_path": "\"\"", "forbidden_dependency_prefixes": "null", "forbidden_source_roots": "null", "allow_temp_target": "false", "allow_temp_dependencies": "false", "timeout_seconds": "60.0"}, "promote_niagara_system_candidate": {"dry_run": "true", "forbidden_dependency_prefixes": "null", "forbidden_source_roots": "null", "allow_non_temp_source": "false", "allow_temp_target": "false", "allow_temp_dependencies": "false", "overwrite_existing": "false", "save": "true", "request_compile": "true", "timeout_seconds": "120.0"}}
REQUIRED_BY_METHOD = {"audit_niagara_promotion_candidate": ["system_path"], "promote_niagara_system_candidate": ["source_system_path", "target_system_path"]}
ATTACHMENT_LIBRARY = 'CubelessNiagaraPromotionAttachmentLibrary'
ATTACHMENT_METHODS = ('install_niagara_promotion_toolset', 'verify_niagara_promotion_toolset', 'remove_niagara_promotion_toolset')
ATTACHMENT_READY_FIELDS = ["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "niagara_promotion_public7_12_schema_verified", "required1_2_default6_10_verified", "json_value_carrier19_schema_verified", "scalar_nullable_original_model_and_body_omission_preserved", "raw_hidden0_transport_timeout_not_Native", "strict_promotion_audit_actual_partial_save_receipt_owned", "niagara_promotion2_native_contract_owned"]
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')

# PS: one Python composition; the two existing Native leaves remain unchanged.
BINDINGS.append('create_niagara_system_from_source_template')
PUBLIC_FIELDS_BY_METHOD['create_niagara_system_from_source_template']=['target_system_path', 'source_system_path', 'source_emitter_indices', 'copy_source_user_parameters', 'save', 'request_compile', 'wait_for_completion', 'timeout_seconds']
LOGICAL_INPUT_SCHEMAS['create_niagara_system_from_source_template']={'properties': {'target_system_path': {'title': 'Target System Path', 'type': 'string'}, 'source_system_path': {'title': 'Source System Path', 'type': 'string'}, 'source_emitter_indices': {'anyOf': [{'items': {'type': 'integer'}, 'type': 'array'}, {'type': 'null'}], 'default': None, 'title': 'Source Emitter Indices'}, 'copy_source_user_parameters': {'title': 'Copy Source User Parameters', 'type': 'boolean', 'default': True}, 'save': {'title': 'Save', 'type': 'boolean', 'default': True}, 'request_compile': {'title': 'Request Compile', 'type': 'boolean', 'default': True}, 'wait_for_completion': {'title': 'Wait For Completion', 'type': 'boolean', 'default': True}, 'timeout_seconds': {'default': 180, 'title': 'Timeout Seconds', 'type': 'integer'}}, 'required': ['target_system_path', 'source_system_path'], 'title': 'create_niagara_system_from_source_templateArguments', 'type': 'object'}
PHYSICAL_DEFAULTS_BY_METHOD['create_niagara_system_from_source_template']={'source_emitter_indices': 'null', 'copy_source_user_parameters': 'true', 'save': 'true', 'request_compile': 'true', 'wait_for_completion': 'true', 'timeout_seconds': '180'}
REQUIRED_BY_METHOD['create_niagara_system_from_source_template']=['target_system_path', 'source_system_path']
ATTACHMENT_READY_FIELDS += ["niagara_source_template_public8_schema_verified","source_template_required2_default6_verified","source_template_existing_future_passthrough_preserved"]

def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS[:2]) and all(v is True for v in methods.values()) and all(defined.values()),library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=3,
        public_input_counts=dict(zip(BINDINGS,(7,12,8))),physical_input_counts=dict(zip(BINDINGS,(7,12,8))),native_input_counts=dict(zip(BINDINGS,(1,1,None))),hidden_input_counts=dict(zip(BINDINGS,(0,0,0))),
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
    if type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=3:raise RuntimeError('Exact NiagaraPromotion Toolset3 required')
    seen=set()
    for row in schema['tools']:
        if type(row) is not dict:raise RuntimeError('Exact tool object required')
        names=[m for m in BINDINGS if row.get('name')==TOOLSET+'.'+m]
        if len(names)!=1 or names[0] in seen:raise RuntimeError('One exact original Niagara promotion methods required')
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
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,reflected_carrier_schema_verified=reflected_carriers,public_input_counts=dict(zip(BINDINGS,(7,12,8))),actual_official_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

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
        raise RuntimeError('Exact ready owner receipt with actual registered NiagaraPromotion wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered NiagaraPromotion wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
