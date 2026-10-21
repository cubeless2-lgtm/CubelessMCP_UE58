"""Validate only original rigid-chain16 in canonical BlueprintTools."""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import rigid_constraint_chain as api


NAME = "ensure_blueprint_rigid_constraint_chain"
FIELDS = {"blueprint_name": ("string",), "body_components": ("array",),
    "root_component_name": ("string", "SceneRoot"), "constraint_prefix": ("string", "PC"),
    "swing1_limit_degrees": ("number", 50.0), "swing2_limit_degrees": ("number", 50.0), "twist_limit_degrees": ("number", 10.0),
    "linear_damping": ("number", 0.05), "angular_damping": ("number", 0.15),
    "projection_linear_alpha": ("number", 0.25), "projection_angular_alpha": ("number", 0.08),
    "projection_linear_tolerance": ("number", 5.0), "projection_angular_tolerance": ("number", 15.0),
    "required_root": ("string", "/Game/Study/ChaosSkeletalMesh"), "save": ("boolean", True), "dry_run": ("boolean", True)}
REQUIRED = {"blueprint_name", "body_components"}


def binding_status():
    methods = api.binding_status()
    wrapper = getattr(unreal, "JsonObjectWrapper", None)
    export_available = isinstance(wrapper, type) and callable(getattr(wrapper, "export_text", None))
    return {"ready": set(methods) == {NAME} and methods[NAME] is True and export_available,
        "methods": methods, "library": api.LIBRARY, "json_object_wrapper_export_text_callable": export_available,
        "schema_verified": False, "behavior_verified": False,
        "verification_scope": "Hidden exact16 callable and stock JsonObjectWrapper.export_text presence only; actual array schema/packing/authoring proof separate"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Canonical Blueprint toolset must provide tools array")
    selected = [r for r in schema["tools"] if type(r) is dict and type(r.get("name")) is str and r["name"].rsplit(".", 1)[-1] == NAME]
    if len(selected) != 1 or type(selected[0].get("inputSchema")) is not dict:
        raise RuntimeError("Canonical BlueprintTools must provide exactly one original rigid-chain method")
    inputs = selected[0]["inputSchema"]; properties = inputs.get("properties", {}); required = inputs.get("required", [])
    if type(properties) is not dict or set(properties) != set(FIELDS) or type(required) is not list or len(required) != 2 or set(required) != REQUIRED:
        raise RuntimeError("Rigid-chain changed original sixteen fields/two required inputs")
    for key, definition in FIELDS.items():
        row = properties[key]
        if type(row) is not dict or row.get("type") != definition[0]:
            raise RuntimeError("Rigid-chain changed reflected type: " + key)
        if len(definition) == 1:
            if "default" in row:
                raise RuntimeError("Rigid-chain required field unexpectedly acquired default: " + key)
        else:
            allowed = (int, float) if definition[0] == "number" else (bool,) if definition[0] == "boolean" else (str,)
            if "default" not in row or type(row["default"]) not in allowed or row["default"] != definition[1]:
                raise RuntimeError("Rigid-chain changed original typed default: " + key)
    items = properties["body_components"].get("items")
    if type(items) is not dict or items.get("type") != "object" or items.get("properties") or items.get("required") or items.get("additionalProperties") is False:
        raise RuntimeError("Stock JsonObjectWrapper must expose generic object items; original body dictionary fields cannot be lost or replaced by string input")
    return {"schema_verified": True, "validated_tool_names": [NAME], "validated_input_counts": {NAME: 16},
        "observed_tool_count": len(schema["tools"]), "body_components_type": "array<object>",
        "actual_call_packing_verified": False}
