"""Live AnimInstance writes, optional tick/refresh and snapshot in one Native call.

One Named journal or the existing Batch parent records the whole Native prefix.
Python never finds a world/actor, sets a property, ticks, refreshes, starts PIE,
compiles, saves, restores or retries the operation itself.
"""
import copy
import math
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import anim_instance_runtime_property_probe as api
from . import state
from .state import OperationError

METHOD = "set_anim_instance_runtime_property_for_probe"
PUBLIC_FIELDS = ("actor_label", "actor_name", "actor_path", "component_name", "properties", "property_name", "value",
    "tick_after_set", "tick_count", "tick_delta_time", "refresh_bone_transforms", "include_snapshot_after",
    "include_previous_values", "state_machine_name", "include_states", "include_state_weights",
    "include_relevant_anim_times", "include_transition_progress", "include_inactive_transition_progress",
    "include_montages", "include_curves", "curve_names", "max_transitions_per_machine", "prefer_pie_world", "require_pie_world")
TEXT_FIELDS = ("actor_label", "actor_name", "actor_path", "component_name", "property_name", "state_machine_name")
BOOL_FIELDS = ("tick_after_set", "refresh_bone_transforms", "include_snapshot_after", "include_previous_values",
    "include_states", "include_state_weights", "include_relevant_anim_times", "include_transition_progress",
    "include_inactive_transition_progress", "include_montages", "include_curves", "prefer_pie_world", "require_pie_world")
LIMIT_FIELDS = ("tick_count", "max_transitions_per_machine")


def _arguments(actor_label, actor_name, actor_path, component_name, properties, property_name, value,
        tick_after_set, tick_count, tick_delta_time, refresh_bone_transforms, include_snapshot_after,
        include_previous_values, state_machine_name, include_states, include_state_weights,
        include_relevant_anim_times, include_transition_progress, include_inactive_transition_progress,
        include_montages, include_curves, curve_names, max_transitions_per_machine, prefer_pie_world, require_pie_world):
    arguments = dict(zip(PUBLIC_FIELDS, (actor_label, actor_name, actor_path, component_name, properties, property_name, value,
        tick_after_set, tick_count, tick_delta_time, refresh_bone_transforms, include_snapshot_after,
        include_previous_values, state_machine_name, include_states, include_state_weights,
        include_relevant_anim_times, include_transition_progress, include_inactive_transition_progress,
        include_montages, include_curves, curve_names, max_transitions_per_machine, prefer_pie_world, require_pie_world)))
    for field in TEXT_FIELDS:
        if type(arguments[field]) is not str:
            raise OperationError("anim_instance_runtime_property_probe_input_invalid", field + " must be a string; None is not an empty filter")
    for field in BOOL_FIELDS:
        if type(arguments[field]) is not bool:
            raise OperationError("anim_instance_runtime_property_probe_input_invalid", field + " must be a boolean")
    for field in LIMIT_FIELDS:
        if type(arguments[field]) is not int or not -(1 << 63) <= arguments[field] < (1 << 63):
            raise OperationError("anim_instance_runtime_property_probe_input_invalid", field + " must be an int64 integer; booleans are not limits")
    if type(tick_delta_time) not in (int, float):
        raise OperationError("anim_instance_runtime_property_probe_input_invalid", "tick_delta_time must be a finite number")
    try:
        arguments["tick_delta_time"] = float(tick_delta_time)
    except (ValueError, OverflowError) as error:
        raise OperationError("anim_instance_runtime_property_probe_input_invalid", "tick_delta_time is outside finite double range") from error
    if not math.isfinite(arguments["tick_delta_time"]):
        raise OperationError("anim_instance_runtime_property_probe_input_invalid", "tick_delta_time must be finite")
    if properties is not None and type(properties) is not dict:
        raise OperationError("anim_instance_runtime_property_probe_input_invalid", "properties must be None or a JSON object")
    if curve_names is not None and (type(curve_names) is not list or any(type(item) is not str for item in curve_names)):
        raise OperationError("anim_instance_runtime_property_probe_input_invalid", "curve_names must be None or a list of strings")
    # Preserve the wrapper's raw string truthiness: whitespace names retain value;
    # an empty property_name omits value, even when the unused Python value is not JSON.
    if not property_name:
        arguments["value"] = None
    api._carrier(arguments["properties"])
    api._carrier(arguments["value"])
    api._carrier(arguments["curve_names"])
    return copy.deepcopy(arguments)


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_twenty_five_public_parameters_preserved=True,
        native_private_anim_instance_source=arguments.get("_anim_instance_source", "main"),
        private_legacy_params_key_scope=("properties", "property_values", "curve_names", "anim_instance_source"),
        private_legacy_rows_order_duplicates_and_errors_native_owned=True,
        value_ignored_when_property_name_empty=not bool(arguments["property_name"]),
        native_world_selection_assignment_merge_order_and_row_validation_authoritative=True,
        native_tick_refresh_snapshot_after_partial_write_authoritative=True,
        native_limits_name_resolution_and_property_type_conversion_authoritative=True,
        adapter_Editor_idle_or_noPIE_gate=False, PIE_SIE_runtime_probe_allowed=True,
        adapter_world_actor_component_or_AnimInstance_queries=0,
        adapter_property_writes=0, adapter_tick_calls=0, adapter_refresh_calls=0,
        adapter_start_play_calls=0, adapter_stop_play_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        asset_modified_false_is_total_SDK_purity_proof=False,
        runtime_only_is_zero_effects_proof=False, snapshot_is_frame_atomic=False,
        actual_named_nullable_wire_verified=False, actual_named_whole_int64_token_preserved=False,
        automatic_action_retry=False, atomic_rollback=False,
        runtime_pose_verified=False, playback_verified=False, unique_external_pipeline_complete=False)


