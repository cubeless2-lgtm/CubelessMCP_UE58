"""Exact original MPC-expression conversion3 schema; behavior stays separate."""
from cubeless_mcp_ue58.compatibility.ue58 import material_collection_parameters as api

BINDINGS = api.BINDINGS
FIELDS = {"material_path": ("string",), "use_runtime_values": ("boolean", True), "save": ("boolean", True)}
OUTPUT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def binding_status():
    methods = api.binding_status()
    jobs = api.async_binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and all(v is True for v in jobs.values()),
        "library": api.LIBRARY, "methods": methods, "async_native_jobs": jobs, "Named_sameJob_continuation": True, "preparation_required": False,
        "schema_verified": False, "actual_call_packing_verified": False, "behavior_verified": False,
        "verification_scope": "Original three-input native binding; mandatory compile, partial graph/world/save and pending outcomes require actual runtime evidence",
        "public_input_count": 3, "public_graph_type_input": False}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Material schema required")
    rows = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str
        and row["name"].rsplit(".", 1)[-1] == BINDINGS[0]]
    if len(rows) != 1:
        raise RuntimeError("Original Material collection-parameter conversion3 missing or duplicated")
    row = rows[0]
    inputs = row.get("inputSchema")
    properties = inputs.get("properties") if type(inputs) is dict else None
    if type(properties) is not dict or set(properties) != set(FIELDS) or inputs.get("required") != ["material_path"]:
        raise RuntimeError("Original Material collection-parameter input field/default set changed")
    for key, expected in FIELDS.items():
        field = properties[key]
        if type(field) is not dict or field.get("type") != expected[0] or field.get("nullable") is True:
            raise RuntimeError("Original collection-parameter input type changed: " + key)
        if len(expected) == 1:
            if "default" in field:
                raise RuntimeError("Required Material path unexpectedly defaulted")
        elif "default" not in field or type(field["default"]) is not bool or field["default"] is not expected[1]:
            raise RuntimeError("Original collection-parameter boolean default changed: " + key)
    output = row.get("outputSchema")
    returned = output.get("properties", {}).get("returnValue") if type(output) is dict else None
    result_properties = returned.get("properties") if type(returned) is dict else None
    if type(returned) is not dict or returned.get("type") != "object" or type(result_properties) is not dict or set(result_properties) != set(OUTPUT_FIELDS):
        raise RuntimeError("Exact standard reflected operation output required")
    for key in OUTPUT_FIELDS:
        if type(result_properties[key]) is not dict or result_properties[key].get("type") != ("boolean" if key == "success" else "string"):
            raise RuntimeError("Collection-parameter output type changed: " + key)
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {BINDINGS[0]: 3}, "observed_tool_count": len(schema["tools"]),
        "public_runtime_values_bool_default_verified": True, "public_save_bool_default_verified": True,
        "actual_call_packing_verified": False, "immediate_compile_or_saved_outcome_verified": False,
        "whole_original_wait_runtime_contract_verified": False}
