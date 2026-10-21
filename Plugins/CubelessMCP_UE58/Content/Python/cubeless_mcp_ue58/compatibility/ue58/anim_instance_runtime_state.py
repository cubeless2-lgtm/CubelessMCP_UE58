"""UE58 AnimInstance runtime snapshot20; PIE/SIE reads remain Native-owned."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessAnimInstanceRuntimeStateLibrary"
METHOD = "inspect_anim_instance_runtime_state"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("anim_instance_runtime_state_carrier_invalid", "Curve names require finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("anim_instance_runtime_state_carrier_bound", "Private curve carrier exceeds1 Mi UE UTF16 units or contains embedded NUL")
    return encoded


def inspect_anim_instance_runtime_state(actor_label="", actor_name="", actor_path="", component_name="", state_machine_name="",
        include_states=True, include_state_weights=True, include_relevant_anim_times=False,
        include_transition_progress=True, include_inactive_transition_progress=False,
        include_montages=True, include_curves=False, curve_names=None,
        max_state_machines=32, max_state_machine_instances_to_probe=256, max_states_per_machine=64,
        max_transitions_per_machine=128, max_curves=128, prefer_pie_world=True, require_pie_world=False):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("anim_instance_runtime_state_native_api_unavailable", "UE58 AnimInstance runtime snapshot binding is unavailable; no fallback executed")
    curve_names_json = _carrier(curve_names)
    raw = fn(actor_label, actor_name, actor_path, component_name, state_machine_name,
        include_states, include_state_weights, include_relevant_anim_times, include_transition_progress,
        include_inactive_transition_progress, include_montages, include_curves, curve_names_json,
        max_state_machines, max_state_machine_instances_to_probe, max_states_per_machine,
        max_transitions_per_machine, max_curves, prefer_pie_world, require_pie_world)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("anim_instance_runtime_state_native_result_invalid", "Native runtime snapshot operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("anim_instance_runtime_state_native_result_invalid", "Native runtime snapshot changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
