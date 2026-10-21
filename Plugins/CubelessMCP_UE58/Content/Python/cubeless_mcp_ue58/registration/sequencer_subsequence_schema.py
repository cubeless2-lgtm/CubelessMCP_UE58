"""Original Subsequence5 subset within canonical Sequencer8, no new defaults."""
from cubeless_mcp_ue58.compatibility.ue58 import sequencer_subsequence as api

BINDINGS = api.BINDINGS
FIELDS = {"sequence_path": ("string",), "subsequence_path": ("string",),
    "start_frame": ("number",), "duration_frames": ("number",), "save": ("boolean", True)}


def binding_status():
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY,
        "methods": methods, "schema_verified": False, "behavior_verified": False,
        "verification_scope": "Exact hidden5 binding presence only; loaded prerequisites/current section/save behavior separate"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Canonical Sequencer schema must provide tools array")
    selected = [r for r in schema["tools"] if type(r) is dict and type(r.get("name")) is str
        and r["name"].rsplit(".", 1)[-1] in BINDINGS]
    if len(selected) != 1:
        raise RuntimeError("Exactly one original Subsequence method required")
    inputs = selected[0].get("inputSchema")
    if type(inputs) is not dict or type(inputs.get("properties")) is not dict or set(inputs["properties"]) != set(FIELDS):
        raise RuntimeError("Changed original five Subsequence fields")
    required = inputs.get("required", [])
    if type(required) is not list or len(required) != 4 or set(required) != set(FIELDS) - {"save"}:
        raise RuntimeError("Changed original four required Subsequence fields")
    for name, definition in FIELDS.items():
        row = inputs["properties"][name]
        if type(row) is not dict or row.get("type") != definition[0]:
            raise RuntimeError("Changed reflected Subsequence type: " + name)
        if len(definition) == 1:
            if "default" in row: raise RuntimeError("Required Subsequence field defaulted: " + name)
        elif type(row.get("default")) is not bool or row["default"] is not True:
            raise RuntimeError("Changed original Subsequence save=True")
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {BINDINGS[0]: 5}, "observed_tool_count": len(schema["tools"]),
        "actual_call_packing_verified": False}
