"""Proposed RigVM links7 in one Named journal or the existing Batch parent.

Native owns nullable alias precedence, row parsing, limits, path/model/pin
resolution, CanLink diagnostics, identity checks and dirty/cache observations.
Python does not load a rig, query pins, add links, compile, save or solve.
"""
import copy
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import rigvm_link_validation as api
from . import state
from .state import OperationError

METHOD = "validate_controlrig_rigvm_links"
PUBLIC_FIELDS = ("control_rig_path", "control_rig_asset_path", "control_rig_class_path",
    "links", "unit_links", "allow_type_casting", "max_links")
PATH_FIELDS = PUBLIC_FIELDS[:3]


def _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
        links, unit_links, allow_type_casting, max_links):
    values = dict(control_rig_path=control_rig_path, control_rig_asset_path=control_rig_asset_path,
        control_rig_class_path=control_rig_class_path, links=links, unit_links=unit_links,
        allow_type_casting=allow_type_casting, max_links=max_links)
    for field in PATH_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("rigvm_link_validation_input_invalid", field + " must be a string; None is not an empty path")
    for field in ("links", "unit_links"):
        if values[field] is not None and type(values[field]) is not list:
            raise OperationError("rigvm_link_validation_input_invalid", field + " must be None or a JSON array; [] is not omission")
    if type(allow_type_casting) is not bool:
        raise OperationError("rigvm_link_validation_input_invalid", "allow_type_casting must be a boolean")
    if type(max_links) is not int or not -(1 << 63) <= max_links < (1 << 63):
        raise OperationError("rigvm_link_validation_input_invalid", "max_links must be an int64 integer; booleans are not limits")
    values = copy.deepcopy(values)
    # Full JSON admission occurs before the journal. Native alone interprets
    # bounded rows, aliases, None versus [], whitespace and limit0/truncated tail.
    api._carrier(values["links"])
    api._carrier(values["unit_links"])
    return values


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_seven_public_parameters_forwarded_unchanged=True,
        nullable_links_preserve_none_and_empty=True, native_spec_order_and_alias_precedence_authoritative=True,
        native_clamp_model_pins_CanLink_and_dirty_observation_authoritative=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0, adapter_AddLink_calls=0,
        adapter_runtime_execute_calls=0, adapter_runtime_solve_calls=0,
        can_link_false_is_operation_failure=False, missing_pin_is_operation_failure=False,
        diagnostic_completed_is_all_requested_links_writable=False,
        legacy_read_only_is_total_SDK_purity_proof=False,
        dirty_unchanged_proves_all_SDK_effects_absent=False,
        explicit_compile_flags_prove_all_implicit_compiles=False,
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
        compiled=native["compiled"], status="native_rigvm_link_validation_compile_receipt",
        completion_scope="Native_explicit_receipt; SDK_load_virtual_getters_and_diagnostic_AST_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_rigvm_link_validation_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_paths": {field: arguments[field] for field in PATH_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "asset_dirty_before": native["data"].get("asset_dirty_before"),
        "asset_dirty_after": native["data"].get("asset_dirty_after"),
        "observed_dirty_changed": native["data"].get("observed_dirty_changed"),
        "legacy_source_modified": native["data"].get("source_modified"),
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_RigVM_link_diagnostic_completion",
        native_postcondition_passed=native["postcondition_passed"],
        requested_link_count=native["data"].get("requested_link_count"),
        validated_link_count=native["data"].get("validated_link_count"),
        included_link_count=native["data"].get("included_link_count"),
        links_truncated=native["data"].get("links_truncated"),
        can_link_count=native["data"].get("can_link_count"),
        cannot_link_count=native["data"].get("cannot_link_count"),
        missing_pin_count=native["data"].get("missing_pin_count"),
        already_linked_count=native["data"].get("already_linked_count"),
        normal_diagnostic_rejections_preserved=True, total_change_state_known=known,
        all_implicit_SDK_effects_observed=False,
        runtime_pose="not_run", playback="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_loader_getter_or_diagnostic_AST_effects_inspect_native_prefix_and_parent_journal_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_rigvm_link_validation_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_rigvm_link_validation_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_paths": {field: arguments[field] for field in PATH_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_rigvm_link_validation_operation"
    try:
        native = api.validate_controlrig_rigvm_links(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "rigvm_link_validation_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_rigvm_link_validation_operation"
        callback(result) # Preserve malformed return/uncertain dispatch before reporting the error.
        raise
    _adopt(result, arguments, native)
    callback(result) # Full Native diagnostic/partial/raw/dirty/unknown prefix precedes errors.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "rigvm_link_validation_native_failed",
            native.get("error") or "Native RigVM link validation failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("rigvm_link_validation_result_unverified", "Native diagnostic completion is unverified; no replay")
    # can_link=False, missing pins, existing links or a truncated limit0 result
    # are normal diagnostics, not operation failure. Unknown still stops Batch.
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["stage"] = "rigvm_link_validation_input_validation"
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False, journal_created=False)
    return result


def validate_controlrig_rigvm_links(control_rig_path: str = "", control_rig_asset_path: str = "", control_rig_class_path: str = "",
        links: Optional[List[Any]] = None, unit_links: Optional[List[Any]] = None,
        allow_type_casting: bool = True, max_links: int = 64) -> Dict[str, Any]:
    try:
        arguments = _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
            links, unit_links, allow_type_casting, max_links)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def validate_controlrig_rigvm_links_action(result, control_rig_path: str = "", control_rig_asset_path: str = "", control_rig_class_path: str = "",
        links: Optional[List[Any]] = None, unit_links: Optional[List[Any]] = None,
        allow_type_casting: bool = True, max_links: int = 64, checkpoint_callback=None):
    return _run_action(result, _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
        links, unit_links, allow_type_casting, max_links), checkpoint_callback)
