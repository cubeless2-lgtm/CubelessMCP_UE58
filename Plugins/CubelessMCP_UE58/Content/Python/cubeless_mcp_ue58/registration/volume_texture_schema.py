"""Exact reflected public13/18 and CU async standard11 schema gate; no jobs started."""
from cubeless_mcp_ue58.compatibility.ue58 import volume_texture as api
from cubeless_mcp_ue58.operations import volume_texture as domain

TOOLSET = "cubeless_mcp_ue58.registration.volume_texture_toolset.CubelessVolumeTextureTools"
CLASS_PATH = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/volume_texture_toolset_PY.CubelessVolumeTextureTools"
BINDINGS = ('create_volume_texture_from_2d_sheet_mcp', 'create_volume_texture_from_raw_rgba16_sheet_mcp')
PUBLIC_FIELDS_BY_METHOD = {'create_volume_texture_from_2d_sheet_mcp': ('source_texture_path', 'target_volume_texture_path', 'tile_size_x', 'tile_size_y', 'dry_run', 'overwrite_existing', 'save_asset', 'srgb', 'compression_settings', 'mip_gen_settings', 'never_stream', 'virtual_texture_streaming', 'address_mode'), 'create_volume_texture_from_raw_rgba16_sheet_mcp': ('raw_file_path', 'source_texture_path', 'target_volume_texture_path', 'sheet_size_x', 'sheet_size_y', 'tile_size_x', 'tile_size_y', 'dry_run', 'overwrite_existing', 'save_assets', 'srgb', 'source_compression_settings', 'volume_compression_settings', 'source_mip_gen_settings', 'volume_mip_gen_settings', 'never_stream_volume', 'virtual_texture_streaming', 'address_mode')}
DEFAULTS_BY_METHOD = {'create_volume_texture_from_2d_sheet_mcp': {'tile_size_x': 128, 'tile_size_y': 128, 'dry_run': False, 'overwrite_existing': False, 'save_asset': True, 'srgb': None, 'compression_settings': 'TC_VectorDisplacementmap', 'mip_gen_settings': 'TMGS_NoMipmaps', 'never_stream': True, 'virtual_texture_streaming': False, 'address_mode': 'TA_Wrap'}, 'create_volume_texture_from_raw_rgba16_sheet_mcp': {'sheet_size_x': 2048, 'sheet_size_y': 1024, 'tile_size_x': 128, 'tile_size_y': 128, 'dry_run': False, 'overwrite_existing': False, 'save_assets': True, 'srgb': False, 'source_compression_settings': 'TC_Default', 'volume_compression_settings': 'TC_VectorDisplacementmap', 'source_mip_gen_settings': 'TMGS_FromTextureGroup', 'volume_mip_gen_settings': 'TMGS_NoMipmaps', 'never_stream_volume': True, 'virtual_texture_streaming': False, 'address_mode': 'TA_Wrap'}}
FIELD_KINDS_BY_METHOD = {'create_volume_texture_from_2d_sheet_mcp': {'source_texture_path': 'string', 'target_volume_texture_path': 'string', 'tile_size_x': 'integer', 'tile_size_y': 'integer', 'dry_run': 'boolean', 'overwrite_existing': 'boolean', 'save_asset': 'boolean', 'srgb': 'nullable_boolean', 'compression_settings': 'string', 'mip_gen_settings': 'string', 'never_stream': 'boolean', 'virtual_texture_streaming': 'boolean', 'address_mode': 'string'}, 'create_volume_texture_from_raw_rgba16_sheet_mcp': {'raw_file_path': 'string', 'source_texture_path': 'string', 'target_volume_texture_path': 'string', 'sheet_size_x': 'integer', 'sheet_size_y': 'integer', 'tile_size_x': 'integer', 'tile_size_y': 'integer', 'dry_run': 'boolean', 'overwrite_existing': 'boolean', 'save_assets': 'boolean', 'srgb': 'boolean', 'source_compression_settings': 'string', 'volume_compression_settings': 'string', 'source_mip_gen_settings': 'string', 'volume_mip_gen_settings': 'string', 'never_stream_volume': 'boolean', 'virtual_texture_streaming': 'boolean', 'address_mode': 'string'}}
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json", "compile_json", "save_json", "validation_json", "retry_json", "data_json")


