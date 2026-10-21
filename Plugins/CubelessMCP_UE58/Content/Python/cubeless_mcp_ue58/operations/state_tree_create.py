"""Original StateTree creator3 with one named journal or Batch parent.

Native preserves idempotence and the original factory/compiler/audit/save
failure prefixes. Python never adds a compiler, saver, rollback or retry.
"""
import copy
from typing import Any, Dict
from cubeless_mcp_ue58.compatibility.ue58 import state_tree_create as api
from . import state
from .state import OperationError

METHOD = "create_state_tree"
PUBLIC_FIELDS = ("name", "package_path", "schema_type")


def _arguments(name, package_path, schema_type):
    values = dict(name=name, package_path=package_path, schema_type=schema_type)
    if any(type(value) is not str for value in values.values()):
        raise OperationError("state_tree_create_input_invalid", "Original creator name/package_path/schema_type must be literal strings")
    return values


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_all_three_parameters_forwarded_unchanged=True,
        Native_original_name_package_schema_idempotence_factory_compile_audit_save_authoritative=True,
        Native_LoadAsset_factory_readiness_implicit_compile_separate_from_explicit_compile_counts=True,
        Native_actual_partial_cleanup_current_compile_save_prefix_authoritative=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        no_new_public_asset_path_overwrite_or_compile_save_flags=True,
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
        compiled=native["compiled"], status="native_creator_compile_receipt",
        implicit_SDK_compile_outcome="separate_native_data_not_inferred_from_explicit_flags")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_creator_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD, "requested_name": arguments["name"],
        "requested_package_path": arguments["package_path"], "requested_schema_type": arguments["schema_type"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None, "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_state_tree_creator_postcondition",
        native_postcondition_passed=native["postcondition_passed"], total_change_state_known=known,
        disk_reload="not_run", packaged_runtime="not_run", unique_external_pipeline="not_run",
        compiler_live_audit_persisted_audit_cleanup_scope="actual_native_data_prefix")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_native_outcome_inspect_actual_asset_and_journal_no_replay" if not known
            else "inspect_actual_native_factory_compile_audit_save_cleanup_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_state_tree_create_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
        pending_operation=copy.deepcopy(arguments))
    # No new Python compile/save option exists. Native determines requested work
    # after original admission/idempotence; before dispatch its outcome is unknown.
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_state_tree_create_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_name": arguments["name"],
        "requested_package_path": arguments["package_path"], "attempted": None,
        "changed": None, "change_state_known": False}]
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_state_tree_create_operation"
    try:
        native = api.create_state_tree(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_state_tree_create_operation"
            callback(result)
        if getattr(error, "code", "") == "state_tree_create_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_not_dispatched")
        raise
    _adopt(result, arguments, native)
    callback(result)  # Complete raw/current/partial/compiler/save/audit prefix precedes failure.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "state_tree_create_native_failed",
            native.get("error") or "Native StateTree creator failed; actual partial and cleanup prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("state_tree_create_result_unverified", "Native creator lacks current postconditions; no replay")
    # SDK unknown total changes remain explicit even if the original idempotent
    # or creation postcondition succeeds. Batch must stop following work on unknown.
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    return result


def create_state_tree(name: str, package_path: str = "/Game/AI/StateTrees",
        schema_type: str = "component") -> Dict[str, Any]:
    try:
        arguments = _arguments(name, package_path, schema_type)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def create_state_tree_action(result, name: str, package_path: str = "/Game/AI/StateTrees",
        schema_type: str = "component", checkpoint_callback=None):
    return _run_action(result, _arguments(name, package_path, schema_type), checkpoint_callback)
