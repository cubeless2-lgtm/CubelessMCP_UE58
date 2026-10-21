"""Original Post Process AnimBP variant17; physical rotation text is private."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import postprocess_anim_demo_variant as api

TOOLSET = 'cubeless_mcp_ue58.registration.postprocess_anim_demo_variant_toolset.CubelessPostProcessAnimDemoVariantTools'
METHOD = 'ensure_postprocess_anim_demo_variant'
BINDINGS = (METHOD,)
PUBLIC_FIELDS_BY_METHOD = {METHOD: ("source_blueprint_name", "source_skeletal_mesh", "variant_name", "target_root", "target_blueprint_name", "target_skeletal_mesh", "bone_name", "rotation", "graph_name", "graph_id", "graph_type", "replace_existing", "overwrite_existing", "compile", "save", "dry_run", "allow_non_sample")}
DEFAULTS_BY_METHOD = {METHOD: {"variant_name": "Variant", "target_root": "/Game/_MCP_Sample/AnimStudy", "target_blueprint_name": "", "target_skeletal_mesh": "", "bone_name": "head", "rotation": None, "graph_name": "AnimGraph", "graph_id": "", "graph_type": "function", "replace_existing": True, "overwrite_existing": False, "compile": True, "save": True, "dry_run": False, "allow_non_sample": False}}
REQUIRED_BY_METHOD = {METHOD: ("source_blueprint_name", "source_skeletal_mesh")}
FIELD_KINDS_BY_METHOD = {METHOD: {"source_blueprint_name": "string", "source_skeletal_mesh": "string", "variant_name": "string", "target_root": "string", "target_blueprint_name": "string", "target_skeletal_mesh": "string", "bone_name": "string", "rotation": "any", "graph_name": "string", "graph_id": "string", "graph_type": "string", "replace_existing": "boolean", "overwrite_existing": "boolean", "compile": "boolean", "save": "boolean", "dry_run": "boolean", "allow_non_sample": "boolean"}}
STRING_FIELDS = ("source_blueprint_name", "source_skeletal_mesh", "variant_name", "target_root", "target_blueprint_name", "target_skeletal_mesh", "bone_name", "graph_name", "graph_id", "graph_type")
EMPTY_STRING_FIELDS = ("target_blueprint_name", "target_skeletal_mesh", "graph_id")
BOOLEAN_FIELDS = ("replace_existing", "overwrite_existing", "compile", "save", "dry_run", "allow_non_sample")
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
ATTACHMENT_LIBRARY = 'CubelessPostProcessAnimDemoVariantAttachmentLibrary'
ATTACHMENT_METHODS = ('install_postprocess_anim_demo_variant_toolset', 'verify_postprocess_anim_demo_variant_toolset', 'remove_postprocess_anim_demo_variant_toolset')
ATTACHMENT_READY_FIELDS = ("ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "ordinary_string10_schema_verified", "rotation_any1_schema_verified", "boolean6_schema_verified", "required_source2_schema_verified", "postprocess_variant17_schema_verified", "empty_string_defaults_verified")


def binding_status(toolset_class):
    methods = api.binding_status()
    defined = {name: callable(getattr(toolset_class, name, None)) for name in BINDINGS}
    return {'ready': False, 'bindings_ready': set(methods) == set(BINDINGS) and all(value is True for value in methods.values()) and all(defined.values()),
        'library': api.LIBRARY, 'methods': methods, 'definition_methods': defined, 'public_toolset': TOOLSET, 'public_tool_count': 1,
        'public_input_counts': {METHOD: 17}, 'physical_input_counts': {METHOD: 17}, 'hidden_input_counts': {METHOD: 17},
        'rotation_json_carrier_count': 1, 'native_signature_json_carrier_count': 3, 'public_required_sources': ('source_blueprint_name', 'source_skeletal_mesh'),
        'schema_verified': False, 'default_verified': False, 'raw_guard_required': True, 'raw_guard_ready': False,
        'rotation_raw_guard_ready': False, 'boolean_raw_guard_ready': False, 'actual_official_wire_verified': False,
        'rotation_Named_wire_verified': False, 'boolean_Named_wire_verified': False, 'empty_string_Named_wire_verified': False,
        'metadata_schema_validation_is_wire_packing_proof': False, 'unique_external_pipeline_complete': False, 'packaged_runtime_verified': False}


def _required(expected, actual):
    return type(actual) is list and len(actual) == len(expected) and all(type(value) is str for value in actual) and set(actual) == set(expected)


def _validate_schema(schema, toolset_class, reflected_carriers=False):
    if (type(schema) is not dict or schema.get('name') != TOOLSET or type(schema.get('tools')) is not list
            or len(schema['tools']) != 1 or not callable(getattr(toolset_class, METHOD, None))):
        raise RuntimeError('Exact canonical Post Process variant Toolset1 is required')
    row = schema['tools'][0]
    if type(row) is not dict or row.get('name') != TOOLSET + '.' + METHOD:
        raise RuntimeError('Only the original Post Process variant method is admitted')
    inputs = row.get('inputSchema'); fields = inputs.get('properties') if type(inputs) is dict else None
    required = REQUIRED_BY_METHOD[METHOD] + EMPTY_STRING_FIELDS if reflected_carriers else REQUIRED_BY_METHOD[METHOD]
    if (type(inputs) is not dict or inputs.get('type') != 'object' or type(fields) is not dict
            or set(fields) != set(PUBLIC_FIELDS_BY_METHOD[METHOD]) or not _required(required, inputs.get('required'))
            or not reflected_carriers and inputs.get('additionalProperties') is not False):
        raise RuntimeError('Logical17 required source2 and physical-required source2/empty3 differ')
    for field, kind in FIELD_KINDS_BY_METHOD[METHOD].items():
        item = fields[field]
        if type(item) is not dict:
            raise RuntimeError('Input property schema must be an object: ' + field)
        if field == 'rotation' and not reflected_carriers:
            if 'type' in item:
                raise RuntimeError('Original unannotated rotation remains whole Any JSON; it is not restricted to an array')
        elif item.get('type') != ('string' if field == 'rotation' else kind):
            raise RuntimeError('Original logical type or private rotation carrier differs: ' + field)
        if field in REQUIRED_BY_METHOD[METHOD] or reflected_carriers and field in EMPTY_STRING_FIELDS:
            if 'default' in item:
                raise RuntimeError('Required sources and SDK physical empty-string defaults must remain absent: ' + field)
        else:
            expected = 'null' if reflected_carriers and field == 'rotation' else DEFAULTS_BY_METHOD[METHOD][field]
            if 'default' not in item or type(item['default']) is not type(expected) or item['default'] != expected:
                raise RuntimeError('Original Post Process variant default differs: ' + field)
    output = row.get('outputSchema'); outer = output.get('properties') if type(output) is dict else None
    if (type(output) is not dict or output.get('type') != 'object' or type(outer) is not dict or set(outer) != {'returnValue'}
            or not _required(('returnValue',), output.get('required'))):
        raise RuntimeError('Exact returnValue outer output envelope is required')
    result = outer['returnValue']; result_fields = result.get('properties') if type(result) is dict else None
    if (type(result) is not dict or result.get('type') != 'object' or type(result_fields) is not dict or set(result_fields) != set(RESULT_FIELDS)
            or not _required(RESULT_FIELDS, result.get('required'))
            or any(type(result_fields[key]) is not dict or result_fields[key].get('type') != ('boolean' if key == 'success' else 'string') for key in RESULT_FIELDS)):
        raise RuntimeError('Exact synchronous standard11 output fields/types are required')
    return {'schema_verified': not reflected_carriers, 'default_verified': not reflected_carriers,
        'logical_adapter_schema_verified': not reflected_carriers, 'reflected_carrier_schema_verified': reflected_carriers,
        'public_input_counts': {METHOD: 17}, 'rotation_json_carrier_count': 1, 'native_signature_json_carrier_count': 3,
        'required_source_count': 2, 'actual_official_wire_verified': False, 'rotation_Named_wire_verified': False,
        'boolean_Named_wire_verified': False, 'empty_string_Named_wire_verified': False,
        'metadata_schema_validation_is_wire_packing_proof': False, 'unique_external_pipeline_complete': False}


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
        raise RuntimeError('Exact ready owner receipt with actual registered Post Process wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered Post Process wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
