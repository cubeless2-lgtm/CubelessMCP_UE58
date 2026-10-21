"""Original locomotion driver public27; private decimal1 and trailing Native context remain hidden."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import anim_bp_locomotion_driver as api
TOOLSET = "cubeless_mcp_ue58.registration.anim_bp_locomotion_driver_toolset.CubelessAnimBPLocomotionDriverTools"
METHOD = "ensure_anim_bp_event_graph_locomotion_driver"
BINDINGS = (METHOD,)
PUBLIC_FIELDS_BY_METHOD = {METHOD: ["source_anim_bp_path", "target_anim_bp_path", "target_root", "variant_name", "speed_variable", "create_variable_if_missing", "include_direction", "direction_variable", "create_direction_variable_if_missing", "include_is_in_air", "is_in_air_variable", "create_is_in_air_variable_if_missing", "include_acceleration", "acceleration_variable", "create_acceleration_variable_if_missing", "include_is_accelerating", "is_accelerating_variable", "create_is_accelerating_variable_if_missing", "is_accelerating_threshold", "guard_pawn_owner", "replace_existing_links", "allow_existing_target_reuse", "overwrite_existing", "compile", "save_asset", "dry_run", "validate_only"]}
DEFAULTS_BY_METHOD = {METHOD: {"target_anim_bp_path": "", "target_root": "/Game/_MCP_Sample/AnimStudy", "variant_name": "LocomotionEventGraph", "speed_variable": "Speed", "create_variable_if_missing": True, "include_direction": False, "direction_variable": "Direction", "create_direction_variable_if_missing": True, "include_is_in_air": False, "is_in_air_variable": "IsInAir", "create_is_in_air_variable_if_missing": True, "include_acceleration": False, "acceleration_variable": "Acceleration", "create_acceleration_variable_if_missing": True, "include_is_accelerating": False, "is_accelerating_variable": "IsAccelerating", "create_is_accelerating_variable_if_missing": True, "is_accelerating_threshold": 3.0, "guard_pawn_owner": True, "replace_existing_links": False, "allow_existing_target_reuse": True, "overwrite_existing": False, "compile": True, "save_asset": True, "dry_run": False, "validate_only": False}}
REQUIRED_BY_METHOD = {METHOD: tuple(["source_anim_bp_path"])}
STRING_FIELDS = tuple(["source_anim_bp_path", "target_anim_bp_path", "target_root", "variant_name", "speed_variable", "direction_variable", "is_in_air_variable", "acceleration_variable", "is_accelerating_variable"])
EMPTY_STRING_FIELDS = tuple(["target_anim_bp_path"])
NULLABLE_FIELDS = tuple([])
BOOLEAN_FIELDS = tuple(["create_variable_if_missing", "include_direction", "create_direction_variable_if_missing", "include_is_in_air", "create_is_in_air_variable_if_missing", "include_acceleration", "create_acceleration_variable_if_missing", "include_is_accelerating", "create_is_accelerating_variable_if_missing", "guard_pawn_owner", "replace_existing_links", "allow_existing_target_reuse", "overwrite_existing", "compile", "save_asset", "dry_run", "validate_only"])
INTEGER_FIELDS = tuple([])
NUMBER_FIELDS = tuple(["is_accelerating_threshold"])
FIELD_KINDS_BY_METHOD = {METHOD: {"source_anim_bp_path": "string", "target_anim_bp_path": "string", "target_root": "string", "variant_name": "string", "speed_variable": "string", "create_variable_if_missing": "boolean", "include_direction": "boolean", "direction_variable": "string", "create_direction_variable_if_missing": "boolean", "include_is_in_air": "boolean", "is_in_air_variable": "string", "create_is_in_air_variable_if_missing": "boolean", "include_acceleration": "boolean", "acceleration_variable": "string", "create_acceleration_variable_if_missing": "boolean", "include_is_accelerating": "boolean", "is_accelerating_variable": "string", "create_is_accelerating_variable_if_missing": "boolean", "is_accelerating_threshold": "number", "guard_pawn_owner": "boolean", "replace_existing_links": "boolean", "allow_existing_target_reuse": "boolean", "overwrite_existing": "boolean", "compile": "boolean", "save_asset": "boolean", "dry_run": "boolean", "validate_only": "boolean"}}
ITEMS_BY_FIELD = {}
RESULT_FIELDS = ('success','operation_id','stage','error_code','error','changes_json','compile_json','save_json','validation_json','retry_json','data_json')
ATTACHMENT_LIBRARY = 'CubelessAnimBPLocomotionDriverAttachmentLibrary'
ATTACHMENT_METHODS = ('install_anim_bp_locomotion_driver_toolset','verify_anim_bp_locomotion_driver_toolset','remove_anim_bp_locomotion_driver_toolset')
ATTACHMENT_READY_FIELDS = tuple(["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "ordinary_string9_schema_verified", "boolean17_schema_verified", "number1_schema_verified", "locomotion_driver27_schema_verified", "required_source1_verified", "empty_string_defaults_verified"])

def binding_status(toolset_class):
    methods = api.binding_status()
    defined = {name: callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False, bindings_ready=set(methods)==set(BINDINGS) and all(value is True for value in methods.values()) and all(defined.values()),
        library=api.LIBRARY, methods=methods, definition_methods=defined, public_toolset=TOOLSET, public_tool_count=1,
        public_input_counts={METHOD:27}, physical_input_counts={METHOD:27}, hidden_input_counts={METHOD:28},
        nullable_json_carrier_count=0, native_signature_json_carrier_count=1, integer_decimal_carrier_count=0, number_decimal_carrier_count=1,
        schema_verified=False, default_verified=False, raw_guard_required=True, raw_guard_ready=False, boolean_raw_guard_ready=False, number_raw_guard_ready=False,
        actual_official_wire_verified=False,  boolean_Named_wire_verified=False, number_Named_wire_verified=False,  empty_string_Named_wire_verified=False,
        metadata_schema_validation_is_wire_packing_proof=False, adapter_noPIE_idle_gate=False, unique_external_pipeline_complete=False, packaged_runtime_verified=False)

def _required(expected,actual):
    return type(actual) is list and len(actual)==len(expected) and all(type(value) is str for value in actual) and set(actual)==set(expected)

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if (type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=1 or not callable(getattr(toolset_class,METHOD,None))):
        raise RuntimeError('Exact canonical AnimBP locomotion EventGraph driver Toolset1 is required')
    row=schema['tools'][0]
    if type(row) is not dict or row.get('name')!=TOOLSET+'.'+METHOD: raise RuntimeError('Only the exact original AnimBP locomotion EventGraph driver method is admitted')
    inputs=row.get('inputSchema');fields=inputs.get('properties') if type(inputs) is dict else None
    required=REQUIRED_BY_METHOD[METHOD]+EMPTY_STRING_FIELDS if reflected_carriers else REQUIRED_BY_METHOD[METHOD]
    if (type(inputs) is not dict or inputs.get('type')!='object' or type(fields) is not dict or set(fields)!=set(PUBLIC_FIELDS_BY_METHOD[METHOD]) or not _required(required,inputs.get('required')) or not reflected_carriers and inputs.get('additionalProperties') is not False):
        raise RuntimeError('Exact public27 and physical source/empty required fields differ')
    for field,kind in FIELD_KINDS_BY_METHOD[METHOD].items():
        item=fields[field]
        private=field in NULLABLE_FIELDS+INTEGER_FIELDS+NUMBER_FIELDS
        if type(item) is not dict or item.get('type')!=('string' if reflected_carriers and private else kind): raise RuntimeError('Original type/private carrier differs: '+field)
        if field in NULLABLE_FIELDS and not reflected_carriers and item.get('items')!=ITEMS_BY_FIELD[field]: raise RuntimeError('Original nullable list item schema differs: '+field)
        if field in REQUIRED_BY_METHOD[METHOD] or reflected_carriers and field in EMPTY_STRING_FIELDS:
            if 'default' in item: raise RuntimeError('Physical required/source default must be absent: '+field)
        else:
            expected=DEFAULTS_BY_METHOD[METHOD][field]
            if reflected_carriers and field in NULLABLE_FIELDS: expected='null'
            elif reflected_carriers and field in INTEGER_FIELDS+NUMBER_FIELDS: expected=str(expected)
            if field in NUMBER_FIELDS and not reflected_carriers:
                if 'default' not in item or type(item['default']) not in (int,float) or item['default']!=expected: raise RuntimeError('Original number default differs: '+field)
            elif 'default' not in item or type(item['default']) is not type(expected) or item['default']!=expected: raise RuntimeError('Original default differs: '+field)
    output = row.get('outputSchema'); outer = output.get('properties') if type(output) is dict else None
    if (type(output) is not dict or output.get('type') != 'object' or type(outer) is not dict or set(outer) != {'returnValue'}
            or not _required(('returnValue',), output.get('required'))):
        raise RuntimeError('Exact returnValue outer output envelope is required')
    result = outer['returnValue']; result_fields = result.get('properties') if type(result) is dict else None
    if (type(result) is not dict or result.get('type') != 'object' or type(result_fields) is not dict or set(result_fields) != set(RESULT_FIELDS)
            or not _required(RESULT_FIELDS, result.get('required'))
            or any(type(result_fields[key]) is not dict or result_fields[key].get('type') != ('boolean' if key == 'success' else 'string') for key in RESULT_FIELDS)):
        raise RuntimeError('Exact synchronous standard11 output fields/types are required')

    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,
        reflected_carrier_schema_verified=reflected_carriers,public_input_counts={METHOD:27},nullable_json_carrier_count=0,native_signature_json_carrier_count=1,
        integer_decimal_carrier_count=0,number_decimal_carrier_count=1,required_source_count=1,actual_official_wire_verified=False,
        boolean_Named_wire_verified=False,number_Named_wire_verified=False,empty_string_Named_wire_verified=False,
        metadata_schema_validation_is_wire_packing_proof=False,unique_external_pipeline_complete=False)

def validate_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class)


def validate_reflected_carrier_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class, reflected_carriers=True)


def validate_registered_metadata(class_schema, attachment_report, toolset_class):
    reflected = validate_reflected_carrier_schema(class_schema, toolset_class)
    if (type(attachment_report) is not dict or attachment_report.get('operation') not in ATTACHMENT_METHODS[:2]
            or not all(attachment_report.get(key) is True for key in ATTACHMENT_READY_FIELDS)
            or attachment_report.get('asset_mutation') is not False or attachment_report.get('metadata_mutation') is not False
            or attachment_report.get('actual_named_wire_verified') is not False
            or attachment_report.get('logical_schema_source') != 'actual_registered_wrapper_GetJsonSchema'
            or attachment_report.get('logical_schema_registry_identity_verified') is not True):
        raise RuntimeError('Exact ready owner receipt with actual registered AnimBP locomotion EventGraph driver wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered AnimBP locomotion EventGraph driver wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
