"""Original MPC sync controller7 through the fixed hidden native binding."""
import json
import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessMaterialCollectionSyncLibrary"
BINDINGS = ("set_material_parameter_collection_sync",)
PREPARATION_METHOD = "prepare_material_collection_sync_toolset"


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def preparation_available():
    return callable(getattr(getattr(unreal, LIBRARY, None), PREPARATION_METHOD, None))


def set_material_parameter_collection_sync(action, source_collection_path, target_collection_path,
        parameter_names, interval_seconds, set_asset_defaults_on_enable, save_on_enable):
    method = BINDINGS[0]
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("material_collection_sync_native_api_unavailable",
            "Exact native sync binding unavailable; no controller start/stop/copy/save dispatched")
    # Original Python sends [] for None/empty; this is private JSON, never a public string field.
    names_json = json.dumps(parameter_names or [], ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    raw = function(action, source_collection_path, target_collection_path, names_json, interval_seconds,
        set_asset_defaults_on_enable, save_on_enable)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_material_collection_sync_result",
                "Native result lacks the original controller operation; raw returned prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value
