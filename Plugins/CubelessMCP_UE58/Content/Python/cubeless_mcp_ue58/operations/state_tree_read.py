"""Original StateTree list/inspect with one named journal or Batch parent.

Loading and readiness can compile implicitly inside the installed StateTree
SDK. Successful bounded observation does not prove total changes are known.
"""
import copy
from typing import Any, Dict
from cubeless_mcp_ue58.compatibility.ue58 import state_tree_read as api
from . import state
from .state import OperationError

PUBLIC_FIELDS_BY_METHOD = {
    "list_state_trees": ("directory", "recursive", "max_results", "offset"),
    "inspect_state_tree": ("asset_path", "max_states"),
}
FIELD_TYPES_BY_METHOD = {
    "list_state_trees": {"directory": str, "recursive": bool, "max_results": int, "offset": int},
    "inspect_state_tree": {"asset_path": str, "max_states": int},
}


def _arguments(method, public):
    if method not in api.BINDINGS or tuple(public) != PUBLIC_FIELDS_BY_METHOD[method]:
        raise OperationError("state_tree_read_input_invalid", "Only ordered original StateTree public fields admitted")
    for field, kind in FIELD_TYPES_BY_METHOD[method].items():
        if type(public[field]) is not kind:
            raise OperationError("state_tree_read_input_invalid", "Original public field type differs: " + field)
        if kind is int and not -(2**63) <= public[field] < 2**63:
            raise OperationError("state_tree_read_input_invalid", "Integer must fit the reflected int64 carrier; Native owns original int32 admission/clamp")
    return {"method": method, "public": copy.deepcopy(public)}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], requested_arguments=copy.deepcopy(arguments["public"]),
        original_parameters_forwarded_unchanged=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        Native_directory_path_dependency_int32_pagination_and_hierarchy_guards_authoritative=True,
        Native_LoadAsset_PostLoad_readiness_implicit_compile_not_explicit_compile_count=True,
        same_hash_or_dirty_does_not_prove_total_SDK_no_change=True,
        current_observation_success_separate_from_total_change_state_known=True,
        automatic_action_retry=False, full_original_contract_complete=False,
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
        observed_state_changed=native["changed"] if known else None,
        mutation_performed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_explicit_compile_receipt",
        implicit_compile_possible=native["data"].get("implicit_state_tree_compile_possible", False),
        total_implicit_compile_outcome="not_inferred_from_explicit_flags")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_explicit_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": "state_tree_observation_scope", "operation": arguments["method"],
        "asset_path": arguments["public"].get("asset_path"), "attempted": native["mutation_attempted"],
        "change_state_known": known, "changed": native["changed"] if known else None,
        "outcome": "positive_observed_change" if known and native["changed"] else "SDK_total_change_outcome_unknown"}]
    result["validation"].update(status="native_state_tree_observation_postcondition",
        native_postcondition_passed=native["postcondition_passed"], total_change_state_known=known,
        disk_reload="not_run", packaged_runtime="not_run", unique_external_pipeline="not_run",
        observation_and_total_SDK_change_scope_separate=True)
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_SDK_load_readiness_outcome_inspect_journal_no_replay" if not known
            else "observed_SDK_change_inspect_actual_state_before_new_call" if native["changed"] else "none_observed")
    result["stage"] = "returned_state_tree_observation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_dispatched")
    result["stage"] = "before_state_tree_observation_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_mutation_attempted=None,
        mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": "state_tree_observation_scope", "operation": arguments["method"],
        "attempted": None, "changed": None, "change_state_known": False, "outcome": "unknown_until_native_returns"}]
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_state_tree_observation"
    try:
        native = getattr(api, arguments["method"])(**arguments["public"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_state_tree_observation"
            callback(result)
        if getattr(error, "code", "") == "state_tree_read_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_not_dispatched")
        raise
    _adopt(result, arguments, native)
    callback(result)  # Entire actual prefix is durable before terminal criteria.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "state_tree_read_native_failed",
            native.get("error") or "Native bounded StateTree observation failed; actual prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("state_tree_read_observation_unverified", "Native observation lacks current postconditions; no replay")
    # A successful inspect can correctly retain unknown total SDK change state.
    result.update(success=True, stage="complete")
    return result


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    return result


def list_state_trees(directory: str = "/Game", recursive: bool = True,
        max_results: int = 200, offset: int = 0) -> Dict[str, Any]:
    try:
        arguments = _arguments("list_state_trees", dict(directory=directory, recursive=recursive,
            max_results=max_results, offset=offset))
    except Exception as error:
        return _rejected("list_state_trees", error)
    return _named(arguments)


def inspect_state_tree(asset_path: str, max_states: int = 500) -> Dict[str, Any]:
    try:
        arguments = _arguments("inspect_state_tree", dict(asset_path=asset_path, max_states=max_states))
    except Exception as error:
        return _rejected("inspect_state_tree", error)
    return _named(arguments)


def list_state_trees_action(result, directory: str = "/Game", recursive: bool = True,
        max_results: int = 200, offset: int = 0, checkpoint_callback=None):
    return _run_action(result, _arguments("list_state_trees", dict(directory=directory, recursive=recursive,
        max_results=max_results, offset=offset)), checkpoint_callback)


def inspect_state_tree_action(result, asset_path: str, max_states: int = 500, checkpoint_callback=None):
    return _run_action(result, _arguments("inspect_state_tree", dict(asset_path=asset_path,
        max_states=max_states)), checkpoint_callback)
