"""Original pin-default validation7 logical inputs and private JSON3/decimal1 are separate."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import rigvm_pin_default_validation as api

TOOLSET = "cubeless_mcp_ue58.registration.rigvm_pin_default_validation_toolset.CubelessRigVMPinDefaultValidationTools"
CLASS_PATH = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/rigvm_pin_default_validation_toolset_PY.CubelessRigVMPinDefaultValidationTools"
METHOD = "validate_controlrig_rigvm_pin_defaults"
BINDINGS = (METHOD,)
PATH_FIELDS = ("control_rig_path", "control_rig_asset_path", "control_rig_class_path")
NULLABLE_FIELDS = ("pin_defaults", "defaults", "pins")
PUBLIC_FIELDS_BY_METHOD = {METHOD: PATH_FIELDS + NULLABLE_FIELDS + ("max_pins",)}
DEFAULTS_BY_METHOD = {METHOD: dict(control_rig_path="", control_rig_asset_path="", control_rig_class_path="",
    pin_defaults=None, defaults=None, pins=None, max_pins=64)}
REQUIRED_BY_METHOD = {METHOD: ()}
FIELD_KINDS_BY_METHOD = {METHOD: dict.fromkeys(PATH_FIELDS, "string") | dict(pin_defaults=["array", "null"],
    defaults=["array", "null"], pins=["array", "null"], max_pins="integer")}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json", "compile_json", "save_json", "validation_json", "retry_json", "data_json")
ATTACHMENT_LIBRARY = "CubelessRigVMPinDefaultValidationAttachmentLibrary"
ATTACHMENT_METHODS = ("install_rigvm_pin_default_validation_toolset", "verify_rigvm_pin_default_validation_toolset", "remove_rigvm_pin_default_validation_toolset")
ATTACHMENT_READY_FIELDS = ("ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified",
    "reflected_carrier_schema_verified", "logical_schema_verified", "path3_schema_verified", "nullable3_schema_verified",
    "integer1_schema_verified", "pin_default_validation7_schema_verified", "empty_path_defaults_verified", "output11_schema_preserved")


def binding_status(toolset_class):
    methods = api.binding_status()
    defined = {name: callable(getattr(toolset_class, name, None)) for name in BINDINGS}
    return {"ready": False, "bindings_ready": set(methods) == set(BINDINGS) and all(methods.values()) and all(defined.values()),
        "library": api.LIBRARY, "methods": methods, "definition_methods": defined, "public_toolset": TOOLSET,
        "public_tool_count": 1, "public_input_counts": {METHOD: 7}, "hidden_input_counts": {METHOD: 7},
        "nullable_json_carrier_count": 3, "decimal_carrier_count": 1, "native_signature_json_carrier_count": 3,
        "schema_verified": False, "default_verified": False, "actual_official_wire_verified": False,
        "empty_path_Named_wire_verified": False, "nullable_Named_wire_verified": False, "integer_Named_wire_verified": False,
        "raw_guard_required": True, "raw_guard_ready": False, "nullable_raw_guard_ready": False, "integer_raw_guard_ready": False,
        "metadata_schema_validation_is_wire_packing_proof": False, "unique_external_pipeline_complete": False}


def _required(expected, value):
    return type(value) is list and len(value) == len(expected) and all(type(item) is str for item in value) and set(value) == set(expected)


def _validate_schema(schema, toolset_class, reflected_carriers=False):
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 1 or not callable(getattr(toolset_class, METHOD, None))):
        raise RuntimeError("Exact canonical RigVM pin-default validation Toolset1 required")
    row = schema["tools"][0]
    if type(row) is not dict or row.get("name") != TOOLSET + "." + METHOD:
        raise RuntimeError("Only the original pin-default validation method is admitted")
    inputs = row.get("inputSchema"); props = inputs.get("properties") if type(inputs) is dict else None
    if (type(inputs) is not dict or inputs.get("type") != "object" or type(props) is not dict
            or set(props) != set(PUBLIC_FIELDS_BY_METHOD[METHOD])
            or not _required(PATH_FIELDS if reflected_carriers else (), inputs.get("required"))):
        raise RuntimeError("Exact pin-default validation7 fields and physical-required/logical-optional path3 differ")
    for field, kind in FIELD_KINDS_BY_METHOD[METHOD].items():
        item = props[field]; carrier = field in NULLABLE_FIELDS or field == "max_pins"
        if type(item) is not dict or item.get("type") != ("string" if carrier and reflected_carriers else kind):
            raise RuntimeError("Original pin-default validation logical type or private carrier differs: " + field)
        if reflected_carriers and field in PATH_FIELDS:
            if "default" in item:
                raise RuntimeError("SDK physical empty-path defaults must remain absent")
        else:
            expected = DEFAULTS_BY_METHOD[METHOD][field]
            if reflected_carriers and field in NULLABLE_FIELDS:
                expected = "null"
            elif reflected_carriers and field == "max_pins":
                expected = "64"
            if "default" not in item or type(item["default"]) is not type(expected) or item["default"] != expected:
                raise RuntimeError("Original pin-default validation defaults differ: " + field)
        if field in NULLABLE_FIELDS and not reflected_carriers:
            if type(item.get("items")) is not dict or item["items"] or "minItems" in item or "maxItems" in item:
                raise RuntimeError("List[Any] must preserve Native selected-prefix, empty-array and ignored-tail semantics")
    output = row.get("outputSchema"); outer = output.get("properties") if type(output) is dict else None
    if (type(output) is not dict or output.get("type") != "object" or type(outer) is not dict or set(outer) != {"returnValue"}
            or not _required(("returnValue",), output.get("required"))):
        raise RuntimeError("Exact returnValue outer output envelope required")
    result = outer["returnValue"]; fields = result.get("properties") if type(result) is dict else None
    if (type(result) is not dict or result.get("type") != "object" or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)
            or not _required(RESULT_FIELDS, result.get("required"))
            or any(type(fields[key]) is not dict or fields[key].get("type") != ("boolean" if key == "success" else "string") for key in RESULT_FIELDS)):
        raise RuntimeError("Exact synchronous standard11 output fields/types required")
    return {"schema_verified": not reflected_carriers, "default_verified": not reflected_carriers,
        "reflected_carrier_schema_verified": reflected_carriers, "logical_adapter_schema_verified": not reflected_carriers,
        "public_input_counts": {METHOD: 7}, "nullable_json_carrier_count": 3, "decimal_carrier_count": 1,
        "native_signature_json_carrier_count": 3, "actual_official_wire_verified": False,
        "empty_path_Named_wire_verified": False, "nullable_Named_wire_verified": False, "integer_Named_wire_verified": False,
        "metadata_schema_validation_is_wire_packing_proof": False, "unique_external_pipeline_complete": False}


def validate_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class)


def validate_reflected_carrier_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class, reflected_carriers=True)


def validate_registered_metadata(class_schema, attachment_report, toolset_class):
    reflected = validate_reflected_carrier_schema(class_schema, toolset_class)
    if (type(attachment_report) is not dict
            or attachment_report.get("operation") not in ("install_rigvm_pin_default_validation_toolset", "verify_rigvm_pin_default_validation_toolset")
            or not all(attachment_report.get(key) is True for key in ATTACHMENT_READY_FIELDS)
            or attachment_report.get("asset_mutation") is not False or attachment_report.get("metadata_mutation") is not False
            or attachment_report.get("logical_schema_source") != "actual_registered_wrapper_GetJsonSchema"
            or attachment_report.get("logical_schema_registry_identity_verified") is not True):
        raise RuntimeError("Exact ready owner receipt with actual registered pin-default validation wrapper schema required")
    logical_text = attachment_report.get("logical_schema_json")
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError("Actual registered pin-default validation wrapper schema missing")
    logical_schema = json.loads(logical_text)
    logical = validate_schema(logical_schema, toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope="fresh_FFunctionLibraryToolset_not_registered_handler",
        logical_schema_source="actual_registered_wrapper_GetJsonSchema", logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text, logical_schema=logical_schema,
        reflected_schema_validation=reflected, logical_schema_validation=logical,
        actual_official_wire_verified=False, empty_path_Named_wire_verified=False,
        nullable_Named_wire_verified=False, integer_Named_wire_verified=False)
