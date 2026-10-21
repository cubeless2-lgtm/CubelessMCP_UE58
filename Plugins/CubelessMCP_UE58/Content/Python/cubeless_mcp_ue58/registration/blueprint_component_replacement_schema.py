"""Validate only original replacement8 in canonical BlueprintTools."""
from cubeless_mcp_ue58.compatibility.ue58 import component_replacement as api


NAME = "replace_blueprint_component_class"
FIELDS = {"blueprint_name": ("string",), "component_name": ("string",), "new_component_class": ("string",),
    "sphere_radius": ("number", -1.0), "dry_run": ("boolean", True), "compile": ("boolean", True),
    "save": ("boolean", True), "refresh_nodes": ("boolean", True)}
REQUIRED = {"blueprint_name", "component_name", "new_component_class"}


def binding_status():
    methods = api.binding_status()
    return {"ready": set(methods) == {NAME} and methods[NAME] is True, "methods": methods, "library": api.LIBRARY,
        "schema_verified": False, "behavior_verified": False,
        "verification_scope": "Exact replacement8 callable presence only; no authoring/compiler/save/runtime proof"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Canonical Blueprint toolset must provide tools array")
    selected = [r for r in schema["tools"] if type(r) is dict and type(r.get("name")) is str and r["name"].rsplit(".", 1)[-1] == NAME]
    if len(selected) != 1 or type(selected[0].get("inputSchema")) is not dict:
        raise RuntimeError("Canonical BlueprintTools must provide exactly one original replacement method")
    inputs = selected[0]["inputSchema"]; properties = inputs.get("properties", {}); required = inputs.get("required", [])
    if type(properties) is not dict or set(properties) != set(FIELDS) or type(required) is not list or len(required) != 3 or set(required) != REQUIRED:
        raise RuntimeError("Replacement changed original eight fields/three required inputs")
    for key, definition in FIELDS.items():
        row = properties[key]
        if type(row) is not dict or row.get("type") != definition[0]:
            raise RuntimeError("Replacement changed reflected type: " + key)
        if len(definition) == 1:
            if "default" in row:
                raise RuntimeError("Replacement required field unexpectedly acquired default: " + key)
        else:
            allowed = (int, float) if definition[0] == "number" else (bool,)
            if "default" not in row or type(row["default"]) not in allowed or row["default"] != definition[1]:
                raise RuntimeError("Replacement changed original typed default: " + key)
    return {"schema_verified": True, "validated_tool_names": [NAME], "validated_input_counts": {NAME: 8},
        "observed_tool_count": len(schema["tools"])}
