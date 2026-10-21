"""One original MPC controller API: named status read, journaled start/stop.

Native owns the initial runtime copy, optional one-time asset save, persistent
ticker and stop state. Returned controller success is distinct from persistence
and all future ticks. No Python poll, restart, second save or nested journal.
"""
import copy
import math

from cubeless_mcp_ue58.compatibility.ue58 import material_collection_sync as api
from . import state
from .state import OperationError


def _arguments(action, source_collection_path, target_collection_path, parameter_names,
        interval_seconds, set_asset_defaults_on_enable, save_on_enable):
    if (any(type(v) is not str for v in (action, source_collection_path, target_collection_path))
            or type(set_asset_defaults_on_enable) is not bool or type(save_on_enable) is not bool):
        raise OperationError("material_collection_sync_input_invalid", "Original action/paths require strings and enable flags require booleans")
    if parameter_names is not None and (type(parameter_names) is not list or any(type(v) is not str for v in parameter_names)):
        raise OperationError("material_collection_sync_input_invalid", "parameter_names must be a string array or null")
    if type(interval_seconds) not in (int, float) or not math.isfinite(interval_seconds):
        raise OperationError("material_collection_sync_input_invalid", "interval_seconds must be a finite JSON number")
    # Native owns lowercase-only action aliases, FName trim/filter/dedup, float32
    # interval clamp and finite source/target/world admission. No dependency guard is invented.
    return {"action": action, "source_collection_path": source_collection_path, "target_collection_path": target_collection_path,
        "parameter_names": copy.deepcopy(parameter_names), "interval_seconds": interval_seconds,
        "set_asset_defaults_on_enable": set_asset_defaults_on_enable, "save_on_enable": save_on_enable}


def _context(result, arguments):
    result["data"].update(operation=api.BINDINGS[0], requested_arguments=copy.deepcopy(arguments),
        public_input_representation="original action='status', source/target='', parameter_names=None, interval_seconds=0.1, False/False",
        public_parameter_names_default_is_null=True,
        reflected_empty_array_omitted_null_empty_scope="Direct array bridge; omission/null/[] mean the original all-name filter. Named frame cannot distinguish those origins; external recorded arguments remain authoritative.",
        Python_explicit_asset_load_calls=0, Python_explicit_runtime_setter_calls=0,
        Python_explicit_compile_calls=0, Python_explicit_save_calls=0, Python_ticker_or_restart_calls=0,
        controller_acknowledgement_is_all_future_tick_completion=False,
        controller_stop_restores_runtime_values=False, saved_false_negates_original_controller_success=False,
        native_phase_authority="full actual controller/cache/initial-copy/tick/save/stop receipts and counters",
        automatic_action_retry=False, complete_legacy_PIE_world_follow_reload_or_renderer_parity_verified=False,
        full_original_sync_contract_verified=False)


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_raw_result=native.get("_raw_native_json"),
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native.get("changed") if known else None,
        mutation_observation_scope="returned controller/initial-copy/selected values/save point; ongoing future tick/renderer/world effects are separate")
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status="native_returned_actual_compile_flags")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else (
            "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted"))
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": api.BINDINGS[0], "controller_action": arguments["action"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native.get("changed") if known else None, "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="returned_native_material_collection_sync",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        future_ticks_and_render_completion="not_inferred_from_initial_controller_receipt",
        stop_value_restoration="not_performed", whole_world_restoration="not_observed")
    result["stage"] = "returned_material_collection_sync_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    callback = checkpoint_callback or state.checkpoint
    action = arguments["action"].lower()  # Routing only; the raw action still reaches Native.
    save_intent = action in ("enable", "start") and arguments["set_asset_defaults_on_enable"] and arguments["save_on_enable"]
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested")
    result["save"].update(requested=save_intent, attempted=False, saved=False, status="intent_prepared_not_dispatched")
    result["data"]["pending_controller_call"] = {"operation": api.BINDINGS[0], **copy.deepcopy(arguments),
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "inspect_actual_controller_and_returned_prefix_do_not_restart_or_retry"}
    result["stage"] = "before_controller_checkpoint"
    callback(result)
    result["stage"] = "native_material_collection_sync"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": api.BINDINGS[0], "controller_action": arguments["action"],
        "attempted": None, "change_state_known": False, "changed": None}]
    try:
        native = api.set_material_parameter_collection_sync(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["compile"]["status"] = "native_returned_unparsed"
            result["save"]["status"] = "native_returned_unparsed"
            result["stage"] = "returned_material_collection_sync_operation"
            callback(result)
        if getattr(error, "code", "") == "material_collection_sync_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_material_collection_sync_failed",
            native.get("error") or "Native controller call failed; full actual state/copy/save prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("material_collection_sync_postcondition_unverified", "Native controller result lacks a known current postcondition; no retry")
    # Original enabled-success and returned savedFalse are separate. Never auto-save/restart.
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "controller_start_stop_or_initial_copy_would_repeat" if action != "status" else "none_observed_for_status_read"


def set_material_parameter_collection_sync(action="status", source_collection_path="", target_collection_path="",
        parameter_names=None, interval_seconds=0.1, set_asset_defaults_on_enable=False, save_on_enable=False):
    try:
        arguments = _arguments(action, source_collection_path, target_collection_path, parameter_names,
            interval_seconds, set_asset_defaults_on_enable, save_on_enable)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        _context(result, {"action": action})
        result["data"].update(admission_rejected=True, native_dispatched=False, native_result_returned=False,
            native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
        return result
    if action.lower() == "status":
        result = state.new_result()
        result["data"]["named_status_journal_performed"] = False
        try:
            _run_action(result, arguments, checkpoint_callback=lambda observed: None)
        except Exception as error:
            state.fail(result, error)
        return result
    return state.mutate(api.BINDINGS[0], arguments, None, lambda result: _run_action(result, arguments))


def set_material_parameter_collection_sync_action(result, action="status", source_collection_path="", target_collection_path="",
        parameter_names=None, interval_seconds=0.1, set_asset_defaults_on_enable=False, save_on_enable=False, checkpoint_callback=None):
    _run_action(result, _arguments(action, source_collection_path, target_collection_path, parameter_names,
        interval_seconds, set_asset_defaults_on_enable, save_on_enable), checkpoint_callback)
