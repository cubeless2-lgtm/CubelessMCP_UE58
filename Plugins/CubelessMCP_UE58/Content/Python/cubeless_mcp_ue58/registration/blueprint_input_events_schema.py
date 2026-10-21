"""Distinct InputAxisEvent10/EnhancedInputAction7 native readiness and exact raw input/output schemas."""
import copy

TOOLSET = "CubelessMCPUE58Editor.CubelessBlueprintExpressionTools"
BINDINGS = ('add_blueprint_input_axis_event_node', 'add_blueprint_enhanced_input_action_node')
PUBLIC_FIELDS_BY_METHOD = {'add_blueprint_input_axis_event_node': ('blueprint_name', 'axis_name', 'node_position', 'consume_input', 'execute_when_paused', 'override_parent_binding', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_enhanced_input_action_node': ('blueprint_name', 'input_action', 'node_position', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}
DEFAULTS_BY_METHOD = {'add_blueprint_input_axis_event_node': {'node_position': None, 'consume_input': True, 'execute_when_paused': False, 'override_parent_binding': False, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}, 'add_blueprint_enhanced_input_action_node': {'node_position': None, 'graph_name': '', 'graph_id': '', 'graph_type': '', 'create_graph_if_missing': False}}
REQUIRED_BY_METHOD = {'add_blueprint_input_axis_event_node': ('blueprint_name', 'axis_name'), 'add_blueprint_enhanced_input_action_node': ('blueprint_name', 'input_action')}
BOOLEAN_FIELDS = ('consume_input', 'execute_when_paused', 'override_parent_binding', 'create_graph_if_missing')
DESCRIPTIONS_BY_METHOD = {'add_blueprint_input_axis_event_node': 'Original legacy InputAxisEvent constructor with explicit consume/paused/override flags. Empty or unmapped axis names remain admitted and may yield SDK compile warnings. Actual event delegate/value pins and partial source/graph/lifecycle receipt only; no mapping settings mutation, explicit compile/save, runtime input or full pipeline claim.', 'add_blueprint_enhanced_input_action_node': 'Original direct EnhancedInputAction event constructor with a literal InputAction reference and original dependency guard. Native resolves the graph before action load and preserves actual late partial prefixes, loaded action class/value-type/property/current-pin proof and duplicate creation. No UI spawner reuse, mapping context mutation, explicit compile/save, runtime input or full pipeline claim.'}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def public_rows():
    rows = []
    for method in BINDINGS:
        properties = {}
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = {} if field == "node_position" else {"type": "boolean" if field in BOOLEAN_FIELDS else "string"}
            if field in DEFAULTS_BY_METHOD[method]:
                item["default"] = copy.deepcopy(DEFAULTS_BY_METHOD[method][field])
            properties[field] = item
        rows.append({"name": TOOLSET + "." + method, "description": DESCRIPTIONS_BY_METHOD[method],
            "inputSchema": {"type": "object", "properties": properties, "required": list(REQUIRED_BY_METHOD[method]), "additionalProperties": False},
            "outputSchema": {"type": "object", "properties": {"returnValue": {"type": "object", "properties": {
                field: {"type": "boolean" if field == "success" else "string"} for field in RESULT_FIELDS},
                "required": list(RESULT_FIELDS), "additionalProperties": False}}, "required": ["returnValue"], "additionalProperties": False}})
    return rows


def validate_row(row, method):
    if method not in BINDINGS or type(row) is not dict or row.get("name") != TOOLSET + "." + method:
        raise RuntimeError("Exact original input-event name required")
    inputs = row.get("inputSchema")
    fields = inputs.get("properties") if type(inputs) is dict else None
    if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
            or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(fields) is not dict
            or tuple(fields) != PUBLIC_FIELDS_BY_METHOD[method]):
        raise RuntimeError("Original input-event fields/order/required envelope changed")
    expected = next(item for item in public_rows() if item["name"] == TOOLSET + "." + method)
    for field in PUBLIC_FIELDS_BY_METHOD[method]:
        item = fields[field]
        if type(item) is not dict or item != expected["inputSchema"]["properties"][field]:
            raise RuntimeError("Original input-event type/default/Any position changed: " + field)
        if field in DEFAULTS_BY_METHOD[method] and type(item.get("default")) is not type(DEFAULTS_BY_METHOD[method][field]):
            raise RuntimeError("Original input-event default type changed: " + field)
    output = row.get("outputSchema")
    outer = output.get("properties") if type(output) is dict else None
    if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
            or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
        raise RuntimeError("Exact standard operation outer envelope required")
    returned = outer["returnValue"]
    result_fields = returned.get("properties") if type(returned) is dict else None
    if (type(returned) is not dict or returned.get("type") != "object" or returned.get("additionalProperties") is not False
            or returned.get("required") != list(RESULT_FIELDS) or type(result_fields) is not dict or set(result_fields) != set(RESULT_FIELDS)
            or any(type(result_fields[k]) is not dict or result_fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS)):
        raise RuntimeError("Exact reflected operation output11 required")
    return True


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_input_events as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "preparation_required": False, "schema_verified": False,
        "actual_official_raw_wire_verified": False, "actual_node_construction_verified": False,
        "full_original_input_event_runtime_or_pipeline_verified": False, "public_tool_count": 2,
        "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS}}


def validate_schema(schema):
    from . import blueprint_expression_schema
    verified = blueprint_expression_schema.validate_schema(schema)
    return {"schema_verified": verified["schema_verified"], "validated_tool_names": list(BINDINGS),
        "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS},
        "original_public_defaults_verified": True, "public_position_Any_schema_verified": True,
        "preparation_required": False, "actual_official_raw_wire_verified": False,
        "actual_input_event_node_or_runtime_or_pipeline_verified": False}
