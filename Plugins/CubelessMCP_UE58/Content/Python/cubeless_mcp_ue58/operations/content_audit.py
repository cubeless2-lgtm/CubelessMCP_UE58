"""Audit14 diagnostic/report receipts with one Named or Batch parent journal.

Native owns Registry work and optional report I/O. Completed diagnosis with
validation_pass=False is a normal result, not an operation failure. Neither a
successful diagnostic nor explicit asset-call counters prove SDK callback purity.
"""
import copy
from typing import Any, Dict

from cubeless_mcp_ue58.compatibility.ue58 import content_audit as api
from . import state
from .state import OperationError

METHOD = "audit_content_root_mcp"
PUBLIC_FIELDS = ("root_path", "recursive", "scan_paths", "include_dependencies", "include_assets",
    "write_report", "forbidden_dependency_prefixes", "required_asset_paths", "expected_asset_count",
    "expected_class_counts", "max_samples", "fail_on_dirty_packages", "fail_on_redirectors",
    "fail_on_forbidden_dependencies")


def _arguments(root_path, recursive, scan_paths, include_dependencies, include_assets,
        write_report, forbidden_dependency_prefixes, required_asset_paths,
        expected_asset_count, expected_class_counts, max_samples, fail_on_dirty_packages,
        fail_on_redirectors, fail_on_forbidden_dependencies):
    values = dict(root_path=root_path, recursive=recursive, scan_paths=scan_paths,
        include_dependencies=include_dependencies, include_assets=include_assets,
        write_report=write_report, forbidden_dependency_prefixes=forbidden_dependency_prefixes,
        required_asset_paths=required_asset_paths, expected_asset_count=expected_asset_count,
        expected_class_counts=expected_class_counts, max_samples=max_samples,
        fail_on_dirty_packages=fail_on_dirty_packages, fail_on_redirectors=fail_on_redirectors,
        fail_on_forbidden_dependencies=fail_on_forbidden_dependencies)
    if type(root_path) is not str or type(max_samples) is not int:
        raise OperationError("content_audit_input_invalid", "root_path must be a string and max_samples an integer")
    for field in ("recursive", "scan_paths", "include_dependencies", "include_assets", "write_report",
            "fail_on_dirty_packages", "fail_on_redirectors", "fail_on_forbidden_dependencies"):
        if type(values[field]) is not bool:
            raise OperationError("content_audit_input_invalid", field + " must be a boolean")
    for field in ("forbidden_dependency_prefixes", "required_asset_paths"):
        value = values[field]
        if value is not None and (type(value) is not list or any(type(item) is not str for item in value)):
            raise OperationError("content_audit_input_invalid", field + " must be a string array or null")
    if expected_asset_count is not None and type(expected_asset_count) is not int:
        raise OperationError("content_audit_input_invalid", "expected_asset_count must be an integer or null")
    if expected_class_counts is not None and (type(expected_class_counts) is not dict
            or any(type(key) is not str or type(value) is not int for key, value in expected_class_counts.items())):
        raise OperationError("content_audit_input_invalid", "expected_class_counts must be a string-to-integer object or null")
    # Native owns root trim/path checks, finite limits, min-sample/count clamp,
    # dependency default semantics and exact class/count criteria. Do not dedupe
    # paths or replace nullable inputs with empty collections in Python.
    return copy.deepcopy(values)


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_fourteen_public_parameters_forwarded_unchanged=True,
        public_nullable_inputs_preserve_null_empty_and_zero=True,
        native_inventory_dependency_dirty_diagnostic_and_report_prefix_authoritative=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        adapter_explicit_report_write_calls=0,
        registry_scan_and_tag_callback_purity_verified=False,
        actual_named_nullable_wire_verified=False,
        automatic_action_retry=False, atomic_rollback=False,
        unique_external_pipeline_complete=False)


