"""Original BlendSpace public17; private carriers do not change logical inputs."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import blendspace_pose_grid as api
TOOLSET = "cubeless_mcp_ue58.registration.blendspace_pose_grid_toolset.CubelessBlendSpacePoseGridTools"
METHOD = "sample_blendspace_runtime_pose_grid"
BINDINGS = (METHOD,)
PUBLIC_FIELDS_BY_METHOD = {METHOD: tuple(["skeletal_mesh", "blendspace_path", "blendspaces", "samples", "sample_bones", "sample_sockets", "actor_label", "sample_time", "settle_tick_count", "tick_count", "tick_delta_time", "include_axis_extremes", "allow_missing_bones", "refresh_bone_transforms", "cleanup", "prefer_pie_world", "require_pie_world"])}
DEFAULTS_BY_METHOD = {METHOD: {"blendspace_path": "", "blendspaces": None, "samples": None, "sample_bones": None, "sample_sockets": None, "actor_label": "MCP_BlendSpace_RuntimePoseGrid", "sample_time": 0.25, "settle_tick_count": 2, "tick_count": 1, "tick_delta_time": 0.03333333333333333, "include_axis_extremes": False, "allow_missing_bones": False, "refresh_bone_transforms": True, "cleanup": True, "prefer_pie_world": True, "require_pie_world": True}}
REQUIRED_BY_METHOD = {METHOD: tuple(["skeletal_mesh"])}
STRING_FIELDS = tuple(["skeletal_mesh", "blendspace_path", "actor_label"])
EMPTY_STRING_FIELDS = tuple(["blendspace_path"])
NULLABLE_FIELDS = tuple(["blendspaces", "samples", "sample_bones", "sample_sockets"])
BOOLEAN_FIELDS = tuple(["include_axis_extremes", "allow_missing_bones", "refresh_bone_transforms", "cleanup", "prefer_pie_world", "require_pie_world"])
INTEGER_FIELDS = tuple(["settle_tick_count", "tick_count"])
NUMBER_FIELDS = tuple(["sample_time", "tick_delta_time"])
FIELD_KINDS_BY_METHOD = {METHOD: {"skeletal_mesh": "string", "blendspace_path": "string", "blendspaces": ["array", "null"], "samples": ["array", "null"], "sample_bones": ["array", "null"], "sample_sockets": ["array", "null"], "actor_label": "string", "sample_time": "number", "settle_tick_count": "integer", "tick_count": "integer", "tick_delta_time": "number", "include_axis_extremes": "boolean", "allow_missing_bones": "boolean", "refresh_bone_transforms": "boolean", "cleanup": "boolean", "prefer_pie_world": "boolean", "require_pie_world": "boolean"}}
ITEMS_BY_FIELD = {"blendspaces": {}, "samples": {"type": "object"}, "sample_bones": {"type": "string"}, "sample_sockets": {"type": "string"}}
RESULT_FIELDS = ('success','operation_id','stage','error_code','error','changes_json','compile_json','save_json','validation_json','retry_json','data_json')
ATTACHMENT_LIBRARY = 'CubelessBlendSpacePoseGridAttachmentLibrary'
ATTACHMENT_METHODS = ('install_blendspace_pose_grid_toolset','verify_blendspace_pose_grid_toolset','remove_blendspace_pose_grid_toolset')
ATTACHMENT_READY_FIELDS = tuple(["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "ordinary_string3_schema_verified", "nullable4_schema_verified", "boolean6_schema_verified", "integer2_schema_verified", "number2_schema_verified", "pose_grid17_schema_verified", "required_source1_verified", "empty_string_defaults_verified"])

def binding_status(toolset_class):
    methods = api.binding_status()
    defined = {name: callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False, bindings_ready=set(methods)==set(BINDINGS) and all(value is True for value in methods.values()) and all(defined.values()),
        library=api.LIBRARY, methods=methods, definition_methods=defined, public_toolset=TOOLSET, public_tool_count=1,
        public_input_counts={METHOD:17}, physical_input_counts={METHOD:17}, hidden_input_counts={METHOD:17},
        nullable_json_carrier_count=4, native_signature_json_carrier_count=4, integer_decimal_carrier_count=2, number_decimal_carrier_count=2,
        schema_verified=False, default_verified=False, raw_guard_required=True, raw_guard_ready=False, nullable_raw_guard_ready=False, boolean_raw_guard_ready=False, integer_raw_guard_ready=False, number_raw_guard_ready=False,
        actual_official_wire_verified=False, nullable_Named_wire_verified=False, boolean_Named_wire_verified=False, integer_Named_wire_verified=False, number_Named_wire_verified=False, empty_string_Named_wire_verified=False,
        metadata_schema_validation_is_wire_packing_proof=False, adapter_noPIE_idle_gate=False, unique_external_pipeline_complete=False, packaged_runtime_verified=False)

def _required(expected,actual):
    return type(actual) is list and len(actual)==len(expected) and all(type(value) is str for value in actual) and set(actual)==set(expected)

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if (type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=1 or not callable(getattr(toolset_class,METHOD,None))):
        raise RuntimeError('Exact canonical BlendSpace Toolset1 is required')
    row=schema['tools'][0]
    if type(row) is not dict or row.get('name')!=TOOLSET+'.'+METHOD: raise RuntimeError('Only the exact original BlendSpace method is admitted')
    inputs=row.get('inputSchema');fields=inputs.get('properties') if type(inputs) is dict else None
    required=REQUIRED_BY_METHOD[METHOD]+EMPTY_STRING_FIELDS if reflected_carriers else REQUIRED_BY_METHOD[METHOD]
    if (type(inputs) is not dict or inputs.get('type')!='object' or type(fields) is not dict or set(fields)!=set(PUBLIC_FIELDS_BY_METHOD[METHOD]) or not _required(required,inputs.get('required')) or not reflected_carriers and inputs.get('additionalProperties') is not False):
        raise RuntimeError('Exact public17 and physical source/empty required fields differ')
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
            if 'default' not in item or type(item['default']) is not type(expected) or item['default']!=expected: raise RuntimeError('Original default differs: '+field)
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
        reflected_carrier_schema_verified=reflected_carriers,public_input_counts={METHOD:17},nullable_json_carrier_count=4,native_signature_json_carrier_count=4,
        integer_decimal_carrier_count=2,number_decimal_carrier_count=2,required_source_count=1,actual_official_wire_verified=False,
        nullable_Named_wire_verified=False,boolean_Named_wire_verified=False,integer_Named_wire_verified=False,number_Named_wire_verified=False,empty_string_Named_wire_verified=False,
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
        raise RuntimeError('Exact ready owner receipt with actual registered BlendSpace wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered BlendSpace wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
