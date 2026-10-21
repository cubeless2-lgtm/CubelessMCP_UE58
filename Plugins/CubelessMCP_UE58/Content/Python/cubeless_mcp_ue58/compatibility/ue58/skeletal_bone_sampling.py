"""UE58 immediate skeletal bone/socket sampling; live world work is Native-owned."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessSkeletalBoneSamplingLibrary"
METHOD = "sample_skeletal_bones_in_sie"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("skeletal_bone_sampling_carrier_invalid", "Bone/socket requests require finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("skeletal_bone_sampling_carrier_bound", "Private sampling carrier exceeds1 Mi UE UTF16 units or contains raw NUL")
    return encoded


def sample_skeletal_bones_in_sie(actor_label="", actor_name="", actor_path="", component_name="",
        bones=None, sockets=None, prefer_pie_world=True, require_pie_world=False):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("skeletal_bone_sampling_native_api_unavailable", "UE58 skeletal sampling binding is unavailable; no fallback executed")
    bones_json = _carrier(bones)
    sockets_json = _carrier(sockets)
    raw = fn(actor_label, actor_name, actor_path, component_name,
        bones_json, sockets_json, prefer_pie_world, require_pie_world)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("skeletal_bone_sampling_native_result_invalid", "Native sampling operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("skeletal_bone_sampling_native_result_invalid", "Native sampling changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
