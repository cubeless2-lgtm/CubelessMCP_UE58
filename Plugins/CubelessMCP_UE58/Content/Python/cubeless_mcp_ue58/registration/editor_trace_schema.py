"""Original trace5 logical numeric arrays and private reflected carriers are separate."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import physical_material_trace as api

TOOLSET = "cubeless_mcp_ue58.registration.editor_trace_toolset.CubelessEditorTraceTools"
CLASS_PATH = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/editor_trace_toolset_PY.CubelessEditorTraceTools"
METHOD = "line_trace_physical_material"
BINDINGS = (METHOD,)
PUBLIC_FIELDS_BY_METHOD = {METHOD: ("start", "end", "trace_channel", "trace_complex", "return_physical_material")}
DEFAULTS_BY_METHOD = {METHOD: dict(trace_channel="visibility", trace_complex=False, return_physical_material=True)}
REQUIRED_BY_METHOD = {METHOD: ("start", "end")}
FIELD_KINDS_BY_METHOD = {METHOD: dict(start="array", end="array", trace_channel="string", trace_complex="boolean", return_physical_material="boolean")}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json", "compile_json", "save_json", "validation_json", "retry_json", "data_json")
ATTACHMENT_LIBRARY = "CubelessEditorTraceAttachmentLibrary"
ATTACHMENT_METHODS = ("install_editor_trace_toolset", "verify_editor_trace_toolset", "remove_editor_trace_toolset")


def binding_status(toolset_class):
    methods = api.binding_status()
    defined = {name: callable(getattr(toolset_class, name, None)) for name in BINDINGS}
    return {"ready": False, "bindings_ready": set(methods) == set(BINDINGS) and all(methods.values()) and all(defined.values()),
        "library": api.LIBRARY, "methods": methods, "definition_methods": defined,
        "public_toolset": TOOLSET, "public_tool_count": 1, "public_input_counts": {METHOD: 5},
        "hidden_input_counts": {METHOD: 5}, "vector_carrier_count": 2,
        "schema_verified": False, "default_verified": False, "actual_official_wire_verified": False,
        "vector_Named_wire_verified": False, "raw_guard_required": True, "raw_guard_ready": False,
        "domain_explicit_asset_compile_save_calls": 0, "SDK_purity_verified": False,
        "unique_external_pipeline_complete": False}


def _required(expected, value):
    return type(value) is list and len(value) == len(expected) and all(type(item) is str for item in value) and set(value) == set(expected)


def _validate_schema(schema, toolset_class, reflected_carriers=False):
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 1 or not callable(getattr(toolset_class, METHOD, None))):
        raise RuntimeError("Exact canonical trace Toolset1 required")
    row = schema["tools"][0]
    if type(row) is not dict or row.get("name") != TOOLSET + "." + METHOD:
        raise RuntimeError("Only the original trace method is admitted")
    inputs = row.get("inputSchema"); props = inputs.get("properties") if type(inputs) is dict else None
    if (type(inputs) is not dict or inputs.get("type") != "object" or type(props) is not dict
            or set(props) != set(PUBLIC_FIELDS_BY_METHOD[METHOD]) or not _required(("start", "end"), inputs.get("required"))):
        raise RuntimeError("Exact trace5 fields and required vectors2 differ")
    for field, kind in FIELD_KINDS_BY_METHOD[METHOD].items():
        item = props[field]; carrier = field in ("start", "end")
        if type(item) is not dict or item.get("type") != ("string" if carrier and reflected_carriers else kind):
            raise RuntimeError("Original trace logical type or private carrier differs: " + field)
        if carrier:
            if "default" in item:
                raise RuntimeError("Required vector carriers/arrays cannot gain defaults")
            if not reflected_carriers and (type(item.get("items")) is not dict or item["items"].get("type") != "number"
                    or item.get("minItems") != 3 or item.get("maxItems") != 3):
                raise RuntimeError("Public vectors require exactly3 number items")
        else:
            expected = DEFAULTS_BY_METHOD[METHOD][field]
            if "default" not in item or item["default"] != expected or type(item["default"]) is not type(expected):
                raise RuntimeError("Original trace visibility/False/True defaults differ")
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
        "public_input_counts": {METHOD: 5}, "vector_carrier_count": 2,
        "actual_official_wire_verified": False, "vector_Named_wire_verified": False,
        "metadata_schema_validation_is_wire_packing_proof": False, "unique_external_pipeline_complete": False}


def validate_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class)


def validate_reflected_carrier_schema(schema, toolset_class):
    return _validate_schema(schema, toolset_class, reflected_carriers=True)


def validate_registered_metadata(class_schema, attachment_report, toolset_class):
    reflected = validate_reflected_carrier_schema(class_schema, toolset_class)
    if (type(attachment_report) is not dict
            or attachment_report.get("operation") not in ("install_editor_trace_toolset", "verify_editor_trace_toolset")
            or not all(attachment_report.get(key) is True for key in ("ready", "verified", "active", "guard_attached",
                "raw_guard_installed", "default_verified", "reflected_carrier_schema_verified", "logical_schema_verified",
                "array2_schema_verified", "trace5_schema_verified", "output11_schema_preserved"))
            or attachment_report.get("asset_mutation") is not False or attachment_report.get("metadata_mutation") is not False
            or attachment_report.get("logical_schema_source") != "actual_registered_wrapper_GetJsonSchema"
            or attachment_report.get("logical_schema_registry_identity_verified") is not True):
        raise RuntimeError("Exact ready owner receipt with actual registered trace wrapper schema required")
    logical_text = attachment_report.get("logical_schema_json")
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError("Actual registered trace wrapper schema missing")
    logical_schema = json.loads(logical_text)
    logical = validate_schema(logical_schema, toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True, class_reflected_schema_verified=True,
        class_schema_getter_scope="fresh_FFunctionLibraryToolset_not_registered_handler",
        logical_schema_source="actual_registered_wrapper_GetJsonSchema", logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True, logical_schema_json=logical_text, logical_schema=logical_schema,
        reflected_schema_validation=reflected, logical_schema_validation=logical,
        actual_official_wire_verified=False, vector_Named_wire_verified=False)
