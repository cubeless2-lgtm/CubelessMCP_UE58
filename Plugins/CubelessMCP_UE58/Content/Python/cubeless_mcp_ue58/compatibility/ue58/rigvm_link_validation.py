"""UE58 proposed RigVM links7; nullable arrays are private JSON carriers2."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessRigVMLinkValidationLibrary"
METHOD = "validate_controlrig_rigvm_links"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("rigvm_link_validation_carrier_invalid", "Nullable link arrays require finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("rigvm_link_validation_carrier_bound", "Private nullable link carrier exceeds1 Mi UE UTF16 units or contains embedded NUL")
    return encoded


def validate_controlrig_rigvm_links(control_rig_path="", control_rig_asset_path="", control_rig_class_path="",
        links=None, unit_links=None, allow_type_casting=True, max_links=64):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("rigvm_link_validation_native_api_unavailable", "UE58 RigVM link validation binding is unavailable; no fallback executed")
    links_json = _carrier(links)
    unit_links_json = _carrier(unit_links)
    raw = fn(control_rig_path, control_rig_asset_path, control_rig_class_path,
        links_json, unit_links_json, allow_type_casting, max_links)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("rigvm_link_validation_native_result_invalid", "Native RigVM link validation operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("rigvm_link_validation_native_result_invalid", "Native RigVM link validation changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
