"""Original StateTree public5 contracts, defaults and existing standard11."""
from cubeless_mcp_ue58.compatibility.ue58 import state_tree_read as api
from cubeless_mcp_ue58.compatibility.ue58 import state_tree_create as create_api
from cubeless_mcp_ue58.compatibility.ue58 import state_tree_mutators as mutation_api

TOOLSET = "cubeless_mcp_ue58.registration.state_tree_toolset.CubelessStateTreeTools"
CLASS_PATH = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/state_tree_toolset_PY.CubelessStateTreeTools"
BINDINGS = ("list_state_trees", "inspect_state_tree", "create_state_tree", "add_state_tree_state", "compile_state_tree")
PUBLIC_FIELDS_BY_METHOD = {
    "list_state_trees": ("directory", "recursive", "max_results", "offset"),
    "inspect_state_tree": ("asset_path", "max_states"),
    "create_state_tree": ("name", "package_path", "schema_type"),
    "add_state_tree_state": ("asset_path", "state_name", "parent_path", "state_type", "selection_behavior", "dry_run"),
    "compile_state_tree": ("asset_path", "save"),
}
DEFAULTS_BY_METHOD = {
    "list_state_trees": {"directory": "/Game", "recursive": True, "max_results": 200, "offset": 0},
    "inspect_state_tree": {"max_states": 500},
    "create_state_tree": {"package_path": "/Game/AI/StateTrees", "schema_type": "component"},
    "add_state_tree_state": {"parent_path": "Root", "state_type": "state", "selection_behavior": "children_in_order", "dry_run": True},
    "compile_state_tree": {"save": True},
}
REQUIRED_BY_METHOD = {"list_state_trees": (), "inspect_state_tree": ("asset_path",), "create_state_tree": ("name",),
    "add_state_tree_state": ("asset_path", "state_name"), "compile_state_tree": ("asset_path",)}
FIELD_KINDS_BY_METHOD = {
    "list_state_trees": {"directory": "string", "recursive": "boolean", "max_results": "integer", "offset": "integer"},
    "inspect_state_tree": {"asset_path": "string", "max_states": "integer"},
    "create_state_tree": {"name": "string", "package_path": "string", "schema_type": "string"},
    "add_state_tree_state": {"asset_path": "string", "state_name": "string", "parent_path": "string",
        "state_type": "string", "selection_behavior": "string", "dry_run": "boolean"},
    "compile_state_tree": {"asset_path": "string", "save": "boolean"},
}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json", "compile_json",
    "save_json", "validation_json", "retry_json", "data_json")


def binding_status(toolset_class=None):
    methods = api.binding_status()
    methods.update(create_api.binding_status())
    methods.update(mutation_api.binding_status())
    defined = {method: callable(getattr(toolset_class, method, None)) for method in BINDINGS}
    bindings_ready = set(methods) == set(BINDINGS) and all(methods.values()) and all(defined.values())
    # Root attaches only actual converter/default/type proof before activation.
    # A callable bool API alone cannot preserve original StrictBool Named JSON.
    return {"ready": False, "bindings_ready": bindings_ready,
        "library": api.LIBRARY, "methods": methods, "definition_methods": defined,
        "libraries": {method: mutation_api.LIBRARY if method in mutation_api.BINDINGS else create_api.LIBRARY if method == "create_state_tree" else api.LIBRARY for method in BINDINGS},
        "public_toolset": TOOLSET, "public_tool_count": 5,
        "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS},
        "hidden_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS},
        "preparation_required": False, "preparation_verified": False,
        "schema_verified": False, "default_verified": False, "actual_official_wire_verified": False,
        "inspect_potential_SDK_compile_and_unknown_total_changes_journaled": True,
        "create_original_factory_compile_audit_save_partial_cleanup_native_authoritative": True,
        "mutation_original_dry_reuse_compile_save_audit_structural_recovery_native_authoritative": True,
        "strict_dry_run_Named_wire_converter_verification_required": True,
        "strict_dry_run_Named_wire_converter_verified": False,
        "observation_not_runtime_persistence_or_complete_pipeline": True}


def _required(expected, value):
    return (type(value) is list and len(value) == len(expected)
        and all(type(item) is str for item in value) and set(value) == set(expected))


def validate_schema(schema, toolset_class):
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 5 or not all(callable(getattr(toolset_class, method, None)) for method in BINDINGS)):
        raise RuntimeError("Exact original StateTree Toolset5 required")
    rows = {row.get("name"): row for row in schema["tools"] if type(row) is dict}
    if len(rows) != 5 or set(rows) != {TOOLSET + "." + method for method in BINDINGS}:
        raise RuntimeError("Only original StateTree list/inspect/create/add/compile methods admitted")
    for method in BINDINGS:
        row = rows[TOOLSET + "." + method]
        inputs = row.get("inputSchema"); props = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or type(props) is not dict
                or set(props) != set(PUBLIC_FIELDS_BY_METHOD[method])
                or not _required(REQUIRED_BY_METHOD[method], inputs.get("required", []))):
            raise RuntimeError("Original StateTree fields/object/required inputs differ: " + method)
        for field, kind in FIELD_KINDS_BY_METHOD[method].items():
            item = props[field]
            if type(item) is not dict or item.get("type") != kind:
                raise RuntimeError("StateTree field type differs: " + field)
            if field in DEFAULTS_BY_METHOD[method]:
                value = DEFAULTS_BY_METHOD[method][field]
                if "default" not in item or type(item["default"]) is not type(value) or item["default"] != value:
                    raise RuntimeError("Original StateTree default differs: " + field)
            elif "default" in item:
                raise RuntimeError("Required StateTree asset input gained a default")
        output = row.get("outputSchema"); outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or type(outer) is not dict
                or set(outer) != {"returnValue"} or not _required(("returnValue",), output.get("required"))):
            raise RuntimeError("Exact standard11 returnValue outer envelope required")
        value = outer["returnValue"]; fields = value.get("properties") if type(value) is dict else None
        if (type(value) is not dict or value.get("type") != "object" or type(fields) is not dict
                or set(fields) != set(RESULT_FIELDS) or not _required(RESULT_FIELDS, value.get("required"))
                or any(type(fields[key]) is not dict or fields[key].get("type") != ("boolean" if key == "success" else "string") for key in RESULT_FIELDS)):
            raise RuntimeError("Exact existing synchronous standard11 result required")
    return {"schema_verified": True, "default_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS},
        "preparation_required": False, "actual_official_wire_verified": False,
        "runtime_or_persistence_or_original_pipeline_verified": False}
