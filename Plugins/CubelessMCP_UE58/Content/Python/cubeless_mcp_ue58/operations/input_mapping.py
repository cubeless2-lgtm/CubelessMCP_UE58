"""Legacy input mapping append/save/readback with one named or Batch parent journal.
Native owns all InputSettings work; accepted reissue may append an exact duplicate.
"""
import copy
from typing import Any, Dict
from cubeless_mcp_ue58.compatibility.ue58 import input_mapping as api
from . import state
from .state import OperationError

METHOD = "create_input_mapping"
PUBLIC_FIELDS = ("action_name", "key", "input_type")
INTERNAL_MODIFIERS = ("shift", "ctrl", "alt", "cmd")


def _arguments(action_name, key, input_type, shift=False, ctrl=False, alt=False, cmd=False):
    values = dict(action_name=action_name, key=key, input_type=input_type)
    if any(type(value) is not str for value in values.values()):
        raise OperationError("input_mapping_input_invalid", "Original action_name/key/input_type must be strings")
    modifiers = dict(shift=shift, ctrl=ctrl, alt=alt, cmd=cmd)
    if any(type(value) is not bool for value in modifiers.values()):
        raise OperationError("input_mapping_input_invalid", "Internal shift/ctrl/alt/cmd must be literal booleans")
    values.update(modifiers)
    return values


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_three_public_parameters_forwarded_unchanged=True,
        original_hidden_modifier_inputs_retained=True,
        Native_append_config_save_readback_and_partial_prefix_authoritative=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, atomic_rollback=False,
        cold_editor_reload_verified=False, packaged_runtime_verified=False,
        unique_external_pipeline_complete=False)


def _adopt(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items()
            if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_input_mapping_compile_receipt",
        implicit_SDK_compile_outcome="separate_native_data_not_inferred_from_explicit_flags")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_input_mapping_config_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD, "requested_action_name": arguments["action_name"],
        "requested_key": arguments["key"], "requested_input_type": arguments["input_type"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None, "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_input_mapping_postcondition",
        native_postcondition_passed=native["postcondition_passed"], total_change_state_known=known,
        disk_reload="not_run", packaged_runtime="not_run", unique_external_pipeline="not_run",
        RAM_config_target_file_and_independent_readback_scope="actual_native_data_prefix")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_native_outcome_inspect_actual_RAM_config_file_and_journal_no_replay" if not known
            else "accepted_reissue_appends_duplicate_mapping_inspect_RAM_disk_and_journal_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_input_mapping_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
        pending_operation=copy.deepcopy(arguments))
    # No new Python compile/save option exists. Native determines requested work
    # after original admission; before dispatch its append/save outcome is unknown.
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_input_mapping_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_action_name": arguments["action_name"],
        "requested_key": arguments["key"], "attempted": None,
        "changed": None, "change_state_known": False}]
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=True, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_input_mapping_operation"
    try:
        native = api.create_input_mapping(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_input_mapping_operation"
            callback(result)
        if getattr(error, "code", "") == "input_mapping_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        raise
    _adopt(result, arguments, native)
    callback(result)  # Complete raw/RAM/file/save prefix precedes failure.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "input_mapping_native_failed",
            native.get("error") or "Native Input mapping failed; actual RAM/cache/disk append/save prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("input_mapping_result_unverified", "Native Input mapping lacks current mapping-array persistence proof; no replay")
    # SDK acquisition and malformed-return uncertainty stay explicit even when
    # the mapping postcondition succeeds. Batch stops following work on unknown.
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    return result


def create_input_mapping(action_name: str, key: str, input_type: str = "Action") -> Dict[str, Any]:
    try:
        arguments = _arguments(action_name, key, input_type)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def create_input_mapping_action(result, action_name: str, key: str, input_type: str = "Action",
        shift: bool = False, ctrl: bool = False, alt: bool = False, cmd: bool = False,
        checkpoint_callback=None):
    return _run_action(result, _arguments(action_name, key, input_type, shift, ctrl, alt, cmd), checkpoint_callback)
