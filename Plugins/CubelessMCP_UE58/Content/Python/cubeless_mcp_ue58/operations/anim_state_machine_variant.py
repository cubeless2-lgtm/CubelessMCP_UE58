"""AnimBP state-machine variant20 with one Named or existing Batch parent journal.

Native owns source/animation/target validation, duplicate/reuse, variables,
state/player/entry/transition/rule authoring and optional compile/save/readback.
Python preserves omission/presence and actual partial receipts. It performs no
asset query, graph write, compiler, save, rollback or automatic retry.
"""
import copy
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import anim_state_machine_variant as api
from . import state
from .state import OperationError

METHOD = "ensure_anim_state_machine_sample_variant"
PUBLIC_FIELDS = ("source_anim_bp_path", "states", "transitions", "target_anim_bp_path", "target_root",
    "variant_name", "state_machine_name", "entry_state", "animation_asset_path", "connect_demo_machine_to_output",
    "replace_existing_root_link", "replace_existing_entry_link", "allow_existing_target_reuse", "overwrite_existing",
    "compile", "save_asset", "dry_run", "validate_only", "max_states", "max_transitions")
TEXT_FIELDS = ("source_anim_bp_path", "target_anim_bp_path", "target_root", "variant_name",
    "state_machine_name", "entry_state", "animation_asset_path")
BOOL_FIELDS = PUBLIC_FIELDS[9:18]
INT_FIELDS = PUBLIC_FIELDS[18:]
LEGACY_COMMAND_FIELDS = api.LEGACY_COMMAND_FIELDS


def _arguments(source_anim_bp_path, states, transitions, target_anim_bp_path, target_root,
        variant_name, state_machine_name, entry_state, animation_asset_path, connect_demo_machine_to_output,
        replace_existing_root_link, replace_existing_entry_link, allow_existing_target_reuse, overwrite_existing,
        compile, save_asset, dry_run, validate_only, max_states, max_transitions, *, _legacy_command_params=None):
    values = dict(zip(PUBLIC_FIELDS, (source_anim_bp_path, states, transitions, target_anim_bp_path, target_root,
        variant_name, state_machine_name, entry_state, animation_asset_path, connect_demo_machine_to_output,
        replace_existing_root_link, replace_existing_entry_link, allow_existing_target_reuse, overwrite_existing,
        compile, save_asset, dry_run, validate_only, max_states, max_transitions)))
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("anim_state_machine_variant_input_invalid", field + " must be a string; Native owns path/name validation")
    for field in ("states", "transitions"):
        if values[field] is not None and type(values[field]) is not list:
            raise OperationError("anim_state_machine_variant_input_invalid", field + " must be a list or None; Native owns row validation")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("anim_state_machine_variant_input_invalid", field + " must be a boolean")
    for field in INT_FIELDS:
        if type(values[field]) is not int or not -(1 << 63) <= values[field] < (1 << 63):
            raise OperationError("anim_state_machine_variant_input_invalid", field + " must be a signed int64; Native owns clamp/count validation")
    for payload in api._limited_payloads(states, transitions, _legacy_command_params):
        api._carrier(payload)
    try:
        return copy.deepcopy(values), copy.deepcopy({} if _legacy_command_params is None else _legacy_command_params)
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("anim_state_machine_variant_input_invalid", "Arguments could not be copied before dispatch") from error


def _context(result, arguments, legacy):
    states_projection, transitions_projection, private_context = api._limited_payloads(
        arguments["states"], arguments["transitions"], legacy)
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        legacy_command_params=copy.deepcopy(legacy), legacy_command_keys=list(legacy),
        native_states_projection=copy.deepcopy(states_projection),
        native_transitions_projection=copy.deepcopy(transitions_projection),
        native_private_context=copy.deepcopy(private_context),
        original_twenty_public_parameter_order_preserved=True,
        public_input_count=20, hidden_Native_input_count=21,
        Native_limited_projection2_and_context12_only=True,
        omitted_null_empty_arrays_and_raw_entry_presence_preserved=True,
        target_alias_normalized_fallback_and_shorthand_Native_authoritative=True,
        graph_selector_state_row_rule_and_player_input_semantics_Native_authoritative=True,
        limits_exact_int64_Native_clamp_and_count_validation=True,
        legacy_source_and_boolean_alias_normalization_owned_by_Batch=True,
        native_source_animation_assets_and_target_guard_authoritative=True,
        native_variables_states_players_entry_transitions_rules_compile_save_partial_authoritative=True,
        adapter_world_or_asset_queries=0, adapter_duplicate_delete_calls=0,
        adapter_variable_node_pin_or_link_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        editor_idle_admission_owned_by_native_and_registration=True,
        source_and_animation_total_unchanged_proof=False, dry_run_is_total_SDK_purity_proof=False,
        actual_named_wire_verified=False, disk_reload_verified=False,
        runtime_state_machine_pose_verified=False, packaged_runtime_verified=False,
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
        compiled=native["compiled"], status="native_anim_state_machine_variant_compile_receipt",
        completion_scope="Native_explicit_receipt; opaque_SDK_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_anim_state_machine_variant_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_anim_state_machine_variant_completion",
        native_postcondition_passed=native["postcondition_passed"],
        full_native_variables_states_players_entry_rules_compile_save_partial_prefix_preserved=True,
        save_failure_not_reinterpreted_as_success=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        disk_reload="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_load_duplicate_variables_states_players_rules_compile_or_save_inspect_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_anim_state_machine_variant_operation"

