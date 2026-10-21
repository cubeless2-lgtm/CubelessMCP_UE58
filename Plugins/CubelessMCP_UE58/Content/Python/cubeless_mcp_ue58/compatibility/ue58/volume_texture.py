"""Exact DX native job adapter; no registry, journal, polling loop or asset calls."""
import json
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessVolumeTextureLibrary"
JOB_CLASS_PATH = "/Script/CubelessMCPUE58Editor.CubelessVolumeTextureJob"
BINDINGS = ('create_volume_texture_from_2d_sheet_mcp', 'create_volume_texture_from_raw_rgba16_sheet_mcp')
PUBLIC_FIELDS_BY_METHOD = {'create_volume_texture_from_2d_sheet_mcp': ('source_texture_path', 'target_volume_texture_path', 'tile_size_x', 'tile_size_y', 'dry_run', 'overwrite_existing', 'save_asset', 'srgb', 'compression_settings', 'mip_gen_settings', 'never_stream', 'virtual_texture_streaming', 'address_mode'), 'create_volume_texture_from_raw_rgba16_sheet_mcp': ('raw_file_path', 'source_texture_path', 'target_volume_texture_path', 'sheet_size_x', 'sheet_size_y', 'tile_size_x', 'tile_size_y', 'dry_run', 'overwrite_existing', 'save_assets', 'srgb', 'source_compression_settings', 'volume_compression_settings', 'source_mip_gen_settings', 'volume_mip_gen_settings', 'never_stream_volume', 'virtual_texture_streaming', 'address_mode')}
LIFECYCLE = ("activate_volume_texture_jobs", "deactivate_volume_texture_jobs", "volume_texture_jobs_status")
DEFAULT_VERIFIER = "verify_volume_texture_toolset_defaults"


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    job_class = getattr(unreal, "CubelessVolumeTextureJob", None)
    return {"methods": {name: callable(getattr(library, name, None)) for name in BINDINGS},
        "job_class_available": job_class is not None,
        "job_snapshot_callable": callable(getattr(job_class, "get_snapshot_json", None)),
        "job_cancel_callable": callable(getattr(job_class, "cancel_job", None)),
        "lifecycle_methods": {name: callable(getattr(library, name, None)) for name in LIFECYCLE},
        "defaults_verifier_callable": callable(getattr(library, DEFAULT_VERIFIER, None))}


def jobs_status():
    function = getattr(getattr(unreal, LIBRARY, None), "volume_texture_jobs_status", None)
    if not callable(function):
        raise OperationError("volume_texture_native_status_unavailable", "Exact Native job status unavailable")
    value = json.loads(function())
    if (type(value) is not dict or type(value.get("active")) is not bool
            or type(value.get("pending_count")) is not int or value["pending_count"] < 0
            or value.get("max_pending") != 1 or value.get("deadline_seconds") != 60):
        raise OperationError("volume_texture_native_status_invalid", "Exact active/pending/private bounds required")
    return value


def verify_toolset_defaults(toolset_class):
    function = getattr(getattr(unreal, LIBRARY, None), DEFAULT_VERIFIER, None)
    if not callable(function):
        raise OperationError("volume_texture_defaults_verifier_unavailable", "Exact readonly class-default verifier unavailable")
    raw = function(toolset_class.static_class())
    value = _decode_result(raw)
    value["_raw_native_json"] = raw
    return value


def start_job(method, public):
    if method not in BINDINGS or tuple(public) != PUBLIC_FIELDS_BY_METHOD[method]:
        raise OperationError("volume_texture_native_arguments_invalid", "Exact original argument order required")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("volume_texture_native_api_unavailable", "No native start was dispatched")
    arguments = []
    for field in PUBLIC_FIELDS_BY_METHOD[method]:
        value = public[field]
        if method == BINDINGS[0] and field == "srgb":
            arguments.extend((False if value is None else value, value is not None))
        else:
            arguments.append(value)
    job = function(*arguments)
    try:
        job_class = getattr(unreal, "CubelessVolumeTextureJob", None)
        if (job_class is None or not isinstance(job, job_class)
                or job.get_class().get_path_name() != JOB_CLASS_PATH
                or not callable(getattr(job, "get_snapshot_json", None))
                or not callable(getattr(job, "cancel_job", None))):
            raise OperationError("volume_texture_native_job_invalid", "Returned job differs; do not repeat the dispatched start")
    except Exception as error:
        error.native_start_returned = True
        error.native_returned_job = job
        raise
    return job


def snapshot(job, method):
    raw = job.get_snapshot_json()
    try:
        value = _decode_result(raw)
        if value.get("operation") != method or type(value["data"].get("native_job_terminal")) is not bool:
            raise OperationError("volume_texture_native_snapshot_invalid", "Exact operation and terminal bool required")
    except Exception as error:
        error.native_snapshot_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value
