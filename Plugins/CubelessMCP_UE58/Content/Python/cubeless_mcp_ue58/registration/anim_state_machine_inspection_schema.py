"""Exact reflected observer6/defaults/standard11 schema gate; no Engine dispatch."""
from cubeless_mcp_ue58.compatibility.ue58 import anim_state_machine_inspection as api

TOOLSET = "cubeless_mcp_ue58.registration.anim_state_machine_inspection_toolset.CubelessAnimStateMachineInspectionTools"
CLASS_PATH = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/anim_state_machine_inspection_toolset_PY.CubelessAnimStateMachineInspectionTools"
BINDINGS = ("inspect_anim_state_machine_transitions",)
PREPARATION = "prepare_anim_state_machine_inspection_toolset"
PUBLIC_FIELDS = ("blueprint_name", "state_machine_name", "include_pins", "include_rule_graph_nodes", "include_state_nodes", "max_rule_graph_nodes")
DEFAULTS = {"state_machine_name": "", "include_pins": True, "include_rule_graph_nodes": True, "include_state_nodes": True, "max_rule_graph_nodes": 256}
FIELD_KINDS = {"blueprint_name": "string", "state_machine_name": "string", "include_pins": "boolean", "include_rule_graph_nodes": "boolean", "include_state_nodes": "boolean", "max_rule_graph_nodes": "integer"}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json", "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def binding_status(toolset_class=None):
    methods = api.binding_status()
    defined = {method: callable(getattr(toolset_class, method, None)) for method in BINDINGS}
    return {"ready": all(methods.values()) and all(defined.values()) and api.preparation_available(), "library": api.LIBRARY,
        "methods": methods, "definition_methods": defined, "public_toolset": TOOLSET,
        "public_tool_count": 1, "public_input_counts": {BINDINGS[0]: 6},
        "hidden_input_counts": {BINDINGS[0]: 6}, "public_input_count": 6,
        "preparation_required": True, "preparation_callable": api.preparation_available(),
        "preparation_verified": False,
        "empty_filter_default_schema_verification_required": True,
        "schema_verified": False, "default_verified": False, "actual_official_wire_verified": False,
        "current_observation_not_runtime_state_machine_evaluation": True}


def _required(expected, value):
    return (type(value) is list and len(value) == len(expected)
        and all(type(item) is str for item in value) and set(value) == set(expected))


def validate_schema(schema, toolset_class):
    method = BINDINGS[0]
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 1 or not callable(getattr(toolset_class, method, None))):
        raise RuntimeError("Exact reflected state-machine observer Toolset1 required")
    row = schema["tools"][0]
    if type(row) is not dict or row.get("name") != TOOLSET + "." + method:
        raise RuntimeError("Exact original observer method required")
    inputs = row.get("inputSchema")
    props = inputs.get("properties") if type(inputs) is dict else None
    if (type(inputs) is not dict or inputs.get("type") != "object" or type(props) is not dict
            or set(props) != set(PUBLIC_FIELDS) or not _required(("blueprint_name",), inputs.get("required"))):
        raise RuntimeError("Exact original6 fields/object/required blueprint input differs")
    for field, kind in FIELD_KINDS.items():
        item = props[field]
        if type(item) is not dict or item.get("type") != kind:
            raise RuntimeError("State-machine field type differs: " + field)
        if field in DEFAULTS:
            if "default" not in item or type(item["default"]) is not type(DEFAULTS[field]) or item["default"] != DEFAULTS[field]:
                raise RuntimeError("Original state-machine default differs: " + field)
        elif "default" in item:
            raise RuntimeError("Required blueprint input gained a default")
    output = row.get("outputSchema")
    outer = output.get("properties") if type(output) is dict else None
    if (type(output) is not dict or output.get("type") != "object" or type(outer) is not dict
            or set(outer) != {"returnValue"} or not _required(("returnValue",), output.get("required"))):
        raise RuntimeError("Exact standard11 returnValue outer envelope required")
    value = outer["returnValue"]
    fields = value.get("properties") if type(value) is dict else None
    if (type(value) is not dict or value.get("type") != "object" or type(fields) is not dict
            or set(fields) != set(RESULT_FIELDS) or not _required(RESULT_FIELDS, value.get("required"))
            or any(type(fields[key]) is not dict or fields[key].get("type") != ("boolean" if key == "success" else "string") for key in RESULT_FIELDS)):
        raise RuntimeError("Exact existing synchronous standard11 result required")
    return {"schema_verified": True, "default_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {method: 6}, "empty_filter_default_verified": True,
        "public_input_count": 6, "preparation_required": True,
        "preparation_verified": False, "actual_official_wire_verified": False,
        "runtime_or_cold_or_original_pipeline_verified": False}
