"""Specific UE58 AnimBP state-machine variant binding; no transport or fallback."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessAnimStateMachineVariantLibrary"
METHOD = "ensure_anim_state_machine_sample_variant"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576
PROJECTION_FIELDS = ("states", "transitions")
CONTEXT_FIELDS = ("graph_name", "graph_type", "target_blueprint_path", "entry_state",
    "replace_existing_sample_machine", "state_name", "animation_asset", "position",
    "always_reset_on_entry", "player_inputs", "asset_player_inputs", "blendspace_inputs")
LEGACY_COMMAND_FIELDS = CONTEXT_FIELDS + PROJECTION_FIELDS


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _limited_payloads(states, transitions, legacy=None):
    if legacy is None:
        legacy = {}
    if type(legacy) is not dict or any(key not in LEGACY_COMMAND_FIELDS for key in legacy):
        raise OperationError("anim_state_machine_variant_input_invalid",
            "Private context permits only confirmed graph, shorthand, entry and array-presence keys")
    context = {key: legacy[key] for key in CONTEXT_FIELDS if key in legacy}
    projections = []
    for key, value in (("states", states), ("transitions", transitions)):
        if key in legacy:
            projections.append({key: legacy[key]})
        else:
            projections.append({} if value is None else {key: value})
    return projections[0], projections[1], context


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("anim_state_machine_variant_carrier_invalid",
            "Limited Native payload requires finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("anim_state_machine_variant_carrier_bound",
            "Each carrier exceeds 1 Mi UTF16 units or contains raw NUL")
    return encoded


def ensure_anim_state_machine_sample_variant(source_anim_bp_path, states=None, transitions=None,
        target_anim_bp_path="", target_root="/Game/_MCP_Sample/AnimStudy",
        variant_name="StateMachineSample", state_machine_name="MCP_StateMachine",
        entry_state="", animation_asset_path="", connect_demo_machine_to_output=True,
        replace_existing_root_link=True, replace_existing_entry_link=True,
        allow_existing_target_reuse=True, overwrite_existing=False, compile=True,
        save_asset=True, dry_run=False, validate_only=False, max_states=16,
        max_transitions=32, *, _legacy_command_params=None):
    try:
        states_json, transitions_json, context_json = (
            _carrier(value) for value in _limited_payloads(states, transitions, _legacy_command_params))
    except Exception as error:
        error.native_call_dispatched = False
        raise
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        error = OperationError("anim_state_machine_variant_native_api_unavailable",
            "UE58 state-machine variant binding is unavailable; no fallback executed")
        error.native_call_dispatched = False
        raise error
    raw = fn(source_anim_bp_path, states_json, transitions_json, target_anim_bp_path,
        target_root, variant_name, state_machine_name, entry_state, animation_asset_path,
        connect_demo_machine_to_output, replace_existing_root_link, replace_existing_entry_link,
        allow_existing_target_reuse, overwrite_existing, compile, save_asset, dry_run,
        validate_only, max_states, max_transitions, context_json)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("anim_state_machine_variant_native_result_invalid",
                "Native operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("anim_state_machine_variant_native_result_invalid",
                "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
