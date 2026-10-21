"""UE58 specific transient BlendSpace pose-grid operation and bounded private DTO."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessBlendSpacePoseGridLibrary"
METHOD = "sample_blendspace_runtime_pose_grid"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576
LEGACY_COMMAND_FIELDS = ("blendspaces", "samples", "sample_bones", "sample_sockets", "bones", "sockets", "blendspace_name")


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("blendspace_pose_grid_carrier_invalid", "Pose-grid requests require finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("blendspace_pose_grid_carrier_bound", "Private grid carrier exceeds 1 Mi UTF16 units or contains raw NUL")
    return encoded


def _native_carriers(blendspaces, samples, sample_bones, sample_sockets, legacy=None):
    if legacy is None:
        legacy = {}
    if type(legacy) is not dict or any(key not in LEGACY_COMMAND_FIELDS for key in legacy):
        raise OperationError("blendspace_pose_grid_input_invalid", "Private projection accepts only the seven confirmed grid fields")
    projections = []
    for canonical, alias, value in (("blendspaces", "blendspace_name", blendspaces),
            ("sample_bones", "bones", sample_bones), ("sample_sockets", "sockets", sample_sockets)):
        projection = {}
        if canonical in legacy:
            projection[canonical] = legacy[canonical]
        elif value is not None:
            projection[canonical] = value
        if alias in legacy:
            projection[alias] = legacy[alias]
        projections.append(projection)
    return (_carrier(projections[0]), _carrier(legacy["samples"] if "samples" in legacy else samples),
        _carrier(projections[1]), _carrier(projections[2]))


def sample_blendspace_runtime_pose_grid(skeletal_mesh, blendspace_path="", blendspaces=None,
        samples=None, sample_bones=None, sample_sockets=None,
        actor_label="MCP_BlendSpace_RuntimePoseGrid", sample_time=0.25, settle_tick_count=2,
        tick_count=1, tick_delta_time=1.0 / 30.0, include_axis_extremes=False,
        allow_missing_bones=False, refresh_bone_transforms=True, cleanup=True,
        prefer_pie_world=True, require_pie_world=True, *, _legacy_command_params=None):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("blendspace_pose_grid_native_api_unavailable", "UE58 BlendSpace pose-grid binding is unavailable; no fallback executed")
    targets_json, samples_json, bones_json, sockets_json = _native_carriers(
        blendspaces, samples, sample_bones, sample_sockets, _legacy_command_params)
    raw = fn(skeletal_mesh, blendspace_path, targets_json, samples_json, bones_json, sockets_json,
        actor_label, sample_time, settle_tick_count, tick_count, tick_delta_time,
        include_axis_extremes, allow_missing_bones, refresh_bone_transforms, cleanup,
        prefer_pie_world, require_pie_world)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("blendspace_pose_grid_native_result_invalid", "Native pose-grid operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("blendspace_pose_grid_native_result_invalid", "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