def _adopt(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], native_observed_changed=native["changed"],
        mutation_state_unknown=not known, change_state_known=known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_runtime_property_probe_compile_receipt")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_runtime_property_probe_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD, "requested_properties": copy.deepcopy(arguments["properties"]),
        "property_name": arguments["property_name"], "attempted": native["mutation_attempted"],
        "change_state_known": known, "changed": native["changed"] if known else None,
        "native_observed_changed": native["changed"], "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_runtime_property_probe_completion",
        native_postcondition_passed=native["postcondition_passed"], total_change_state_known=known,
        full_property_rows_previous_echo_tick_refresh_snapshot_and_errors_preserved=True,
        partial_failures_not_reinterpreted_or_rolled_back=True,
        frame_atomic_snapshot=False, runtime_pose="not_run", playback="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False, atomic_rollback=False,
        duplicate_risk="unknown_runtime_write_tick_refresh_or_snapshot_effects_inspect_prefix_before_new_call" if not known
            else "partial_or_attempted_runtime_changes_inspect_prefix" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_anim_instance_runtime_property_probe_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, change_state_known=True,
        pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_anim_instance_runtime_property_probe_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True, change_state_known=False)
    result["changes"] = [{"kind": METHOD, "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_anim_instance_runtime_property_probe_operation"
    try:
        native = api.set_anim_instance_runtime_property_for_probe(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "native_call_dispatched", None) is False:
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False, change_state_known=True)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_anim_instance_runtime_property_probe_operation"
        callback(result)  # Persist malformed raw and unknown runtime work before error.
        raise
    _adopt(result, arguments, native)
    callback(result)  # All partial rows, actual work and snapshot precede failure.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_instance_runtime_property_probe_native_failed",
            native.get("error") or "Native runtime-property probe failed; partial state is retained")
    if not native["postcondition_passed"]:
        raise OperationError("anim_instance_runtime_property_probe_result_unverified", "Native postcondition is unverified; inspect prefix without replay")
    result.update(success=True, stage="complete")
    return result


