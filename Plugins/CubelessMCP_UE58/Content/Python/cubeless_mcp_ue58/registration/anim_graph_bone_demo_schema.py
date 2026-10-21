"""Original ModifyBone8 subset of the existing fixed AnimGraphAuthoring owner."""
import copy
TOOLSET = 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools'
BINDINGS = ('ensure_anim_graph_modify_bone_demo',)
PUBLIC_FIELDS_BY_METHOD = {'ensure_anim_graph_modify_bone_demo': ('blueprint_name', 'bone_name', 'rotation', 'graph_name', 'graph_id', 'graph_type', 'replace_existing', 'allow_non_sample')}
DEFAULTS_BY_METHOD = {'ensure_anim_graph_modify_bone_demo': {'bone_name': 'head', 'rotation': None, 'graph_name': 'AnimGraph', 'graph_id': '', 'graph_type': 'function', 'replace_existing': False, 'allow_non_sample': False}}
REQUIRED_BY_METHOD = {'ensure_anim_graph_modify_bone_demo': ('blueprint_name',)}
FIELD_KINDS_BY_METHOD = {'ensure_anim_graph_modify_bone_demo': {'blueprint_name': 'string', 'bone_name': 'string', 'rotation': 'any', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'replace_existing': 'boolean', 'allow_non_sample': 'boolean'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools.ensure_anim_graph_modify_bone_demo', 'description': 'Original ModifyBone demo8 preserves whole Any rotation, public None becomes [0,0,6]. Native owns four create/reuse lifecycles, unconditional Root XY write, bone-space additive rotation/default callback and four ordered local/component pose edges. Earlier created nodes, settings and broken links survive late refusal; legacy graph_changed and actual Root/current mutation truth remain distinct. No Python bone-existence guard, rollback, cleanup, writer replay, compiler/save or runtime/cold/full pipeline proof.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'bone_name': {'type': 'string', 'default': 'head'}, 'rotation': {'default': None}, 'graph_name': {'type': 'string', 'default': 'AnimGraph'}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}, 'replace_existing': {'type': 'boolean', 'default': False}, 'allow_non_sample': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}

def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_bone_demo as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY,
        "methods": methods, "public_toolset": TOOLSET, "public_tool_count": 1,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_rotation_None_default": [0, 0, 6], "original_public_input_count": 8,
        "Native_original_ordered_partial_chain_and_Root_position_authoritative": True,
        "actual_behavior_runtime_or_full_pipeline_verified": False}


def validate_schema(schema):
    if type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list:
        raise RuntimeError("Exact existing AnimGraphAuthoring owner schema required")
    method = BINDINGS[0]
    rows = [r for r in schema["tools"] if type(r) is dict and r.get("name") == TOOLSET + "." + method]
    if len(rows) != 1:
        raise RuntimeError("Exactly one original ModifyBone8 tool required")
    expected = _PUBLIC_SCHEMA["tools"][0]
    if rows[0].get("inputSchema") != expected["inputSchema"] or rows[0].get("outputSchema") != expected["outputSchema"]:
        raise RuntimeError("Original ModifyBone8 whole Any/defaults and standard11 IO differ")
    inputs = rows[0]["inputSchema"]
    output = rows[0]["outputSchema"]
    inner = output["properties"]["returnValue"]
    if (tuple(inputs["properties"]) != PUBLIC_FIELDS_BY_METHOD[method]
            or inputs.get("additionalProperties") is not False
            or output.get("additionalProperties") is not False
            or inner.get("additionalProperties") is not False):
        raise RuntimeError("Original ModifyBone8 ordered fields and exact bounded object flags differ")
    for field, value in DEFAULTS_BY_METHOD[method].items():
        actual = inputs["properties"][field].get("default")
        if type(actual) is not type(value) or actual != value:
            raise RuntimeError("Original ModifyBone8 default type/value differs: " + field)
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {method: 8}, "preparation_required": False}
