"""Exact new original Range4/Remove4 subset within canonical Sequencer7."""
import unreal

from cubeless_mcp_ue58.compatibility.ue58 import sequencer_maintenance as api

BINDINGS = api.BINDINGS
FIELDS = {
    BINDINGS[0]: {"sequence_path": ("string",), "playback_start_frame": ("number",),
        "playback_end_frame": ("number",), "save": ("boolean", True)},
    BINDINGS[1]: {"sequence_path": ("string",), "binding_id": ("string", ""),
        "actor_label": ("string", ""), "save": ("boolean", True)},
}


def binding_status():
    methods = api.binding_status()
    prepare = callable(getattr(getattr(unreal, api.LIBRARY, None), api.PREPARATION, None))
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and prepare,
        "library": api.LIBRARY, "methods": methods, "preparation_method": api.PREPARATION,
        "empty_selector_preparation_callable": prepare, "preparation_requires_toolset_class": True,
        "schema_verified": False, "behavior_verified": False,
        "verification_scope": "Exact hidden4/4 and Remove default preparation callable presence; actual schema/default/phase/save behavior separate"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Canonical Sequencer schema must provide tools array")
    selected = [r for r in schema["tools"] if type(r) is dict and type(r.get("name")) is str and r["name"].rsplit(".", 1)[-1] in BINDINGS]
    tools = {r["name"].rsplit(".", 1)[-1]: r.get("inputSchema") for r in selected}
    if len(selected) != 2 or set(tools) != set(BINDINGS):
        raise RuntimeError("Exactly two original Sequencer maintenance methods required")
    for name, fields in FIELDS.items():
        inputs = tools[name]
        if type(inputs) is not dict or type(inputs.get("properties")) is not dict or set(inputs["properties"]) != set(fields):
            raise RuntimeError("Changed original four fields: " + name)
        required = inputs.get("required", [])
        wanted = {k for k, definition in fields.items() if len(definition) == 1}
        if type(required) is not list or len(required) != len(wanted) or set(required) != wanted:
            raise RuntimeError("Changed original required fields: " + name)
        for key, definition in fields.items():
            row = inputs["properties"][key]
            if type(row) is not dict or row.get("type") != definition[0]:
                raise RuntimeError("Changed reflected maintenance input type: " + name + "." + key)
            if len(definition) == 1:
                if "default" in row: raise RuntimeError("Required maintenance field defaulted: " + name + "." + key)
            elif "default" not in row or type(row["default"]) is not (bool if definition[0] == "boolean" else str) or row["default"] != definition[1]:
                raise RuntimeError("Changed original non-null default: " + name + "." + key)
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {name: 4 for name in BINDINGS}, "observed_tool_count": len(schema["tools"]), "actual_call_packing_verified": False}
