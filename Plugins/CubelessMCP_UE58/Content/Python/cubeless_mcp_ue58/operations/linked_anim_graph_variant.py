"""Linked AnimGraph variant14 with one Named or existing Batch parent journal.

Native owns source/linked validation, target duplicate/reuse, class attachment,
node reconstruction, links and optional compile/save with full partial results.
Python performs no asset query, graph write, compiler, save, rollback or retry.
"""
import copy
from typing import Any, Dict

from cubeless_mcp_ue58.compatibility.ue58 import linked_anim_graph_variant as api
from . import state
from .state import OperationError

METHOD = "ensure_linked_anim_graph_sample_variant"
PUBLIC_FIELDS = ("source_anim_bp_path", "linked_anim_bp_path", "target_anim_bp_path", "target_root",
    "variant_name", "insert_mode", "allow_existing_target_reuse", "overwrite_existing", "replace_existing_links",
    "require_input_pose_pin", "compile", "save_asset", "dry_run", "validate_only")
TEXT_FIELDS = PUBLIC_FIELDS[:6]
BOOL_FIELDS = PUBLIC_FIELDS[6:]
LEGACY_COMMAND_FIELDS = api.LEGACY_COMMAND_FIELDS


def _arguments(source_anim_bp_path, linked_anim_bp_path, target_anim_bp_path, target_root,
        variant_name, insert_mode, allow_existing_target_reuse, overwrite_existing, replace_existing_links,
        require_input_pose_pin, compile, save_asset, dry_run, validate_only, *, _legacy_command_params=None):
    values = dict(zip(PUBLIC_FIELDS, (source_anim_bp_path, linked_anim_bp_path, target_anim_bp_path, target_root,
        variant_name, insert_mode, allow_existing_target_reuse, overwrite_existing, replace_existing_links,
        require_input_pose_pin, compile, save_asset, dry_run, validate_only)))
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("linked_anim_graph_variant_input_invalid", field + " must be a string; Native owns path/mode validation")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("linked_anim_graph_variant_input_invalid", field + " must be a boolean")
    context = api._private_graph_context(_legacy_command_params)
    api._carrier(context)
    legacy = {} if _legacy_command_params is None else _legacy_command_params
    try:
        # Layer values are deliberately irrelevant: the original command rejects
        # key presence before reading them. Preserve presence without serializing
        # ignored values; the Batch parent retains its original command parameters.
        narrowed = {key: copy.deepcopy(legacy[key]) for key in api.GRAPH_FIELDS + api.TARGET_ALIAS_FIELDS if key in legacy}
        narrowed.update({key: None for key in api.UNSUPPORTED_LAYER_FIELDS if key in legacy})
        return copy.deepcopy(values), narrowed
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("linked_anim_graph_variant_input_invalid", "Arguments could not be copied before dispatch") from error


def _context(result, arguments, legacy):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        legacy_command_params=copy.deepcopy(legacy), legacy_command_keys=list(legacy),
        private_graph_context=api._private_graph_context(legacy),
        original_fourteen_public_parameter_order_preserved=True,
        public_input_count=14, hidden_Native_input_count=15,
        Native_limited_private_context_JSON_only=True,
        graph_selector_presence_raw_JSON_and_Layer_key_presence_preserved=True,
        target_alias_presence_and_normalized_fallback_Native_authoritative=True,
        ignored_Layer_values_not_interpreted=True,
        legacy_source_and_boolean_alias_normalization_owned_by_Batch=True,
        native_source_linked_skeleton_generated_class_and_target_guard_authoritative=True,
        native_duplicate_reuse_reconstruct_links_compile_save_and_partial_authoritative=True,
        adapter_world_or_asset_queries=0, adapter_duplicate_delete_calls=0,
        adapter_node_class_reconstruct_or_link_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        editor_idle_admission_owned_by_native_and_registration=True,
        source_and_linked_total_unchanged_proof=False, dry_run_is_total_SDK_purity_proof=False,
        actual_named_wire_verified=False, disk_reload_verified=False,
        runtime_linked_pose_verified=False, packaged_runtime_verified=False,
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
        compiled=native["compiled"], status="native_linked_anim_graph_variant_compile_receipt",
        completion_scope="Native_explicit_receipt; opaque_SDK_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_linked_anim_graph_variant_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_linked_anim_graph_variant_completion",
        native_postcondition_passed=native["postcondition_passed"],
        full_native_target_duplicate_class_reconstruct_links_compile_save_and_partial_prefix_preserved=True,
        save_failure_not_reinterpreted_as_success=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        disk_reload="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_load_duplicate_class_reconstruct_links_compile_or_save_inspect_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_linked_anim_graph_variant_operation"

