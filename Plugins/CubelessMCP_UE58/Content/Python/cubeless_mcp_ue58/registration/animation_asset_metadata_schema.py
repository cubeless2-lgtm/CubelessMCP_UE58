"""Exact reflected metadata observer17/defaults/standard11 schema gate; no Engine dispatch."""
from cubeless_mcp_ue58.compatibility.ue58 import animation_asset_metadata as api

TOOLSET = 'cubeless_mcp_ue58.registration.animation_asset_metadata_toolset.CubelessAnimationAssetMetadataTools'
CLASS_PATH = '/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/animation_asset_metadata_toolset_PY.CubelessAnimationAssetMetadataTools'
BINDINGS = ('inspect_animation_asset_metadata_guarded',)
PUBLIC_FIELDS = ('asset_path', 'include_notifies', 'include_notify_details', 'include_curves', 'include_curve_keys', 'include_sync_markers', 'include_montage_sections', 'include_montage_slots', 'include_metadata', 'max_notifies', 'max_curves', 'max_curve_keys_per_curve', 'max_markers', 'max_sections', 'max_slots', 'max_segments_per_slot', 'require_supported_class')
DEFAULTS = {'include_notifies': True, 'include_notify_details': True, 'include_curves': True, 'include_curve_keys': False, 'include_sync_markers': True, 'include_montage_sections': True, 'include_montage_slots': True, 'include_metadata': False, 'max_notifies': 256, 'max_curves': 256, 'max_curve_keys_per_curve': 0, 'max_markers': 256, 'max_sections': 128, 'max_slots': 64, 'max_segments_per_slot': 256, 'require_supported_class': False}
FIELD_KINDS = {'asset_path': 'string', 'include_notifies': 'boolean', 'include_notify_details': 'boolean', 'include_curves': 'boolean', 'include_curve_keys': 'boolean', 'include_sync_markers': 'boolean', 'include_montage_sections': 'boolean', 'include_montage_slots': 'boolean', 'include_metadata': 'boolean', 'max_notifies': 'integer', 'max_curves': 'integer', 'max_curve_keys_per_curve': 'integer', 'max_markers': 'integer', 'max_sections': 'integer', 'max_slots': 'integer', 'max_segments_per_slot': 'integer', 'require_supported_class': 'boolean'}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json", "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def binding_status(toolset_class=None):
    methods = api.binding_status()
    defined = {method: callable(getattr(toolset_class, method, None)) for method in BINDINGS}
    return {"ready": all(methods.values()) and all(defined.values()), "library": api.LIBRARY,
        "methods": methods, "definition_methods": defined, "public_toolset": TOOLSET,
        "public_tool_count": 1, "public_input_counts": {BINDINGS[0]: 17},
        "hidden_input_counts": {BINDINGS[0]: 17}, "public_input_count": 17,
        "preparation_required": False, "schema_verified": False, "default_verified": False,
        "actual_official_wire_verified": False,
        "unsupported_require_false_completed_diagnostic": True,
        "const_preview_getter_readonly_repair_selected": True,
        "current_metadata_not_runtime_animation_evaluation": True}


def _required(expected, value):
    return (type(value) is list and len(value) == len(expected)
        and all(type(item) is str for item in value) and set(value) == set(expected))


def validate_schema(schema, toolset_class):
    method = BINDINGS[0]
    if (type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list
            or len(schema["tools"]) != 1 or not callable(getattr(toolset_class, method, None))):
        raise RuntimeError("Exact reflected animation metadata Toolset1 required")
    row = schema["tools"][0]
    if type(row) is not dict or row.get("name") != TOOLSET + "." + method:
        raise RuntimeError("Exact original metadata observer method required")
    inputs = row.get("inputSchema")
    props = inputs.get("properties") if type(inputs) is dict else None
    if (type(inputs) is not dict or inputs.get("type") != "object" or type(props) is not dict
            or set(props) != set(PUBLIC_FIELDS) or not _required(("asset_path",), inputs.get("required"))):
        raise RuntimeError("Exact original17 fields/object/required asset input differs")
    for field, kind in FIELD_KINDS.items():
        item = props[field]
        if type(item) is not dict or item.get("type") != kind:
            raise RuntimeError("Animation metadata field type differs: " + field)
        if field in DEFAULTS:
            if "default" not in item or type(item["default"]) is not type(DEFAULTS[field]) or item["default"] != DEFAULTS[field]:
                raise RuntimeError("Original animation metadata default differs: " + field)
        elif "default" in item:
            raise RuntimeError("Required asset input gained a default")
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
        "public_input_counts": {method: 17}, "public_input_count": 17,
        "preparation_required": False, "actual_official_wire_verified": False,
        "runtime_or_cold_or_original_pipeline_verified": False}
