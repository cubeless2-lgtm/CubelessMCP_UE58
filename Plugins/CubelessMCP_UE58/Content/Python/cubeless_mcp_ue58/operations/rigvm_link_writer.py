"""Guarded RigVM link writer10 in one Named journal or the existing Batch parent.

Native owns target restrictions, alias/spec selection, all-prefix writability,
existing-link reuse, ordered AddLink and removed-link observations, auto-VM/init,
compile/save and partial failures. Python performs no Engine graph work.
"""
import copy
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import rigvm_link_writer as api
from . import state
from .state import OperationError

METHOD = "add_controlrig_rigvm_links"
PUBLIC_FIELDS = ("control_rig_path", "control_rig_asset_path", "control_rig_class_path",
    "links", "unit_links", "allow_type_casting", "dry_run", "compile", "save_asset", "max_links")
PATH_FIELDS = PUBLIC_FIELDS[:3]
ARRAY_FIELDS = PUBLIC_FIELDS[3:5]
BOOL_FIELDS = PUBLIC_FIELDS[5:9]


def _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
        links, unit_links, allow_type_casting, dry_run, compile, save_asset, max_links):
    values = dict(control_rig_path=control_rig_path, control_rig_asset_path=control_rig_asset_path,
        control_rig_class_path=control_rig_class_path, links=links, unit_links=unit_links,
        allow_type_casting=allow_type_casting, dry_run=dry_run, compile=compile,
        save_asset=save_asset, max_links=max_links)
    for field in PATH_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("rigvm_link_writer_input_invalid", field + " must be a string; None is not an empty path")
    for field in ARRAY_FIELDS:
        if values[field] is not None and type(values[field]) is not list:
            raise OperationError("rigvm_link_writer_input_invalid", field + " must be None or a JSON array; [] is not omission")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("rigvm_link_writer_input_invalid", field + " must be a boolean")
    if type(max_links) is not int or not -(1 << 63) <= max_links < (1 << 63):
        raise OperationError("rigvm_link_writer_input_invalid", "max_links must be an int64 integer; booleans are not limits")
    values = copy.deepcopy(values)
    # Native alone selects aliases, bounded rows, target roots and writer candidates.
    for field in ARRAY_FIELDS:
        api._carrier(values[field])
    return values


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_ten_public_parameters_forwarded_unchanged=True,
        nullable_link_specs_preserve_none_and_empty=True,
        native_aliases_clamp_target_guard_and_all_prefix_preflight_authoritative=True,
        native_existing_link_reuse_partial_apply_and_compile_save_authoritative=True,
        native_added_removed_link_and_dirty_observations_authoritative=True,
        permitted_writer_roots="Native_only: /Game/_MCP_Sample/; /Game/_MCP_Temp/; /Game/CubelessMCPTests/",
        general_Game_or_user_asset_write_permission=False,
        legacy_aliases_not_public_parameters=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_AddLink_calls=0, adapter_RequestAutoVMRecompilation_calls=0,
        adapter_RequestControlRigInit_calls=0, adapter_explicit_compile_calls=0,
        adapter_explicit_save_calls=0, adapter_runtime_execute_calls=0, adapter_runtime_solve_calls=0,
        cannot_link_alone_proves_writer_completion=False,
        existing_link_noop_can_complete_without_AddLink=True,
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
        compiled=native["compiled"], status="native_rigvm_link_writer_compile_receipt",
        completion_scope="Native_explicit_receipt; all_implicit_loader_controller_autoVM_init_and_callback_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_rigvm_link_writer_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_paths": {field: arguments[field] for field in PATH_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "target_dirty_before": native["data"].get("target_dirty_before"),
        "target_dirty_after": native["data"].get("target_dirty_after"),
        "observed_dirty_changed": native["data"].get("observed_dirty_changed"),
        "observed_explicit_change": native["data"].get("observed_explicit_change"),
        "links_added_count": native["data"].get("links_added_count"),
        "partial_target_modified": native["data"].get("partial_target_modified"),
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_guarded_RigVM_link_writer_completion",
        native_postcondition_passed=native["postcondition_passed"],
        requested_link_count=native["data"].get("requested_link_count"),
        validated_link_count=native["data"].get("validated_link_count"),
        included_link_count=native["data"].get("included_link_count"),
        links_truncated=native["data"].get("links_truncated"),
        can_link_count=native["data"].get("can_link_count"),
        cannot_link_count=native["data"].get("cannot_link_count"),
        missing_pin_count=native["data"].get("missing_pin_count"),
        already_linked_count=native["data"].get("already_linked_count"),
        would_add_count=native["data"].get("would_add_count"),
        links_added_count=native["data"].get("links_added_count"),
        native_writer_completion_not_inferred_from_diagnostic_counts=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        runtime_pose="not_run", playback="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_partial_AddLink_removal_compile_init_save_or_SDK_effects_inspect_native_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_rigvm_link_writer_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_rigvm_link_writer_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_paths": {field: arguments[field] for field in PATH_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=arguments["compile"], attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=arguments["save_asset"], attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_rigvm_link_writer_operation"
    try:
        native = api.add_controlrig_rigvm_links(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "rigvm_link_writer_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_rigvm_link_writer_operation"
        callback(result) # Preserve malformed return/uncertain dispatch before reporting the error.
        raise
    _adopt(result, arguments, native)
    callback(result) # Full Native actual/partial/raw/compile-save/unknown prefix precedes errors.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "rigvm_link_writer_native_failed",
            native.get("error") or "Native guarded link writer failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("rigvm_link_writer_result_unverified", "Native writer completion is unverified; no replay")
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["stage"] = "rigvm_link_writer_input_validation"
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False, journal_created=False)
    return result


def add_controlrig_rigvm_links(control_rig_path: str = "", control_rig_asset_path: str = "", control_rig_class_path: str = "",
        links: Optional[List[Any]] = None, unit_links: Optional[List[Any]] = None,
        allow_type_casting: bool = False, dry_run: bool = False, compile: bool = True,
        save_asset: bool = False, max_links: int = 64) -> Dict[str, Any]:
    try:
        arguments = _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
            links, unit_links, allow_type_casting, dry_run, compile, save_asset, max_links)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def add_controlrig_rigvm_links_action(result, control_rig_path: str = "", control_rig_asset_path: str = "", control_rig_class_path: str = "",
        links: Optional[List[Any]] = None, unit_links: Optional[List[Any]] = None,
        allow_type_casting: bool = False, dry_run: bool = False, compile: bool = True,
        save_asset: bool = False, max_links: int = 64, checkpoint_callback=None):
    return _run_action(result, _arguments(control_rig_path, control_rig_asset_path, control_rig_class_path,
        links, unit_links, allow_type_casting, dry_run, compile, save_asset, max_links), checkpoint_callback)
