"""Transient BlendSpace pose-grid operation17 with one durable parent journal.

Native owns world selection, temporary actor setup, evaluation/ticks, sampling,
baselines/deltas and requested cleanup. This is runtime mutation, not a pure read.
No extra engine query, tick, cleanup, compiler, save or automatic retry occurs here.
"""
import copy
import math
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import blendspace_pose_grid as api
from . import state
from .state import OperationError

METHOD = "sample_blendspace_runtime_pose_grid"
PUBLIC_FIELDS = ("skeletal_mesh", "blendspace_path", "blendspaces", "samples", "sample_bones", "sample_sockets",
    "actor_label", "sample_time", "settle_tick_count", "tick_count", "tick_delta_time", "include_axis_extremes",
    "allow_missing_bones", "refresh_bone_transforms", "cleanup", "prefer_pie_world", "require_pie_world")
TEXT_FIELDS = ("skeletal_mesh", "blendspace_path", "actor_label")
ARRAY_FIELDS = ("blendspaces", "samples", "sample_bones", "sample_sockets")
NAME_FIELDS = ("sample_bones", "sample_sockets")
INT_FIELDS = ("settle_tick_count", "tick_count")
NUMBER_FIELDS = ("sample_time", "tick_delta_time")
BOOL_FIELDS = PUBLIC_FIELDS[11:]
LEGACY_COMMAND_FIELDS = api.LEGACY_COMMAND_FIELDS


def _arguments(skeletal_mesh, blendspace_path, blendspaces, samples, sample_bones, sample_sockets,
        actor_label, sample_time, settle_tick_count, tick_count, tick_delta_time, include_axis_extremes,
        allow_missing_bones, refresh_bone_transforms, cleanup, prefer_pie_world, require_pie_world,
        *, _legacy_command_params=None):
    values = dict(zip(PUBLIC_FIELDS, (skeletal_mesh, blendspace_path, blendspaces, samples, sample_bones,
        sample_sockets, actor_label, sample_time, settle_tick_count, tick_count, tick_delta_time,
        include_axis_extremes, allow_missing_bones, refresh_bone_transforms, cleanup, prefer_pie_world, require_pie_world)))
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("blendspace_pose_grid_input_invalid", field + " must be a string")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("blendspace_pose_grid_input_invalid", field + " must be a boolean")
    for field in INT_FIELDS:
        if type(values[field]) is not int or not -(1 << 63) <= values[field] < (1 << 63):
            raise OperationError("blendspace_pose_grid_input_invalid", field + " must be a signed int64; Native owns clamping")
    for field in NUMBER_FIELDS:
        if type(values[field]) not in (int, float):
            raise OperationError("blendspace_pose_grid_input_invalid", field + " must be a finite number")
        try:
            number = float(values[field])
        except (ValueError, OverflowError) as error:
            raise OperationError("blendspace_pose_grid_input_invalid", field + " is outside the finite double range") from error
        if not math.isfinite(number):
            raise OperationError("blendspace_pose_grid_input_invalid", field + " must be finite")
        values[field] = number
    legacy = {} if _legacy_command_params is None else _legacy_command_params
    if type(legacy) is not dict or any(field not in LEGACY_COMMAND_FIELDS for field in legacy):
        raise OperationError("blendspace_pose_grid_input_invalid", "Private projection permits only the seven confirmed grid keys")
    for field in ARRAY_FIELDS:
        if field in legacy:
            values[field] = legacy[field]
        elif values[field] is not None and (type(values[field]) is not list
                or field in NAME_FIELDS and any(type(item) is not str for item in values[field])):
            raise OperationError("blendspace_pose_grid_input_invalid", field + " must match its nullable public list type")
    # Preserve raw field presence and both aliases in three fixed projection objects.
    # Row semantics, fallback, FName and per-sample diagnostics remain Native-owned.
    api._native_carriers(values["blendspaces"], values["samples"], values["sample_bones"], values["sample_sockets"], legacy)
    try:
        return copy.deepcopy(values), copy.deepcopy(legacy)
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("blendspace_pose_grid_input_invalid", "Arguments could not be copied before dispatch") from error


def _context(result, arguments, legacy):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        legacy_command_params=copy.deepcopy(legacy), legacy_command_keys=list(legacy),
        original_seventeen_public_parameter_order_preserved=True,
        native_target_sample_priority_and_bone_socket_fallback_authoritative=True,
        native_temporary_actor_component_and_per_sample_animation_ticks_authoritative=True,
        native_pose_baseline_deltas_partial_errors_and_cleanup_authoritative=True,
        private_projection_objects_only=True, none_empty_alias_presence_and_order_preserved=True,
        runtime_mutation_not_pure_observation=True, adapter_Editor_idle_or_noPIE_gate=False,
        PIE_SIE_Game_GamePreview_selection_native_authoritative=True,
        adapter_asset_load_calls=0, adapter_world_or_component_queries=0,
        adapter_actor_create_destroy_calls=0, adapter_tick_or_refresh_calls=0,
        adapter_start_play_calls=0, adapter_stop_play_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        cleanup_failure_not_automatically_retried=True, total_SDK_effects_fully_observed=False,
        actual_named_nullable_or_numeric_wire_verified=False, snapshot_is_frame_atomic=False,
        packaged_runtime_verified=False, unique_external_pipeline_complete=False,
        automatic_action_retry=False, atomic_rollback=False)


