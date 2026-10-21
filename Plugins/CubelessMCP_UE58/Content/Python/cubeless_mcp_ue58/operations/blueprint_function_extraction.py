"""Guarded Blueprint node selection to native function extraction.

Native owns all selection/GUID/name/temp checks, a directly owned Blueprint
Editor adapter, diagnostic graphs, extraction, requested compiler/save and
cleanup. Even dry_run may create temporary diagnostic graphs and UI state.
Dirty/status restoration is not complete SDK rollback. Python adds no engine
query, UI operation, compiler, save or uncertain retry.
"""
import copy
from typing import Any, Dict

from cubeless_mcp_ue58.compatibility.ue58 import blueprint_function_extraction as api
from . import state
from .state import OperationError

METHOD = "collapse_blueprint_nodes_to_function"
PUBLIC_FIELDS = ("blueprint_name", "graph_name", "function_name", "node_ids", "graph_type",
    "expected_node_count", "allow_non_temp", "dry_run", "compile", "save_asset")
TEXT_FIELDS = ("blueprint_name", "graph_name", "function_name", "graph_type")
BOOL_FIELDS = ("allow_non_temp", "dry_run", "compile", "save_asset")
LEGACY_COMMAND_FIELDS = api.LEGACY_COMMAND_FIELDS
_FORBIDDEN_MCP_REFERENCE_TOKENS = (
    "/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal",
    "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor")


def _reject_legacy_command_dependency_refs(params):
    """Original public Batch value guard; direct Named/Action do not use it."""
    def visit(field_name, value):
        if isinstance(value, str):
            candidate = value.strip().strip("\"'").replace("\\", "/").lower()
            if candidate and any(token in candidate for token in _FORBIDDEN_MCP_REFERENCE_TOKENS):
                error = OperationError("blueprint_function_extraction_dependency_reference",
                    "Refusing MCP-only reference in '" + field_name + "'. Finished assets must not depend on the editor bridge.")
                error.field_name = field_name
                error.reason = "mcp_only_reference"
                raise error
            return
        if isinstance(value, dict):
            for key, child in value.items():
                visit(field_name + "." + str(key), child)
            return
        if isinstance(value, (list, tuple, set)):
            for index, child in enumerate(value):
                visit(field_name + "[" + str(index) + "]", child)
    visit("params", params)


def _arguments(blueprint_name, graph_name, function_name, node_ids, graph_type, expected_node_count, allow_non_temp, dry_run, compile, save_asset, *, _legacy_command_params=None):
    values = dict(zip(PUBLIC_FIELDS, (blueprint_name, graph_name, function_name, node_ids, graph_type, expected_node_count, allow_non_temp, dry_run, compile, save_asset)))
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("blueprint_function_extraction_input_invalid", field + " must be a string; Native owns path/name interpretation")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("blueprint_function_extraction_input_invalid", field + " must be a boolean")
    if type(expected_node_count) is not int or not -(1 << 63) <= expected_node_count < (1 << 63):
        raise OperationError("blueprint_function_extraction_input_invalid", "expected_node_count must be a signed int64")
    if type(node_ids) is not list or not 1 <= len(node_ids) <= 2048 or any(type(row) is not str for row in node_ids):
        raise OperationError("blueprint_function_extraction_input_invalid", "node_ids must contain 1..2048 GUID strings; Native owns parse, duplicates and expected count")
    api._carrier(node_ids)
    api._carrier(api._private_context(_legacy_command_params))
    try:
        return copy.deepcopy(values), copy.deepcopy(_legacy_command_params)
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("blueprint_function_extraction_input_invalid", "Arguments could not be copied before dispatch") from error


