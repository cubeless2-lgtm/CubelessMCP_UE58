"""StateTree Add6/Compile2 composite Native outcomes in one parent journal.

Native owns original dry-run/reuse, compiler/save/audit and conditional
structural recovery. Python never adds an asset action or retries uncertainty.
"""
import copy
from typing import Any, Dict
from cubeless_mcp_ue58.compatibility.ue58 import state_tree_mutators as api
from . import state
from .state import OperationError

PUBLIC_FIELDS_BY_METHOD = {
    "add_state_tree_state": ("asset_path", "state_name", "parent_path", "state_type", "selection_behavior", "dry_run"),
    "compile_state_tree": ("asset_path", "save"),
}
FIELD_TYPES_BY_METHOD = {
    "add_state_tree_state": {"asset_path": str, "state_name": str, "parent_path": str,
        "state_type": str, "selection_behavior": str, "dry_run": bool},
    "compile_state_tree": {"asset_path": str, "save": bool},
}


def _arguments(method, public):
    if method not in api.BINDINGS or tuple(public) != PUBLIC_FIELDS_BY_METHOD[method]:
        raise OperationError("state_tree_mutators_input_invalid", "Only ordered original StateTree mutation public fields admitted")
    for field, kind in FIELD_TYPES_BY_METHOD[method].items():
        if type(public[field]) is not kind:
            raise OperationError("state_tree_mutators_input_invalid", "Original public field requires exact JSON type without coercion: " + field)
    return {"method": method, "public": copy.deepcopy(public)}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], requested_arguments=copy.deepcopy(arguments["public"]),
        original_all_parameters_forwarded_unchanged=True,
        original_dry_run_exact_boolean_default_true=True,
        Native_original_parent_path_state_type_selection_duplicate_dryrun_authoritative=True,
        Native_original_requested_compiler_save_live_persisted_audit_failure_prefix_authoritative=True,
        Native_conditional_structural_recovery_not_atomic_rollback=True,
        Native_LoadAsset_readiness_implicit_compile_separate_from_explicit_compile_counts=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, atomic_rollback=False,
        disk_reload_verified=False, packaged_runtime_verified=False,
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
        compiled=native["compiled"], status="native_state_tree_mutation_compile_receipt",
        implicit_SDK_compile_outcome="separate_native_data_not_inferred_from_explicit_flags")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_state_tree_mutation_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"]["asset_path"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None, "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_state_tree_mutation_postcondition",
        native_postcondition_passed=native["postcondition_passed"], total_change_state_known=known,
        disk_reload="not_run", packaged_runtime="not_run", unique_external_pipeline="not_run",
        compiler_live_persisted_audit_structural_recovery_scope="actual_native_data_prefix")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_native_outcome_inspect_actual_asset_and_journal_no_replay" if not known
            else "inspect_actual_native_compile_save_partial_recovery_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_state_tree_mutation_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
        pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_state_tree_mutation_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"]["asset_path"],
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_state_tree_mutation_operation"
    try:
        native = getattr(api, arguments["method"])(**arguments["public"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_state_tree_mutation_operation"
            callback(result)
        if getattr(error, "code", "") == "state_tree_mutators_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_not_dispatched")
        raise
    _adopt(result, arguments, native)
    callback(result)  # Entire actual partial/current/compiler/save/recovery prefix precedes criteria.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "state_tree_mutators_native_failed",
            native.get("error") or "Native StateTree mutation failed; actual partial and recovery prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("state_tree_mutators_result_unverified", "Native mutation lacks current postconditions; no replay")
    # Dry/reuse success and SDK total change certainty remain distinct.
    result.update(success=True, stage="complete")
    return result


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    return result


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def add_state_tree_state(asset_path: str, state_name: str, parent_path: str = "Root",
        state_type: str = "state", selection_behavior: str = "children_in_order",
        dry_run: bool = True) -> Dict[str, Any]:
    try:
        arguments = _arguments("add_state_tree_state", dict(asset_path=asset_path, state_name=state_name,
            parent_path=parent_path, state_type=state_type, selection_behavior=selection_behavior, dry_run=dry_run))
    except Exception as error:
        return _rejected("add_state_tree_state", error)
    return _named(arguments)


def compile_state_tree(asset_path: str, save: bool = True) -> Dict[str, Any]:
    try:
        arguments = _arguments("compile_state_tree", dict(asset_path=asset_path, save=save))
    except Exception as error:
        return _rejected("compile_state_tree", error)
    return _named(arguments)


def add_state_tree_state_action(result, asset_path: str, state_name: str, parent_path: str = "Root",
        state_type: str = "state", selection_behavior: str = "children_in_order", dry_run: bool = True,
        checkpoint_callback=None):
    return _run_action(result, _arguments("add_state_tree_state", dict(asset_path=asset_path, state_name=state_name,
        parent_path=parent_path, state_type=state_type, selection_behavior=selection_behavior, dry_run=dry_run)), checkpoint_callback)


def compile_state_tree_action(result, asset_path: str, save: bool = True, checkpoint_callback=None):
    return _run_action(result, _arguments("compile_state_tree", dict(asset_path=asset_path, save=save)), checkpoint_callback)
