"""Exact raw Registry schema/readiness for original local-variable12.

Source readiness does not assert active registration or actual Any packing.
No reflected canonical class/default preparation is used for this named tool.
"""
from cubeless_mcp_ue58.compatibility.ue58 import local_variable as api

TOOLSET_NAME = "CubelessMCPUE58Editor.CubelessLocalVariableTools"
BINDINGS = api.BINDINGS
PUBLIC_FIELDS = ("blueprint_name", "variable_name", "variable_type", "default_value", "category",
    "tooltip", "type_object", "is_array", "graph_name", "graph_id", "graph_type", "create_graph_if_missing")
DEFAULTS = {"default_value": None, "category": "", "tooltip": "", "type_object": "", "is_array": False,
    "graph_name": "", "graph_id": "", "graph_type": "function", "create_graph_if_missing": False}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def binding_status():
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY,
        "methods": methods, "preparation_required": False, "raw_toolset_registration_required": True,
        "public_toolset_name": TOOLSET_NAME, "expected_defaults": dict(DEFAULTS),
        "schema_verified": False, "behavior_verified": False, "actual_call_packing_verified": False,
        "native_operation_peer_required": True}


def validate_schema(schema):
    if type(schema) is not dict or schema.get("name") != TOOLSET_NAME or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual exact LocalVariable raw toolset schema required")
    tools = schema["tools"]
    if len(tools) != 1 or type(tools[0]) is not dict or tools[0].get("name") != TOOLSET_NAME + "." + BINDINGS[0]:
        raise RuntimeError("One exact original local-variable named tool required")
    inputs = tools[0].get("inputSchema", {})
    fields = inputs.get("properties", {})
    if (type(fields) is not dict or set(fields) != set(PUBLIC_FIELDS)
            or inputs.get("required") != list(PUBLIC_FIELDS[:3]) or inputs.get("additionalProperties") is not False):
        raise RuntimeError("Original twelve fields/three required strings changed")
    for name in PUBLIC_FIELDS:
        field = fields[name]
        if type(field) is not dict:
            raise RuntimeError("Invalid local-variable field schema")
        if name == "default_value":
            if set(field) != {"default"} or field["default"] is not None:
                raise RuntimeError("Original unrestricted JSON Any/defaultNone schema required")
        elif field.get("type") != ("boolean" if name in ("is_array", "create_graph_if_missing") else "string"):
            raise RuntimeError("Original field type changed: " + name)
        if name in DEFAULTS:
            expected = DEFAULTS[name]
            if "default" not in field or type(field["default"]) is not type(expected) or field["default"] != expected:
                raise RuntimeError("Original field default changed: " + name)
        elif "default" in field:
            raise RuntimeError("Required strings must not be defaulted")
    output = tools[0].get("outputSchema", {})
    envelope = output.get("properties", {}).get("returnValue", {})
    result_fields = envelope.get("properties", {})
    if (output.get("required") != ["returnValue"] or output.get("additionalProperties") is not False
            or set(result_fields) != set(RESULT_FIELDS) or envelope.get("required") != list(RESULT_FIELDS)
            or envelope.get("additionalProperties") is not False):
        raise RuntimeError("Existing exact reflected result envelope required")
    for name in RESULT_FIELDS:
        if result_fields[name].get("type") != ("boolean" if name == "success" else "string"):
            raise RuntimeError("Existing reflected result field type changed")
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {BINDINGS[0]: 12}, "expected_defaults": dict(DEFAULTS),
        "original_Any_schema_verified": True, "preparation_required": False,
        "behavior_verified": False, "actual_call_packing_verified": False}
