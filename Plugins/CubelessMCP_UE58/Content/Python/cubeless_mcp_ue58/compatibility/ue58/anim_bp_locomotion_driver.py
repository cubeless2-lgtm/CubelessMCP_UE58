"""Specific UE58 locomotion EventGraph binding; no transport or engine fallback."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessAnimBPLocomotionDriverLibrary"
METHOD = "ensure_anim_bp_event_graph_locomotion_driver"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576
LEGACY_COMMAND_FIELDS = ("target_blueprint_path",)


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _private_context(legacy):
    if legacy is None:
        return {}
    if type(legacy) is not dict or any(key not in LEGACY_COMMAND_FIELDS for key in legacy):
        raise OperationError("anim_bp_locomotion_driver_input_invalid",
            "Private context permits only the confirmed target_blueprint_path alias")
    return {key: legacy[key] for key in LEGACY_COMMAND_FIELDS if key in legacy}


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("anim_bp_locomotion_driver_carrier_invalid",
            "Limited target context requires finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("anim_bp_locomotion_driver_carrier_bound",
            "Private target carrier exceeds 1 Mi UTF16 units or contains raw NUL")
    return encoded


def ensure_anim_bp_event_graph_locomotion_driver(source_anim_bp_path,
        target_anim_bp_path="", target_root="/Game/_MCP_Sample/AnimStudy",
        variant_name="LocomotionEventGraph", speed_variable="Speed",
        create_variable_if_missing=True, include_direction=False, direction_variable="Direction",
        create_direction_variable_if_missing=True, include_is_in_air=False, is_in_air_variable="IsInAir",
        create_is_in_air_variable_if_missing=True, include_acceleration=False, acceleration_variable="Acceleration",
        create_acceleration_variable_if_missing=True, include_is_accelerating=False,
        is_accelerating_variable="IsAccelerating", create_is_accelerating_variable_if_missing=True,
        is_accelerating_threshold=3.0, guard_pawn_owner=True, replace_existing_links=False,
        allow_existing_target_reuse=True, overwrite_existing=False, compile=True,
        save_asset=True, dry_run=False, validate_only=False, *, _legacy_command_params=None):
    try:
        context_json = _carrier(_private_context(_legacy_command_params))
    except Exception as error:
        error.native_call_dispatched = False
        raise
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        error = OperationError("anim_bp_locomotion_driver_native_api_unavailable",
            "UE58 locomotion EventGraph binding is unavailable; no fallback executed")
        error.native_call_dispatched = False
        raise error
    raw = fn(source_anim_bp_path, target_anim_bp_path, target_root, variant_name,
        speed_variable, create_variable_if_missing, include_direction, direction_variable,
        create_direction_variable_if_missing, include_is_in_air, is_in_air_variable,
        create_is_in_air_variable_if_missing, include_acceleration, acceleration_variable,
        create_acceleration_variable_if_missing, include_is_accelerating, is_accelerating_variable,
        create_is_accelerating_variable_if_missing, is_accelerating_threshold, guard_pawn_owner,
        replace_existing_links, allow_existing_target_reuse, overwrite_existing, compile,
        save_asset, dry_run, validate_only, context_json)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("anim_bp_locomotion_driver_native_result_invalid",
                "Native operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("anim_bp_locomotion_driver_native_result_invalid",
                "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
