"""Original Range4/Remove4, one journal and exact returned native prefixes.

Native owns save and original terminal decimal/removal verification. Native
floor or generic success is never relabeled as the strict original contract.
"""
import copy

from cubeless_mcp_ue58.compatibility.ue58 import sequencer_maintenance as api
from . import sequencer as shared, state
from .state import OperationError


def _selector(value, field, maximum):
    # Original GUID parser/name comparison sees the literal caller spelling.
    # Do not strip labels or normalize GUID formats through a Python parser.
    if type(value) is not str or "\x00" in value or len(value) > maximum:
        raise OperationError("sequencer_maintenance_selector_invalid", field + " requires bounded original non-null text")
    return value


def _range_arguments(sequence_path, playback_start_frame, playback_end_frame, save):
    return {"sequence_path": shared._path(sequence_path),
        "playback_start_frame": shared._number(playback_start_frame, "playback_start_frame"),
        "playback_end_frame": shared._number(playback_end_frame, "playback_end_frame"), "save": shared._bool(save, "save")}


def _remove_arguments(sequence_path, binding_id, actor_label, save):
    return {"sequence_path": shared._path(sequence_path), "binding_id": _selector(binding_id, "binding_id", 128),
        "actor_label": _selector(actor_label, "actor_label", 512), "save": shared._bool(save, "save")}


def _context(result, identity, arguments):
    result["data"].update(operation=identity, requested_arguments=copy.deepcopy(arguments),
        public_input_representation="original four behavior fields; non-null literal selector text",
        original_save_requested=arguments.get("save", True), automatic_action_retry=False,
        Python_explicit_asset_load_calls=0, Python_explicit_compile_calls=0, Python_explicit_save_calls=0,
        native_save_authority="actual native save flags, save_attempt_count and ordered save_calls",
        legacy_verification_authority="native source-computed original decimal/removal/save predicates; not generic floor success",
        library_cdo_before_native_entry_independently_observed=False, all_implicit_Engine_effects_measured=False,
        Actor_cleanup_or_property_evaluation_verified=False, playback_execution_verified=False,
        renderer_verified=False, cold_persistence_verified=False, full_legacy_sequencer_parity=False)


def _apply_native(result, identity, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native), native_raw_result=native.get("_raw_native_json"),
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native.get("changed") if known else None,
        mutation_observation_scope="returned typed native state; original performed/semantic changed retained in legacy_mutation_contract")
    _context(result, identity, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"])
    save_status = "native_returned_saved" if native["saved"] else (
        "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status=save_status)
    no_writer = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_writer else [{"kind": identity, "asset_path": arguments["sequence_path"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native.get("changed") if known else None, "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="returned_native_sequencer_maintenance",
        native_postcondition_passed=native["postcondition_passed"],
        original_verification_scope="native exact decimal/removal checks and save receipts; raw complete/partial phases retained",
        disk_reload="not_run", Actor_property_evaluation="not_run", playback="not_run")
    result["stage"] = "returned_sequencer_maintenance_operation"


def _run_action(result, identity, arguments, checkpoint_callback=None):
    _context(result, identity, arguments)
    callback = checkpoint_callback or state.checkpoint
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    result["save"]["requested"] = arguments["save"]
    result["stage"] = "before_mutation_checkpoint"
    result["data"]["pending_mutation"] = {"operation": identity, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_asset_and_journal"}
    callback(result)
    result["stage"] = "native_sequencer_maintenance"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity, "asset_path": arguments["sequence_path"],
        "attempted": None, "change_state_known": False, "changed": None}]
    try:
        native = getattr(api, identity)(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["save"]["status"] = "native_returned_unparsed"
            result["stage"] = "returned_sequencer_maintenance_operation"
            callback(result)
        if getattr(error, "code", "") == "sequencer_maintenance_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result["changes"] = []
        raise
    _apply_native(result, identity, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "sequencer_maintenance_native_failed", native.get("error") or "Native maintenance failed; complete/partial write/save prefix retained, no retry")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("sequencer_maintenance_unverified", "Native success lacks known complete maintenance postcondition; no retry or second save")
    if native["compile_requested"] or native["compile_attempted"] or native["compiled"] is not None:
        raise OperationError("sequencer_maintenance_unexpected_compile", "Unexpected native compile remains authoritative in returned raw prefix")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "sequence_maintenance_would_repeat_inspect_journal" if native["mutation_attempted"] else "none_observed"


def _named(identity, normalize, *arguments):
    try:
        values = normalize(*arguments)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        caller_save = {"save": arguments[-1]} if arguments and type(arguments[-1]) is bool else {}
        _context(result, identity, caller_save)
        result["data"].update(admission_rejected=True, native_dispatched=False, native_result_returned=False,
            native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
            save_request_admitted=False)
        return result
    return state.mutate(identity, values, None, lambda result: _run_action(result, identity, values))


def set_level_sequence_playback_range(sequence_path, playback_start_frame, playback_end_frame, save=True):
    return _named(api.BINDINGS[0], _range_arguments, sequence_path, playback_start_frame, playback_end_frame, save)


def remove_level_sequence_binding(sequence_path, binding_id="", actor_label="", save=True):
    return _named(api.BINDINGS[1], _remove_arguments, sequence_path, binding_id, actor_label, save)


def set_level_sequence_playback_range_action(result, sequence_path, playback_start_frame, playback_end_frame, save=True, checkpoint_callback=None):
    _run_action(result, api.BINDINGS[0], _range_arguments(sequence_path, playback_start_frame, playback_end_frame, save), checkpoint_callback)


def remove_level_sequence_binding_action(result, sequence_path, binding_id="", actor_label="", save=True, checkpoint_callback=None):
    _run_action(result, api.BINDINGS[1], _remove_arguments(sequence_path, binding_id, actor_label, save), checkpoint_callback)
