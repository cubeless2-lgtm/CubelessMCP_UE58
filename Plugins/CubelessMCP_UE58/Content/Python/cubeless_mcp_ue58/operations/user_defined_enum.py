"""Enum creation6 receipts in one Named journal or the existing Batch parent.

Native owns paths, label normalization/duplicates, existing-asset behavior,
planned/created entries and auto MAX, factory/discard and actual package saving.
Python does not load, edit, compile, save or inspect Engine assets itself.
"""
import copy
from typing import Any, Dict

from cubeless_mcp_ue58.compatibility.ue58 import user_defined_enum as api
from . import state
from .state import OperationError

METHOD = "create_user_defined_enum"
PUBLIC_FIELDS = ("asset_path", "display_names", "tooltips", "dry_run", "save", "skip_if_exists")


def _arguments(asset_path, display_names, tooltips, dry_run, save, skip_if_exists):
    if type(asset_path) is not str:
        raise OperationError("user_defined_enum_input_invalid", "asset_path must be a string")
    if type(display_names) is not list or not 1 <= len(display_names) <= 32 or any(type(value) is not str for value in display_names):
        raise OperationError("user_defined_enum_input_invalid", "display_names must be an ordered list of1..32 strings")
    if tooltips is not None and (type(tooltips) is not list or len(tooltips) != len(display_names)
            or any(type(value) is not str for value in tooltips)):
        raise OperationError("user_defined_enum_input_invalid", "tooltips must be None or a string list matching display_names; [] is not omission")
    if type(dry_run) is not bool or type(save) is not bool or type(skip_if_exists) is not bool:
        raise OperationError("user_defined_enum_input_invalid", "dry_run, save and skip_if_exists must be booleans")
    # Native retains authority for /Game/MCP paths, trim, case-insensitive duplicate
    # checks, deterministic names/ordinals/MAX and fresh-only package ownership.
    values = dict(asset_path=asset_path, display_names=copy.deepcopy(display_names), tooltips=copy.deepcopy(tooltips),
        dry_run=dry_run, save=save, skip_if_exists=skip_if_exists)
    # Serialization admission precedes journal creation and any possible Native dispatch.
    api._carrier(values["display_names"])
    api._carrier(values["tooltips"])
    return values


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_six_public_parameters_forwarded_unchanged=True,
        nullable_tooltips_preserve_none_and_empty=True,
        native_path_label_existing_entries_MAX_factory_discard_and_save_authoritative=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        adapter_structural_rollback_calls=0,
        dry_run_is_total_SDK_purity_proof=False,
        skipped_existing_proves_requested_labels_equivalent=False,
        conditional_fresh_discard_is_atomic_rollback=False,
        explicit_compile_flags_prove_implicit_Blueprint_completion=False,
        actual_named_array_nullable_wire_verified=False,
        automatic_action_retry=False, atomic_rollback=False, unique_external_pipeline_complete=False)


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
        compiled=native["compiled"], status="native_user_defined_enum_compile_receipt",
        completion_scope="Native_explicit_receipt; implicit_PostEditChange_or_dependent_Blueprint_callbacks_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_user_defined_enum_package_save_receipt", save_calls=copy.deepcopy(native["data"].get("save_calls")),
        dry_run=arguments["dry_run"], skipped_existing=native["data"].get("skipped_existing"))
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD, "requested_asset_path": arguments["asset_path"],
        "dry_run": arguments["dry_run"], "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None, "created": native["data"].get("created"),
        "skipped_existing": native["data"].get("skipped_existing"),
        "observed_explicit_change": native["data"].get("observed_explicit_change"),
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_user_defined_enum_entries_MAX_and_optional_save_postcondition",
        native_postcondition_passed=native["postcondition_passed"], native_verification_pass=native["data"].get("verification_pass"),
        entry_count=native["data"].get("entry_count"), readback_source=native["data"].get("readback_source"),
        actual_native_entries=copy.deepcopy(native["data"].get("entries")), total_change_state_known=known,
        all_implicit_SDK_effects_observed=False, full_discard_restoration_inferred=False,
        skipped_existing_requested_label_equivalence="not_checked_by_original_API",
        disk_reload="not_run", packaged_runtime="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_existing_load_factory_discard_or_save_outcome_inspect_actual_asset_package_files_and_journal_no_replay" if not known
            else "fresh_enum_or_partial_package_may_already_exist_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_user_defined_enum_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["save"], attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_user_defined_enum_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_asset_path": arguments["asset_path"], "dry_run": arguments["dry_run"],
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_user_defined_enum_operation"
    try:
        native = api.create_user_defined_enum(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "user_defined_enum_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_user_defined_enum_operation"
        callback(result) # Preserve the returned/malformed/exception or unavailable prefix before raising.
        raise
    _adopt(result, arguments, native)
    callback(result) # Actual creation/discard/entries/compile/save/raw/unknown prefix precedes errors.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "user_defined_enum_native_failed",
            native.get("error") or "Native enum creation failed; actual partial factory/discard/save prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("user_defined_enum_result_unverified", "Native enum entries/MAX/save postcondition is unverified; no replay")
    # Successful dry-run/readback/creation can coexist with unknown total SDK effects.
    # Keep null changes and force subsequent Batch work to stop instead of replaying.
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["stage"] = "user_defined_enum_input_validation"
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False, journal_created=False)
    return result


def create_user_defined_enum(asset_path: str, display_names: list[str], tooltips: list[str] | None = None,
        dry_run: bool = True, save: bool = False, skip_if_exists: bool = False) -> Dict[str, Any]:
    try:
        arguments = _arguments(asset_path, display_names, tooltips, dry_run, save, skip_if_exists)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def create_user_defined_enum_action(result, asset_path: str, display_names: list[str], tooltips: list[str] | None = None,
        dry_run: bool = True, save: bool = False, skip_if_exists: bool = False, checkpoint_callback=None):
    return _run_action(result, _arguments(asset_path, display_names, tooltips, dry_run, save, skip_if_exists), checkpoint_callback)
