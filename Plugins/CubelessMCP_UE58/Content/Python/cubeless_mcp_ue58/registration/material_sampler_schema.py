"""Exact original six-input sampler action schema; runtime outcomes stay separate."""
from cubeless_mcp_ue58.compatibility.ue58 import material_sampler as api

BINDINGS = api.BINDINGS
FIELDS = {"material_path": ("string",), "texture_path": ("string",),
    "sampler_type": ("string", "SAMPLERTYPE_LinearColor"), "dry_run": ("boolean", False),
    "save": ("boolean", True), "compile": ("boolean", True)}
OUTPUT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def binding_status():
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()),
        "library": api.LIBRARY, "methods": methods, "preparation_required": False,
        "schema_verified": False, "actual_call_packing_verified": False, "behavior_verified": False,
        "verification_scope": "Original sampler6 callable; TextureSample filter/dry/matching/edit and compile/direct-save need actual evidence",
        "public_input_count": 6, "action_success_proves_shader_or_save_completion": False}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Material schema required")
    rows = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str
        and row["name"].rsplit(".", 1)[-1] == BINDINGS[0]]
    if len(rows) != 1:
        raise RuntimeError("Original Material sampler6 missing or duplicated")
    inputs = rows[0].get("inputSchema")
    properties = inputs.get("properties") if type(inputs) is dict else None
    if type(properties) is not dict or set(properties) != set(FIELDS) or inputs.get("required") != ["material_path", "texture_path"]:
        raise RuntimeError("Original sampler6 field/required/default set changed")
    for key, expected in FIELDS.items():
        field = properties[key]
        if type(field) is not dict or field.get("type") != expected[0] or field.get("nullable") is True:
            raise RuntimeError("Original sampler input type changed: " + key)
        if len(expected) == 1:
            if "default" in field:
                raise RuntimeError("Required sampler path unexpectedly defaulted")
        elif ("default" not in field or type(field["default"]) is not type(expected[1])
                or field["default"] != expected[1]):
            raise RuntimeError("Original sampler input default changed: " + key)
    output = rows[0].get("outputSchema")
    returned = output.get("properties", {}).get("returnValue") if type(output) is dict else None
    fields = returned.get("properties") if type(returned) is dict else None
    if type(returned) is not dict or returned.get("type") != "object" or type(fields) is not dict or set(fields) != set(OUTPUT_FIELDS):
        raise RuntimeError("Exact standard reflected operation output required")
    for key in OUTPUT_FIELDS:
        if type(fields[key]) is not dict or fields[key].get("type") != ("boolean" if key == "success" else "string"):
            raise RuntimeError("Sampler output type changed: " + key)
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {BINDINGS[0]: 6}, "observed_tool_count": len(schema["tools"]),
        "original_defaults_verified": True, "actual_call_packing_verified": False,
        "sampler_behavior_verified": False, "shader_or_save_completion_verified": False}
