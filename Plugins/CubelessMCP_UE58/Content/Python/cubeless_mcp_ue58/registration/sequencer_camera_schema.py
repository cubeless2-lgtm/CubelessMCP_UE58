"""Original CameraCut6 subset and its exact two-empty-selector preparation."""
import unreal

from cubeless_mcp_ue58.compatibility.ue58 import sequencer_camera as api

BINDINGS = api.BINDINGS
FIELDS = {"sequence_path": ("string",), "start_frame": ("number",), "end_frame": ("number",),
    "binding_id": ("string", ""), "actor_label": ("string", ""), "save": ("boolean", True)}
DEFAULTS = {BINDINGS[0] + ".binding_id": "", BINDINGS[0] + ".actor_label": ""}


def binding_status():
    methods = api.binding_status()
    prepare = callable(getattr(getattr(unreal, api.LIBRARY, None), api.PREPARATION, None))
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and prepare,
        "library": api.LIBRARY, "methods": methods, "preparation_method": api.PREPARATION,
        "empty_selector_preparation_callable": prepare, "preparation_requires_toolset_class": True,
        "schema_verified": False, "behavior_verified": False,
        "verification_scope": "Exact hidden6 and CameraCut default preparation callable presence; actual prepared schema/packing/phase/save behavior separate"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Canonical Sequencer schema must provide tools array")
    selected = [r for r in schema["tools"] if type(r) is dict and type(r.get("name")) is str
        and r["name"].rsplit(".", 1)[-1] in BINDINGS]
    if len(selected) != 1:
        raise RuntimeError("Exactly one original CameraCut method required")
    inputs = selected[0].get("inputSchema")
    if type(inputs) is not dict or type(inputs.get("properties")) is not dict or set(inputs["properties"]) != set(FIELDS):
        raise RuntimeError("Changed original six CameraCut fields")
    required = inputs.get("required", [])
    if type(required) is not list or len(required) != 3 or set(required) != {"sequence_path", "start_frame", "end_frame"}:
        raise RuntimeError("Changed original three required CameraCut fields")
    for name, definition in FIELDS.items():
        row = inputs["properties"][name]
        if type(row) is not dict or row.get("type") != definition[0]:
            raise RuntimeError("Changed reflected CameraCut type: " + name)
        if len(definition) == 1:
            if "default" in row: raise RuntimeError("Required CameraCut field defaulted: " + name)
        elif "default" not in row or type(row["default"]) is not (bool if definition[0] == "boolean" else str) or row["default"] != definition[1]:
            raise RuntimeError("Changed original non-null CameraCut default: " + name)
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {BINDINGS[0]: 6}, "observed_tool_count": len(schema["tools"]),
        "actual_call_packing_verified": False}
