"""Exact original sync7 and attached three-field default preparation contract."""
from cubeless_mcp_ue58.compatibility.ue58 import material_collection_sync as api

BINDINGS = api.BINDINGS
FIELDS = {"action": ("string", "status"), "source_collection_path": ("string", ""), "target_collection_path": ("string", ""),
    "parameter_names": (["array", "null"], None), "interval_seconds": ("number", 0.1),
    "set_asset_defaults_on_enable": ("boolean", False), "save_on_enable": ("boolean", False)}
EXPECTED_DEFAULTS = {BINDINGS[0] + ".source_collection_path": "", BINDINGS[0] + ".target_collection_path": "",
    BINDINGS[0] + ".parameter_names": None}
PREPARATION_METHOD = api.PREPARATION_METHOD
OUTPUT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")
PREPARATION_CLASS = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/material_toolset_PY.CubelessMaterialTools"


def validate_preparation(receipt):
    """Validate an already retained native prep receipt; never invoke or retry prep."""
    if type(receipt) is not dict or receipt.get("operation") != PREPARATION_METHOD or receipt.get("class_path") != PREPARATION_CLASS:
        raise RuntimeError("Exact returned Material sync preparation receipt required")
    for key in ("success", "postcondition_passed", "default_verified", "exact_converter_active",
            "type_schema_verified", "nullable_schema_verified", "change_state_known"):
        if receipt.get(key) is not True:
            raise RuntimeError("Material sync preparation proof missing: " + key)
    for key in ("mutation_attempted", "changed", "asset_mutation_attempted", "compile_requested", "compile_attempted",
            "save_requested", "save_attempted", "saved", "outcome_unknown", "actual_call_packing_verified"):
        if receipt.get(key) is not False:
            raise RuntimeError("Material sync preparation common zero/packing scope changed: " + key)
    if (receipt.get("compiled", False) is not None or receipt.get("error_code") != "" or receipt.get("error") != ""
            or receipt.get("stage") != "material_collection_sync_schema_preparation"):
        raise RuntimeError("Material sync preparation error/compiler fields changed")
    for key, expected in (("property_count", 3), ("verified_property_count", 3), ("verified_parameter_count", 3),
            ("verification_helper_attempt_count", 1), ("verification_helper_return_count", 1)):
        if type(receipt.get(key)) not in (int, float) or receipt[key] != expected:
            raise RuntimeError("Material sync preparation count changed: " + key)
    defaults = receipt.get("defaults")
    if type(defaults) is not dict or set(defaults) != set(EXPECTED_DEFAULTS):
        raise RuntimeError("Material sync preparation defaults3 required")
    for key, expected in EXPECTED_DEFAULTS.items():
        if type(defaults[key]) is not type(expected) or defaults[key] != expected:
            raise RuntimeError("Material sync preparation exact default changed: " + key)
    rows = receipt.get("property_observations")
    expected_names = {"source_collection_path", "target_collection_path", "parameter_names"}
    if type(rows) is not list or len(rows) != 3 or any(type(row) is not dict for row in rows):
        raise RuntimeError("Material sync preparation raw rows3 required")
    if {row.get("property") for row in rows} != expected_names:
        raise RuntimeError("Material sync preparation property names changed")
    attempts = changed = 0
    for row in rows:
        name = row["property"]
        for key in ("exact_converter_active", "type_schema_verified", "default_verified"):
            if row.get(key) is not True:
                raise RuntimeError("Material sync preparation row proof missing: " + name + "." + key)
        schema = row.get("type_schema")
        expected_type = ["array", "null"] if name == "parameter_names" else "string"
        if type(schema) is not dict or schema.get("type") != expected_type:
            raise RuntimeError("Material sync preparation row schema changed: " + name)
        if name == "parameter_names" and schema.get("items", {}).get("type") != "string":
            raise RuntimeError("Material sync preparation names items type changed")
        expected = None if name == "parameter_names" else ""
        if "public_default" not in row or type(row["public_default"]) is not type(expected) or row["public_default"] != expected:
            raise RuntimeError("Material sync preparation row default changed: " + name)
        for key in ("metadata_write_attempted", "metadata_write_returned"):
            if key in row and type(row[key]) is not bool:
                raise RuntimeError("Material sync preparation row RAM counter type changed")
        if type(row.get("metadata_before")) is not str or type(row.get("metadata_after")) is not str:
            raise RuntimeError("Material sync preparation raw RAM metadata strings required")
        attempted = row.get("metadata_write_attempted") is True
        returned = row.get("metadata_write_returned") is True
        if attempted is not returned:
            raise RuntimeError("Material sync preparation returned write proof missing")
        attempts += attempted
        changed += row["metadata_before"] != row["metadata_after"]
    for key in ("metadata_write_attempt_count", "metadata_changed_count"):
        if type(receipt.get(key)) not in (int, float):
            raise RuntimeError("Material sync preparation RAM count type changed")
    if receipt.get("metadata_write_attempt_count") != attempts or receipt.get("metadata_changed_count") != changed or not 0 <= changed <= attempts <= 3:
        raise RuntimeError("Material sync preparation RAM counts differ from raw rows")
    if receipt.get("metadata_mutation_attempted") is not (attempts > 0) or receipt.get("metadata_changed") is not (changed > 0):
        raise RuntimeError("Material sync preparation separate RAM flags differ")
    return {"preparation_verified": True, "nullable_schema_verified": True, "verified_parameter_count": 3,
        "metadata_write_attempt_count": attempts, "metadata_changed_count": changed,
        "actual_call_packing_verified": False, "controller_behavior_verified": False}