def _run_action(result, arguments, legacy, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments, legacy)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, change_state_known=True, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_anim_state_machine_variant_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, change_state_known=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=arguments["compile"], attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=arguments["save_asset"], attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_anim_state_machine_variant_operation"
    try:
        native = api.ensure_anim_state_machine_sample_variant(*(arguments[field] for field in PUBLIC_FIELDS), _legacy_command_params=legacy)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "native_call_dispatched", None) is False:
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                change_state_known=True, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_anim_state_machine_variant_operation"
        callback(result)
        raise
    _adopt(result, arguments, legacy, native)
    callback(result)  # Keep full partial/native/unknown prefix before error criteria.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_state_machine_variant_native_failed",
            native.get("error") or "Native state-machine variant failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("anim_state_machine_variant_result_unverified", "Native variant postcondition is unverified; no replay")
    result.update(success=True, stage="complete")
    return result

def _adopt_rejection(result, error):
    state.fail(result, error)
    result["stage"] = "anim_state_machine_variant_input_validation"
    result["changes"] = []
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
        change_state_known=True, mutation_state_unknown=False, journal_created=False)
    return result

def _rejected(error):
    return _adopt_rejection(state.new_result(), error)

def ensure_anim_state_machine_sample_variant(source_anim_bp_path: str, states: Optional[List[Dict[str, Any]]]=None, transitions: Optional[List[Dict[str, Any]]]=None, target_anim_bp_path: str='', target_root: str='/Game/_MCP_Sample/AnimStudy', variant_name: str='StateMachineSample', state_machine_name: str='MCP_StateMachine', entry_state: str='', animation_asset_path: str='', connect_demo_machine_to_output: bool=True, replace_existing_root_link: bool=True, replace_existing_entry_link: bool=True, allow_existing_target_reuse: bool=True, overwrite_existing: bool=False, compile: bool=True, save_asset: bool=True, dry_run: bool=False, validate_only: bool=False, max_states: int=16, max_transitions: int=32) -> Dict[str, Any]:
    try:
        arguments, legacy = _arguments(source_anim_bp_path, states, transitions, target_anim_bp_path, target_root, variant_name, state_machine_name, entry_state, animation_asset_path, connect_demo_machine_to_output, replace_existing_root_link, replace_existing_entry_link, allow_existing_target_reuse, overwrite_existing, compile, save_asset, dry_run, validate_only, max_states, max_transitions)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments, legacy))


def ensure_anim_state_machine_sample_variant_action(result, source_anim_bp_path: str, states: Optional[List[Dict[str, Any]]]=None, transitions: Optional[List[Dict[str, Any]]]=None, target_anim_bp_path: str='', target_root: str='/Game/_MCP_Sample/AnimStudy', variant_name: str='StateMachineSample', state_machine_name: str='MCP_StateMachine', entry_state: str='', animation_asset_path: str='', connect_demo_machine_to_output: bool=True, replace_existing_root_link: bool=True, replace_existing_entry_link: bool=True, allow_existing_target_reuse: bool=True, overwrite_existing: bool=False, compile: bool=True, save_asset: bool=True, dry_run: bool=False, validate_only: bool=False, max_states: int=16, max_transitions: int=32, checkpoint_callback=None, *, _legacy_command_params=None):
    try:
        arguments, legacy = _arguments(source_anim_bp_path, states, transitions, target_anim_bp_path, target_root, variant_name, state_machine_name, entry_state, animation_asset_path, connect_demo_machine_to_output, replace_existing_root_link, replace_existing_entry_link, allow_existing_target_reuse, overwrite_existing, compile, save_asset, dry_run, validate_only, max_states, max_transitions, _legacy_command_params=_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result, error)
        result["data"]["parent_journal_only"] = True
        (checkpoint_callback or state.checkpoint)(result)
        raise
    return _run_action(result, arguments, legacy, checkpoint_callback)
