"""Immediate skeletal bone/socket observer8 with one Named or existing Batch journal.

Native owns world priority, first matching actor/component selection, name fallback,
bone/socket validity and current transforms. Python never starts/stops/ticks play,
updates a pose, loads an asset, compiles or saves. Read-only describes requested
work; it does not prove purity of opaque SDK getters or a frame-atomic snapshot.
"""
import copy
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import skeletal_bone_sampling as api
from . import state
from .state import OperationError

METHOD = "sample_skeletal_bones_in_sie"
PUBLIC_FIELDS = ("actor_label", "actor_name", "actor_path", "component_name",
    "bones", "sockets", "prefer_pie_world", "require_pie_world")
TEXT_FIELDS = PUBLIC_FIELDS[:4]
NAME_FIELDS = PUBLIC_FIELDS[4:6]
BOOL_FIELDS = PUBLIC_FIELDS[6:8]
LEGACY_COMMAND_FIELDS = ("bones", "sockets")


def _arguments(actor_label, actor_name, actor_path, component_name,
        bones, sockets, prefer_pie_world, require_pie_world, *, _legacy_command_params=None):
    values = dict(actor_label=actor_label, actor_name=actor_name, actor_path=actor_path,
        component_name=component_name, bones=bones, sockets=sockets,
        prefer_pie_world=prefer_pie_world, require_pie_world=require_pie_world)
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("skeletal_bone_sampling_input_invalid", field + " must be a string; None is not an empty filter")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("skeletal_bone_sampling_input_invalid", field + " must be a boolean")
    if _legacy_command_params is not None and type(_legacy_command_params) is not dict:
        raise OperationError("skeletal_bone_sampling_input_invalid", "Private legacy projection must be a dictionary")
    legacy = {} if _legacy_command_params is None else _legacy_command_params
    if any(field not in LEGACY_COMMAND_FIELDS for field in legacy):
        raise OperationError("skeletal_bone_sampling_input_invalid", "Private legacy projection permits bones/sockets only")
    for field in NAME_FIELDS:
        if field in legacy:
            # Keep raw JSON field presence and value kinds. Native alone applies
            # nonarray/null/empty/invalid-row fallback and {name,type} parsing.
            values[field] = legacy[field]
        elif values[field] is not None and (type(values[field]) is not list
                or any(type(item) is not str for item in values[field])):
            raise OperationError("skeletal_bone_sampling_input_invalid", field + " must be None or a list of strings")
        api._carrier(values[field])
    try:
        return copy.deepcopy(values), copy.deepcopy(legacy)
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("skeletal_bone_sampling_input_invalid", "Sampling arguments could not be copied before dispatch") from error


def _context(result, arguments, legacy):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        legacy_command_params=copy.deepcopy(legacy), legacy_command_keys=list(legacy),
        original_eight_public_parameter_order_preserved=True,
        formal_named_bones_sockets_nullable_list_string=True,
        private_legacy_raw_bones_sockets_only=True,
        none_empty_nonarray_and_descriptor_fallback_native_authoritative=True,
        native_world_priority_first_actor_component_and_transform_snapshot_authoritative=True,
        adapter_Editor_idle_or_noPIE_gate=False, PIE_SIE_runtime_reads_allowed=True,
        adapter_asset_load_calls=0, adapter_world_or_component_queries=0,
        adapter_property_writes=0, adapter_explicit_modify_calls=0,
        adapter_start_play_calls=0, adapter_stop_play_calls=0, adapter_tick_calls=0,
        adapter_pose_update_or_refresh_calls=0, adapter_explicit_compile_calls=0,
        adapter_explicit_save_calls=0, missing_bone_socket_warning_promoted_to_operation_error=False,
        read_only_is_total_SDK_purity_proof=False, snapshot_is_frame_atomic=False,
        actual_named_nullable_wire_verified=False, pose_snapshot_is_observed_not_execution_proof=True,
        playback_verified=False, unique_external_pipeline_complete=False,
        automatic_action_retry=False, atomic_rollback=False)


def _adopt(result, arguments, legacy, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments, legacy)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_skeletal_bone_sampling_compile_receipt",
        completion_scope="Native_explicit_receipt; opaque_getter_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_skeletal_bone_sampling_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_filters": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_skeletal_bone_sampling_completion",
        native_postcondition_passed=native["postcondition_passed"],
        full_native_metadata_transforms_warnings_and_partial_rows_preserved=True,
        missing_bone_socket_diagnostics_preserved=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        frame_atomic_snapshot=False, playback="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_world_actor_component_getter_effects_inspect_partial_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_skeletal_bone_sampling_operation"


def _run_action(result, arguments, legacy, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments, legacy)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_skeletal_bone_sampling_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_filters": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_skeletal_bone_sampling_operation"
    try:
        native = api.sample_skeletal_bones_in_sie(*(arguments[field] for field in PUBLIC_FIELDS))
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "skeletal_bone_sampling_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_skeletal_bone_sampling_operation"
        callback(result)
        raise
    _adopt(result, arguments, legacy, native)
    callback(result)  # Keep full partial/native/unknown prefix before error criteria.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "skeletal_bone_sampling_native_failed",
            native.get("error") or "Native skeletal sampling failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("skeletal_bone_sampling_result_unverified", "Native sampling completion is unverified; no replay")
    result.update(success=True, stage="complete")
    return result


def _adopt_rejection(result, error):
    state.fail(result, error)
    result["stage"] = "skeletal_bone_sampling_input_validation"
    result["changes"] = []
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
        mutation_state_unknown=False, journal_created=False)
    return result


def _rejected(error):
    return _adopt_rejection(state.new_result(), error)


def sample_skeletal_bones_in_sie(actor_label: str = "", actor_name: str = "", actor_path: str = "",
        component_name: str = "", bones: Optional[List[str]] = None, sockets: Optional[List[str]] = None,
        prefer_pie_world: bool = True, require_pie_world: bool = False) -> Dict[str, Any]:
    try:
        arguments, legacy = _arguments(actor_label, actor_name, actor_path, component_name,
            bones, sockets, prefer_pie_world, require_pie_world)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments, legacy))


def sample_skeletal_bones_in_sie_action(result, actor_label: str = "", actor_name: str = "",
        actor_path: str = "", component_name: str = "", bones: Optional[List[str]] = None,
        sockets: Optional[List[str]] = None, prefer_pie_world: bool = True, require_pie_world: bool = False,
        checkpoint_callback=None, *, _legacy_command_params=None):
    try:
        arguments, legacy = _arguments(actor_label, actor_name, actor_path, component_name,
            bones, sockets, prefer_pie_world, require_pie_world, _legacy_command_params=_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result, error)
        result["data"]["parent_journal_only"] = True
        (checkpoint_callback or state.checkpoint)(result)
        raise
    return _run_action(result, arguments, legacy, checkpoint_callback)
