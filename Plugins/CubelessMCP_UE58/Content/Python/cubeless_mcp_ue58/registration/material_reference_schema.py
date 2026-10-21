"""Strict original reference5/4 subset and independent preparation/packing gates."""
import unreal

from cubeless_mcp_ue58.compatibility.ue58 import material_references as api

BINDINGS = api.BINDINGS
PREPARATION = api.PREPARATION
FIELDS = {
    "replace_material_collection_references": {
        "material_path": ("string",), "target_collection_path": ("string",),
        "source_collection_path": ("string", ""), "graph_type": ("string", "auto"), "save": ("boolean", True)},
    "replace_material_texture_references": {
        "material_path": ("string",), "replacements": ("object",),
        "graph_type": ("string", "auto"), "save": ("boolean", True)}}
DEFAULTS = {"replace_material_collection_references.source_collection_path": ""}
OUTPUT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def binding_status():
    methods = api.binding_status()
    jobs = api.async_binding_status()
    preparation = callable(getattr(getattr(unreal, api.LIBRARY, None), PREPARATION, None))
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and preparation and all(v is True for v in jobs.values()),
        "library": api.LIBRARY, "methods": methods, "async_native_jobs": jobs, "Named_sameJob_continuation": True, "preparation_required": True,
        "preparation_callable": preparation, "schema_verified": False,
        "preparation_verified": False, "actual_call_packing_verified": False, "behavior_verified": False,
        "public_map_representation": "stock direct FString-to-FString map; public object additionalProperties string",
        "verification_scope": "Exact native bindings and one empty-source default helper presence; asset/compile/save outcomes are separate"}


def _output(row):
    output = row.get("outputSchema")
    returned = output.get("properties", {}).get("returnValue") if type(output) is dict else None
    properties = returned.get("properties") if type(returned) is dict else None
    if type(returned) is not dict or returned.get("type") != "object" or type(properties) is not dict or set(properties) != set(OUTPUT_FIELDS):
        raise RuntimeError("Material references require the exact standard reflected operation output")
    for key in OUTPUT_FIELDS:
        if type(properties[key]) is not dict or properties[key].get("type") != ("boolean" if key == "success" else "string"):
            raise RuntimeError("Material reference output type changed: " + key)


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Material tools schema required")
    rows = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str
        and row["name"].rsplit(".", 1)[-1] in BINDINGS]
    tools = {row["name"].rsplit(".", 1)[-1]: row for row in rows}
    if len(rows) != 2 or set(tools) != set(BINDINGS):
        raise RuntimeError("Original Material reference5/4 subset missing or duplicated")
    for name, expected in FIELDS.items():
        actual = tools[name].get("inputSchema")
        properties = actual.get("properties") if type(actual) is dict else None
        required = [key for key, value in expected.items() if len(value) == 1]
        if type(properties) is not dict or set(properties) != set(expected) or actual.get("required") != required:
            raise RuntimeError("Original Material reference required/default field set changed: " + name)
        for key, value in expected.items():
            field = properties[key]
            if type(field) is not dict or field.get("type") != value[0] or field.get("nullable") is True:
                raise RuntimeError("Original Material reference type changed: " + name + "." + key)
            if key == "replacements":
                additional = field.get("additionalProperties")
                if type(additional) is not dict or additional.get("type") != "string" or additional.get("nullable") is True:
                    raise RuntimeError("Texture replacements must remain an object with string values")
            if len(value) == 1:
                if "default" in field:
                    raise RuntimeError("Required Material reference input unexpectedly defaulted: " + key)
            elif "default" not in field or type(field["default"]) is not type(value[1]) or field["default"] != value[1]:
                raise RuntimeError("Original Material reference default changed: " + name + "." + key)
        _output(tools[name])
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {name: len(fields) for name, fields in FIELDS.items()},
        "observed_tool_count": len(schema["tools"]), "public_string_map_schema_verified": True,
        "preparation_verified": False, "actual_call_packing_verified": False,
        "immediate_compile_or_saved_outcome_verified": False, "whole_original_wait_contract_verified": False}
