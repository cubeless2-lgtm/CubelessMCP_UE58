"""Pure exact public raw5 schema; each native library readiness is a separate gate."""
import copy

TOOLSET = "CubelessMCPUE58Editor.CubelessBlueprintSelectionTools"
CLASS = "CubelessBlueprintSelectionTools"
SELECTION_BINDINGS = ("add_blueprint_select_node", "add_blueprint_switch_enum_node")
BINDINGS = SELECTION_BINDINGS + ('add_blueprint_enum_literal_node', 'add_blueprint_is_valid_node', 'add_blueprint_switch_int_node')
PUBLIC_FIELDS_BY_METHOD = {
    BINDINGS[0]: ("blueprint_name", "value_type", "node_position", "graph_name", "graph_id", "graph_type", "create_graph_if_missing"),
    BINDINGS[1]: ("blueprint_name", "enum_type", "has_default_pin", "node_position", "graph_name", "graph_id", "graph_type", "create_graph_if_missing")}
DEFAULTS_BY_METHOD = {
    BINDINGS[0]: {"value_type": "int", "node_position": None, "graph_name": "", "graph_id": "", "graph_type": "", "create_graph_if_missing": False},
    BINDINGS[1]: {"has_default_pin": False, "node_position": None, "graph_name": "", "graph_id": "", "graph_type": "", "create_graph_if_missing": False}}
REQUIRED_BY_METHOD = {BINDINGS[0]: ("blueprint_name",), BINDINGS[1]: ("blueprint_name", "enum_type")}
from . import blueprint_branching_schema as _branching_schema
PUBLIC_FIELDS_BY_METHOD.update(_branching_schema.PUBLIC_FIELDS_BY_METHOD)
DEFAULTS_BY_METHOD.update(_branching_schema.DEFAULTS_BY_METHOD)
REQUIRED_BY_METHOD.update(_branching_schema.REQUIRED_BY_METHOD)
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")
MAX_ARGUMENT_BYTES = 131072
MAX_RESULT_BYTES = 16 * 1024 * 1024


def public_schema():
    rows = []
    for method in BINDINGS:
        properties = {}
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            schema = (_branching_schema.field_schema(field) if method in _branching_schema.BINDINGS else
                {} if field == "node_position" else {"type": "boolean" if field in ("has_default_pin", "create_graph_if_missing") else "string"})
            if field in DEFAULTS_BY_METHOD[method]:
                schema["default"] = copy.deepcopy(DEFAULTS_BY_METHOD[method][field])
            properties[field] = schema
        rows.append({"name": TOOLSET + "." + method,
            "description": ("Original typed selection helper; finite public Engine node creation, no explicit compile/save or replay."
                if method in SELECTION_BINDINGS else 'Original enum literal, pure IsValid object/class and integer-switch helpers; actual Engine nodes/defaults/cases and partial cleanup prefix, no explicit compile/save, runtime evaluation or replay.'),
            "inputSchema": {"type": "object", "properties": properties, "required": list(REQUIRED_BY_METHOD[method]), "additionalProperties": False},
            "outputSchema": {"type": "object", "properties": {"returnValue": {"type": "object", "properties": {
                field: {"type": "boolean" if field == "success" else "string"} for field in RESULT_FIELDS},
                "required": list(RESULT_FIELDS), "additionalProperties": False}}, "required": ["returnValue"], "additionalProperties": False}})
    return {"name": TOOLSET, "version": "1.0", "tools": rows}


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_selection as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(SELECTION_BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "preparation_required": False, "reflected_Graph_class_modified": False,
        "schema_verified": False, "actual_official_raw_wire_verified": False, "actual_node_construction_verified": False,
        "full_original_runtime_or_mixed_pipeline_verified": False, "public_tool_count": 2, "public_input_counts": {BINDINGS[0]: 7, BINDINGS[1]: 8}}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 5):
        raise RuntimeError("Exact actual selection raw5 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("Selection method name missing or duplicated: " + method)
        inputs = rows[0].get("inputSchema")
        fields = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or type(fields) is not dict or tuple(fields) != PUBLIC_FIELDS_BY_METHOD[method]
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method])):
            raise RuntimeError("Original selection input set/order changed: " + method)
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = fields[field]
            if type(item) is not dict:
                raise RuntimeError("Selection field schema missing")
            if field in ("node_position", "value"):
                if any(k in item for k in ("type", "anyOf", "oneOf", "allOf", "enum", "items", "$ref")):
                    raise RuntimeError("Original JSON Any value/position was narrowed")
            elif field == "case_values":
                if item.get("type") != ["array", "null"] or item.get("items") != {"type": "integer"}:
                    raise RuntimeError("Original nullable integer case array was changed")
            elif item.get("type") != ("boolean" if field in ("has_default_pin", "create_graph_if_missing") else "integer" if field in ("start_index", "case_count") else "string"):
                raise RuntimeError("Original selection field type changed: " + field)
            if field in DEFAULTS_BY_METHOD[method]:
                expected = DEFAULTS_BY_METHOD[method][field]
                if "default" not in item or type(item["default"]) is not type(expected) or item["default"] != expected:
                    raise RuntimeError("Original selection default changed: " + field)
            elif "default" in item:
                raise RuntimeError("Original required selection field was defaulted")
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
                or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
            raise RuntimeError("Exact standard operation outer envelope required")
        returned = outer["returnValue"]
        fields = returned.get("properties") if type(returned) is dict else None
        if (type(returned) is not dict or returned.get("type") != "object" or returned.get("additionalProperties") is not False
                or returned.get("required") != list(RESULT_FIELDS) or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)):
            raise RuntimeError("Exact reflected operation output11 required")
        if any(type(fields[k]) is not dict or fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS):
            raise RuntimeError("Selection operation output field type changed")
    return {"schema_verified": True, "original_public_defaults_verified": True, "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS},
        "public_position_Any_wire_schema_verified": True, "actual_official_raw_wire_verified": False,
        "actual_node_construction_or_runtime_verified": False, "preparation_required": False}
