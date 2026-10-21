"""Original AnimGraph9/15 IO, potential default graph creation qualified."""
import copy

TOOLSET = 'CubelessMCPUE58Editor.CubelessAnimGraphInspectionTools'
BINDINGS = ('inspect_anim_graph_node_settings', 'inspect_anim_graph_protected_topology')
PUBLIC_FIELDS_BY_METHOD = {'inspect_anim_graph_node_settings': ('blueprint_name', 'node_id', 'node_type', 'title_contains', 'include_pins', 'max_depth', 'graph_name', 'graph_id', 'graph_type'), 'inspect_anim_graph_protected_topology': ('blueprint_name', 'node_id', 'node_type', 'title_contains', 'include_pins', 'include_pose_pins', 'include_links', 'include_non_pose_links', 'include_settings', 'max_depth', 'max_nodes', 'max_links', 'graph_name', 'graph_id', 'graph_type')}
DEFAULTS_BY_METHOD = {'inspect_anim_graph_node_settings': {'node_id': '', 'node_type': '', 'title_contains': '', 'include_pins': True, 'max_depth': 4, 'graph_name': 'AnimGraph', 'graph_id': '', 'graph_type': 'function'}, 'inspect_anim_graph_protected_topology': {'node_id': '', 'node_type': '', 'title_contains': '', 'include_pins': True, 'include_pose_pins': True, 'include_links': True, 'include_non_pose_links': False, 'include_settings': False, 'max_depth': 3, 'max_nodes': 512, 'max_links': 2048, 'graph_name': 'AnimGraph', 'graph_id': '', 'graph_type': 'function'}}
REQUIRED_BY_METHOD = {'inspect_anim_graph_node_settings': ("blueprint_name",), 'inspect_anim_graph_protected_topology': ("blueprint_name",)}
FIELD_KINDS_BY_METHOD = {'inspect_anim_graph_node_settings': {'blueprint_name': 'string', 'node_id': 'string', 'node_type': 'string', 'title_contains': 'string', 'include_pins': 'boolean', 'max_depth': 'integer', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string'}, 'inspect_anim_graph_protected_topology': {'blueprint_name': 'string', 'node_id': 'string', 'node_type': 'string', 'title_contains': 'string', 'include_pins': 'boolean', 'include_pose_pins': 'boolean', 'include_links': 'boolean', 'include_non_pose_links': 'boolean', 'include_settings': 'boolean', 'max_depth': 'integer', 'max_nodes': 'integer', 'max_links': 'integer', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphInspectionTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessAnimGraphInspectionTools.inspect_anim_graph_node_settings', 'description': 'Original Settings9 preserves original case-insensitive FString node GUID/filter/optional pins and reflected authored FAnimNode properties with native depth clamp. Both names are potential writers: all-empty graph selectors can create the original EventGraph then fail AnimationGraph type admission. Native actual read/creation/dirty/partial flags and complete raw prefix precede errors. No wrapper filtering/clamping/reflection, extra compiler/save, rollback, replay or runtime pose/cold/full pipeline proof.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'node_id': {'type': 'string', 'default': ''}, 'node_type': {'type': 'string', 'default': ''}, 'title_contains': {'type': 'string', 'default': ''}, 'include_pins': {'type': 'boolean', 'default': True}, 'max_depth': {'type': 'integer', 'default': 4}, 'graph_name': {'type': 'string', 'default': 'AnimGraph'}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphInspectionTools.inspect_anim_graph_protected_topology', 'description': 'Original ProtectedTopology15 preserves case-insensitive node GUID/filter/pose/component-pose pins/preferred summaries/direction-normalized protected links, optional settings and native limits. Both names are potential writers: all-empty graph selectors can create the original EventGraph then fail AnimationGraph type admission. Native actual read/creation/dirty/partial flags and complete raw prefix precede errors. No wrapper filtering/clamping/reflection, extra compiler/save, rollback, replay or runtime pose/cold/full pipeline proof.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'node_id': {'type': 'string', 'default': ''}, 'node_type': {'type': 'string', 'default': ''}, 'title_contains': {'type': 'string', 'default': ''}, 'include_pins': {'type': 'boolean', 'default': True}, 'include_pose_pins': {'type': 'boolean', 'default': True}, 'include_links': {'type': 'boolean', 'default': True}, 'include_non_pose_links': {'type': 'boolean', 'default': False}, 'include_settings': {'type': 'boolean', 'default': False}, 'max_depth': {'type': 'integer', 'default': 3}, 'max_nodes': {'type': 'integer', 'default': 512}, 'max_links': {'type': 'integer', 'default': 2048}, 'graph_name': {'type': 'string', 'default': 'AnimGraph'}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}

def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)



def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_inspection as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 2,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "both_names_empty_selector_default_graph_creation_native_owned": True,
        "actual_read_or_creation_not_hardcoded": True,
        "actual_anim_graph_inspection_behavior_verified": False, "full_original_anim_graph_inspection_runtime_or_pipeline_verified": False}



def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 2):
        raise RuntimeError("Exact anim_graph_inspection raw2 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("AnimGraphInspection method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("Original anim_graph_inspection input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if type(item) is not dict or item != expected["inputSchema"]["properties"][field]:
                raise RuntimeError("Original anim_graph_inspection scalar type/default changed: " + field)
            if field in DEFAULTS_BY_METHOD[method] and type(item.get("default")) is not type(DEFAULTS_BY_METHOD[method][field]):
                raise RuntimeError("Original anim_graph_inspection default type changed: " + field)
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
                or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
            raise RuntimeError("Exact anim_graph_inspection operation outer envelope required")
        inner = outer["returnValue"]
        fields = inner.get("properties") if type(inner) is dict else None
        if (type(inner) is not dict or inner.get("type") != "object" or inner.get("additionalProperties") is not False
                or inner.get("required") != list(RESULT_FIELDS) or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)
                or any(type(fields[k]) is not dict or fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS)):
            raise RuntimeError("Exact standard anim_graph_inspection operation output11 required")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "original9_15_filter_flag_limit_graph_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_anim_graph_inspection_behavior_or_pipeline_verified": False}
