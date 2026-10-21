"""Original Project Input3/Audit14/Reference12 schemas; private carriers remain hidden."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import input_mapping as api
from cubeless_mcp_ue58.compatibility.ue58 import content_audit as audit_api
from cubeless_mcp_ue58.compatibility.ue58 import asset_reference as reference_api
TOOLSET = "cubeless_mcp_ue58.registration.project_toolset.CubelessProjectTools"
CLASS_PATH = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/project_toolset_PY.CubelessProjectTools"
BINDINGS = ("create_input_mapping",)
PUBLIC_FIELDS_BY_METHOD = {"create_input_mapping": ("action_name", "key", "input_type")}
DEFAULTS_BY_METHOD = {"create_input_mapping": {"input_type": "Action"}}
REQUIRED_BY_METHOD = {"create_input_mapping": ("action_name", "key")}
FIELD_KINDS_BY_METHOD = {"create_input_mapping": {"action_name": "string", "key": "string", "input_type": "string"}}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json", "compile_json", "save_json", "validation_json", "retry_json", "data_json")

AUDIT_METHOD = "audit_content_root_mcp"
AUDIT_FIELDS = ("root_path", "recursive", "scan_paths", "include_dependencies", "include_assets", "write_report",
    "forbidden_dependency_prefixes", "required_asset_paths", "expected_asset_count", "expected_class_counts",
    "max_samples", "fail_on_dirty_packages", "fail_on_redirectors", "fail_on_forbidden_dependencies")
AUDIT_NULLABLE_FIELDS = ("forbidden_dependency_prefixes", "required_asset_paths", "expected_asset_count", "expected_class_counts")
# Keep the existing Input3 constants unchanged; extend only this Project class.
BINDINGS = BINDINGS + (AUDIT_METHOD,)
PUBLIC_FIELDS_BY_METHOD.update({AUDIT_METHOD: AUDIT_FIELDS})
DEFAULTS_BY_METHOD.update({AUDIT_METHOD: {"recursive": True, "scan_paths": True, "include_dependencies": True,
    "include_assets": False, "write_report": False, "forbidden_dependency_prefixes": None,
    "required_asset_paths": None, "expected_asset_count": None, "expected_class_counts": None,
    "max_samples": 50, "fail_on_dirty_packages": False, "fail_on_redirectors": True,
    "fail_on_forbidden_dependencies": True}})
REQUIRED_BY_METHOD.update({AUDIT_METHOD: ("root_path",)})
FIELD_KINDS_BY_METHOD.update({AUDIT_METHOD: {"root_path": "string", "recursive": "boolean", "scan_paths": "boolean",
    "include_dependencies": "boolean", "include_assets": "boolean", "write_report": "boolean",
    "forbidden_dependency_prefixes": ["array", "null"], "required_asset_paths": ["array", "null"],
    "expected_asset_count": ["integer", "null"], "expected_class_counts": ["object", "null"],
    "max_samples": "integer", "fail_on_dirty_packages": "boolean", "fail_on_redirectors": "boolean",
    "fail_on_forbidden_dependencies": "boolean"}})

REFERENCE_METHOD = "replace_asset_references_mcp"
REFERENCE_FIELDS = ("replacements", "target_root", "target_paths", "source_root", "string_replacements",
    "name_replacements", "dry_run", "save_assets", "scan_paths", "refresh_materials",
    "include_package_objects", "allow_non_game_targets")
REFERENCE_NULLABLE_FIELDS = ("replacements", "target_paths", "string_replacements", "name_replacements")
REFERENCE_MAP_FIELDS = ("replacements", "string_replacements", "name_replacements")
BINDINGS = BINDINGS + (REFERENCE_METHOD,)
PUBLIC_FIELDS_BY_METHOD[REFERENCE_METHOD] = REFERENCE_FIELDS
DEFAULTS_BY_METHOD[REFERENCE_METHOD] = dict(replacements=None, target_root="", target_paths=None,
    source_root="", string_replacements=None, name_replacements=None, dry_run=True,
    save_assets=False, scan_paths=True, refresh_materials=True,
    include_package_objects=False, allow_non_game_targets=False)
REQUIRED_BY_METHOD[REFERENCE_METHOD] = ()
FIELD_KINDS_BY_METHOD[REFERENCE_METHOD] = {field: ["object", "null"] if field in REFERENCE_MAP_FIELDS
    else ["array", "null"] if field == "target_paths" else "string" if field in ("target_root", "source_root")
    else "boolean" for field in REFERENCE_FIELDS}
NULLABLE_FIELDS_BY_METHOD = {AUDIT_METHOD: AUDIT_NULLABLE_FIELDS, REFERENCE_METHOD: REFERENCE_NULLABLE_FIELDS}

def binding_status(toolset_class):
    methods = api.binding_status()
    methods.update(audit_api.binding_status())
    methods.update(reference_api.binding_status())
    defined = {method: callable(getattr(toolset_class, method, None)) for method in BINDINGS}
    return {"ready": False, "bindings_ready": set(methods) == set(BINDINGS) and all(methods.values()) and all(defined.values()),
        "library": api.LIBRARY, "libraries": {"create_input_mapping": api.LIBRARY, AUDIT_METHOD: audit_api.LIBRARY, REFERENCE_METHOD: reference_api.LIBRARY},
        "methods": methods, "definition_methods": defined,
        "public_toolset": TOOLSET, "public_tool_count": 3,
        "public_input_counts": {"create_input_mapping": 3, AUDIT_METHOD: 14, REFERENCE_METHOD: 12},
        "hidden_input_counts": {"create_input_mapping": 7, AUDIT_METHOD: 14, REFERENCE_METHOD: 12}, "schema_verified": False, "default_verified": False,
        "preparation_required": False, "actual_official_wire_verified": False,
        "nullable_Named_wire_verification_required": True, "nullable_Named_wire_verified": False,
        "nullable_fields": list(AUDIT_NULLABLE_FIELDS),
        "nullable_fields_by_method": {method: list(fields) for method, fields in NULLABLE_FIELDS_BY_METHOD.items()}, "metadata_schema_validation_is_wire_packing_proof": False,
        "reflected_nullable_carrier_type": "FString_JSON_text", "reflected_nullable_carrier_default": "null",
        "reflected_carrier_schema_verified": False, "logical_adapter_schema_verified": False,
        "nullable_raw_guard_required": True, "nullable_raw_guard_ready": False,
        "non_idempotent_legacy_append": True, "config_persistence_native_authoritative": True,
        "content_audit_native_diagnostic_and_report_prefix_authoritative": True,
        "content_audit_criteria_failure_is_operation_failure": False,
        "runtime_binding_verified": False, "unique_external_pipeline_complete": False}

def _required(expected, value):
    return (type(value) is list and len(value) == len(expected)
        and all(type(item) is str for item in value) and set(value) == set(expected))


def _validate_schema(schema, toolset_class, reflected_carriers=False):
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 3 or not all(callable(getattr(toolset_class, method, None)) for method in BINDINGS)):
        raise RuntimeError("Exact original Project Input3, content audit14 and reference replacement12 Toolset3 required")
    rows = {row.get("name"): row for row in schema["tools"] if type(row) is dict}
    if len(rows) != 3 or set(rows) != {TOOLSET + "." + method for method in BINDINGS}:
        raise RuntimeError("Only the three exact original Project methods admitted")
    for method in BINDINGS:
        row = rows[TOOLSET + "." + method]
        inputs = row.get("inputSchema"); props = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or type(props) is not dict
                or set(props) != set(PUBLIC_FIELDS_BY_METHOD[method])
                or not _required(("target_root", "source_root") if reflected_carriers and method == REFERENCE_METHOD
                    else REQUIRED_BY_METHOD[method], inputs.get("required", []))):
            raise RuntimeError("Original Project fields/object/required inputs differ: " + method)
        for field, kind in FIELD_KINDS_BY_METHOD[method].items():
            item = props[field]
            carrier = reflected_carriers and field in NULLABLE_FIELDS_BY_METHOD.get(method, ())
            actual_kind = "string" if carrier else kind
            if type(item) is not dict or item.get("type") != actual_kind:
                raise RuntimeError("Original Project logical type or private carrier differs: " + field)
            if not reflected_carriers and method == AUDIT_METHOD and field in ("forbidden_dependency_prefixes", "required_asset_paths"):
                items = item.get("items")
                if type(items) is not dict or items.get("type") != "string":
                    raise RuntimeError("Original nullable audit arrays require string items: " + field)
            if not reflected_carriers and method == REFERENCE_METHOD:
                if field == "target_paths":
                    items = item.get("items")
                    if type(items) is not dict or items.get("type") != "string":
                        raise RuntimeError("Original reference target_paths require string items")
                elif field in REFERENCE_MAP_FIELDS:
                    values = item.get("additionalProperties")
                    if type(values) is not dict or values.get("type") != "string":
                        raise RuntimeError("Original nullable reference maps require string values")
            if not reflected_carriers and method == AUDIT_METHOD and field == "expected_class_counts":
                values = item.get("additionalProperties")
                if type(values) is not dict or values.get("type") != "integer":
                    raise RuntimeError("Original nullable audit class map requires integer values")
            if reflected_carriers and method == REFERENCE_METHOD and field in ("target_root", "source_root"):
                # Actual UE58 FFunctionLibrary schema drops empty FString defaults.
                # The owning raw adapter supplies explicit empty strings and presents
                # the original optional defaults only in its logical public schema.
                if "default" in item:
                    raise RuntimeError("Actual reflected root schema must retain absent empty-string default")
                continue
            if field in DEFAULTS_BY_METHOD[method]:
                value = "null" if carrier else DEFAULTS_BY_METHOD[method][field]
                if "default" not in item or type(item["default"]) is not type(value) or item["default"] != value:
                    raise RuntimeError("Original Project default differs: " + field)
            elif "default" in item:
                raise RuntimeError("Required original Project input gained a default")
        output = row.get("outputSchema"); outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or type(outer) is not dict
                or set(outer) != {"returnValue"} or not _required(("returnValue",), output.get("required"))):
            raise RuntimeError("Exact standard11 returnValue outer envelope required")
        value = outer["returnValue"]; fields = value.get("properties") if type(value) is dict else None
        if (type(value) is not dict or value.get("type") != "object" or type(fields) is not dict
                or set(fields) != set(RESULT_FIELDS) or not _required(RESULT_FIELDS, value.get("required"))
                or any(type(fields[key]) is not dict or fields[key].get("type") != ("boolean" if key == "success" else "string") for key in RESULT_FIELDS)):
            raise RuntimeError("Exact existing synchronous standard11 result required")
    if reflected_carriers:
        return {"reflected_carrier_schema_verified": True, "reflected_carrier_defaults_verified": True,
            "reflected_nullable_carrier_type": "FString_JSON_text", "reflected_nullable_carrier_default": "null",
            "reflected_empty_root_defaults_missing": True,
            "reflected_required_reference_roots": ["target_root", "source_root"],
            "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS},
            "schema_verified": False, "default_verified": False, "logical_adapter_schema_verified": False,
            "nullable_Named_wire_verification_required": True, "nullable_Named_wire_verified": False,
            "actual_official_wire_verified": False, "metadata_schema_validation_is_wire_packing_proof": False}
    return {"schema_verified": True, "default_verified": True, "original_public_defaults_verified": True,
        "logical_adapter_schema_verified": True, "reflected_carrier_schema_verified": False,
        "logical_empty_root_defaults_restored": True,
        "raw_adapter_empty_root_default_injection_required": True,
        "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS},
        "preparation_required": False, "actual_official_wire_verified": False,
        "nullable_Named_wire_verification_required": True, "nullable_Named_wire_verified": False,
        "nullable_fields": list(AUDIT_NULLABLE_FIELDS),
        "nullable_fields_by_method": {method: list(fields) for method, fields in NULLABLE_FIELDS_BY_METHOD.items()}, "metadata_schema_validation_is_wire_packing_proof": False,
        "runtime_or_persistence_or_original_pipeline_verified": False}


def validate_schema(schema, toolset_class):
    """Validate the adapter's public Audit14/Reference12 nullable schemas and Input3/std11.

    Root must use the installed wrapper's actual GetJsonSchema, not the raw
    class's FString schema. Metadata conformance does not verify Named payloads.
    """
    return _validate_schema(schema, toolset_class)


def validate_reflected_carrier_schema(schema, toolset_class):
    """Validate only each guarded method's hidden FString4 carrier ABI/defaults.

    This is not public logical schema equivalence or actual raw-wire admission.
    No nullable value is converted, projected or defaulted by this validator.
    """
    return _validate_schema(schema, toolset_class, reflected_carriers=True)


def validate_registered_metadata(class_schema, attachment_report, toolset_class):
    """Check the class carrier schema and the actual registered wrapper schema.

    UE58 UToolsetRegistry.GetToolsetJsonSchema(cls) constructs a temporary
    FFunctionLibraryToolset from the class; it never queries Registry.Find.
    Native receipt source/identity and actual wrapper JSON are required here.
    Neither metadata scope verifies Named wire payloads or asset behavior.
    """
    reflected = validate_reflected_carrier_schema(class_schema, toolset_class)
    if (type(attachment_report) is not dict
            or attachment_report.get("operation") not in ("install_content_audit_toolset", "verify_content_audit_toolset")
            or not all(attachment_report.get(key) is True for key in
                ("ready", "verified", "active", "guard_attached", "raw_guard_installed", "default_verified",
                 "reflected_carrier_schema_verified", "logical_schema_verified", "input3_schema_preserved", "output11_schema_preserved", "reference12_schema_verified"))
            or attachment_report.get("asset_mutation") is not False
            or attachment_report.get("metadata_mutation") is not False
            or attachment_report.get("logical_schema_source") != "actual_registered_wrapper_GetJsonSchema"
            or attachment_report.get("logical_schema_registry_identity_verified") is not True):
        raise RuntimeError("Exact ready owner receipt with actual registered Project wrapper schema required")
    logical_text = attachment_report.get("logical_schema_json")
    if type(logical_text) is not str or not logical_text:
        raise RuntimeError("Actual registered Project wrapper logical schema JSON is missing")
    logical_schema = json.loads(logical_text)
    logical = validate_schema(logical_schema, toolset_class)
    return dict(logical, reflected_carrier_schema_verified=True,
        class_reflected_schema_verified=True, class_schema_getter_scope="fresh_FFunctionLibraryToolset_not_registered_handler",
        logical_schema_source="actual_registered_wrapper_GetJsonSchema",
        logical_schema_registry_identity_verified=True, actual_registry_wrapper_schema_verified=True,
        logical_schema_json=logical_text, logical_schema=logical_schema,
        reflected_schema_validation=reflected, logical_schema_validation=logical,
        actual_official_wire_verified=False, nullable_Named_wire_verified=False)