def _context(result, arguments, legacy):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        legacy_command_params=copy.deepcopy(legacy), legacy_command_keys=[] if legacy is None else list(legacy),
        native_private_context=copy.deepcopy(api._private_context(legacy)),
        public_input_count=10, hidden_Native_input_count=11,
        original_ten_public_parameter_order_preserved=True,
        limited_private_graph_id_raw_presence1_only=True,
        original_Named_dependency_guard_present=False,
        original_public_Batch_recursive_values_guard_owned_by_Batch=True,
        legacy_save_asset_OR_save_normalization_owned_by_Batch=True,
        native_GUID_selection_count_name_temp_diagnostic_extract_compile_save_partial_authoritative=True,
        expected_count_exact_int64_no_Python_round_or_clamp=True,
        adapter_asset_graph_or_selection_queries=0, adapter_UI_or_extraction_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        dry_diagnostic_graph_UI_or_message_effects_are_not_purity=True,
        dirty_status_restoration_is_not_total_rollback=True,
        current_idle_admission_owned_by_native_and_registration=True,
        actual_named_wire_verified=False, disk_reload_verified=False,
        function_runtime_execution_verified=False, packaged_runtime_verified=False,
        unique_external_pipeline_complete=False, automatic_action_retry=False, atomic_rollback=False)


def _adopt(result, arguments, legacy, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_result_data=copy.deepcopy(native["data"]),
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], change_state_known=known, mutation_state_unknown=not known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments, legacy)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_blueprint_function_extraction_compile_receipt",
        completion_scope="Native_explicit_compiler_receipt; opaque_SDK_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_blueprint_function_extraction_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_targets": {field: arguments[field] for field in TEXT_FIELDS}, "node_ids": copy.deepcopy(arguments["node_ids"]), "dry_run": arguments["dry_run"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_blueprint_function_extraction_completion",
        native_postcondition_passed=native["postcondition_passed"],
        full_native_legacy_selection_extraction_compile_save_cleanup_prefix_preserved=True,
        legacy_success_does_not_override_standard_postcondition_failure=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        disk_reload="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_diagnostic_UI_graph_extraction_compile_save_inspect_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_blueprint_function_extraction_operation"


def _run_action(result, arguments, legacy, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments, legacy)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, change_state_known=True, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_blueprint_function_extraction_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, change_state_known=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=arguments["compile"], attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=arguments["save_asset"], attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_blueprint_function_extraction_operation"
    try:
        native = api.collapse_blueprint_nodes_to_function(*(arguments[field] for field in PUBLIC_FIELDS), _legacy_command_params=legacy)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "native_call_dispatched", None) is False:
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                change_state_known=True, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_blueprint_function_extraction_operation"
        callback(result)
        raise
    _adopt(result, arguments, legacy, native)
    callback(result)  # Keep the full actual prefix before terminal error criteria.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_function_extraction_native_failed",
            native.get("error") or "Native Blueprint function extraction failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("blueprint_function_extraction_result_unverified", "Native Blueprint function extraction postcondition is unverified; no replay")
    result.update(success=True, stage="complete")
    return result


def _adopt_rejection(result, error):
    state.fail(result, error)
    result["stage"] = "blueprint_function_extraction_input_validation"
    result["changes"] = []
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
        change_state_known=True, mutation_state_unknown=False, journal_created=False)
    return result


def _rejected(error):
    # A new UUID is not evidence of a file; this path does not create a journal.
    return _adopt_rejection(state.new_result(), error)


def collapse_blueprint_nodes_to_function(blueprint_name: str, graph_name: str, function_name: str, node_ids: list[str], graph_type: str='event', expected_node_count: int=-1, allow_non_temp: bool=False, dry_run: bool=True, compile: bool=True, save_asset: bool=False) -> Dict[str, Any]:
    try:
        arguments, legacy = _arguments(blueprint_name, graph_name, function_name, node_ids, graph_type, expected_node_count, allow_non_temp, dry_run, compile, save_asset)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments, legacy))


def collapse_blueprint_nodes_to_function_action(result, blueprint_name: str, graph_name: str, function_name: str, node_ids: list[str], graph_type: str='event', expected_node_count: int=-1, allow_non_temp: bool=False, dry_run: bool=True, compile: bool=True, save_asset: bool=False, checkpoint_callback=None, *, _legacy_command_params=None):
    try:
        arguments, legacy = _arguments(blueprint_name, graph_name, function_name, node_ids, graph_type, expected_node_count, allow_non_temp, dry_run, compile, save_asset, _legacy_command_params=_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result, error)
        result["data"]["parent_journal_only"] = True
        (checkpoint_callback or state.checkpoint)(result)
        raise
    return _run_action(result, arguments, legacy, checkpoint_callback)
