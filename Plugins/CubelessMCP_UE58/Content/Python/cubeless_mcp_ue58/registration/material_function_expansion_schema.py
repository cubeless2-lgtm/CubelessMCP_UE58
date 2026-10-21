"""Original expansion7 and one exact empty node_id default preparation contract."""
from cubeless_mcp_ue58.compatibility.ue58 import material_function_expansion as api

BINDINGS = api.BINDINGS
TOOLSET = "cubeless_mcp_ue58.registration.material_toolset.CubelessMaterialTools"
FIELDS = {"material_path": ("string",), "node_id": ("string", ""), "recursive": ("boolean", True),
    "exclude_engine_functions": ("boolean", True), "max_passes": ("integer", 8), "save": ("boolean", True),
    "allow_partial_save": ("boolean", False)}
EXPECTED_DEFAULTS = {BINDINGS[0] + ".node_id": ""}
PREPARATION_METHOD = api.PREPARATION_METHOD
PREPARATION_CLASS = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/material_toolset_PY.CubelessMaterialTools"
OUTPUT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def validate_preparation(receipt):
    """Validate an already stored exact receipt, never perform or retry preparation."""
    if type(receipt) is not dict or receipt.get("operation") != PREPARATION_METHOD or receipt.get("class_path") != PREPARATION_CLASS:
        raise RuntimeError("Exact Material function expansion preparation receipt required")
    for key in ("success", "postcondition_passed", "change_state_known", "default_verified", "exact_converter_active", "type_schema_verified"):
        if receipt.get(key) is not True:
            raise RuntimeError("Expansion preparation proof missing: " + key)
    for key in ("mutation_attempted", "changed", "asset_mutation_attempted", "compile_requested", "compile_attempted",
            "save_requested", "save_attempted", "saved", "outcome_unknown", "actual_call_packing_verified"):
        if receipt.get(key) is not False:
            raise RuntimeError("Expansion preparation common zero/packing scope changed: " + key)
    if (receipt.get("compiled", False) is not None or receipt.get("error_code") != "" or receipt.get("error") != ""
            or receipt.get("stage") != "material_function_expansion_schema_preparation"):
        raise RuntimeError("Expansion preparation compiler/error/stage differs")
    for key, expected in (("property_count", 1), ("verified_property_count", 1), ("verified_parameter_count", 1),
            ("verification_helper_attempt_count", 1), ("verification_helper_return_count", 1)):
        if type(receipt.get(key)) not in (int, float) or receipt[key] != expected:
            raise RuntimeError("Expansion preparation count differs: " + key)
    defaults = receipt.get("defaults")
    if type(defaults) is not dict or set(defaults) != set(EXPECTED_DEFAULTS) or any(type(defaults[k]) is not str or defaults[k] != "" for k in defaults):
        raise RuntimeError("Expansion exact empty node_id default required")
    rows = receipt.get("property_observations")
    if type(rows) is not list or len(rows) != 1 or type(rows[0]) is not dict or rows[0].get("property") != "node_id":
        raise RuntimeError("Expansion raw node_id observation required")
    row = rows[0]
    for key in ("default_verified", "exact_converter_active", "type_schema_verified"):
        if row.get(key) is not True:
            raise RuntimeError("Expansion node_id proof missing: " + key)
    if (row.get("type_schema", {}).get("type") != "string" or "public_default" not in row
            or type(row["public_default"]) is not str or row["public_default"] != ""
            or type(row.get("metadata_before")) is not str or type(row.get("metadata_after")) is not str):
        raise RuntimeError("Expansion node_id schema/default/raw metadata differs")
    if type(row.get("metadata_write_attempted")) is not bool or type(row.get("metadata_write_returned")) is not bool:
        raise RuntimeError("Expansion node_id RAM write flags missing")
    attempted = row["metadata_write_attempted"]
    changed = row["metadata_before"] != row["metadata_after"]
    if attempted is not row["metadata_write_returned"] or (changed and not attempted):
        raise RuntimeError("Expansion preparation actual RAM prefix incomplete")
    for key, expected in (("metadata_write_attempt_count", int(attempted)), ("metadata_changed_count", int(changed))):
        if type(receipt.get(key)) not in (int, float) or receipt[key] != expected:
            raise RuntimeError("Expansion preparation RAM count differs: " + key)
    if receipt.get("metadata_mutation_attempted") is not attempted or receipt.get("metadata_changed") is not changed:
        raise RuntimeError("Expansion preparation separate RAM flags differ")
    return {"preparation_verified": True, "type_schema_verified": True, "verified_parameter_count": 1,
        "metadata_write_attempt_count": int(attempted), "metadata_changed_count": int(changed),
        "actual_call_packing_verified": False, "expansion_behavior_verified": False}


