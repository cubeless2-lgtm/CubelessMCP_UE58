"""Guarded RigVM pin default writer11 in one Named journal or Batch parent.

Native owns proposal presence/normalization, target restrictions, all-prefix
writability, already-matching reuse, ordered SetPinDefaultValue, resize/linked
pin effects, auto-VM/init, compile/save and partial failures. Python does no
Engine graph work and never turns a writer rejection into a diagnostic success.
"""
import copy
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import rigvm_pin_default_writer as api
from . import state
from .state import OperationError

METHOD = "set_controlrig_rigvm_pin_defaults"
PUBLIC_FIELDS = ("control_rig_path", "control_rig_asset_path", "control_rig_class_path",
    "pin_defaults", "defaults", "resize_arrays", "set_value_on_linked_pins", "dry_run", "compile", "save_asset", "max_pins")
PATH_FIELDS = PUBLIC_FIELDS[:3]
ARRAY_FIELDS = PUBLIC_FIELDS[3:5]
BOOL_FIELDS = PUBLIC_FIELDS[5:10]


def _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
        pin_defaults, defaults, resize_arrays, set_value_on_linked_pins, dry_run, compile, save_asset, max_pins):
    values = dict(control_rig_path=control_rig_path, control_rig_asset_path=control_rig_asset_path,
        control_rig_class_path=control_rig_class_path, pin_defaults=pin_defaults, defaults=defaults,
        resize_arrays=resize_arrays, set_value_on_linked_pins=set_value_on_linked_pins, dry_run=dry_run, compile=compile,
        save_asset=save_asset, max_pins=max_pins)
    for field in PATH_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("rigvm_pin_default_writer_input_invalid", field + " must be a string; None is not an empty path")
    for field in ARRAY_FIELDS:
        if values[field] is not None and type(values[field]) is not list:
            raise OperationError("rigvm_pin_default_writer_input_invalid", field + " must be None or a JSON array; [] is not omission")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("rigvm_pin_default_writer_input_invalid", field + " must be a boolean")
    if type(max_pins) is not int or not -(1 << 63) <= max_pins < (1 << 63):
        raise OperationError("rigvm_pin_default_writer_input_invalid", "max_pins must be an int64 integer; booleans are not limits")
    values = copy.deepcopy(values)
    # Native alone selects aliases, bounded rows, target roots and writer candidates.
    for field in ARRAY_FIELDS:
        api._carrier(values[field])
    return values


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_eleven_public_parameters_forwarded_unchanged=True,
        nullable_pin_specs_preserve_none_and_empty=True,
        native_aliases_clamp_target_guard_and_all_prefix_preflight_authoritative=True,
        native_proposal_presence_already_matching_resize_linked_and_partial_set_authoritative=True,
        native_default_UI_array_linked_and_dirty_observations_authoritative=True,
        permitted_writer_roots="Native_only: /Game/_MCP_Sample/; /Game/_MCP_Temp/; /Game/CubelessMCPTests/",
        general_Game_or_user_asset_write_permission=False,
        legacy_aliases_not_public_parameters=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_SetPinDefaultValue_calls=0, adapter_RequestAutoVMRecompilation_calls=0,
        adapter_RequestControlRigInit_calls=0, adapter_explicit_compile_calls=0,
        adapter_explicit_save_calls=0, adapter_runtime_execute_calls=0, adapter_runtime_solve_calls=0,
        diagnostic_validity_alone_proves_writer_completion=False,
        already_matching_noop_can_complete_without_setter=True,
        dry_run_proves_all_SDK_effects_absent=False,
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
        compiled=native["compiled"], status="native_rigvm_pin_default_writer_compile_receipt",
        completion_scope="Native_explicit_receipt; all_implicit_loader_controller_autoVM_init_and_callback_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_rigvm_pin_default_writer_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_paths": {field: arguments[field] for field in PATH_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "target_dirty_before": native["data"].get("target_dirty_before"),
        "target_dirty_after": native["data"].get("target_dirty_after"),
        "observed_dirty_changed": native["data"].get("observed_dirty_changed"),
        "observed_explicit_change": native["data"].get("observed_explicit_change"),
        "defaults_set_count": native["data"].get("defaults_set_count"),
        "partial_target_modified": native["data"].get("partial_target_modified"),
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_guarded_RigVM_pin_default_writer_completion",
        native_postcondition_passed=native["postcondition_passed"],
        requested_pin_count=native["data"].get("requested_pin_count"),
        validated_pin_count=native["data"].get("validated_pin_count"),
        included_pin_count=native["data"].get("included_pin_count"),
        pins_truncated=native["data"].get("pins_truncated"),
        pin_found_count=native["data"].get("pin_found_count"),
        missing_pin_count=native["data"].get("missing_pin_count"),
        can_provide_default_count=native["data"].get("can_provide_default_count"),
        cannot_provide_default_count=native["data"].get("cannot_provide_default_count"),
        proposed_default_valid_count=native["data"].get("proposed_default_valid_count"),
        proposed_default_invalid_count=native["data"].get("proposed_default_invalid_count"),
        already_matching_count=native["data"].get("already_matching_count"),
        would_set_count=native["data"].get("would_set_count"),
        defaults_set_count=native["data"].get("defaults_set_count"),
        native_writer_completion_not_inferred_from_diagnostic_counts=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        runtime_pose="not_run", playback="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_partial_set_resize_linked_UI_compile_init_save_or_SDK_effects_inspect_native_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_rigvm_pin_default_writer_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_rigvm_pin_default_writer_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_paths": {field: arguments[field] for field in PATH_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=arguments["compile"], attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=arguments["save_asset"], attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_rigvm_pin_default_writer_operation"
    try:
        native = api.set_controlrig_rigvm_pin_defaults(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "rigvm_pin_default_writer_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_rigvm_pin_default_writer_operation"
        callback(result) # Preserve malformed return/uncertain dispatch before reporting the error.
        raise
    _adopt(result, arguments, native)
    callback(result) # Full Native actual/partial/raw/compile-save/unknown prefix precedes errors.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "rigvm_pin_default_writer_native_failed",
            native.get("error") or "Native guarded pin default writer failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("rigvm_pin_default_writer_result_unverified", "Native writer completion is unverified; no replay")
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["stage"] = "rigvm_pin_default_writer_input_validation"
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False, journal_created=False)
    return result


def set_controlrig_rigvm_pin_defaults(control_rig_path: str = "", control_rig_asset_path: str = "", control_rig_class_path: str = "",
        pin_defaults: Optional[List[Any]] = None, defaults: Optional[List[Any]] = None,
        resize_arrays: bool = False, set_value_on_linked_pins: bool = False, dry_run: bool = False, compile: bool = True,
        save_asset: bool = False, max_pins: int = 64) -> Dict[str, Any]:
    try:
        arguments = _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
            pin_defaults, defaults, resize_arrays, set_value_on_linked_pins, dry_run, compile, save_asset, max_pins)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def set_controlrig_rigvm_pin_defaults_action(result, control_rig_path: str = "", control_rig_asset_path: str = "", control_rig_class_path: str = "",
        pin_defaults: Optional[List[Any]] = None, defaults: Optional[List[Any]] = None,
        resize_arrays: bool = False, set_value_on_linked_pins: bool = False, dry_run: bool = False, compile: bool = True,
        save_asset: bool = False, max_pins: int = 64, checkpoint_callback=None):
    return _run_action(result, _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
        pin_defaults, defaults, resize_arrays, set_value_on_linked_pins, dry_run, compile, save_asset, max_pins), checkpoint_callback)