def binding_status():
    methods = api.binding_status()
    prep = api.preparation_available()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()) and prep,
        "library": api.LIBRARY, "methods": methods, "preparation_required": True,
        "preparation_method": PREPARATION_METHOD, "preparation_method_available": prep,
        "prepared_defaults": dict(EXPECTED_DEFAULTS), "schema_verified": False,
        "nullable_schema_verified": False, "actual_call_packing_verified": False, "behavior_verified": False,
        "controller_activation_or_current_job_observed": False, "public_tool_count": 1, "public_input_count": 7,
        "verification_scope": "Native callable plus prep binding only; actual preparation/schema/job lifecycle/packing are independent gates"}


def validate_schema(schema):
    if type(schema) is not dict or type(schema.get("tools")) is not list:
        raise RuntimeError("Actual canonical Material schema required")
    rows = [row for row in schema["tools"] if type(row) is dict and type(row.get("name")) is str
        and row["name"].rsplit(".", 1)[-1] == BINDINGS[0]]
    if len(rows) != 1:
        raise RuntimeError("Original Material sync7 missing or duplicated")
    inputs = rows[0].get("inputSchema")
    properties = inputs.get("properties") if type(inputs) is dict else None
    if type(properties) is not dict or set(properties) != set(FIELDS) or inputs.get("required", []) != []:
        raise RuntimeError("Original sync7 optional field/default set changed")
    for key, expected in FIELDS.items():
        field = properties[key]
        if type(field) is not dict or field.get("type") != expected[0]:
            raise RuntimeError("Original sync input type changed: " + key)
        if key == "parameter_names":
            if field.get("items", {}).get("type") != "string" or "default" not in field or field["default"] is not None:
                raise RuntimeError("Exact nullable string-array/defaultNull required")
        elif (field.get("nullable") is True or "default" not in field or type(field["default"]) is not type(expected[1])
                or field["default"] != expected[1]):
            raise RuntimeError("Original sync default/nonnullable type changed: " + key)
    output = rows[0].get("outputSchema")
    returned = output.get("properties", {}).get("returnValue") if type(output) is dict else None
    fields = returned.get("properties") if type(returned) is dict else None
    if type(returned) is not dict or returned.get("type") != "object" or type(fields) is not dict or set(fields) != set(OUTPUT_FIELDS):
        raise RuntimeError("Exact standard reflected operation output required")
    for key in OUTPUT_FIELDS:
        if type(fields[key]) is not dict or fields[key].get("type") != ("boolean" if key == "success" else "string"):
            raise RuntimeError("Sync output type changed: " + key)
    return {"schema_verified": True, "nullable_schema_verified": True, "validated_tool_names": list(BINDINGS),
        "validated_input_counts": {BINDINGS[0]: 7}, "observed_tool_count": len(schema["tools"]),
        "original_defaults_verified": True, "actual_call_packing_verified": False,
        "sync_behavior_or_future_tick_completion_verified": False}