def _run_action(result, arguments, legacy, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments, legacy)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, change_state_known=True, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_linked_anim_graph_variant_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, change_state_known=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=arguments["compile"], attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=arguments["save_asset"], attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_linked_anim_graph_variant_operation"
    try:
        native = api.ensure_linked_anim_graph_sample_variant(*(arguments[field] for field in PUBLIC_FIELDS), _legacy_command_params=legacy)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "native_call_dispatched", None) is False:
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                change_state_known=True, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_linked_anim_graph_variant_operation"
        callback(result)
        raise
    _adopt(result, arguments, legacy, native)
    callback(result)  # Keep full partial/native/unknown prefix before error criteria.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "linked_anim_graph_variant_native_failed",
            native.get("error") or "Native linked AnimGraph variant failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("linked_anim_graph_variant_result_unverified", "Native variant postcondition is unverified; no replay")
    result.update(success=True, stage="complete")
    return result

def _adopt_rejection(result, error):
    state.fail(result, error)
    result["stage"] = "linked_anim_graph_variant_input_validation"
    result["changes"] = []
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
        change_state_known=True, mutation_state_unknown=False, journal_created=False)
    return result

def _rejected(error):
    return _adopt_rejection(state.new_result(), error)

def ensure_linked_anim_graph_sample_variant(source_anim_bp_path: str, linked_anim_bp_path: str, target_anim_bp_path: str='', target_root: str='/Game/_MCP_Sample/AnimStudy', variant_name: str='LinkedAnimGraphSample', insert_mode: str='input_pose_to_linked_graph_to_root', allow_existing_target_reuse: bool=True, overwrite_existing: bool=False, replace_existing_links: bool=True, require_input_pose_pin: bool=False, compile: bool=True, save_asset: bool=False, dry_run: bool=False, validate_only: bool=False) -> Dict[str, Any]:
    try:
        arguments, legacy = _arguments(source_anim_bp_path, linked_anim_bp_path, target_anim_bp_path, target_root, variant_name, insert_mode, allow_existing_target_reuse, overwrite_existing, replace_existing_links, require_input_pose_pin, compile, save_asset, dry_run, validate_only)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments, legacy))


def ensure_linked_anim_graph_sample_variant_action(result, source_anim_bp_path: str, linked_anim_bp_path: str, target_anim_bp_path: str='', target_root: str='/Game/_MCP_Sample/AnimStudy', variant_name: str='LinkedAnimGraphSample', insert_mode: str='input_pose_to_linked_graph_to_root', allow_existing_target_reuse: bool=True, overwrite_existing: bool=False, replace_existing_links: bool=True, require_input_pose_pin: bool=False, compile: bool=True, save_asset: bool=False, dry_run: bool=False, validate_only: bool=False, checkpoint_callback=None, *, _legacy_command_params=None):
    try:
        arguments, legacy = _arguments(source_anim_bp_path, linked_anim_bp_path, target_anim_bp_path, target_root, variant_name, insert_mode, allow_existing_target_reuse, overwrite_existing, replace_existing_links, require_input_pose_pin, compile, save_asset, dry_run, validate_only, _legacy_command_params=_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result, error)
        result["data"]["parent_journal_only"] = True
        (checkpoint_callback or state.checkpoint)(result)
        raise
    return _run_action(result, arguments, legacy, checkpoint_callback)
