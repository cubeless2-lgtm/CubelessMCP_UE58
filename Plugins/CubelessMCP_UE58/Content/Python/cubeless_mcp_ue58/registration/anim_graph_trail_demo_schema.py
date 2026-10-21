"""Original Trail19 subset of existing fixed AnimGraphAuthoring owner."""
import copy
TOOLSET = 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools'
BINDINGS = ('ensure_anim_graph_trail_demo',)
PUBLIC_FIELDS_BY_METHOD = {'ensure_anim_graph_trail_demo': ('blueprint_name', 'trail_bone', 'base_joint', 'chain_length', 'chain_bone_axis', 'alpha', 'fake_velocity', 'graph_name', 'graph_id', 'graph_type', 'replace_existing', 'allow_non_sample', 'invert_chain_bone_axis', 'reorient_parent_to_child', 'actor_space_fake_velocity', 'relaxation_speed_scale', 'limit_stretch', 'stretch_limit', 'max_delta_time')}
DEFAULTS_BY_METHOD = {'ensure_anim_graph_trail_demo': {'trail_bone': 'VB VBHead', 'base_joint': 'head', 'chain_length': 2, 'chain_bone_axis': 'X', 'alpha': 1.0, 'fake_velocity': None, 'graph_name': 'AnimGraph', 'graph_id': '', 'graph_type': 'function', 'replace_existing': False, 'allow_non_sample': False, 'invert_chain_bone_axis': False, 'reorient_parent_to_child': True, 'actor_space_fake_velocity': False, 'relaxation_speed_scale': 1.0, 'limit_stretch': False, 'stretch_limit': 0.0, 'max_delta_time': 0.0}}
REQUIRED_BY_METHOD = {'ensure_anim_graph_trail_demo': ('blueprint_name',)}
FIELD_KINDS_BY_METHOD = {'ensure_anim_graph_trail_demo': {'blueprint_name': 'string', 'trail_bone': 'string', 'base_joint': 'string', 'chain_length': 'integer', 'chain_bone_axis': 'string', 'alpha': 'number', 'fake_velocity': 'any', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'replace_existing': 'boolean', 'allow_non_sample': 'boolean', 'invert_chain_bone_axis': 'boolean', 'reorient_parent_to_child': 'boolean', 'actor_space_fake_velocity': 'boolean', 'relaxation_speed_scale': 'number', 'limit_stretch': 'boolean', 'stretch_limit': 'number', 'max_delta_time': 'number'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools.ensure_anim_graph_trail_demo', 'description': 'Original Trail demo19 preserves whole Any fake_velocity; public None becomes [0,0,0]. Four scalar numbers and chain integer retain their values without Python clamp or conversion; Native owns float32 projection, original minimum2, axis/name handling, chain arrays and loaded unsafe SDK health admission. Four ordered local/component pose edges, Root XY writes and earlier nodes/settings/defaults/broken links retain full actual failure prefix; legacy graph_changed is distinct. No consumer bone/shape guard, cleanup, rollback, writer replay, compiler/save, runtime Trail/preview/cold/full-pipeline proof.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'trail_bone': {'type': 'string', 'default': 'VB VBHead'}, 'base_joint': {'type': 'string', 'default': 'head'}, 'chain_length': {'type': 'integer', 'default': 2}, 'chain_bone_axis': {'type': 'string', 'default': 'X'}, 'alpha': {'type': 'number', 'default': 1.0}, 'fake_velocity': {'default': None}, 'graph_name': {'type': 'string', 'default': 'AnimGraph'}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}, 'replace_existing': {'type': 'boolean', 'default': False}, 'allow_non_sample': {'type': 'boolean', 'default': False}, 'invert_chain_bone_axis': {'type': 'boolean', 'default': False}, 'reorient_parent_to_child': {'type': 'boolean', 'default': True}, 'actor_space_fake_velocity': {'type': 'boolean', 'default': False}, 'relaxation_speed_scale': {'type': 'number', 'default': 1.0}, 'limit_stretch': {'type': 'boolean', 'default': False}, 'stretch_limit': {'type': 'number', 'default': 0.0}, 'max_delta_time': {'type': 'number', 'default': 0.0}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}

def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_trail_demo as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY,
        "methods": methods, "public_toolset": TOOLSET, "public_tool_count": 1,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_fake_velocity_None_default": [0, 0, 0], "original_public_input_count": 19,
        "Native_original_chain_fields_and_loaded_SDK_health_authoritative": True,
        "actual_behavior_runtime_or_full_pipeline_verified": False}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 1):
        raise RuntimeError("Exact existing AnimGraphAuthoring owner projected one-tool schema required")
    method = BINDINGS[0]
    row = schema["tools"][0]
    expected = _PUBLIC_SCHEMA["tools"][0]
    if (type(row) is not dict or row.get("name") != TOOLSET + "." + method
            or row.get("inputSchema") != expected["inputSchema"] or row.get("outputSchema") != expected["outputSchema"]):
        raise RuntimeError("Original Trail19 whole Any/defaults and standard11 IO differ")
    inputs = row["inputSchema"]
    output = row["outputSchema"]
    inner = output["properties"]["returnValue"]
    if (tuple(inputs["properties"]) != PUBLIC_FIELDS_BY_METHOD[method]
            or inputs.get("additionalProperties") is not False
            or output.get("additionalProperties") is not False
            or inner.get("additionalProperties") is not False):
        raise RuntimeError("Original Trail19 ordered fields and exact object flags differ")
    for field, value in DEFAULTS_BY_METHOD[method].items():
        actual = inputs["properties"][field].get("default")
        if type(actual) is not type(value) or actual != value:
            raise RuntimeError("Original Trail19 default type/value differs: " + field)
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {method: 19}, "preparation_required": False}
