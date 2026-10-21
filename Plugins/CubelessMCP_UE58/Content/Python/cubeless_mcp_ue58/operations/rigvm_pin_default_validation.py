"""Proposed pin defaults7 in one Named journal or the existing Batch parent.

Native owns array/path alias precedence, proposal presence and serialization,
pin resolution, default-value checks and metadata clamps, bounded projections,
identity and dirty/callback observations. Python performs no Engine graph work.
"""
import copy
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import rigvm_pin_default_validation as api
from . import state
from .state import OperationError

METHOD = "validate_controlrig_rigvm_pin_defaults"
PUBLIC_FIELDS = ("control_rig_path", "control_rig_asset_path", "control_rig_class_path",
    "pin_defaults", "defaults", "pins", "max_pins")
PATH_FIELDS = PUBLIC_FIELDS[:3]
ARRAY_FIELDS = PUBLIC_FIELDS[3:6]


def _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
        pin_defaults, defaults, pins, max_pins):
    values = dict(control_rig_path=control_rig_path, control_rig_asset_path=control_rig_asset_path,
        control_rig_class_path=control_rig_class_path, pin_defaults=pin_defaults,
        defaults=defaults, pins=pins, max_pins=max_pins)
    for field in PATH_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("rigvm_pin_default_validation_input_invalid", field + " must be a string; None is not an empty path")
    for field in ARRAY_FIELDS:
        if values[field] is not None and type(values[field]) is not list:
            raise OperationError("rigvm_pin_default_validation_input_invalid", field + " must be None or a JSON array; [] is not omission")
    if type(max_pins) is not int or not -(1 << 63) <= max_pins < (1 << 63):
        raise OperationError("rigvm_pin_default_validation_input_invalid", "max_pins must be an int64 integer; booleans are not limits")
    values = copy.deepcopy(values)
    # Keep row/proposal dictionaries opaque. Missing and explicit null proposal
    # values, primary [] and alias None, limit0 and ignored tail are Native rules.
    for field in ARRAY_FIELDS:
        api._carrier(values[field])
    return values


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_seven_public_parameters_forwarded_unchanged=True,
        nullable_pin_specs_preserve_none_and_empty=True,
        native_spec_aliases_and_proposal_presence_authoritative=True,
        native_clamp_pin_default_type_metadata_and_dirty_observation_authoritative=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        adapter_SetPinDefaultValue_calls=0, adapter_array_resize_calls=0,
        adapter_runtime_execute_calls=0, adapter_runtime_solve_calls=0,
        invalid_proposed_default_is_operation_failure=False, missing_pin_is_operation_failure=False,
        diagnostic_completed_is_default_write_approval=False,
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
        compiled=native["compiled"], status="native_rigvm_pin_default_validation_compile_receipt",
        completion_scope="Native_explicit_receipt; SDK_load_getter_type_resolution_and_implicit_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_rigvm_pin_default_validation_save_receipt")
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
    result["validation"].update(status="native_RigVM_pin_default_diagnostic_completion",
        native_postcondition_passed=native["postcondition_passed"],
        requested_pin_count=native["data"].get("requested_pin_count"),
        validated_pin_count=native["data"].get("validated_pin_count"),
        included_pin_count=native["data"].get("included_pin_count"),
        pins_truncated=native["data"].get("pins_truncated"),
        pin_found_count=native["data"].get("pin_found_count"),
        missing_pin_count=native["data"].get("missing_pin_count"),
        can_provide_default_count=native["data"].get("can_provide_default_count"),
        cannot_provide_default_count=native["data"].get("cannot_provide_default_count"),
        proposed_default_provided_count=native["data"].get("proposed_default_provided_count"),
        proposed_default_valid_count=native["data"].get("proposed_default_valid_count"),
        proposed_default_invalid_count=native["data"].get("proposed_default_invalid_count"),
        default_write_candidate_for_review_count=native["data"].get("default_write_candidate_for_review_count"),
        normal_diagnostic_rejections_preserved=True, total_change_state_known=known,
        all_implicit_SDK_effects_observed=False,
        runtime_pose="not_run", playback="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_loader_getter_type_resolution_or_dirty_effects_inspect_native_prefix_and_parent_journal_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_rigvm_pin_default_validation_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_rigvm_pin_default_validation_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_paths": {field: arguments[field] for field in PATH_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_rigvm_pin_default_validation_operation"
    try:
        native = api.validate_controlrig_rigvm_pin_defaults(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "rigvm_pin_default_validation_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_rigvm_pin_default_validation_operation"
        callback(result) # Preserve malformed return/uncertain dispatch before reporting the error.
        raise
    _adopt(result, arguments, native)
    callback(result) # Full Native proposal/partial/raw/dirty/unknown prefix precedes errors.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "rigvm_pin_default_validation_native_failed",
            native.get("error") or "Native pin default validation failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("rigvm_pin_default_validation_result_unverified", "Native diagnostic completion is unverified; no replay")
    # Invalid proposals, missing pins, inability to provide a default or limit0
    # truncation are normal completed diagnostics, not approval to write values.
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["stage"] = "rigvm_pin_default_validation_input_validation"
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False, journal_created=False)
    return result


def validate_controlrig_rigvm_pin_defaults(control_rig_path: str = "", control_rig_asset_path: str = "", control_rig_class_path: str = "",
        pin_defaults: Optional[List[Any]] = None, defaults: Optional[List[Any]] = None,
        pins: Optional[List[Any]] = None, max_pins: int = 64) -> Dict[str, Any]:
    try:
        arguments = _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
            pin_defaults, defaults, pins, max_pins)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def validate_controlrig_rigvm_pin_defaults_action(result, control_rig_path: str = "", control_rig_asset_path: str = "", control_rig_class_path: str = "",
        pin_defaults: Optional[List[Any]] = None, defaults: Optional[List[Any]] = None,
        pins: Optional[List[Any]] = None, max_pins: int = 64, checkpoint_callback=None):
    return _run_action(result, _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
        pin_defaults, defaults, pins, max_pins), checkpoint_callback)
