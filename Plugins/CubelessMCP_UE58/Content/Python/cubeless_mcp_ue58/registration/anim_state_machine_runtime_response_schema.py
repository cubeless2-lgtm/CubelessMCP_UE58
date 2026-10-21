"""Public28 logical inputs and physical JSON3/int-decimal6/double-text1 are separate."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import anim_state_machine_runtime_response as api

TOOLSET = 'cubeless_mcp_ue58.registration.anim_state_machine_runtime_response_toolset.CubelessAnimStateRuntimeResponseTools'
CLASS_PATH = '/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/anim_state_machine_runtime_response_toolset_PY.CubelessAnimStateRuntimeResponseTools'
METHOD = 'sample_anim_state_machine_runtime_response'
BINDINGS = (METHOD,)
STRING_FIELDS = ('actor_label', 'actor_name', 'actor_path', 'component_name', 'name', 'state_machine_name')
EMPTY_STRING_FIELDS = ('actor_label', 'actor_name', 'actor_path', 'component_name', 'state_machine_name')
NULLABLE_FIELDS = ('cases', 'properties', 'curve_names')
BOOLEAN_FIELDS = ('refresh_bone_transforms', 'restore_after_case', 'include_baseline', 'include_states', 'include_state_weights', 'include_relevant_anim_times', 'include_transition_progress', 'include_inactive_transition_progress', 'include_montages', 'include_curves', 'prefer_pie_world', 'require_pie_world')
INTEGER_FIELDS = ('tick_count', 'max_state_machines', 'max_state_machine_instances_to_probe', 'max_states_per_machine', 'max_transitions_per_machine', 'max_curves')
NUMBER_FIELDS = ('tick_delta_time',)
PUBLIC_FIELDS_BY_METHOD = {METHOD: ('actor_label', 'actor_name', 'actor_path', 'component_name', 'cases', 'properties', 'name', 'tick_count', 'tick_delta_time', 'refresh_bone_transforms', 'restore_after_case', 'include_baseline', 'state_machine_name', 'include_states', 'include_state_weights', 'include_relevant_anim_times', 'include_transition_progress', 'include_inactive_transition_progress', 'include_montages', 'include_curves', 'curve_names', 'max_state_machines', 'max_state_machine_instances_to_probe', 'max_states_per_machine', 'max_transitions_per_machine', 'max_curves', 'prefer_pie_world', 'require_pie_world')}
DEFAULTS_BY_METHOD = {METHOD: {'actor_label': '', 'actor_name': '', 'actor_path': '', 'component_name': '', 'cases': None, 'properties': None, 'name': 'case_0', 'tick_count': 1, 'tick_delta_time': 0.03333333333333333, 'refresh_bone_transforms': True, 'restore_after_case': True, 'include_baseline': True, 'state_machine_name': '', 'include_states': True, 'include_state_weights': True, 'include_relevant_anim_times': False, 'include_transition_progress': True, 'include_inactive_transition_progress': False, 'include_montages': True, 'include_curves': False, 'curve_names': None, 'max_state_machines': 32, 'max_state_machine_instances_to_probe': 256, 'max_states_per_machine': 64, 'max_transitions_per_machine': 128, 'max_curves': 128, 'prefer_pie_world': True, 'require_pie_world': False}}
REQUIRED_BY_METHOD = {METHOD: ()}
FIELD_KINDS_BY_METHOD = {METHOD: dict.fromkeys(STRING_FIELDS,"string") | dict.fromkeys(BOOLEAN_FIELDS,"boolean") | dict.fromkeys(INTEGER_FIELDS,"integer") | dict.fromkeys(NUMBER_FIELDS,"number") | {"cases":["array","null"],"properties":["object","null"],"curve_names":["array","null"]}}
RESULT_FIELDS = ("success","operation_id","stage","error_code","error","changes_json","compile_json","save_json","validation_json","retry_json","data_json")
ATTACHMENT_LIBRARY = "CubelessAnimStateRuntimeResponseAttachmentLibrary"
ATTACHMENT_METHODS = ("install_anim_state_machine_runtime_response_toolset","verify_anim_state_machine_runtime_response_toolset","remove_anim_state_machine_runtime_response_toolset")
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'output11_schema_preserved', 'ordinary_string6_schema_verified', 'nullable3_schema_verified', 'boolean12_schema_verified', 'integer6_schema_verified', 'number1_schema_verified', 'runtime_response28_schema_verified', 'empty_string_defaults_verified')


def binding_status(toolset_class):
    methods=api.binding_status();defined={name:callable(getattr(toolset_class,name,None)) for name in BINDINGS}
    return {"ready":False,"bindings_ready":set(methods)==set(BINDINGS) and all(v is True for v in methods.values()) and all(defined.values()),
        "library":api.LIBRARY,"methods":methods,"definition_methods":defined,"public_toolset":TOOLSET,"public_tool_count":1,
        "public_input_counts":{METHOD:28},"physical_input_counts":{METHOD:28},"hidden_input_counts":{METHOD:29},
        "nullable_json_carrier_count":3,"decimal_carrier_count":6,"number_text_carrier_count":1,"native_signature_json_carrier_count":3,
        "schema_verified":False,"default_verified":False,"raw_guard_required":True,"raw_guard_ready":False,"nullable_raw_guard_ready":False,
        "boolean_raw_guard_ready":False,"integer_raw_guard_ready":False,"number_raw_guard_ready":False,
        "actual_official_wire_verified":False,"nullable_Named_wire_verified":False,"integer_Named_wire_verified":False,"boolean_Named_wire_verified":False,
        "number_Named_wire_verified":False,"empty_string_Named_wire_verified":False,"metadata_schema_validation_is_wire_packing_proof":False,
        "unique_external_pipeline_complete":False,"packaged_runtime_verified":False}

def _required(expected, value):
    return type(value) is list and len(value) == len(expected) and all(type(item) is str for item in value) and set(value) == set(expected)


def _validate_schema(schema, toolset_class, reflected_carriers=False):
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 1 or not callable(getattr(toolset_class, METHOD, None))):
        raise RuntimeError("Exact canonical AnimInstance runtime response sampler Toolset1 required")
    row = schema["tools"][0]
    if type(row) is not dict or row.get("name") != TOOLSET + "." + METHOD:
        raise RuntimeError("Only the original runtime response method is admitted")
    inputs = row.get("inputSchema"); props = inputs.get("properties") if type(inputs) is dict else None
    if (type(inputs) is not dict or inputs.get("type") != "object" or type(props) is not dict
            or set(props) != set(PUBLIC_FIELDS_BY_METHOD[METHOD])
            or not _required(EMPTY_STRING_FIELDS if reflected_carriers else (), inputs.get("required"))):
        raise RuntimeError("Exact runtime response28 fields and physical-required empty5/name case_0 logical-optional ordinary6 differ")
    for field, kind in FIELD_KINDS_BY_METHOD[METHOD].items():
        item = props[field]; carrier = field in NULLABLE_FIELDS or field in INTEGER_FIELDS or field in NUMBER_FIELDS
        if type(item) is not dict:
            raise RuntimeError("Input property schema must be an object: " + field)
        if item.get("type") != ("string" if carrier and reflected_carriers else kind):
            raise RuntimeError("Original logical type or private JSON/int/double carrier differs: " + field)
        if reflected_carriers and field in EMPTY_STRING_FIELDS:
            if "default" in item:
                raise RuntimeError("SDK physical empty-string defaults must remain absent")
        else:
            expected = DEFAULTS_BY_METHOD[METHOD][field]
            if reflected_carriers and field in NULLABLE_FIELDS:
                expected = "null"
            elif reflected_carriers and field in INTEGER_FIELDS:
                expected = str(expected)
            elif reflected_carriers and field in NUMBER_FIELDS:
                expected = "0.03333333333333333"
            if "default" not in item or type(item["default"]) is not type(expected) or item["default"] != expected:
                raise RuntimeError("Original runtime defaults differ: " + field)
        if field == "curve_names" and not reflected_carriers:
            if item.get("items") != {"type": "string"} or "minItems" in item or "maxItems" in item:
                raise RuntimeError("Optional List[str] preserves None/empty/order for Native selection")
        if field == "cases" and not reflected_carriers:
            if item.get("items") != {"type":"object"} or "minItems" in item or "maxItems" in item:
                raise RuntimeError("Formal nullable case list keeps arbitrary object row content for Native selection")
        if field == "properties" and not reflected_carriers and item.get("additionalProperties") != {}:
            raise RuntimeError("Original properties map values remain Any JSON")
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
        "public_input_counts": {METHOD: 28}, "nullable_json_carrier_count": 3, "decimal_carrier_count": 6, "number_text_carrier_count": 1,
        "native_signature_json_carrier_count": 3, "actual_official_wire_verified": False,
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
            or attachment_report.get("operation") not in ("install_anim_state_machine_runtime_response_toolset", "verify_anim_state_machine_runtime_response_toolset")
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
        nullable_Named_wire_verified=False, integer_Named_wire_verified=False, boolean_Named_wire_verified=False, number_Named_wire_verified=False)
