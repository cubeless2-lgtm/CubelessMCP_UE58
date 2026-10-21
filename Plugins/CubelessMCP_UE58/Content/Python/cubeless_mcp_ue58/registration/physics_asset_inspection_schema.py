"""Exact reflected PhysicsAsset observer12/defaults/standard11 schema gate; no Engine dispatch."""
from cubeless_mcp_ue58.compatibility.ue58 import physics_asset_inspection as api

TOOLSET = "cubeless_mcp_ue58.registration.physics_asset_inspection_toolset.CubelessPhysicsAssetInspectionTools"
CLASS_PATH = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/physics_asset_inspection_toolset_PY.CubelessPhysicsAssetInspectionTools"
BINDINGS = ("inspect_physics_asset_constraints_guarded",)
PREPARATION = "prepare_physics_asset_inspection_toolset"
PUBLIC_FIELDS = ('physics_asset_path', 'skeletal_mesh_path', 'include_bodies', 'include_constraints', 'include_profiles', 'include_solver_settings', 'include_collision_summary', 'max_bodies', 'max_constraints', 'max_profiles_per_item', 'max_shapes_per_body', 'require_supported_class')
DEFAULTS = {'physics_asset_path': '', 'skeletal_mesh_path': '', 'include_bodies': True, 'include_constraints': True, 'include_profiles': True, 'include_solver_settings': True, 'include_collision_summary': True, 'max_bodies': 128, 'max_constraints': 128, 'max_profiles_per_item': 16, 'max_shapes_per_body': 32, 'require_supported_class': False}
FIELD_KINDS = {'physics_asset_path': 'string', 'skeletal_mesh_path': 'string', 'include_bodies': 'boolean', 'include_constraints': 'boolean', 'include_profiles': 'boolean', 'include_solver_settings': 'boolean', 'include_collision_summary': 'boolean', 'max_bodies': 'integer', 'max_constraints': 'integer', 'max_profiles_per_item': 'integer', 'max_shapes_per_body': 'integer', 'require_supported_class': 'boolean'}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json", "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def binding_status(toolset_class=None):
    methods = api.binding_status()
    defined = {method: callable(getattr(toolset_class, method, None)) for method in BINDINGS}
    return {"ready": all(methods.values()) and all(defined.values()) and api.preparation_available(),
        "library": api.LIBRARY, "methods": methods, "definition_methods": defined, "public_toolset": TOOLSET,
        "public_tool_count": 1, "public_input_counts": {BINDINGS[0]: 12},
        "hidden_input_counts": {BINDINGS[0]: 12}, "public_input_count": 12,
        "preparation_required": True, "preparation_callable": api.preparation_available(),
        "preparation_verified": False, "empty_path_defaults_schema_verification_required": True,
        "schema_verified": False, "default_verified": False, "actual_official_wire_verified": False,
        "unsupported_require_false_completed_diagnostic": True,
        "current_authored_metadata_not_live_physics_simulation": True}


def _required(expected, value):
    return (type(value) is list and len(value) == len(expected)
        and all(type(item) is str for item in value) and set(value) == set(expected))


def validate_schema(schema, toolset_class):
    method = BINDINGS[0]
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 1 or not callable(getattr(toolset_class, method, None))):
        raise RuntimeError("Exact reflected PhysicsAsset inspection Toolset1 required")
    row = schema["tools"][0]
    if type(row) is not dict or row.get("name") != TOOLSET + "." + method:
        raise RuntimeError("Exact original PhysicsAsset observer method required")
    inputs = row.get("inputSchema")
    props = inputs.get("properties") if type(inputs) is dict else None
    if (type(inputs) is not dict or inputs.get("type") != "object" or type(props) is not dict
            or set(props) != set(PUBLIC_FIELDS) or not _required((), inputs.get("required", []))):
        raise RuntimeError("Exact optional original12 fields/object/no-required input differs")
    for field, kind in FIELD_KINDS.items():
        item = props[field]
        if type(item) is not dict or item.get("type") != kind:
            raise RuntimeError("PhysicsAsset field type differs: " + field)
        if ("default" not in item or type(item["default"]) is not type(DEFAULTS[field])
                or item["default"] != DEFAULTS[field]):
            raise RuntimeError("Original PhysicsAsset default differs: " + field)
    output = row.get("outputSchema")
    outer = output.get("properties") if type(output) is dict else None
    if (type(output) is not dict or output.get("type") != "object" or type(outer) is not dict
            or set(outer) != {"returnValue"} or not _required(("returnValue",), output.get("required"))):
        raise RuntimeError("Exact standard11 returnValue outer envelope required")
    value = outer["returnValue"]
    result_fields = value.get("properties") if type(value) is dict else None
    if (type(value) is not dict or value.get("type") != "object" or type(result_fields) is not dict
            or set(result_fields) != set(RESULT_FIELDS) or not _required(RESULT_FIELDS, value.get("required"))
            or any(type(result_fields[key]) is not dict or result_fields[key].get("type") != ("boolean" if key == "success" else "string") for key in RESULT_FIELDS)):
        raise RuntimeError("Exact existing synchronous standard11 result required")
    return {"schema_verified": True, "default_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {method: 12}, "public_input_count": 12,
        "empty_path_defaults_verified": True, "preparation_required": True,
        "preparation_verified": False, "actual_official_wire_verified": False,
        "runtime_or_cold_or_original_pipeline_verified": False}
