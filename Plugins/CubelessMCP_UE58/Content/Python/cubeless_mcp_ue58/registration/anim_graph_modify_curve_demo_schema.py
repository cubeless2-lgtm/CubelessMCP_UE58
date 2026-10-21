"""Original ModifyCurve9 subset of the existing fixed AnimGraphAuthoring owner."""
import copy
TOOLSET = 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools'
BINDINGS = ('ensure_anim_graph_modify_curve_demo',)
PUBLIC_FIELDS_BY_METHOD = {'ensure_anim_graph_modify_curve_demo': ('blueprint_name', 'curve_values', 'alpha', 'apply_mode', 'graph_name', 'graph_id', 'graph_type', 'replace_existing', 'allow_non_sample')}
DEFAULTS_BY_METHOD = {'ensure_anim_graph_modify_curve_demo': {'curve_values': None, 'alpha': 1.0, 'apply_mode': 'Add', 'graph_name': 'AnimGraph', 'graph_id': '', 'graph_type': 'function', 'replace_existing': False, 'allow_non_sample': False}}
REQUIRED_BY_METHOD = {'ensure_anim_graph_modify_curve_demo': ('blueprint_name',)}
FIELD_KINDS_BY_METHOD = {'ensure_anim_graph_modify_curve_demo': {'blueprint_name': 'string', 'curve_values': 'nullable_number_map', 'alpha': 'number', 'apply_mode': 'string', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'replace_existing': 'boolean', 'allow_non_sample': 'boolean'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools.ensure_anim_graph_modify_curve_demo', 'description': 'Original ModifyCurve demo9 preserves nullable numeric curve map and scalar alpha without Python coercion or clamp. Public None expands IK_blend_interact=1.0 and IKBlend_l=1.0; explicit empty map reaches original Native fallback unchanged. Native owns FName entry rules/sort, float32 alpha projection, unrestricted apply-mode normalization, dynamic arrays/map/pin reconstruction and two ordered local-pose edges. Root XY writes and earlier created nodes/settings/defaults/broken links survive late refusal; full actual prefix is authoritative. Structural changes may regenerate skeleton/classes. No Python curve-existence guard, rollback, retry, compiler/save or runtime/cold/full-pipeline proof.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'curve_values': {'type': ['object', 'null'], 'additionalProperties': {'type': 'number'}, 'default': None}, 'alpha': {'type': 'number', 'default': 1.0}, 'apply_mode': {'type': 'string', 'default': 'Add'}, 'graph_name': {'type': 'string', 'default': 'AnimGraph'}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}, 'replace_existing': {'type': 'boolean', 'default': False}, 'allow_non_sample': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}

def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_modify_curve_demo as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY,
        "methods": methods, "public_toolset": TOOLSET, "public_tool_count": 1,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_curve_values_None_default": {"IK_blend_interact": 1.0, "IKBlend_l": 1.0},
        "original_public_input_count": 9, "Native_original_array_map_pin_and_partial_chain_authoritative": True,
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
        raise RuntimeError("Original ModifyCurve9 nullable numeric map/defaults and standard11 IO differ")
    inputs = row["inputSchema"]
    output = row["outputSchema"]
    inner = output["properties"]["returnValue"]
    if (tuple(inputs["properties"]) != PUBLIC_FIELDS_BY_METHOD[method]
            or inputs.get("additionalProperties") is not False
            or output.get("additionalProperties") is not False
            or inner.get("additionalProperties") is not False):
        raise RuntimeError("Original ModifyCurve9 ordered fields and exact object flags differ")
    for field, value in DEFAULTS_BY_METHOD[method].items():
        actual = inputs["properties"][field].get("default")
        if type(actual) is not type(value) or actual != value:
            raise RuntimeError("Original ModifyCurve9 default type/value differs: " + field)
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {method: 9}, "preparation_required": False}
