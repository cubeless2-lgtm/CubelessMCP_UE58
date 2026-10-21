"""Runtime snapshot20 in one Named journal or the existing Batch parent.

Native owns PIE/Game/GamePreview/editor priority, actor/component filters and
current AnimInstance machine/transition/montage/curve reads. Python never starts
or ticks PIE/SIE, changes properties, loads an asset, compiles or saves.
"""
import copy
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import anim_instance_runtime_state as api
from . import state
from .state import OperationError

METHOD = "inspect_anim_instance_runtime_state"
PUBLIC_FIELDS = ("actor_label", "actor_name", "actor_path", "component_name", "state_machine_name",
    "include_states", "include_state_weights", "include_relevant_anim_times", "include_transition_progress",
    "include_inactive_transition_progress", "include_montages", "include_curves", "curve_names",
    "max_state_machines", "max_state_machine_instances_to_probe", "max_states_per_machine",
    "max_transitions_per_machine", "max_curves", "prefer_pie_world", "require_pie_world")
TEXT_FIELDS = PUBLIC_FIELDS[:5]
BOOL_FIELDS = PUBLIC_FIELDS[5:12] + PUBLIC_FIELDS[18:20]
LIMIT_FIELDS = PUBLIC_FIELDS[13:18]


def _arguments(actor_label, actor_name, actor_path, component_name, state_machine_name,
        include_states, include_state_weights, include_relevant_anim_times, include_transition_progress,
        include_inactive_transition_progress, include_montages, include_curves, curve_names,
        max_state_machines, max_state_machine_instances_to_probe, max_states_per_machine,
        max_transitions_per_machine, max_curves, prefer_pie_world, require_pie_world):
    values = dict(actor_label=actor_label, actor_name=actor_name, actor_path=actor_path,
        component_name=component_name, state_machine_name=state_machine_name,
        include_states=include_states, include_state_weights=include_state_weights,
        include_relevant_anim_times=include_relevant_anim_times, include_transition_progress=include_transition_progress,
        include_inactive_transition_progress=include_inactive_transition_progress,
        include_montages=include_montages, include_curves=include_curves, curve_names=curve_names,
        max_state_machines=max_state_machines, max_state_machine_instances_to_probe=max_state_machine_instances_to_probe,
        max_states_per_machine=max_states_per_machine, max_transitions_per_machine=max_transitions_per_machine,
        max_curves=max_curves, prefer_pie_world=prefer_pie_world, require_pie_world=require_pie_world)
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("anim_instance_runtime_state_input_invalid", field + " must be a string; None is not an empty filter")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("anim_instance_runtime_state_input_invalid", field + " must be a boolean")
    for field in LIMIT_FIELDS:
        value = values[field]
        if type(value) is not int or not -(1 << 63) <= value < (1 << 63):
            raise OperationError("anim_instance_runtime_state_input_invalid", field + " must be an int64 integer; booleans are not limits")
    if curve_names is not None and (type(curve_names) is not list or any(type(item) is not str for item in curve_names)):
        raise OperationError("anim_instance_runtime_state_input_invalid", "curve_names must be None or a list of strings")
    values = copy.deepcopy(values)
    # Keep None, [], empty names, duplicates and name text unchanged. Native alone
    # owns auto fallback, requested_only, FName bounds/meaning and all limits.
    api._carrier(values["curve_names"])
    return values


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_twenty_public_parameters_forwarded_unchanged=True,
        curve_names_preserve_none_empty_and_text=True,
        native_world_selection_filters_and_runtime_snapshot_authoritative=True,
        native_limits_name_resolution_and_missing_data_diagnostics_authoritative=True,
        adapter_Editor_idle_or_noPIE_gate=False, PIE_SIE_runtime_reads_allowed=True,
        adapter_asset_load_calls=0, adapter_world_or_AnimInstance_queries=0,
        adapter_property_writes=0, adapter_explicit_modify_calls=0,
        adapter_start_play_calls=0, adapter_stop_play_calls=0, adapter_tick_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        missing_machine_montage_or_curve_promoted_to_operation_error=False,
        read_only_is_total_SDK_purity_proof=False,
        ongoing_runtime_progress_is_not_an_adapter_tick=True,
        snapshot_is_stable_cross_call_or_frame_atomic=False,
        actual_named_nullable_wire_verified=False, actual_named_whole_int64_token_preserved=False,
        automatic_action_retry=False, atomic_rollback=False,
        runtime_pose_verified=False, playback_verified=False, unique_external_pipeline_complete=False)


