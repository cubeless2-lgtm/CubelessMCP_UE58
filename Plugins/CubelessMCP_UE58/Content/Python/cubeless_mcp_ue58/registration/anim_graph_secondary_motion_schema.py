"""Original SecondaryMotion19 subset of the fixed AnimGraphAuthoring owner."""
import copy
TOOLSET = 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools'
BINDINGS = ('ensure_anim_graph_secondary_motion_chain',)
PUBLIC_FIELDS_BY_METHOD = {'ensure_anim_graph_secondary_motion_chain': ('blueprint_name', 'dynamics_type', 'root_bone', 'chain_end_bone', 'alpha', 'gravity', 'angular_limit_degrees', 'body_extents', 'linear_damping', 'angular_damping', 'kawaii_damping', 'kawaii_stiffness', 'graph_name', 'graph_id', 'graph_type', 'replace_existing', 'allow_non_sample', 'compile', 'save')}
DEFAULTS_BY_METHOD = {'ensure_anim_graph_secondary_motion_chain': {'alpha': 1.0, 'gravity': None, 'angular_limit_degrees': 60.0, 'body_extents': None, 'linear_damping': 0.75, 'angular_damping': 0.75, 'kawaii_damping': 0.08, 'kawaii_stiffness': 0.03, 'graph_name': 'AnimGraph', 'graph_id': '', 'graph_type': 'function', 'replace_existing': True, 'allow_non_sample': False, 'compile': True, 'save': True}}
REQUIRED_BY_METHOD = {'ensure_anim_graph_secondary_motion_chain': ('blueprint_name', 'dynamics_type', 'root_bone', 'chain_end_bone')}
FIELD_KINDS_BY_METHOD = {'ensure_anim_graph_secondary_motion_chain': {'blueprint_name': 'string', 'dynamics_type': 'string', 'root_bone': 'string', 'chain_end_bone': 'string', 'alpha': 'number', 'gravity': 'nullable_number_list', 'angular_limit_degrees': 'number', 'body_extents': 'nullable_number_list', 'linear_damping': 'number', 'angular_damping': 'number', 'kawaii_damping': 'number', 'kawaii_stiffness': 'number', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'replace_existing': 'boolean', 'allow_non_sample': 'boolean', 'compile': 'boolean', 'save': 'boolean'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools.ensure_anim_graph_secondary_motion_chain', 'description': 'Original SecondaryMotion19 preserves required names, strip/lower dynamics aliases and nullable numeric lists; None becomes gravity [0.0,0.0,-980.0] and extents [5.0,5.0,30.0]. Six numbers retain original values without Python conversion/clamp. Native owns bone/chain admission, four reciprocal pose edges, AnimDynamics constraints or optional Kawaii settings, compilation followed by fresh GUID/current semantic proof and requested SaveLoadedAsset(false). Earlier mutation/compiler/save prefixes are retained before callback/error; no consumer second compiler/save, rollback/replay, optional-plugin availability assumption, runtime motion, cold or full-pipeline proof.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'dynamics_type': {'type': 'string'}, 'root_bone': {'type': 'string'}, 'chain_end_bone': {'type': 'string'}, 'alpha': {'type': 'number', 'default': 1.0}, 'gravity': {'type': ['array', 'null'], 'items': {'type': 'number'}, 'default': None}, 'angular_limit_degrees': {'type': 'number', 'default': 60.0}, 'body_extents': {'type': ['array', 'null'], 'items': {'type': 'number'}, 'default': None}, 'linear_damping': {'type': 'number', 'default': 0.75}, 'angular_damping': {'type': 'number', 'default': 0.75}, 'kawaii_damping': {'type': 'number', 'default': 0.08}, 'kawaii_stiffness': {'type': 'number', 'default': 0.03}, 'graph_name': {'type': 'string', 'default': 'AnimGraph'}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}, 'replace_existing': {'type': 'boolean', 'default': True}, 'allow_non_sample': {'type': 'boolean', 'default': False}, 'compile': {'type': 'boolean', 'default': True}, 'save': {'type': 'boolean', 'default': True}}, 'required': ['blueprint_name', 'dynamics_type', 'root_bone', 'chain_end_bone'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}

def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_secondary_motion as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY,
        "methods": methods, "public_toolset": TOOLSET, "public_tool_count": 1,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_gravity_None_default": [0.0, 0.0, -980.0], "original_body_extents_None_default": [5.0, 5.0, 30.0],
        "original_public_input_count": 19, "original_dynamics_strip_lower_aliases": True,
        "Native_requested_compiler_current_proof_and_save_authoritative": True,
        "optional_Kawaii_branch_retained_availability_verified": False,
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
        raise RuntimeError("Original SecondaryMotion19 typed-list/defaults and standard11 IO differ")
    inputs = row["inputSchema"]
    output = row["outputSchema"]
    inner = output["properties"]["returnValue"]
    if (tuple(inputs["properties"]) != PUBLIC_FIELDS_BY_METHOD[method]
            or inputs.get("additionalProperties") is not False
            or output.get("additionalProperties") is not False
            or inner.get("additionalProperties") is not False):
        raise RuntimeError("Original SecondaryMotion19 ordered fields and exact object flags differ")
    for field, value in DEFAULTS_BY_METHOD[method].items():
        actual = inputs["properties"][field].get("default")
        if type(actual) is not type(value) or actual != value:
            raise RuntimeError("Original SecondaryMotion19 default type/value differs: " + field)
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {method: 19}, "preparation_required": False}
