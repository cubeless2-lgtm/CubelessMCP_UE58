"""Original runtime20 on official Registry; JSON1/decimal5 carriers are private."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import anim_instance_runtime_state as runtime_state

@unreal.uclass()
class CubelessAnimInstanceRuntimeTools(unreal.ToolsetDefinition):
    """Inspect existing AnimInstance state in PIE/SIE/game or the allowed editor fallback."""

    @toolset_registry.tool_call
    @staticmethod
    def inspect_anim_instance_runtime_state(actor_label: str = "", actor_name: str = "", actor_path: str = "",
            component_name: str = "", state_machine_name: str = "", include_states: bool = True,
            include_state_weights: bool = True, include_relevant_anim_times: bool = False,
            include_transition_progress: bool = True, include_inactive_transition_progress: bool = False,
            include_montages: bool = True, include_curves: bool = False, curve_names: str = "null",
            max_state_machines: str = "32", max_state_machine_instances_to_probe: str = "256",
            max_states_per_machine: str = "64", max_transitions_per_machine: str = "128", max_curves: str = "128",
            prefer_pie_world: bool = True, require_pie_world: bool = False) -> CubelessPythonOperationResult:
        """Inspect existing runtime state machines, state weights, optional relevant animation times, transition progress, montages and curves. Public20 retains optional empty actor/component/machine strings, curve_names as null or an array of strings, five integer limits and nine exact JSON flags. Native owns world/component/AnimInstance selection, limit clamps, runtime/parallel-evaluation safety, diagnostic counts and actual observation/change-unknown state. PIE/SIE/game reading is permitted; this tool does not start play, tick, update, initialize, compile or save. None, [], empty names and order are preserved for Native curve selection. Full returned/partial failure and compile-save receipt state reaches the parent journal before errors. No atomic rollback or uncertain-request retry is promised. Official Named HTTP numbers may already be rounded before this adapter; exact large-limit Batch action verification is separate."""
        rejected, report, raw = _runtime_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            names = _decode_curve_names(curve_names)
            limits = _decode_limits(max_state_machines, max_state_machine_instances_to_probe,
                max_states_per_machine, max_transitions_per_machine, max_curves)
        except Exception as carrier_error:
            result = runtime_state._rejected(runtime_state.OperationError("anim_instance_runtime_carrier_invalid", str(carrier_error)))
            result["stage"] = "anim_instance_runtime_registration_carrier_admission"
            result["data"].update(registration_guard=report, registration_guard_raw=raw,
                native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(runtime_state.inspect_anim_instance_runtime_state(actor_label, actor_name, actor_path,
            component_name, state_machine_name, include_states, include_state_weights, include_relevant_anim_times,
            include_transition_progress, include_inactive_transition_progress, include_montages, include_curves,
            names, limits[0], limits[1], limits[2], limits[3], limits[4], prefer_pie_world, require_pie_world))


def _runtime_guard_admission():
    """Only this exact current registered wrapper delegation is admitted."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessAnimInstanceRuntimeAttachmentLibrary", None), "verify_anim_instance_runtime_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact AnimInstance runtime raw-input admission helper unavailable")
        raw = helper(CubelessAnimInstanceRuntimeTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_anim_instance_runtime_toolset"
                and report.get("delegated_call_admitted") is True and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "inspect_anim_instance_runtime_state"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded runtime inspection delegation required; no Native inspection dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = runtime_state._rejected(runtime_state.OperationError("anim_instance_runtime_raw_guard_admission_missing", message))
    result["stage"] = "anim_instance_runtime_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError("Non-finite JSON constant is unsupported: " + value)


def _decode_curve_names(text):
    """Restore None/list[str] without trimming names or deciding Native fallback."""
    if type(text) is not str or len(text.encode("utf-16-le")) // 2 > 1048576:
        raise ValueError("Curve names require at most 1Mi UE UTF16 units")
    value = json.loads(text, parse_constant=_invalid_constant)
    if value is not None and (type(value) is not list or any(type(name) is not str for name in value)):
        raise ValueError("Curve names must decode as null or an array of strings")
    return value


def _decode_limits(*values):
    """Decode private canonical decimals; Native retains the original five clamps."""
    if len(values) != 5:
        raise ValueError("Exactly five private integer carriers required")
    limits = []
    for text in values:
        if type(text) is not str or not text or len(text) > 20:
            raise ValueError("Private runtime limit requires a bounded canonical decimal")
        value = int(text)
        if str(value) != text or value < -(1 << 63) or value > (1 << 63) - 1:
            raise ValueError("Private runtime limit must be an exact canonical int64 decimal")
        limits.append(value)
    return limits