def _adopt(result, arguments, legacy, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], change_state_known=known, mutation_state_unknown=not known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments, legacy)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_blendspace_pose_grid_compile_receipt",
        completion_scope="Native_explicit_receipt; opaque_SDK_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_blendspace_pose_grid_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_blendspace_pose_grid_completion",
        native_postcondition_passed=native["postcondition_passed"],
        full_native_runtime_setup_ticks_pose_samples_deltas_cleanup_and_partial_rows_preserved=True,
        cleanup_failure_not_reinterpreted_or_retried=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        frame_atomic_snapshot=False, playback="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_runtime_spawn_setup_tick_pose_capture_or_cleanup_inspect_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_blendspace_pose_grid_operation"

def _run_action(result, arguments, legacy, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments, legacy)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, change_state_known=True, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_blendspace_pose_grid_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, change_state_known=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_blendspace_pose_grid_operation"
    try:
        native = api.sample_blendspace_runtime_pose_grid(*(arguments[field] for field in PUBLIC_FIELDS), _legacy_command_params=legacy)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "blendspace_pose_grid_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                change_state_known=True, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_blendspace_pose_grid_operation"
        callback(result)
        raise
    _adopt(result, arguments, legacy, native)
    callback(result)  # Keep full partial/native/unknown prefix before error criteria.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blendspace_pose_grid_native_failed",
            native.get("error") or "Native BlendSpace pose-grid failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("blendspace_pose_grid_result_unverified", "Native pose-grid postcondition is unverified; no replay")
    result.update(success=True, stage="complete")
    return result

def _adopt_rejection(result, error):
    state.fail(result, error)
    result["stage"] = "blendspace_pose_grid_input_validation"
    result["changes"] = []
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
        change_state_known=True, mutation_state_unknown=False, journal_created=False)
    return result

def _rejected(error):
    return _adopt_rejection(state.new_result(), error)

def sample_blendspace_runtime_pose_grid(skeletal_mesh: str, blendspace_path: str='', blendspaces: Optional[List[Any]]=None, samples: Optional[List[Dict[str, Any]]]=None, sample_bones: Optional[List[str]]=None, sample_sockets: Optional[List[str]]=None, actor_label: str='MCP_BlendSpace_RuntimePoseGrid', sample_time: float=0.25, settle_tick_count: int=2, tick_count: int=1, tick_delta_time: float=1.0 / 30.0, include_axis_extremes: bool=False, allow_missing_bones: bool=False, refresh_bone_transforms: bool=True, cleanup: bool=True, prefer_pie_world: bool=True, require_pie_world: bool=True) -> Dict[str, Any]:
    try:
        arguments, legacy = _arguments(skeletal_mesh, blendspace_path, blendspaces, samples, sample_bones, sample_sockets, actor_label, sample_time, settle_tick_count, tick_count, tick_delta_time, include_axis_extremes, allow_missing_bones, refresh_bone_transforms, cleanup, prefer_pie_world, require_pie_world)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments, legacy))


def sample_blendspace_runtime_pose_grid_action(result, skeletal_mesh: str, blendspace_path: str='', blendspaces: Optional[List[Any]]=None, samples: Optional[List[Dict[str, Any]]]=None, sample_bones: Optional[List[str]]=None, sample_sockets: Optional[List[str]]=None, actor_label: str='MCP_BlendSpace_RuntimePoseGrid', sample_time: float=0.25, settle_tick_count: int=2, tick_count: int=1, tick_delta_time: float=1.0 / 30.0, include_axis_extremes: bool=False, allow_missing_bones: bool=False, refresh_bone_transforms: bool=True, cleanup: bool=True, prefer_pie_world: bool=True, require_pie_world: bool=True, checkpoint_callback=None, *, _legacy_command_params=None):
    try:
        arguments, legacy = _arguments(skeletal_mesh, blendspace_path, blendspaces, samples, sample_bones, sample_sockets, actor_label, sample_time, settle_tick_count, tick_count, tick_delta_time, include_axis_extremes, allow_missing_bones, refresh_bone_transforms, cleanup, prefer_pie_world, require_pie_world, _legacy_command_params=_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result, error)
        result["data"]["parent_journal_only"] = True
        (checkpoint_callback or state.checkpoint)(result)
        raise
    return _run_action(result, arguments, legacy, checkpoint_callback)