def _adopt(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items()
            if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_content_audit_compile_receipt",
        scope="explicit_engine_asset_compile", SDK_implicit_effects="separate_unknown_native_data")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_content_audit_report_save_receipt",
        scope="optional_audit_report_file_not_Unreal_asset_save",
        actual_report_path=native["data"].get("report_path"),
        report_saved=native["data"].get("report_saved"),
        report_snapshot_prefix_only=native["data"].get("report_snapshot_prefix_only"))
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD, "requested_root_path": arguments["root_path"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None, "postcondition_passed": native["postcondition_passed"],
        "observed_report_or_directory_changed": native["data"].get("observed_report_or_directory_changed")}]
    result["validation"].update(status="native_content_audit_diagnostic_and_report_postcondition",
        native_postcondition_passed=native["postcondition_passed"],
        audit_completed=native["data"].get("audit_completed"),
        inventory_complete=native["data"].get("inventory_complete"),
        criteria_validation_pass=native["data"].get("validation_pass"),
        criteria_failure_is_operation_failure=False, total_change_state_known=known,
        actual_report_persistence_scope="returned_Native_file_bool_size_exact_readback_prefix",
        asset_reload="not_run", packaged_runtime="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_Registry_callbacks_or_report_outcome_inspect_actual_state_and_journal_no_replay" if not known
            else "new_report_or_scan_would_repeat_inspect_existing_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_content_audit_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
        pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["write_report"], attempted=False, saved=False,
        status="native_not_dispatched", scope="optional_audit_report_file_not_Unreal_asset_save")
    result["stage"] = "before_content_audit_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_root_path": arguments["root_path"],
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_content_audit_operation"
    try:
        native = api.audit_content_root_mcp(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_content_audit_operation"
            callback(result)
        if getattr(error, "code", "") == "content_audit_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_not_dispatched")
        raise
    _adopt(result, arguments, native)
    callback(result)  # Full partial raw/data/report receipt precedes error judgement.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "content_audit_native_failed",
            native.get("error") or "Native audit/report operation failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("content_audit_result_unverified", "Native diagnosis/report postcondition is unverified; no replay")
    # A completed diagnosis with validation_pass=False is still successful.
    # Unknown SDK callback effects remain unknown and stop following Batch work.
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    return result


def audit_content_root_mcp(root_path: str, recursive: bool = True, scan_paths: bool = True,
        include_dependencies: bool = True, include_assets: bool = False, write_report: bool = False,
        forbidden_dependency_prefixes: list[str] | None = None, required_asset_paths: list[str] | None = None,
        expected_asset_count: int | None = None, expected_class_counts: dict[str, int] | None = None,
        max_samples: int = 50, fail_on_dirty_packages: bool = False, fail_on_redirectors: bool = True,
        fail_on_forbidden_dependencies: bool = True) -> Dict[str, Any]:
    try:
        arguments = _arguments(root_path, recursive, scan_paths, include_dependencies, include_assets,
            write_report, forbidden_dependency_prefixes, required_asset_paths, expected_asset_count,
            expected_class_counts, max_samples, fail_on_dirty_packages, fail_on_redirectors,
            fail_on_forbidden_dependencies)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def audit_content_root_mcp_action(result, root_path: str, recursive: bool = True, scan_paths: bool = True,
        include_dependencies: bool = True, include_assets: bool = False, write_report: bool = False,
        forbidden_dependency_prefixes: list[str] | None = None, required_asset_paths: list[str] | None = None,
        expected_asset_count: int | None = None, expected_class_counts: dict[str, int] | None = None,
        max_samples: int = 50, fail_on_dirty_packages: bool = False, fail_on_redirectors: bool = True,
        fail_on_forbidden_dependencies: bool = True, checkpoint_callback=None):
    return _run_action(result, _arguments(root_path, recursive, scan_paths, include_dependencies, include_assets,
        write_report, forbidden_dependency_prefixes, required_asset_paths, expected_asset_count,
        expected_class_counts, max_samples, fail_on_dirty_packages, fail_on_redirectors,
        fail_on_forbidden_dependencies), checkpoint_callback)
