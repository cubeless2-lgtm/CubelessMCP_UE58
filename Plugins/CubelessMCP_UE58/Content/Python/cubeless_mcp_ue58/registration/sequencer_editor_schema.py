"""Original Open1 required string input; no new default converter or class."""
from cubeless_mcp_ue58.compatibility.ue58 import sequencer_editor as api
BINDINGS = api.BINDINGS

def binding_status():
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY,
        "methods": methods, "schema_verified": False, "behavior_verified": False,
        "verification_scope": "Exact one-input native UI binding presence; actual UI bool/observations separate"}

def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Canonical Sequencer schema requires tools array")
    rows = [r for r in schema["tools"] if type(r) is dict and type(r.get("name")) is str and r["name"].rsplit(".", 1)[-1] in BINDINGS]
    if len(rows) != 1:
        raise RuntimeError("Exactly one original Sequencer Editor method required")
    inputs = rows[0].get("inputSchema")
    if type(inputs) is not dict or inputs.get("required") != ["sequence_path"] or set(inputs.get("properties", {})) != {"sequence_path"}:
        raise RuntimeError("Changed original Open1 required input")
    row = inputs["properties"]["sequence_path"]
    if type(row) is not dict or row.get("type") != "string" or "default" in row:
        raise RuntimeError("Changed original Open1 string type/default")
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS), "validated_input_counts": {BINDINGS[0]: 1},
        "observed_tool_count": len(schema["tools"]), "actual_UI_open_verified": False}
