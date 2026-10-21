"""Required solver-intent request object|string1; physical JSON carrier1 stays private."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import controlrig_solver_intents as api

TOOLSET = "cubeless_mcp_ue58.registration.controlrig_solver_intents_toolset.CubelessControlRigSolverIntentTools"
CLASS_PATH = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/controlrig_solver_intents_toolset_PY.CubelessControlRigSolverIntentTools"
METHOD = "validate_controlrig_solver_intents"
BINDINGS = (METHOD,)
PUBLIC_FIELDS_BY_METHOD = {METHOD: ("request",)}
DEFAULTS_BY_METHOD = {METHOD: {}}
REQUIRED_BY_METHOD = {METHOD: ("request",)}
FIELD_KINDS_BY_METHOD = {METHOD: {"request": ["object", "string"]}}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json", "compile_json", "save_json", "validation_json", "retry_json", "data_json")
ATTACHMENT_LIBRARY = "CubelessControlRigSolverIntentsAttachmentLibrary"
ATTACHMENT_METHODS = ("install_controlrig_solver_intents_toolset", "verify_controlrig_solver_intents_toolset", "remove_controlrig_solver_intents_toolset")
ATTACHMENT_READY_FIELDS = ("ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "request_union1_schema_verified", "request_required_verified", "output11_schema_preserved")


def binding_status(toolset_class):
    methods = api.binding_status()
    available = (type(methods) is dict and set(methods) == set(BINDINGS) and methods.get(METHOD) is True
        and callable(getattr(api, METHOD, None)))
    defined = callable(getattr(toolset_class, METHOD, None))
    return {"ready": False, "bindings_ready": available and defined,
        "library": None, "validator": api.VALIDATOR, "methods": methods, "definition_methods": {METHOD: defined},
        "public_toolset": TOOLSET, "public_tool_count": 1, "public_input_counts": {METHOD: 1}, "hidden_input_counts": {METHOD: 1},
        "pure_python_validator": True, "native_operation_library_required": False, "engine_validator_required": False,
        "json_union_carrier_count": 1, "decimal_carrier_count": 0,
        "schema_verified": False, "default_verified": False, "actual_official_wire_verified": False,
        "union_Named_wire_verified": False, "raw_guard_required": True, "raw_guard_ready": False, "union_raw_guard_ready": False,
        "registration_helper_metadata_queries_are_validator_engine_work": False,
        "metadata_schema_validation_is_wire_packing_proof": False, "unique_external_pipeline_complete": False}


def _required(expected, value):
    return type(value) is list and len(value) == len(expected) and all(type(item) is str for item in value) and set(value) == set(expected)


def _validate_schema(schema, toolset_class, reflected_carriers=False):
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 1 or not callable(getattr(toolset_class, METHOD, None))):
        raise RuntimeError("Exact canonical solver-intent validation Toolset1 required")
    row = schema["tools"][0]
    if type(row) is not dict or row.get("name") != TOOLSET + "." + METHOD:
        raise RuntimeError("Only the original solver-intent validation method is admitted")
    inputs = row.get("inputSchema"); props = inputs.get("properties") if type(inputs) is dict else None
    if (type(inputs) is not dict or inputs.get("type") != "object" or type(props) is not dict
            or set(props) != {"request"} or not _required(("request",), inputs.get("required"))):
        raise RuntimeError("Exactly one required request input is required in physical and logical schemas")
    request = props["request"]
    if (type(request) is not dict or request.get("type") != ("string" if reflected_carriers else ["object", "string"])
            or "default" in request):
        raise RuntimeError("Request must remain required without a default; public object|string differs from its private FString")
    if not reflected_carriers and any(key in request for key in ("properties", "required", "additionalProperties", "minProperties", "maxProperties")):
        raise RuntimeError("Request object contents and request-string parsing must remain domain authority")
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
        "public_input_counts": {METHOD: 1}, "json_union_carrier_count": 1,
        "required_request_without_default_verified": True, "actual_official_wire_verified": False,
        "union_Named_wire_verified": False, "pure_python_validator": True, "native_operation_library_required": False,
        "metadata_schema_validation_is_wire_packing_proof": False, "unique_external_pipeline_complete": False}


def validate_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class)


def validate_reflected_carrier_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class, reflected_carriers=True)


def validate_registered_metadata(class_schema, attachment_report, toolset_class):
    reflected = validate_reflected_carrier_schema(class_schema, toolset_class)
    if (type(attachment_report) is not dict
            or attachment_report.get("operation") not in ("install_controlrig_solver_intents_toolset", "verify_controlrig_solver_intents_toolset")
            or not all(attachment_report.get(key) is True for key in ATTACHMENT_READY_FIELDS)
            or attachment_report.get("asset_mutation") is not False or attachment_report.get("metadata_mutation") is not False
            or attachment_report.get("logical_schema_source") != "actual_registered_wrapper_GetJsonSchema"
            or attachment_report.get("logical_schema_registry_identity_verified") is not True):
        raise RuntimeError("Exact ready owner receipt with actual registered solver-intent union schema required")
    logical_text = attachment_report.get("logical_schema_json")
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError("Actual registered solver-intent wrapper schema missing")
    logical_schema = json.loads(logical_text)
    logical = validate_schema(logical_schema, toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope="fresh_FFunctionLibraryToolset_not_registered_handler",
        logical_schema_source="actual_registered_wrapper_GetJsonSchema", logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text, logical_schema=logical_schema,
        reflected_schema_validation=reflected, logical_schema_validation=logical,
        actual_official_wire_verified=False, union_Named_wire_verified=False)
