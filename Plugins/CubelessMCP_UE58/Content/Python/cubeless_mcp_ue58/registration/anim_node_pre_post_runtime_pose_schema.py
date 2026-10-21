"""Original AnimGraph node pre/post public35; logical types stay public while carriers remain private."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import anim_node_pre_post_runtime_pose as api
TOOLSET = "cubeless_mcp_ue58.registration.anim_node_pre_post_runtime_pose_toolset.CubelessAnimNodePrePostRuntimePoseTools"
METHOD = "sample_anim_node_pre_post_runtime_pose"
BINDINGS = (METHOD,)
PUBLIC_FIELDS_BY_METHOD = {METHOD: ["blueprint_name", "anim_blueprint", "graph_name", "graph_id", "graph_type", "node_id", "node_type", "title_contains", "sample_bones", "sample_sockets", "mode", "skeletal_mesh", "temp_root", "cleanup", "overwrite_existing_temp_assets", "run_id", "actor_label", "actor_name", "actor_path", "component_name", "anim_instance_source", "prefer_pie_world", "require_pie_world", "tick_count", "tick_delta_time", "settle_tick_count", "refresh_bone_transforms", "allow_missing_bones", "pose_link_max_depth", "input_pose_mode", "input_pose_field_path", "input_pose_index", "include_pins", "max_depth", "dry_run"]}
DEFAULTS_BY_METHOD = {METHOD: {"blueprint_name": "", "anim_blueprint": "", "graph_name": "AnimGraph", "graph_id": "", "graph_type": "function", "node_id": "", "node_type": "", "title_contains": "", "sample_bones": None, "sample_sockets": None, "mode": "active_component_tick_delta", "skeletal_mesh": "", "temp_root": "/Game/_MCP_Temp/AnimNodePrePost", "cleanup": True, "overwrite_existing_temp_assets": True, "run_id": "", "actor_label": "", "actor_name": "", "actor_path": "", "component_name": "", "anim_instance_source": "main", "prefer_pie_world": True, "require_pie_world": False, "tick_count": 1, "tick_delta_time": 0.03333333333333333, "settle_tick_count": 0, "refresh_bone_transforms": True, "allow_missing_bones": False, "pose_link_max_depth": 4, "input_pose_mode": "first", "input_pose_field_path": "", "input_pose_index": -1, "include_pins": True, "max_depth": 3, "dry_run": True}}
REQUIRED_BY_METHOD = {METHOD: ()}
STRING_FIELDS = tuple(["blueprint_name", "anim_blueprint", "graph_name", "graph_id", "graph_type", "node_id", "node_type", "title_contains", "mode", "skeletal_mesh", "temp_root", "run_id", "actor_label", "actor_name", "actor_path", "component_name", "anim_instance_source", "input_pose_mode", "input_pose_field_path"])
EMPTY_STRING_FIELDS = tuple(["blueprint_name", "anim_blueprint", "graph_id", "node_id", "node_type", "title_contains", "skeletal_mesh", "run_id", "actor_label", "actor_name", "actor_path", "component_name", "input_pose_field_path"])
NULLABLE_FIELDS = tuple(["sample_bones", "sample_sockets"])
BOOLEAN_FIELDS = tuple(["cleanup", "overwrite_existing_temp_assets", "prefer_pie_world", "require_pie_world", "refresh_bone_transforms", "allow_missing_bones", "include_pins", "dry_run"])
INTEGER_FIELDS = tuple(["tick_count", "settle_tick_count", "pose_link_max_depth", "input_pose_index", "max_depth"])
NUMBER_FIELDS = tuple(["tick_delta_time"])
FIELD_KINDS_BY_METHOD = {METHOD: {"blueprint_name": "string", "anim_blueprint": "string", "graph_name": "string", "graph_id": "string", "graph_type": "string", "node_id": "string", "node_type": "string", "title_contains": "string", "sample_bones": ["array", "null"], "sample_sockets": ["array", "null"], "mode": "string", "skeletal_mesh": "string", "temp_root": "string", "cleanup": "boolean", "overwrite_existing_temp_assets": "boolean", "run_id": "string", "actor_label": "string", "actor_name": "string", "actor_path": "string", "component_name": "string", "anim_instance_source": "string", "prefer_pie_world": "boolean", "require_pie_world": "boolean", "tick_count": "integer", "tick_delta_time": "number", "settle_tick_count": "integer", "refresh_bone_transforms": "boolean", "allow_missing_bones": "boolean", "pose_link_max_depth": "integer", "input_pose_mode": "string", "input_pose_field_path": "string", "input_pose_index": "integer", "include_pins": "boolean", "max_depth": "integer", "dry_run": "boolean"}}
ITEMS_BY_FIELD = {field: {'type': 'string'} for field in NULLABLE_FIELDS}
RESULT_FIELDS = ('success','operation_id','stage','error_code','error','changes_json','compile_json','save_json','validation_json','retry_json','data_json')
ATTACHMENT_LIBRARY = "CubelessAnimNodePrePostRuntimePoseAttachmentLibrary"
ATTACHMENT_METHODS = ('install_anim_node_pre_post_runtime_pose_toolset','verify_anim_node_pre_post_runtime_pose_toolset','remove_anim_node_pre_post_runtime_pose_toolset')
ATTACHMENT_READY_FIELDS = tuple(["ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "output11_schema_preserved", "ordinary_string19_schema_verified", "nullable2_schema_verified", "boolean8_schema_verified", "integer5_schema_verified", "number1_schema_verified", "node_prepost35_schema_verified", "empty_string_defaults_verified"])

def binding_status(toolset_class):
    methods = api.binding_status()
    defined = {name: callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return dict(ready=False, bindings_ready=set(methods)==set(BINDINGS) and all(value is True for value in methods.values()) and all(defined.values()),
        library=api.LIBRARY, methods=methods, definition_methods=defined, public_toolset=TOOLSET, public_tool_count=1,
        public_input_counts={METHOD:35}, physical_input_counts={METHOD:35}, hidden_input_counts={METHOD:36},
        nullable_json_carrier_count=2, native_signature_json_carrier_count=3, integer_decimal_carrier_count=5, number_decimal_carrier_count=1,
        schema_verified=False, default_verified=False, raw_guard_required=True, raw_guard_ready=False, nullable_raw_guard_ready=False, boolean_raw_guard_ready=False, integer_raw_guard_ready=False, number_raw_guard_ready=False,
        actual_official_wire_verified=False, nullable_Named_wire_verified=False, boolean_Named_wire_verified=False, integer_Named_wire_verified=False, number_Named_wire_verified=False, empty_string_Named_wire_verified=False,
        metadata_schema_validation_is_wire_packing_proof=False, adapter_noPIE_idle_gate=False, unique_external_pipeline_complete=False, packaged_runtime_verified=False)

def _required(expected,actual):
    return type(actual) is list and len(actual)==len(expected) and all(type(value) is str for value in actual) and set(actual)==set(expected)

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if (type(schema) is not dict or schema.get('name')!=TOOLSET or type(schema.get('tools')) is not list or len(schema['tools'])!=1 or not callable(getattr(toolset_class,METHOD,None))):
        raise RuntimeError('Exact canonical AnimGraph node pre/post probe Toolset1 is required')
    row=schema['tools'][0]
    if type(row) is not dict or row.get('name')!=TOOLSET+'.'+METHOD: raise RuntimeError('Only the exact original AnimGraph node pre/post probe method is admitted')
    inputs=row.get('inputSchema');fields=inputs.get('properties') if type(inputs) is dict else None
    required=REQUIRED_BY_METHOD[METHOD]+EMPTY_STRING_FIELDS if reflected_carriers else REQUIRED_BY_METHOD[METHOD]
    if (type(inputs) is not dict or inputs.get('type')!='object' or type(fields) is not dict or set(fields)!=set(PUBLIC_FIELDS_BY_METHOD[METHOD]) or not _required(required,inputs.get('required')) or not reflected_carriers and inputs.get('additionalProperties') is not False):
        raise RuntimeError('Exact public35 and physical empty required fields differ')
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
        reflected_carrier_schema_verified=reflected_carriers,public_input_counts={METHOD:35},nullable_json_carrier_count=2,native_signature_json_carrier_count=3,
        integer_decimal_carrier_count=5,number_decimal_carrier_count=1,required_source_count=0,actual_official_wire_verified=False,
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
        raise RuntimeError('Exact ready owner receipt with actual registered AnimGraph node pre/post probe wrapper schema is required')
    logical_text = attachment_report.get('logical_schema_json')
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError('Actual registered AnimGraph node pre/post probe wrapper schema is missing')
    logical = validate_schema(json.loads(logical_text), toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope='fresh_FFunctionLibraryToolset_not_registered_handler',
        logical_schema_source='actual_registered_wrapper_GetJsonSchema', logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text,
        reflected_schema_validation=reflected, logical_schema_validation=logical)
