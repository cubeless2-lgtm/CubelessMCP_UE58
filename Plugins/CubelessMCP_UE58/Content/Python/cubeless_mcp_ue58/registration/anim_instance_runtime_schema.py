"""Original AnimInstance runtime20 inputs, reflected carriers and actual Registry schema are separate."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import anim_instance_runtime_state as api

TOOLSET = "cubeless_mcp_ue58.registration.anim_instance_runtime_toolset.CubelessAnimInstanceRuntimeTools"
CLASS_PATH = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/anim_instance_runtime_toolset_PY.CubelessAnimInstanceRuntimeTools"
METHOD = "inspect_anim_instance_runtime_state"
BINDINGS = (METHOD,)
STRING_FIELDS = ("actor_label", "actor_name", "actor_path", "component_name", "state_machine_name")
NULLABLE_FIELDS = ("curve_names",)
BOOLEAN_FIELDS = ("include_states", "include_state_weights", "include_relevant_anim_times", "include_transition_progress", "include_inactive_transition_progress", "include_montages", "include_curves", "prefer_pie_world", "require_pie_world")
INTEGER_FIELDS = ("max_state_machines", "max_state_machine_instances_to_probe", "max_states_per_machine", "max_transitions_per_machine", "max_curves")
PUBLIC_FIELDS_BY_METHOD = {METHOD: ("actor_label", "actor_name", "actor_path", "component_name", "state_machine_name", "include_states", "include_state_weights", "include_relevant_anim_times", "include_transition_progress", "include_inactive_transition_progress", "include_montages", "include_curves", "curve_names", "max_state_machines", "max_state_machine_instances_to_probe", "max_states_per_machine", "max_transitions_per_machine", "max_curves", "prefer_pie_world", "require_pie_world")}
DEFAULTS_BY_METHOD = {METHOD: {"actor_label": "", "actor_name": "", "actor_path": "", "component_name": "", "state_machine_name": "", "include_states": True, "include_state_weights": True, "include_relevant_anim_times": False, "include_transition_progress": True, "include_inactive_transition_progress": False, "include_montages": True, "include_curves": False, "curve_names": None, "max_state_machines": 32, "max_state_machine_instances_to_probe": 256, "max_states_per_machine": 64, "max_transitions_per_machine": 128, "max_curves": 128, "prefer_pie_world": True, "require_pie_world": False}}
REQUIRED_BY_METHOD = {METHOD: ()}
FIELD_KINDS_BY_METHOD = {METHOD: dict.fromkeys(STRING_FIELDS, "string") | dict.fromkeys(BOOLEAN_FIELDS, "boolean")
    | dict.fromkeys(INTEGER_FIELDS, "integer") | {"curve_names": ["array", "null"]}}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json", "compile_json", "save_json", "validation_json", "retry_json", "data_json")
ATTACHMENT_LIBRARY = "CubelessAnimInstanceRuntimeAttachmentLibrary"
ATTACHMENT_METHODS = ("install_anim_instance_runtime_toolset", "verify_anim_instance_runtime_toolset", "remove_anim_instance_runtime_toolset")
ATTACHMENT_READY_FIELDS = ("ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified", "string5_schema_verified", "nullable1_schema_verified", "boolean9_schema_verified", "integer5_schema_verified", "runtime20_schema_verified", "empty_string_defaults_verified", "output11_schema_preserved")


def binding_status(toolset_class):
    methods = api.binding_status()
    defined = {name: callable(getattr(toolset_class, name, None)) for name in BINDINGS}
    return {"ready": False, "bindings_ready": set(methods) == set(BINDINGS) and all(methods.values()) and all(defined.values()),
        "library": api.LIBRARY, "methods": methods, "definition_methods": defined, "public_toolset": TOOLSET,
        "public_tool_count": 1, "public_input_counts": {METHOD: 20}, "hidden_input_counts": {METHOD: 20},
        "nullable_json_carrier_count": 1, "decimal_carrier_count": 5, "native_signature_json_carrier_count": 1,
        "schema_verified": False, "default_verified": False, "actual_official_wire_verified": False,
        "empty_string_Named_wire_verified": False, "nullable_Named_wire_verified": False,
        "integer_Named_wire_verified": False, "boolean_Named_wire_verified": False,
        "raw_guard_required": True, "raw_guard_ready": False, "nullable_raw_guard_ready": False,
        "integer_raw_guard_ready": False, "boolean_raw_guard_ready": False,
        "metadata_schema_validation_is_wire_packing_proof": False, "unique_external_pipeline_complete": False}


def _required(expected, value):
    return type(value) is list and len(value) == len(expected) and all(type(item) is str for item in value) and set(value) == set(expected)


def _validate_schema(schema, toolset_class, reflected_carriers=False):
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 1 or not callable(getattr(toolset_class, METHOD, None))):
        raise RuntimeError("Exact canonical AnimInstance runtime Toolset1 required")
    row = schema["tools"][0]
    if type(row) is not dict or row.get("name") != TOOLSET + "." + METHOD:
        raise RuntimeError("Only the original runtime inspection method is admitted")
    inputs = row.get("inputSchema"); props = inputs.get("properties") if type(inputs) is dict else None
    if (type(inputs) is not dict or inputs.get("type") != "object" or type(props) is not dict
            or set(props) != set(PUBLIC_FIELDS_BY_METHOD[METHOD])
            or not _required(STRING_FIELDS if reflected_carriers else (), inputs.get("required"))):
        raise RuntimeError("Exact runtime20 fields and physical-required/logical-optional string5 differ")
    for field, kind in FIELD_KINDS_BY_METHOD[METHOD].items():
        item = props[field]; carrier = field in NULLABLE_FIELDS or field in INTEGER_FIELDS
        if type(item) is not dict or item.get("type") != ("string" if carrier and reflected_carriers else kind):
            raise RuntimeError("Original runtime logical type or private carrier differs: " + field)
        if reflected_carriers and field in STRING_FIELDS:
            if "default" in item:
                raise RuntimeError("SDK physical empty-string defaults must remain absent")
        else:
            expected = DEFAULTS_BY_METHOD[METHOD][field]
            if reflected_carriers and field in NULLABLE_FIELDS:
                expected = "null"
            elif reflected_carriers and field in INTEGER_FIELDS:
                expected = str(expected)
            if "default" not in item or type(item["default"]) is not type(expected) or item["default"] != expected:
                raise RuntimeError("Original runtime defaults differ: " + field)
        if field in NULLABLE_FIELDS and not reflected_carriers:
            if item.get("items") != {"type": "string"} or "minItems" in item or "maxItems" in item:
                raise RuntimeError("Optional List[str] preserves None, empty arrays, names and Native selection")
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
        "public_input_counts": {METHOD: 20}, "nullable_json_carrier_count": 1, "decimal_carrier_count": 5,
        "native_signature_json_carrier_count": 1, "actual_official_wire_verified": False,
        "empty_string_Named_wire_verified": False, "nullable_Named_wire_verified": False,
        "integer_Named_wire_verified": False, "boolean_Named_wire_verified": False,
        "metadata_schema_validation_is_wire_packing_proof": False, "unique_external_pipeline_complete": False}


def validate_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class)


def validate_reflected_carrier_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class, reflected_carriers=True)


def validate_registered_metadata(class_schema, attachment_report, toolset_class):
    reflected = validate_reflected_carrier_schema(class_schema, toolset_class)
    if (type(attachment_report) is not dict
            or attachment_report.get("operation") not in ("install_anim_instance_runtime_toolset", "verify_anim_instance_runtime_toolset")
            or not all(attachment_report.get(key) is True for key in ATTACHMENT_READY_FIELDS)
            or attachment_report.get("asset_mutation") is not False or attachment_report.get("metadata_mutation") is not False
            or attachment_report.get("logical_schema_source") != "actual_registered_wrapper_GetJsonSchema"
            or attachment_report.get("logical_schema_registry_identity_verified") is not True):
        raise RuntimeError("Exact ready owner receipt with actual registered runtime wrapper schema required")
    logical_text = attachment_report.get("logical_schema_json")
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError("Actual registered runtime wrapper schema missing")
    logical_schema = json.loads(logical_text)
    logical = validate_schema(logical_schema, toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope="fresh_FFunctionLibraryToolset_not_registered_handler",
        logical_schema_source="actual_registered_wrapper_GetJsonSchema", logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text, logical_schema=logical_schema,
        reflected_schema_validation=reflected, logical_schema_validation=logical,
        actual_official_wire_verified=False, empty_string_Named_wire_verified=False,
        nullable_Named_wire_verified=False, integer_Named_wire_verified=False, boolean_Named_wire_verified=False)