def binding_status(toolset_class):
    native = api.binding_status()
    try:
        jobs, error = api.jobs_status(), ""
    except Exception as failure:
        jobs, error = {}, str(failure)
    local = domain.binding_status()
    present = {name: callable(getattr(toolset_class, name, None)) for name in BINDINGS}
    ready = (all(native["methods"].values()) and all(present.values()) and native["job_class_available"]
        and native["job_snapshot_callable"] and native["job_cancel_callable"]
        and native["defaults_verifier_callable"] and jobs.get("active") is True and local["active"]
        and all(native["lifecycle_methods"].values()))
    return {"ready": ready, "library": api.LIBRARY, "methods": native["methods"], "definition_methods": present,
        "public_toolset": TOOLSET, "public_tool_count": 2,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "hidden_input_counts": {BINDINGS[0]: 14, BINDINGS[1]: 18}, "native_jobs": jobs, "domain": local,
        "preparation_required": False, "readonly_defaults_verification_required": True,
        "schema_verified": False, "default_verified": False, "actual_official_async_wire_verified": False,
        "async_standard11_Value": True, "ordinary_Batch_integration_ready": bool(ready and local.get("batch_volume_texture", {}).get("active") is True), "error": error}


def _required(names, value):
    return type(value) is list and len(value) == len(names) and all(type(x) is str for x in value) and set(value) == set(names)


def validate_schema(schema, toolset_class):
    if type(schema) is not dict or schema.get("name") != TOOLSET or type(schema.get("tools")) is not list or len(schema["tools"]) != 2:
        raise RuntimeError("Exact reflected VolumeTextureTools2 schema required")
    if not all(callable(getattr(toolset_class, m, None)) for m in BINDINGS):
        raise RuntimeError("Exact two reflected original definitions required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("Volume method missing/duplicated: " + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        required = tuple(name for name in PUBLIC_FIELDS_BY_METHOD[method] if name not in DEFAULTS_BY_METHOD[method])
        if (type(inputs) is not dict or inputs.get("type") != "object" or type(properties) is not dict
                or set(properties) != set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(required, inputs.get("required"))):
            raise RuntimeError("Exact original volume input fields/envelope/required differ")
        for field, kind in FIELD_KINDS_BY_METHOD[method].items():
            item = properties[field]
            if type(item) is not dict:
                raise RuntimeError("Volume field schema absent: " + field)
            expected = ["boolean", "null"] if kind == "nullable_boolean" else kind
            if item.get("type") != expected:
                raise RuntimeError("Volume field type differs: " + field)
            if field in DEFAULTS_BY_METHOD[method]:
                default = DEFAULTS_BY_METHOD[method][field]
                if "default" not in item or type(item["default"]) is not type(default) or item["default"] != default:
                    raise RuntimeError("Volume original default differs: " + field)
            elif "default" in item:
                raise RuntimeError("Volume required field gained default: " + field)
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or type(outer) is not dict
                or set(outer) != {"returnValue"} or not _required(("returnValue",), output.get("required"))):
            raise RuntimeError("Exact async returnValue outer envelope required")
        value = outer["returnValue"]
        props = value.get("properties") if type(value) is dict else None
        if (type(value) is not dict or value.get("type") != "object" or type(props) is not dict
                or set(props) != set(RESULT_FIELDS) or not _required(RESULT_FIELDS, value.get("required"))
                or any(type(props[k]) is not dict or props[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS)):
            raise RuntimeError("Exact CU standard11 async Value required")
    return {"schema_verified": True, "original_public_defaults_verified": True, "default_verified": True,
        "nullable_srgb_default_None_verified": True, "async_standard11_Value_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "actual_official_async_wire_verified": False,
        "ordinary_Batch_integration_verified": False, "runtime_sampling_or_pipeline_verified": False}
