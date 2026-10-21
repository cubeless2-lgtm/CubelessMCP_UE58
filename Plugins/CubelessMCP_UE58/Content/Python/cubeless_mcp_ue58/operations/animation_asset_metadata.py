"""Original metadata observer17; Native nested diagnostics and actual prefix remain authoritative."""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import animation_asset_metadata as api
from . import state
from .state import OperationError

METHOD = "inspect_animation_asset_metadata_guarded"


def _arguments(asset_path,
        include_notifies,
        include_notify_details,
        include_curves,
        include_curve_keys,
        include_sync_markers,
        include_montage_sections,
        include_montage_slots,
        include_metadata,
        max_notifies,
        max_curves,
        max_curve_keys_per_curve,
        max_markers,
        max_sections,
        max_slots,
        max_segments_per_slot,
        require_supported_class):
    if type(asset_path) is not str:
        raise OperationError("animation_asset_metadata_input_invalid", "Asset path must be a literal string; Native owns normalization and loading")
    if any(type(value) is not bool for value in (include_notifies, include_notify_details, include_curves, include_curve_keys, include_sync_markers, include_montage_sections, include_montage_slots, include_metadata, require_supported_class)):
        raise OperationError("animation_asset_metadata_input_invalid", "Original include and supported-class flags must be booleans")
    if any(type(value) is not int or not -(2**63) <= value < 2**63 for value in (max_notifies, max_curves, max_curve_keys_per_curve, max_markers, max_sections, max_slots, max_segments_per_slot)):
        raise OperationError("animation_asset_metadata_input_invalid", "Limits must fit the private int64 carriers; Native owns the original lower-zero clamps")
    return dict(asset_path=asset_path,
        include_notifies=include_notifies,
        include_notify_details=include_notify_details,
        include_curves=include_curves,
        include_curve_keys=include_curve_keys,
        include_sync_markers=include_sync_markers,
        include_montage_sections=include_montage_sections,
        include_montage_slots=include_montage_slots,
        include_metadata=include_metadata,
        max_notifies=max_notifies,
        max_curves=max_curves,
        max_curve_keys_per_curve=max_curve_keys_per_curve,
        max_markers=max_markers,
        max_sections=max_sections,
        max_slots=max_slots,
        max_segments_per_slot=max_segments_per_slot,
        require_supported_class=require_supported_class)


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        adapter_requested_observer=True, named_mutation_journal_created=False,
        native_load_PostLoad_notify_name_cache_effects_not_global_purity_proof=True,
        const_preview_getter_repair_selected_without_clearing_authored_preview=True,
        native_original_limits_truncation_counts_and_nested_payload_authoritative=True,
        unsupported_require_false_is_completed_diagnostic=True,
        warnings_errors_and_unavailable_fields_not_blanket_failure_predicates=True,
        automatic_action_retry=False, full_original_contract_complete=False,
        cold_persistence_verified=False, animation_evaluation_or_notify_handlers_verified=False,
        external_original_pipeline_complete=False)


def _adopt(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items()
            if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_observer_returned")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_observer_returned")
    result["changes"] = [] if known and native["changed"] is False and not native["mutation_attempted"] else [
        {"kind": "animation_asset_metadata_observation_scope", "attempted": native["mutation_attempted"],
            "change_state_known": known, "changed": native["changed"] if known else None,
            "outcome": "current_scoped_prefix" if known else "unknown_after_dispatch"}]
    result["stage"] = "returned_animation_asset_metadata_observation"
    result["validation"].update(status="native_animation_asset_metadata_postcondition",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        packaged_runtime="not_run", initial_load_effects="not_fully_instrumented")


def inspect_animation_asset_metadata_guarded_action(result, asset_path,
        include_notifies=True,
        include_notify_details=True,
        include_curves=True,
        include_curve_keys=False,
        include_sync_markers=True,
        include_montage_sections=True,
        include_montage_slots=True,
        include_metadata=False,
        max_notifies=256,
        max_curves=256,
        max_curve_keys_per_curve=0,
        max_markers=256,
        max_sections=128,
        max_slots=64,
        max_segments_per_slot=256,
        require_supported_class=False, checkpoint_callback=None):
    arguments = _arguments(asset_path,
            include_notifies,
            include_notify_details,
            include_curves,
            include_curve_keys,
            include_sync_markers,
            include_montage_sections,
            include_montage_slots,
            include_metadata,
            max_notifies,
            max_curves,
            max_curve_keys_per_curve,
            max_markers,
            max_sections,
            max_slots,
            max_segments_per_slot,
            require_supported_class)
    _context(result, arguments)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        mutation_state_unknown=True, mutation_performed=None)
    result["changes"] = [{"kind": "animation_asset_metadata_observation_scope", "attempted": None,
        "change_state_known": False, "changed": None, "outcome": "unknown_until_native_returns"}]
    result["compile"].update(requested=False, attempted=None, compiled=None, status="unknown_until_native_returns")
    result["save"].update(requested=False, attempted=None, saved=None, status="unknown_until_native_returns")
    result["stage"] = "native_animation_asset_metadata_observation"
    try:
        native = api.inspect_animation_asset_metadata_guarded(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_animation_asset_metadata_observation"
            if checkpoint_callback:
                checkpoint_callback(result)
        if getattr(error, "code", "") == "native_animation_asset_metadata_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_not_dispatched")
        raise
    _adopt(result, arguments, native)  # Actual nested data/raw/flags precede every criterion.
    if checkpoint_callback:
        checkpoint_callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_animation_asset_metadata_failed",
            native.get("error") or "Native animation metadata prefix retained")
    if native["mutation_attempted"] or native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("animation_asset_metadata_unexpected_work", "Unexpected direct work retained; no second observation")
    if not native["change_state_known"] or native["changed"] is not False or not native["postcondition_passed"]:
        raise OperationError("animation_asset_metadata_observation_unverified", "Current selected read proof incomplete; no replay")
    # Unsupported requireFalse can carry legacy errors[] and still be a valid diagnostic success.
    result.update(success=True, stage="complete")
    return result


def inspect_animation_asset_metadata_guarded(asset_path,
        include_notifies=True,
        include_notify_details=True,
        include_curves=True,
        include_curve_keys=False,
        include_sync_markers=True,
        include_montage_sections=True,
        include_montage_slots=True,
        include_metadata=False,
        max_notifies=256,
        max_curves=256,
        max_curve_keys_per_curve=0,
        max_markers=256,
        max_sections=128,
        max_slots=64,
        max_segments_per_slot=256,
        require_supported_class=False):
    result = state.new_result()
    result["data"].update(operation=METHOD, native_dispatched=False, native_result_returned=False,
        mutation_state_unknown=False, mutation_performed=False)
    try:
        inspect_animation_asset_metadata_guarded_action(result, asset_path,
            include_notifies,
            include_notify_details,
            include_curves,
            include_curve_keys,
            include_sync_markers,
            include_montage_sections,
            include_montage_slots,
            include_metadata,
            max_notifies,
            max_curves,
            max_curve_keys_per_curve,
            max_markers,
            max_sections,
            max_slots,
            max_segments_per_slot,
            require_supported_class)
        result.update(success=True, stage="complete")
    except Exception as error:
        state.fail(result, error)
    return result
