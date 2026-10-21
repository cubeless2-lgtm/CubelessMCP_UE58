"""UE58 guarded RigVM pin default writer11; nullable spec arrays use carriers2."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessRigVMPinDefaultWriterLibrary"
METHOD = "set_controlrig_rigvm_pin_defaults"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("rigvm_pin_default_writer_carrier_invalid", "Nullable pin arrays require finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("rigvm_pin_default_writer_carrier_bound", "Private nullable pin carrier exceeds1 Mi UE UTF16 units or contains embedded NUL")
    return encoded


def set_controlrig_rigvm_pin_defaults(control_rig_path="", control_rig_asset_path="", control_rig_class_path="",
        pin_defaults=None, defaults=None, resize_arrays=False, set_value_on_linked_pins=False, dry_run=False, compile=True, save_asset=False, max_pins=64):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("rigvm_pin_default_writer_native_api_unavailable", "UE58 guarded RigVM pin default writer binding is unavailable; no fallback executed")
    pin_defaults_json = _carrier(pin_defaults)
    defaults_json = _carrier(defaults)
    raw = fn(control_rig_path, control_rig_asset_path, control_rig_class_path,
        pin_defaults_json, defaults_json, resize_arrays, set_value_on_linked_pins, dry_run, compile, save_asset, max_pins)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("rigvm_pin_default_writer_native_result_invalid", "Native pin default writer operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("rigvm_pin_default_writer_native_result_invalid", "Native pin default writer changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
