"""Exact Float6 subset for the existing canonical Sequencer toolset."""
import unreal

from cubeless_mcp_ue58.compatibility.ue58 import sequencer_float as api


BINDINGS = api.BINDINGS
NAME = api.NAME
FIELDS = {"sequence_path": ("string",), "property_path": ("string",), "keys": ("array",),
    "binding_id": ("string", ""), "actor_label": ("string", ""), "save": ("boolean", True)}
REQUIRED = {"sequence_path", "property_path", "keys"}


def binding_status():
    methods = api.binding_status()
    observers = api.observation_binding_status()
    wrapper = getattr(unreal, "JsonObjectWrapper", None)
    export_available = isinstance(wrapper, type) and callable(getattr(wrapper, "export_text", None))
    prepare_available = callable(getattr(getattr(unreal, api.LIBRARY, None), api.PREPARATION, None))
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and all(observers.values()) and export_available and prepare_available,
        "methods": methods, "library": api.LIBRARY, "observation_methods": observers,
        "json_object_wrapper_export_text_callable": export_available,
        "empty_selector_preparation_callable": prepare_available, "preparation_method": api.PREPARATION,
        "preparation_requires_toolset_class": True, "schema_verified": False, "behavior_verified": False,
        "verification_scope": "Agreed hidden5/public observer/stock wrapper callable presence only; native final source/build and actual canonical schema/array packing/keys/save behavior remain separate"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Canonical Sequencer toolset must provide tools array")
    selected = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str and row["name"].rsplit(".", 1)[-1] == NAME]
    if len(selected) != 1 or type(selected[0].get("inputSchema")) is not dict:
        raise RuntimeError("Canonical Sequencer toolset must provide exactly one original Float method")
    inputs = selected[0]["inputSchema"]; properties = inputs.get("properties", {}); required = inputs.get("required", [])
    if type(properties) is not dict or set(properties) != set(FIELDS) or type(required) is not list or len(required) != 3 or set(required) != REQUIRED:
        raise RuntimeError("Float method changed original six fields/three required inputs")
    for key, definition in FIELDS.items():
        row = properties[key]
        if type(row) is not dict or row.get("type") != definition[0]:
            raise RuntimeError("Float method changed reflected input type: " + key)
        if len(definition) == 1:
            if "default" in row:
                raise RuntimeError("Required Float input unexpectedly acquired default: " + key)
        elif "default" not in row or type(row["default"]) is not (bool if definition[0] == "boolean" else str) or row["default"] != definition[1]:
            raise RuntimeError("Float method changed original non-null typed default: " + key)
    items = properties["keys"].get("items")
    if type(items) is not dict or items.get("type") != "object" or items.get("properties") or items.get("required") or items.get("additionalProperties") is False:
        raise RuntimeError("Float keys must remain original generic array<object>, not JSON text or stripped fields")
    return {"schema_verified": True, "validated_tool_names": [NAME], "validated_input_counts": {NAME: 6},
        "observed_tool_count": len(schema["tools"]), "keys_type": "array<object>", "actual_call_packing_verified": False}
