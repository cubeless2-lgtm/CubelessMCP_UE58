"""Original cache-refresh2 schema; binding, packing and behavior are distinct."""
from cubeless_mcp_ue58.compatibility.ue58 import material_cache as api

BINDINGS = api.BINDINGS
FIELDS = {"material_path": ("string",), "save": ("boolean", True)}
OUTPUT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def binding_status():
    methods = api.binding_status()
    jobs = api.async_binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and all(v is True for v in jobs.values()),
        "library": api.LIBRARY, "methods": methods, "async_native_jobs": jobs, "Named_sameJob_continuation": True, "preparation_required": False,
        "schema_verified": False, "actual_call_packing_verified": False, "behavior_verified": False,
        "verification_scope": "Original refresh2 callable; cache/compiler/dependency/save require actual returned evidence",
        "public_input_count": 2, "blocking_finish_private_clear_full_parity_verified": False}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Material schema required")
    rows = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str
        and row["name"].rsplit(".", 1)[-1] == BINDINGS[0]]
    if len(rows) != 1:
        raise RuntimeError("Original Material cache-refresh2 missing or duplicated")
    inputs = rows[0].get("inputSchema")
    properties = inputs.get("properties") if type(inputs) is dict else None
    if type(properties) is not dict or set(properties) != set(FIELDS) or inputs.get("required") != ["material_path"]:
        raise RuntimeError("Original refresh2 field/required/default set changed")
    for key, expected in FIELDS.items():
        field = properties[key]
        if type(field) is not dict or field.get("type") != expected[0] or field.get("nullable") is True:
            raise RuntimeError("Original refresh input type changed: " + key)
        if len(expected) == 1:
            if "default" in field:
                raise RuntimeError("Required material path unexpectedly defaulted")
        elif ("default" not in field or type(field["default"]) is not type(expected[1])
                or field["default"] != expected[1]):
            raise RuntimeError("Original refresh input default changed: " + key)
    output = rows[0].get("outputSchema")
    returned = output.get("properties", {}).get("returnValue") if type(output) is dict else None
    fields = returned.get("properties") if type(returned) is dict else None
    if type(returned) is not dict or returned.get("type") != "object" or type(fields) is not dict or set(fields) != set(OUTPUT_FIELDS):
        raise RuntimeError("Exact standard reflected operation output required")
    for key in OUTPUT_FIELDS:
        if type(fields[key]) is not dict or fields[key].get("type") != ("boolean" if key == "success" else "string"):
            raise RuntimeError("Cache-refresh output type changed: " + key)
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {BINDINGS[0]: 2}, "observed_tool_count": len(schema["tools"]),
        "original_defaults_verified": True, "actual_call_packing_verified": False,
        "cache_refresh_behavior_verified": False, "blocking_finish_private_clear_full_parity_verified": False}
