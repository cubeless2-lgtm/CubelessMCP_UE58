"""Validate only the two original defaults tools in canonical BlueprintTools."""
import unreal

from cubeless_mcp_ue58.compatibility.ue58 import blueprint_defaults as api


CONTRACTS = {
    "set_blueprint_actor_tick_settings": ({"blueprint_name"}, {
        "blueprint_name": ("string",), "tick_hz": ("number", 60.0),
        "tick_interval": ("number", -1.0), "can_ever_tick": ("boolean", True),
        "start_with_tick_enabled": ("boolean", True),
        "allow_tick_on_dedicated_server": ("boolean", True),
        "tick_even_when_paused": ("boolean", False), "dry_run": ("boolean", True),
        "compile": ("boolean", True), "save": ("boolean", True)}),
    "set_skeletal_mesh_component_anim_defaults": ({"blueprint_name", "component_name"}, {
        "blueprint_name": ("string",), "component_name": ("string",),
        "skeletal_mesh": ("string", ""), "anim_class": ("string", ""),
        "physics_asset": ("string", None), "simulate_physics": ("boolean", None),
        "collision_enabled": ("string", None), "collision_response": ("string", None),
        "enable_gravity": ("boolean", None), "visibility_based_anim_tick_option": ("string", None),
        "compile": ("boolean", True), "save": ("boolean", True)}),
}
EMPTY_DEFAULTS = {
    "set_skeletal_mesh_component_anim_defaults.skeletal_mesh": "",
    "set_skeletal_mesh_component_anim_defaults.anim_class": "",
}


def binding_status():
    methods = api.binding_status()
    library = getattr(unreal, api.LIBRARY, None)
    preparation_callable = callable(getattr(library, "prepare_blueprint_defaults_toolset", None))
    methods_ready = set(methods) == set(CONTRACTS) and all(value is True for value in methods.values())
    return {"ready": methods_ready and preparation_callable, "methods_ready": methods_ready,
        "methods": methods, "library": api.LIBRARY, "preparation_callable": preparation_callable,
        "schema_verified": False, "behavior_verified": False,
        "verification_scope": "Two exact native defaults bindings and metadata preparation presence; no authoring/compiler/save/runtime proof"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Canonical Blueprint toolset must provide a tools array")
    rows = schema["tools"]
    selected = {}
    for row in rows:
        if type(row) is not dict or type(row.get("name")) is not str:
            raise RuntimeError("Malformed canonical Blueprint tool entry")
        name = row["name"].rsplit(".", 1)[-1]
        if name not in CONTRACTS:
            continue
        if name in selected:
            raise RuntimeError("Duplicate original Blueprint defaults tool: " + name)
        selected[name] = row.get("inputSchema")
    if set(selected) != set(CONTRACTS):
        raise RuntimeError("Canonical BlueprintTools must retain both original defaults names")
    for name, (required, fields) in CONTRACTS.items():
        inputs = selected[name]
        if type(inputs) is not dict:
            raise RuntimeError(name + " has no input schema")
        props = inputs.get("properties", {})
        actual_required = inputs.get("required", [])
        if (type(props) is not dict or set(props) != set(fields)
                or type(actual_required) is not list or len(actual_required) != len(required)
                or set(actual_required) != required):
            raise RuntimeError(name + " changed original input/required fields")
        for key, contract in fields.items():
            actual = props[key]
            if type(actual) is not dict or actual.get("type") != contract[0]:
                raise RuntimeError(name + "." + key + " changed its original reflected type")
            if len(contract) == 1:
                if "default" in actual:
                    raise RuntimeError(name + "." + key + " unexpectedly acquired a default")
                continue
            default = actual.get("default")
            allowed = (int, float) if contract[0] == "number" else (type(contract[1]),)
            if "default" not in actual or type(default) not in allowed or default != contract[1]:
                raise RuntimeError(name + "." + key + " changed its original typed default")
    # The remaining existing canonical tools are not reclassified as these two.
    return {"schema_verified": True, "validated_tool_names": sorted(CONTRACTS),
        "observed_tool_count": len(rows),
        "validated_input_counts": {name: len(fields) for name, (_, fields) in CONTRACTS.items()}}
