"""Current read1 binding and schema, no nullable/default preparation."""
from cubeless_mcp_ue58.compatibility.ue58 import niagara_rapid_iteration as api

BINDINGS = api.BINDINGS


def binding_status():
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY,
        "methods": methods, "preparation_required": False, "schema_verified": False,
        "behavior_verified": False, "actual_call_packing_verified": False}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual Niagara module tools array required")
    rows = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str
        and row["name"].rsplit(".", 1)[-1] == BINDINGS[0]]
    if len(rows) != 1:
        raise RuntimeError("One exact RI observation tool required")
    inputs = rows[0].get("inputSchema", {})
    fields = inputs.get("properties", {})
    if (type(fields) is not dict or set(fields) != {"system_path"}
            or inputs.get("required") != ["system_path"] or fields["system_path"].get("type") != "string"
            or "default" in fields["system_path"]):
        raise RuntimeError("RI read1 required literal path schema changed")
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {BINDINGS[0]: 1}, "preparation_required": False,
        "behavior_verified": False, "actual_call_packing_verified": False}
