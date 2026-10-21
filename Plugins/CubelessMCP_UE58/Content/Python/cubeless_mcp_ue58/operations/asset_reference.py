"""Original reference-remap12 composite receipts in one Named or Batch parent.

Native owns resolution, archive/graph/property remapping, optional material
refresh and actual package save. Dry-run skips explicit target writes, but its
Registry and LoadAsset callbacks are not proven pure or exact write predictions.
"""
import copy
from typing import Any, Dict

from cubeless_mcp_ue58.compatibility.ue58 import asset_reference as api
from . import state
from .state import OperationError

METHOD = "replace_asset_references_mcp"
PUBLIC_FIELDS = ("replacements", "target_root", "target_paths", "source_root", "string_replacements",
    "name_replacements", "dry_run", "save_assets", "scan_paths", "refresh_materials",
    "include_package_objects", "allow_non_game_targets")
NULLABLE_MAP_FIELDS = ("replacements", "string_replacements", "name_replacements")


def _arguments(replacements, target_root, target_paths, source_root, string_replacements,
        name_replacements, dry_run, save_assets, scan_paths, refresh_materials,
        include_package_objects, allow_non_game_targets):
    values = dict(replacements=replacements, target_root=target_root, target_paths=target_paths,
        source_root=source_root, string_replacements=string_replacements, name_replacements=name_replacements,
        dry_run=dry_run, save_assets=save_assets, scan_paths=scan_paths, refresh_materials=refresh_materials,
        include_package_objects=include_package_objects, allow_non_game_targets=allow_non_game_targets)
    if type(target_root) is not str or type(source_root) is not str:
        raise OperationError("asset_reference_input_invalid", "target_root and source_root must be strings")
    for field in NULLABLE_MAP_FIELDS:
        value = values[field]
        if value is not None and (type(value) is not dict
                or any(type(key) is not str or type(item) is not str for key, item in value.items())):
            raise OperationError("asset_reference_input_invalid", field + " must be a string-to-string object or null")
    if target_paths is not None and (type(target_paths) is not list or any(type(item) is not str for item in target_paths)):
        raise OperationError("asset_reference_input_invalid", "target_paths must be a string array or null")
    for field in ("dry_run", "save_assets", "scan_paths", "refresh_materials", "include_package_objects", "allow_non_game_targets"):
        if type(values[field]) is not bool:
            raise OperationError("asset_reference_input_invalid", field + " must be a boolean")
    # No trim/dedupe/reorder, map expansion, mount/type/path validation or
    # None-to-empty substitution is performed by the Python domain adapter.
    return copy.deepcopy(values)


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_twelve_public_parameters_forwarded_unchanged=True,
        nullable_map3_array1_preserve_null_and_empty=True,
        native_scope_resolution_archive_graph_property_material_and_save_authoritative=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        adapter_shader_wait_or_poll_calls=0, adapter_structural_rollback_calls=0,
        dry_run_is_total_SDK_purity_proof=False, dry_run_is_exact_write_count_prediction=False,
        explicit_compile_flags_prove_implicit_compile_or_shader_completion=False,
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
        compiled=native["compiled"], status="native_asset_reference_compile_receipt",
        observation=native["data"].get("compile_observation"),
        completion_scope="Native_actual_requested_attempted_completion_receipt; SDK_implicit_or_scheduled_fanout_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_asset_reference_package_save_receipt",
        saved_count=native["data"].get("saved_count"), save_calls=copy.deepcopy(native["data"].get("save_calls")),
        dry_run_skipped=native["data"].get("save_skipped_due_to_dry_run"))
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD, "requested_target_root": arguments["target_root"],
        "requested_target_paths": copy.deepcopy(arguments["target_paths"]), "dry_run": arguments["dry_run"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None, "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_asset_reference_work_and_optional_save_postcondition",
        native_postcondition_passed=native["postcondition_passed"], total_change_state_known=known,
        dry_run_predicts_mutation_counts=False, all_implicit_SDK_effects_observed=False,
        explicit_counts_and_partial_resolution_and_save_scope="full_Native_data_and_raw_receipt",
        shader_or_implicit_Blueprint_compile_completion="not_inferred_from_call_return",
        disk_reload="not_run", packaged_runtime="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_load_remap_compile_or_save_outcome_inspect_actual_assets_files_and_journal_no_replay" if not known
            else "accepted_remap_refresh_or_save_may_repeat_inspect_actual_assets_and_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_asset_reference_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
        pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["save_assets"], attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_asset_reference_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_target_root": arguments["target_root"],
        "dry_run": arguments["dry_run"], "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_asset_reference_operation"
    try:
        native = api.replace_asset_references_mcp(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_asset_reference_operation"
            callback(result)
        if getattr(error, "code", "") == "asset_reference_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_not_dispatched")
        raise
    _adopt(result, arguments, native)
    callback(result)  # Actual scope/remap/compile/save and malformed prefix precede errors.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "asset_reference_native_failed",
            native.get("error") or "Native asset reference replacement failed; actual partial load/remap/compile/save prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("asset_reference_result_unverified", "Native reference replacement postcondition is unverified; no replay")
    # Completed Native work can coexist with unknown total SDK effects and null
    # shader/compile completion. Preserve those; following Batch work must stop.
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    return result


def replace_asset_references_mcp(replacements: dict[str, str] | None = None, target_root: str = "",
        target_paths: list[str] | None = None, source_root: str = "",
        string_replacements: dict[str, str] | None = None, name_replacements: dict[str, str] | None = None,
        dry_run: bool = True, save_assets: bool = False, scan_paths: bool = True, refresh_materials: bool = True,
        include_package_objects: bool = False, allow_non_game_targets: bool = False) -> Dict[str, Any]:
    try:
        arguments = _arguments(replacements, target_root, target_paths, source_root,
            string_replacements, name_replacements, dry_run, save_assets, scan_paths,
            refresh_materials, include_package_objects, allow_non_game_targets)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def replace_asset_references_mcp_action(result, replacements: dict[str, str] | None = None, target_root: str = "",
        target_paths: list[str] | None = None, source_root: str = "",
        string_replacements: dict[str, str] | None = None, name_replacements: dict[str, str] | None = None,
        dry_run: bool = True, save_assets: bool = False, scan_paths: bool = True, refresh_materials: bool = True,
        include_package_objects: bool = False, allow_non_game_targets: bool = False, checkpoint_callback=None):
    return _run_action(result, _arguments(replacements, target_root, target_paths, source_root,
        string_replacements, name_replacements, dry_run, save_assets, scan_paths,
        refresh_materials, include_package_objects, allow_non_game_targets), checkpoint_callback)