def _adopt(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_anim_instance_runtime_compile_receipt",
        completion_scope="Native_explicit_receipt; runtime_getters_callbacks_and_implicit_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_anim_instance_runtime_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_filters": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_AnimInstance_runtime_snapshot_completion",
        native_postcondition_passed=native["postcondition_passed"],
        sampled_world_type=native["data"].get("sampled_world_type"),
        sampled_world_name=native["data"].get("sampled_world_name"),
        matched_actor_count=native["data"].get("matched_actor_count"),
        matched_component_count=native["data"].get("matched_component_count"),
        machine_transition_montage_curve_snapshot_preserved_in_data=True,
        native_missing_data_and_warning_criteria_preserved=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        frame_atomic_snapshot=False, runtime_pose="not_run", playback="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_world_actor_component_AnimInstance_getter_callback_effects_inspect_partial_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_anim_instance_runtime_state_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_anim_instance_runtime_state_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_filters": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_anim_instance_runtime_state_operation"
    try:
        native = api.inspect_anim_instance_runtime_state(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "anim_instance_runtime_state_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_anim_instance_runtime_state_operation"
        callback(result) # Retain malformed raw/uncertain runtime read before error reporting.
        raise
    _adopt(result, arguments, native)
    callback(result) # Preserve the whole partial snapshot and actual state before criteria.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_instance_runtime_state_native_failed",
            native.get("error") or "Native runtime snapshot failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("anim_instance_runtime_state_result_unverified", "Native snapshot completion is unverified; no replay")
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["stage"] = "anim_instance_runtime_state_input_validation"
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False, journal_created=False)
    return result


def inspect_anim_instance_runtime_state(actor_label: str = "", actor_name: str = "", actor_path: str = "",
        component_name: str = "", state_machine_name: str = "", include_states: bool = True,
        include_state_weights: bool = True, include_relevant_anim_times: bool = False,
        include_transition_progress: bool = True, include_inactive_transition_progress: bool = False,
        include_montages: bool = True, include_curves: bool = False, curve_names: Optional[List[str]] = None,
        max_state_machines: int = 32, max_state_machine_instances_to_probe: int = 256,
        max_states_per_machine: int = 64, max_transitions_per_machine: int = 128, max_curves: int = 128,
        prefer_pie_world: bool = True, require_pie_world: bool = False) -> Dict[str, Any]:
    try:
        arguments = _arguments(actor_label, actor_name, actor_path, component_name, state_machine_name,
            include_states, include_state_weights, include_relevant_anim_times, include_transition_progress,
            include_inactive_transition_progress, include_montages, include_curves, curve_names,
            max_state_machines, max_state_machine_instances_to_probe, max_states_per_machine,
            max_transitions_per_machine, max_curves, prefer_pie_world, require_pie_world)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def inspect_anim_instance_runtime_state_action(result, actor_label: str = "", actor_name: str = "", actor_path: str = "",
        component_name: str = "", state_machine_name: str = "", include_states: bool = True,
        include_state_weights: bool = True, include_relevant_anim_times: bool = False,
        include_transition_progress: bool = True, include_inactive_transition_progress: bool = False,
        include_montages: bool = True, include_curves: bool = False, curve_names: Optional[List[str]] = None,
        max_state_machines: int = 32, max_state_machine_instances_to_probe: int = 256,
        max_states_per_machine: int = 64, max_transitions_per_machine: int = 128, max_curves: int = 128,
        prefer_pie_world: bool = True, require_pie_world: bool = False, checkpoint_callback=None):
    return _run_action(result, _arguments(actor_label, actor_name, actor_path, component_name, state_machine_name,
        include_states, include_state_weights, include_relevant_anim_times, include_transition_progress,
        include_inactive_transition_progress, include_montages, include_curves, curve_names,
        max_state_machines, max_state_machine_instances_to_probe, max_states_per_machine,
        max_transitions_per_machine, max_curves, prefer_pie_world, require_pie_world), checkpoint_callback)
