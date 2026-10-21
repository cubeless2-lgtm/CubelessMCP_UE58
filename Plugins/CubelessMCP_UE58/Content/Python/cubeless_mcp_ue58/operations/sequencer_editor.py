"""Original Open1 UI operation, preserving returned bool and bounded effects."""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import sequencer_editor as api
from . import state
from .sequencer import _path
from .state import OperationError

def _arguments(sequence_path):
    return {"sequence_path": _path(sequence_path)}

def _context(result):
    result["data"].update(UI_operation=True, public_behavior_input_count=1,
        native_library=api.LIBRARY, operation=api.BINDINGS[0],
        explicit_asset_load_compile_save_calls=0,
        effect_scope="UI dispatch may focus/open toolkit, invoke notifications/delegates/module initialization, update MRU/cache and flush Editor config; opening is not a pure read",
        mutation_observation_scope="Typed bounded current target/config/dirty/disk/editor-presence observations; not an exhaustive asset mutation or UI focus/visibility proof",
        legacy_success_authority="Actual OpenEditorForAsset bool; separate FindEditorForAsset(false) observation does not replace it",
        legacy_mutation_contract_available=False, full_legacy_sequencer_parity=False,
        cold_persistence_verified=False, playback_or_render_verified=False)

def _apply_native(result, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native), native_raw_result=native.get("_raw_native_json"),
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native.get("changed") if known else None)
    _context(result)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"])
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else ("native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted"))
    result["changes"] = [] if known and not native["mutation_attempted"] else [{"kind": api.BINDINGS[0],
        "effect": "Editor_UI", "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native.get("changed") if known else None, "asset_mutation_proven": False}]
    result["validation"].update(status="returned_native_sequencer_editor", native_postcondition_passed=native["postcondition_passed"],
        legacy_opened_bool=native["data"].get("opened"), observation_scope="Bounded native target/config/dirty/editor-presence; UI toolkit callbacks and deep asset data not exhaustively measured",
        UI_focus_visibility="not_run", playback="not_run", disk_reload="not_run", whole_original_pipeline="not_run")
    result["stage"] = "returned_sequencer_editor_operation"

def _run_action(result, arguments, checkpoint_callback=None):
    _context(result)
    callback = checkpoint_callback or state.checkpoint
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    result["stage"] = "before_mutation_checkpoint"
    result["data"]["pending_mutation"] = {"operation": api.BINDINGS[0], **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_toolkit_and_asset_config_journal"}
    callback(result)
    result["stage"] = "native_sequencer_editor"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": api.BINDINGS[0], "effect": "Editor_UI", "attempted": None, "change_state_known": False, "changed": None}]
    try:
        native = api.open_level_sequence_in_editor(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["save"]["status"] = "native_returned_unparsed"
            result["stage"] = "returned_sequencer_editor_operation"
            callback(result)
        if getattr(error, "code", "") == "sequencer_editor_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result["changes"] = []
        raise
    _apply_native(result, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "sequencer_editor_native_failed", native.get("error") or "Native UI call failed; retained prefix, no retry")
    if not native["change_state_known"] or not native["postcondition_passed"] or native["data"].get("opened") is not True:
        raise OperationError("sequencer_editor_unverified", "Actual bool/prefix observations incomplete; inspect UI/config before another dispatch")
    if native["compile_requested"] or native["compile_attempted"] or native["compiled"] is not None or native["save_requested"] or native["save_attempted"] or native["saved"]:
        raise OperationError("sequencer_editor_unexpected_asset_action", "Unexpected explicit asset compile/save flags remain authoritative in raw receipt")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "UI_focus_notifications_callbacks_can_repeat_inspect_journal" if native["mutation_attempted"] else "none_observed"

def open_level_sequence_in_editor(sequence_path):
    try:
        arguments = _arguments(sequence_path)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        _context(result)
        result["data"].update(admission_rejected=True, native_dispatched=False, native_result_returned=False,
            native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
        return result
    return state.mutate(api.BINDINGS[0], arguments, None, lambda result: _run_action(result, arguments))

def open_level_sequence_in_editor_action(result, sequence_path, checkpoint_callback=None):
    _run_action(result, _arguments(sequence_path), checkpoint_callback)