def _adopt_rejection(result, error):
    result["changes"] = []
    state.fail(result, error)
    result["stage"] = "anim_instance_runtime_property_probe_input_validation"
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
        mutation_state_unknown=False, change_state_known=True, journal_created=False,
        journal_created_scope="new operation or child journal; existing Batch parent is retained")
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["validation"].update(status="input_rejected", total_change_state_known=True)
    return result


def _rejected(error):
    return _adopt_rejection(state.new_result(), error)


def set_anim_instance_runtime_property_for_probe(actor_label: str = "", actor_name: str = "", actor_path: str = "", component_name: str = "",
        properties: Optional[Dict[str, Any]] = None, property_name: str = "", value: Any = None,
        tick_after_set: bool = False, tick_count: int = 1, tick_delta_time: float = 1.0 / 30.0,
        refresh_bone_transforms: bool = True, include_snapshot_after: bool = True, include_previous_values: bool = True,
        state_machine_name: str = "", include_states: bool = True, include_state_weights: bool = True,
        include_relevant_anim_times: bool = False, include_transition_progress: bool = True,
        include_inactive_transition_progress: bool = False, include_montages: bool = True, include_curves: bool = False,
        curve_names: Optional[List[str]] = None, max_transitions_per_machine: int = 128,
        prefer_pie_world: bool = True, require_pie_world: bool = False) -> Dict[str, Any]:
    try:
        arguments = _arguments(actor_label, actor_name, actor_path, component_name, properties, property_name, value,
            tick_after_set, tick_count, tick_delta_time, refresh_bone_transforms, include_snapshot_after,
            include_previous_values, state_machine_name, include_states, include_state_weights,
            include_relevant_anim_times, include_transition_progress, include_inactive_transition_progress,
            include_montages, include_curves, curve_names, max_transitions_per_machine, prefer_pie_world, require_pie_world)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def set_anim_instance_runtime_property_for_probe_action(result, actor_label: str = "", actor_name: str = "", actor_path: str = "", component_name: str = "",
        properties: Optional[Dict[str, Any]] = None, property_name: str = "", value: Any = None,
        tick_after_set: bool = False, tick_count: int = 1, tick_delta_time: float = 1.0 / 30.0,
        refresh_bone_transforms: bool = True, include_snapshot_after: bool = True, include_previous_values: bool = True,
        state_machine_name: str = "", include_states: bool = True, include_state_weights: bool = True,
        include_relevant_anim_times: bool = False, include_transition_progress: bool = True,
        include_inactive_transition_progress: bool = False, include_montages: bool = True, include_curves: bool = False,
        curve_names: Optional[List[str]] = None, max_transitions_per_machine: int = 128,
        prefer_pie_world: bool = True, require_pie_world: bool = False, checkpoint_callback=None, *, _anim_instance_source="main", _legacy_command_params=None):
    try:
        _, _, selected_source = api._legacy_projection(properties, curve_names, _anim_instance_source, _legacy_command_params)
        legacy = _legacy_command_params or {}
        public_properties = None if "properties" in legacy else properties
        public_curves = None if "curve_names" in legacy else curve_names
        arguments = _arguments(actor_label, actor_name, actor_path, component_name, public_properties, property_name, value,
            tick_after_set, tick_count, tick_delta_time, refresh_bone_transforms, include_snapshot_after,
            include_previous_values, state_machine_name, include_states, include_state_weights,
            include_relevant_anim_times, include_transition_progress, include_inactive_transition_progress,
            include_montages, include_curves, public_curves, max_transitions_per_machine, prefer_pie_world, require_pie_world)
        arguments["_anim_instance_source"] = selected_source
        if _legacy_command_params is not None:
            arguments["_legacy_command_params"] = copy.deepcopy(_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result, error)
        result["data"]["parent_journal_only"] = True
        result["stage"] = "returned_anim_instance_runtime_property_probe_operation"
        (checkpoint_callback or state.checkpoint)(result)
        raise OperationError(result["error_code"], result["error"]) from error
    return _run_action(result, arguments, checkpoint_callback)
