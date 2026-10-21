"""Original subsequence5 with one journal and native exact-save authority.

The original response had no mutation_result_v1 contract. Stronger finite
parent/child postconditions remain distinct from that legacy response.
"""
import copy

from cubeless_mcp_ue58.compatibility.ue58 import sequencer_subsequence as api
from . import sequencer as shared, state
from .state import OperationError


def _arguments(sequence_path, subsequence_path, start_frame, duration_frames, save):
    return {"sequence_path": shared._path(sequence_path), "subsequence_path": shared._path(subsequence_path),
        "start_frame": shared._number(start_frame, "start_frame"),
        "duration_frames": shared._number(duration_frames, "duration_frames"), "save": shared._bool(save, "save")}


def _context(result, arguments):
    result["data"].update(operation=api.BINDINGS[0], requested_arguments=copy.deepcopy(arguments),
        public_input_representation="original five behavior fields; two canonical package paths",
        original_save_requested=arguments.get("save", True), automatic_action_retry=False,
        Python_explicit_asset_load_calls=0, Python_explicit_compile_calls=0, Python_explicit_save_calls=0,
        native_save_authority="actual native save flags, save_attempt_count and ordered save_calls",
        legacy_contract_scope="Original Subsequence had no mutation_result_v1; native typed postcondition is separately scoped",
        library_cdo_before_native_entry_independently_observed=False, all_implicit_Engine_effects_measured=False,
        playback_execution_verified=False, Actor_property_evaluation_verified=False, renderer_verified=False,
        cold_persistence_verified=False, full_legacy_sequencer_parity=False)


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native), native_raw_result=native.get("_raw_native_json"),
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native.get("changed") if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"])
    save_status = "native_returned_saved" if native["saved"] else (
        "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status=save_status)
    no_writer = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_writer else [{"kind": api.BINDINGS[0], "asset_path": arguments["sequence_path"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native.get("changed") if known else None, "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="returned_native_sequencer_subsequence",
        native_postcondition_passed=native["postcondition_passed"],
        scope="Finite actual attached section/child/range/row/timecode and parent-child preservation; raw complete/partial phases retained",
        disk_reload="not_run", playback="not_run", whole_original_pipeline="not_run")
    result["stage"] = "returned_sequencer_subsequence_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    callback = checkpoint_callback or state.checkpoint
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    result["save"]["requested"] = arguments["save"]
    result["stage"] = "before_mutation_checkpoint"
    result["data"]["pending_mutation"] = {"operation": api.BINDINGS[0], **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_assets_and_journal"}
    callback(result)
    result["stage"] = "native_sequencer_subsequence"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": api.BINDINGS[0], "asset_path": arguments["sequence_path"],
        "attempted": None, "change_state_known": False, "changed": None}]
    try:
        native = api.add_level_sequence_subsequence(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["save"]["status"] = "native_returned_unparsed"
            result["stage"] = "returned_sequencer_subsequence_operation"
            callback(result)
        if getattr(error, "code", "") == "sequencer_subsequence_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "sequencer_subsequence_native_failed", native.get("error") or "Native subsequence failed; actual parent/child/save prefix retained, no retry")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("sequencer_subsequence_unverified", "Native success lacks known finite parent/child postcondition; no retry or second save")
    if native["compile_requested"] or native["compile_attempted"] or native["compiled"] is not None:
        raise OperationError("sequencer_subsequence_unexpected_compile", "Unexpected native compile remains authoritative in returned raw prefix")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "subsequence_would_add_another_section_inspect_journal" if native["mutation_attempted"] else "none_observed"


def add_level_sequence_subsequence(sequence_path, subsequence_path, start_frame, duration_frames, save=True):
    try:
        arguments = _arguments(sequence_path, subsequence_path, start_frame, duration_frames, save)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        _context(result, {"save": save} if type(save) is bool else {})
        result["data"].update(admission_rejected=True, native_dispatched=False, native_result_returned=False,
            native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
            save_request_admitted=False)
        return result
    return state.mutate(api.BINDINGS[0], arguments, None, lambda result: _run_action(result, arguments))


def add_level_sequence_subsequence_action(result, sequence_path, subsequence_path, start_frame, duration_frames,
        save=True, checkpoint_callback=None):
    _run_action(result, _arguments(sequence_path, subsequence_path, start_frame, duration_frames, save), checkpoint_callback)
