"""UE58 enum creation6; array and nullable tooltip JSON are private Native ABI."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessUserDefinedEnumLibrary"
METHOD = "create_user_defined_enum"
BINDINGS = (METHOD,)
MAX_CARRIER_CHARS = 1048576


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _carrier(value):
    # Preserve None as JSON null; [] is not omission and must not gain empty defaults.
    encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    if len(encoded) > MAX_CARRIER_CHARS:
        raise OperationError("user_defined_enum_carrier_bound", "Enum private JSON carrier exceeds1 Mi characters")
    return encoded


def create_user_defined_enum(asset_path, display_names, tooltips=None, dry_run=True, save=False, skip_if_exists=False):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("user_defined_enum_native_api_unavailable", "UE58 enum creation binding is unavailable; no fallback executed")
    display_json = _carrier(display_names)
    tooltip_json = _carrier(tooltips)
    raw = fn(asset_path, display_json, tooltip_json, dry_run, save, skip_if_exists)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("user_defined_enum_native_result_invalid", "Native enum operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("user_defined_enum_native_result_invalid", "Native enum changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