def binding_status():
    methods = api.binding_status(); prep = api.preparation_available()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and prep,
        "library": api.LIBRARY, "methods": methods, "preparation_required": True,
        "preparation_method": PREPARATION_METHOD, "preparation_method_available": prep,
        "prepared_defaults": dict(EXPECTED_DEFAULTS), "schema_verified": False,
        "type_schema_verified": False, "actual_call_packing_verified": False, "behavior_verified": False,
        "public_tool_count": 1, "public_input_count": 7,
        "verification_scope": "Callable operation/preparation bindings only; exact actual prep/schema/packing/expansion remain independent gates"}


def validate_schema(schema):
    if type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Material schema required")
    rows = [r for r in schema["tools"] if type(r) is dict and type(r.get("name")) is str
        and r["name"].rsplit(".", 1)[-1] == BINDINGS[0]]
    if len(rows) != 1 or rows[0]["name"] != TOOLSET + "." + BINDINGS[0]:
        raise RuntimeError("Original expand_material_function_calls missing or duplicated")
    inputs = rows[0].get("inputSchema")
    props = inputs.get("properties") if type(inputs) is dict else None
    if (type(props) is not dict or inputs.get("type") != "object" or set(props) != set(FIELDS)
            or inputs.get("required", []) != ["material_path"]):
        raise RuntimeError("Original expansion7 fields/required path differ")
    for key, expected in FIELDS.items():
        row = props[key]
        if type(row) is not dict or row.get("type") != expected[0] or row.get("nullable") is True:
            raise RuntimeError("Original expansion input type differs: " + key)
        if len(expected) == 1:
            if "default" in row: raise RuntimeError("Required Material path must not be defaulted")
        elif "default" not in row or type(row["default"]) is not type(expected[1]) or row["default"] != expected[1]:
            raise RuntimeError("Original expansion exact default differs: " + key)
    output = rows[0].get("outputSchema")
    outer = output.get("properties") if type(output) is dict else None
    if (type(output) is not dict or output.get("type") != "object" or type(outer) is not dict
            or set(outer) != {"returnValue"} or output.get("required") != ["returnValue"]):
        raise RuntimeError("Exact reflected expansion outer returnValue envelope required")
    returned = outer["returnValue"]
    props = returned.get("properties") if type(returned) is dict else None
    if type(returned) is not dict or returned.get("type") != "object" or type(props) is not dict or set(props) != set(OUTPUT_FIELDS):
        raise RuntimeError("Exact reflected expansion output11 required")
    required = returned.get("required")
    if type(required) is not list or len(required) != len(OUTPUT_FIELDS) or set(required) != set(OUTPUT_FIELDS):
        raise RuntimeError("Every reflected expansion output field must be required once")
    for key in OUTPUT_FIELDS:
        if type(props[key]) is not dict or props[key].get("type") != ("boolean" if key == "success" else "string"):
            raise RuntimeError("Expansion output field type differs: " + key)
    return {"schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {BINDINGS[0]: 7}, "observed_tool_count": len(schema["tools"]),
        "original_defaults_verified": True, "actual_call_packing_verified": False,
        "all_nodes_expanded_shader_save_or_runtime_verified": False}
