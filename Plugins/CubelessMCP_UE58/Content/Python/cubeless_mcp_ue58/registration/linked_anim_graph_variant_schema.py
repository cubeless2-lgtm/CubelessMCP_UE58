"""Linked AnimGraph public14; command-only graph context is never a public input."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import linked_anim_graph_variant as api
TOOLSET = "cubeless_mcp_ue58.registration.linked_anim_graph_variant_toolset.CubelessLinkedAnimGraphVariantTools"
METHOD = "ensure_linked_anim_graph_sample_variant"
BINDINGS = (METHOD,)
PUBLIC_FIELDS_BY_METHOD = {METHOD: tuple(["source_anim_bp_path", "linked_anim_bp_path", "target_anim_bp_path", "target_root", "variant_name", "insert_mode", "allow_existing_target_reuse", "overwrite_existing", "replace_existing_links", "require_input_pose_pin", "compile", "save_asset", "dry_run", "validate_only"])}
DEFAULTS_BY_METHOD = {METHOD: {"target_anim_bp_path": "", "target_root": "/Game/_MCP_Sample/AnimStudy", "variant_name": "LinkedAnimGraphSample", "insert_mode": "input_pose_to_linked_graph_to_root", "allow_existing_target_reuse": True, "overwrite_existing": False, "replace_existing_links": True, "require_input_pose_pin": False, "compile": True, "save_asset": False, "dry_run": False, "validate_only": False}}
REQUIRED_BY_METHOD = {METHOD: tuple(["source_anim_bp_path", "linked_anim_bp_path"])}
STRING_FIELDS = tuple(["source_anim_bp_path", "linked_anim_bp_path", "target_anim_bp_path", "target_root", "variant_name", "insert_mode"])
EMPTY_STRING_FIELDS = ("target_anim_bp_path",)
BOOLEAN_FIELDS = tuple(["allow_existing_target_reuse", "overwrite_existing", "replace_existing_links", "require_input_pose_pin", "compile", "save_asset", "dry_run", "validate_only"])
FIELD_KINDS_BY_METHOD = {METHOD: {"source_anim_bp_path": "string", "linked_anim_bp_path": "string", "target_anim_bp_path": "string", "target_root": "string", "variant_name": "string", "insert_mode": "string", "allow_existing_target_reuse": "boolean", "overwrite_existing": "boolean", "replace_existing_links": "boolean", "require_input_pose_pin": "boolean", "compile": "boolean", "save_asset": "boolean", "dry_run": "boolean", "validate_only": "boolean"}}
RESULT_FIELDS = ('success','operation_id','stage','error_code','error','changes_json','compile_json','save_json','validation_json','retry_json','data_json')
ATTACHMENT_LIBRARY = "CubelessLinkedAnimGraphVariantAttachmentLibrary"
ATTACHMENT_METHODS = tuple(["install_linked_anim_graph_variant_toolset", "verify_linked_anim_graph_variant_toolset", "remove_linked_anim_graph_variant_toolset"])
ATTACHMENT_READY_FIELDS = tuple(["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "ordinary_string6_schema_verified", "boolean8_schema_verified", "required_source2_verified", "variant14_schema_verified", "empty_string_defaults_verified"])

def binding_status(toolset_class):
    methods=api.binding_status()
    defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False,bindings_ready=set(methods)==set(BINDINGS) and all(value is True for value in methods.values()) and all(defined.values()),
        library=api.LIBRARY,methods=methods,definition_methods=defined,public_toolset=TOOLSET,public_tool_count=1,
        public_input_counts={METHOD:14},physical_input_counts={METHOD:14},hidden_input_counts={METHOD:15},
        nullable_json_carrier_count=0,native_signature_json_carrier_count=1,integer_decimal_carrier_count=0,number_decimal_carrier_count=0,
        schema_verified=False,default_verified=False,raw_guard_required=True,raw_guard_ready=False,boolean_raw_guard_ready=False,
        actual_official_wire_verified=False,boolean_Named_wire_verified=False,empty_string_Named_wire_verified=False,
        metadata_schema_validation_is_wire_packing_proof=False,adapter_noPIE_idle_gate=False,unique_external_pipeline_complete=False,packaged_runtime_verified=False)

def _required(expected,actual):
    return type(actual) is list and len(actual)==len(expected) and all(type(value) is str for value in actual) and set(actual)==set(expected)

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if (type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=1 or not callable(getattr(toolset_class,METHOD,None))):
        raise RuntimeError('Exact canonical Linked AnimGraph Toolset1 is required')
    row=schema['tools'][0]
    if type(row) is not dict or row.get('name')!=TOOLSET+'.'+METHOD: raise RuntimeError('Only the exact Linked AnimGraph variant method is admitted')
    inputs=row.get('inputSchema');fields=inputs.get('properties') if type(inputs) is dict else None
    required=REQUIRED_BY_METHOD[METHOD]+EMPTY_STRING_FIELDS if reflected_carriers else REQUIRED_BY_METHOD[METHOD]
    if (type(inputs) is not dict or inputs.get('type')!='object' or type(fields) is not dict or set(fields)!=set(PUBLIC_FIELDS_BY_METHOD[METHOD]) or not _required(required,inputs.get('required')) or not reflected_carriers and inputs.get('additionalProperties') is not False):
        raise RuntimeError('Exact public14 and physical source2/empty-target required fields differ')
    for field,kind in FIELD_KINDS_BY_METHOD[METHOD].items():
        item=fields[field]
        if type(item) is not dict or item.get('type')!=kind: raise RuntimeError('Original field type differs: '+field)
        if field in REQUIRED_BY_METHOD[METHOD] or reflected_carriers and field in EMPTY_STRING_FIELDS:
            if 'default' in item: raise RuntimeError('Physical required/source default must be absent: '+field)
        else:
            expected=DEFAULTS_BY_METHOD[METHOD][field]
            if 'default' not in item or type(item['default']) is not type(expected) or item['default']!=expected: raise RuntimeError('Original default differs: '+field)
    output=row.get('outputSchema');outer=output.get('properties') if type(output) is dict else None
    if (type(output) is not dict or output.get('type')!='object' or type(outer) is not dict or set(outer)!={'returnValue'} or not _required(('returnValue',),output.get('required'))):
        raise RuntimeError('Exact returnValue outer output envelope is required')
    result=outer['returnValue'];result_fields=result.get('properties') if type(result) is dict else None
    if (type(result) is not dict or result.get('type')!='object' or type(result_fields) is not dict or set(result_fields)!=set(RESULT_FIELDS) or not _required(RESULT_FIELDS,result.get('required')) or any(type(result_fields[key]) is not dict or result_fields[key].get('type')!=('boolean' if key=='success' else 'string') for key in RESULT_FIELDS)):
        raise RuntimeError('Exact synchronous standard11 output fields/types are required')
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,
        reflected_carrier_schema_verified=reflected_carriers,public_input_counts={METHOD:14},nullable_json_carrier_count=0,native_signature_json_carrier_count=1,
        integer_decimal_carrier_count=0,number_decimal_carrier_count=0,required_source_count=2,actual_official_wire_verified=False,
        boolean_Named_wire_verified=False,empty_string_Named_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False,unique_external_pipeline_complete=False)

def validate_schema(schema,toolset_class):
    return _validate_schema(schema,toolset_class)

def validate_reflected_carrier_schema(schema,toolset_class):
    return _validate_schema(schema,toolset_class,reflected_carriers=True)

def validate_registered_metadata(class_schema,attachment_report,toolset_class):
    reflected=validate_reflected_carrier_schema(class_schema,toolset_class)
    if (type(attachment_report) is not dict or attachment_report.get('operation') not in ATTACHMENT_METHODS[:2]
            or not all(attachment_report.get(key) is True for key in ATTACHMENT_READY_FIELDS)
            or attachment_report.get('asset_mutation') is not False or attachment_report.get('metadata_mutation') is not False
            or attachment_report.get('actual_named_wire_verified') is not False
            or attachment_report.get('logical_schema_source')!='actual_registered_wrapper_GetJsonSchema'
            or attachment_report.get('logical_schema_registry_identity_verified') is not True):
        raise RuntimeError('Exact ready owner receipt with actual registered Linked AnimGraph wrapper schema is required')
    logical_text=attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text: raise RuntimeError('Actual registered wrapper schema is missing')
    logical=validate_schema(json.loads(logical_text),toolset_class)
    return dict(logical,reflected_carrier_schema_verified=True,class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema',logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True,logical_schema_json=logical_text,reflected_schema_validation=reflected,logical_schema_validation=logical)
