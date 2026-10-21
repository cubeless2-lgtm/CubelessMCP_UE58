"""Compound BlendSpace duplicate/reuse, bounded edits, resampling and optional save.

Native owns paths, row diagnostics, partial writes and UE58 engine effects.
Python records the actual receipt before failure, without extra engine work,
rollback, transport dispatch or uncertain-action retry.
"""
import copy
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import blendspace_variant as api
from . import state
from .state import OperationError

METHOD = "ensure_blendspace_sample_variant"
PUBLIC_FIELDS = ("source_blendspace", "variant_name", "target_root", "target_blendspace", "axis_edits",
    "sample_edits", "add_samples", "overwrite_existing", "save", "dry_run", "validate_only", "allow_non_sample", "force_resample")
TEXT_FIELDS = PUBLIC_FIELDS[:4]
EDIT_FIELDS = PUBLIC_FIELDS[4:7]
BOOL_FIELDS = PUBLIC_FIELDS[7:]
LEGACY_COMMAND_FIELDS = EDIT_FIELDS


def _arguments(source_blendspace, variant_name, target_root, target_blendspace, axis_edits,
        sample_edits, add_samples, overwrite_existing, save, dry_run, validate_only,
        allow_non_sample, force_resample, *, _legacy_command_params=None):
    values = dict(zip(PUBLIC_FIELDS, (source_blendspace, variant_name, target_root, target_blendspace,
        axis_edits, sample_edits, add_samples, overwrite_existing, save, dry_run, validate_only,
        allow_non_sample, force_resample)))
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("blendspace_variant_input_invalid", field + " must be a string")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("blendspace_variant_input_invalid", field + " must be a boolean")
    legacy = {} if _legacy_command_params is None else _legacy_command_params
    if type(legacy) is not dict or any(field not in LEGACY_COMMAND_FIELDS for field in legacy):
        raise OperationError("blendspace_variant_input_invalid", "Private projection permits only axis_edits/sample_edits/add_samples")
    for field in EDIT_FIELDS:
        if field in legacy:
            values[field] = legacy[field]
        elif values[field] is not None and type(values[field]) is not list:
            raise OperationError("blendspace_variant_input_invalid", field + " must be None or a list")
        # Row types, selectors, values and diagnostic order remain Native-owned.
        api._carrier(values[field])
    try:
        return copy.deepcopy(values), copy.deepcopy(legacy)
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("blendspace_variant_input_invalid", "Arguments could not be copied before dispatch") from error


def _context(result, arguments, legacy):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        legacy_command_params=copy.deepcopy(legacy), legacy_command_keys=list(legacy),
        original_thirteen_public_parameter_order_preserved=True,
        native_source_target_aliases_duplicate_reuse_and_allowed_inplace_authoritative=True,
        native_axis_sample_add_row_errors_and_partial_changes_authoritative=True,
        native_validate_resample_postedit_save_and_callbacks_authoritative=True,
        formal_named_edit_arrays_nullable=True, private_raw_edit_fields_only=True,
        none_empty_and_nonarray_private_values_preserved=True,
        adapter_world_or_asset_queries=0, adapter_duplicate_delete_calls=0,
        adapter_axis_sample_or_resample_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        editor_idle_admission_owned_by_native_and_registration=True,
        dry_run_is_total_SDK_purity_proof=False, actual_named_nullable_wire_verified=False,
        disk_reload_verified=False, packaged_runtime_verified=False,
        unique_external_pipeline_complete=False, automatic_action_retry=False, atomic_rollback=False)


def _adopt(result, arguments, legacy, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], change_state_known=known, mutation_state_unknown=not known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments, legacy)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_blendspace_variant_compile_receipt",
        completion_scope="Native_explicit_receipt; opaque_SDK_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_blendspace_variant_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_blendspace_variant_completion",
        native_postcondition_passed=native["postcondition_passed"],
        full_native_axis_sample_add_resample_save_and_partial_rows_preserved=True,
        save_failure_not_reinterpreted_as_success=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        disk_reload="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_asset_load_duplicate_edits_resample_callbacks_or_save_inspect_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_blendspace_variant_operation"

def _run_action(result, arguments, legacy, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments, legacy)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, change_state_known=True, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["save"], attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_blendspace_variant_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, change_state_known=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=arguments["save"], attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_blendspace_variant_operation"
    try:
        native = api.ensure_blendspace_sample_variant(*(arguments[field] for field in PUBLIC_FIELDS))
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "blendspace_variant_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                change_state_known=True, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=arguments["save"], attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_blendspace_variant_operation"
        callback(result)
        raise
    _adopt(result, arguments, legacy, native)
    callback(result)  # Keep full partial/native/unknown prefix before error criteria.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blendspace_variant_native_failed",
            native.get("error") or "Native BlendSpace variant failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("blendspace_variant_result_unverified", "Native variant postcondition is unverified; no replay")
    result.update(success=True, stage="complete")
    return result

def _adopt_rejection(result, error):
    state.fail(result, error)
    result["stage"] = "blendspace_variant_input_validation"
    result["changes"] = []
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
        change_state_known=True, mutation_state_unknown=False, journal_created=False)
    return result

def _rejected(error):
    return _adopt_rejection(state.new_result(), error)

def ensure_blendspace_sample_variant(source_blendspace: str, variant_name: str='Variant', target_root: str='/Game/_MCP_Sample/AnimStudy', target_blendspace: str='', axis_edits: Optional[List[Dict[str, Any]]]=None, sample_edits: Optional[List[Dict[str, Any]]]=None, add_samples: Optional[List[Dict[str, Any]]]=None, overwrite_existing: bool=False, save: bool=True, dry_run: bool=False, validate_only: bool=False, allow_non_sample: bool=False, force_resample: bool=False) -> Dict[str, Any]:
    try:
        arguments, legacy = _arguments(source_blendspace, variant_name, target_root, target_blendspace, axis_edits, sample_edits, add_samples, overwrite_existing, save, dry_run, validate_only, allow_non_sample, force_resample)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments, legacy))


def ensure_blendspace_sample_variant_action(result, source_blendspace: str, variant_name: str='Variant', target_root: str='/Game/_MCP_Sample/AnimStudy', target_blendspace: str='', axis_edits: Optional[List[Dict[str, Any]]]=None, sample_edits: Optional[List[Dict[str, Any]]]=None, add_samples: Optional[List[Dict[str, Any]]]=None, overwrite_existing: bool=False, save: bool=True, dry_run: bool=False, validate_only: bool=False, allow_non_sample: bool=False, force_resample: bool=False, checkpoint_callback=None, *, _legacy_command_params=None):
    try:
        arguments, legacy = _arguments(source_blendspace, variant_name, target_root, target_blendspace, axis_edits, sample_edits, add_samples, overwrite_existing, save, dry_run, validate_only, allow_non_sample, force_resample, _legacy_command_params=_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result, error)
        result["data"]["parent_journal_only"] = True
        (checkpoint_callback or state.checkpoint)(result)
        raise
    return _run_action(result, arguments, legacy, checkpoint_callback)
