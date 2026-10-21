"""Fixed native animation metadata observer17; no Python asset loads or fallback."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessAnimationAssetMetadataLibrary"
METHOD = "inspect_animation_asset_metadata_guarded"
BINDINGS = (METHOD,)


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {METHOD: callable(getattr(library, METHOD, None))}


def inspect_animation_asset_metadata_guarded(asset_path,
        include_notifies,
        include_notify_details,
        include_curves,
        include_curve_keys,
        include_sync_markers,
        include_montage_sections,
        include_montage_slots,
        include_metadata,
        max_notifies,
        max_curves,
        max_curve_keys_per_curve,
        max_markers,
        max_sections,
        max_slots,
        max_segments_per_slot,
        require_supported_class):
    function = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(function):
        raise OperationError("native_animation_asset_metadata_api_unavailable", "Exact native metadata observer unavailable; no asset load or fallback dispatched")
    raw = function(asset_path,
            include_notifies,
            include_notify_details,
            include_curves,
            include_curve_keys,
            include_sync_markers,
            include_montage_sections,
            include_montage_slots,
            include_metadata,
            max_notifies,
            max_curves,
            max_curve_keys_per_curve,
            max_markers,
            max_sections,
            max_slots,
            max_segments_per_slot,
            require_supported_class)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD:
            raise OperationError("invalid_native_animation_asset_metadata_result", "Returned observer operation differs; raw prefix retained")
        if "changed" not in native or native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("invalid_native_animation_asset_metadata_result", "Native changed must be bool or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
