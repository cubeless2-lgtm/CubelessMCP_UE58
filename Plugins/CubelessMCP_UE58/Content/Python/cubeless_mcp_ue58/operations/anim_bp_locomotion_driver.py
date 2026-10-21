"""Locomotion EventGraph driver27 with one Named or existing Batch parent journal.

Native owns source/target validation, duplicate/reuse, variables, EventGraph
nodes/links/defaults, requested compile/save and topology readback. Python retains
full partial receipts and performs no asset query, write, compiler, save or retry.
"""
import copy
import math
from typing import Any, Dict

from cubeless_mcp_ue58.compatibility.ue58 import anim_bp_locomotion_driver as api
from . import state
from .state import OperationError

METHOD = "ensure_anim_bp_event_graph_locomotion_driver"
PUBLIC_FIELDS = ("source_anim_bp_path", "target_anim_bp_path", "target_root", "variant_name", "speed_variable",
    "create_variable_if_missing", "include_direction", "direction_variable", "create_direction_variable_if_missing",
    "include_is_in_air", "is_in_air_variable", "create_is_in_air_variable_if_missing", "include_acceleration",
    "acceleration_variable", "create_acceleration_variable_if_missing", "include_is_accelerating",
    "is_accelerating_variable", "create_is_accelerating_variable_if_missing", "is_accelerating_threshold",
    "guard_pawn_owner", "replace_existing_links", "allow_existing_target_reuse", "overwrite_existing",
    "compile", "save_asset", "dry_run", "validate_only")
TEXT_FIELDS = ("source_anim_bp_path", "target_anim_bp_path", "target_root", "variant_name", "speed_variable",
    "direction_variable", "is_in_air_variable", "acceleration_variable", "is_accelerating_variable")
NUMBER_FIELDS = ("is_accelerating_threshold",)
BOOL_FIELDS = tuple(field for field in PUBLIC_FIELDS if field not in TEXT_FIELDS + NUMBER_FIELDS)
LEGACY_COMMAND_FIELDS = api.LEGACY_COMMAND_FIELDS

DEPENDENCY_REFERENCE_FIELDS = ("source_anim_bp_path", "target_anim_bp_path", "target_root", "speed_variable",
    "direction_variable", "is_in_air_variable", "acceleration_variable", "is_accelerating_variable")
_FORBIDDEN_MCP_REFERENCE_TOKENS = (
    "/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal",
    "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor")


def _reject_dependency_reference(field_name, value):
    if not isinstance(value, str):
        return
    candidate = value.strip().strip("\"'").replace("\\", "/").lower()
    if candidate and any(token in candidate for token in _FORBIDDEN_MCP_REFERENCE_TOKENS):
        error = OperationError("anim_bp_locomotion_driver_dependency_reference",
            "Refusing to persist MCP-only reference in '" + field_name +
            "'. UnrealMCP is an editor authoring bridge; finished assets must not depend on it.")
        error.field_name = field_name
        error.reason = "mcp_only_reference"
        raise error


def _reject_public_dependency_references(values):
    # The original Named wrapper checks these eight strings even when an
    # optional variable is disabled. variant_name is deliberately not included.
    for field_name in DEPENDENCY_REFERENCE_FIELDS:
        _reject_dependency_reference(field_name, values[field_name])


def _reject_legacy_command_dependency_refs(params):
    # The original public execute_batch recursively checks values, not keys.
    # Root calls this for this FH command before aliases/defaults are applied.
    # Raw Native and private Action do not apply this public-surface policy.
    def visit(field_name, value):
        if isinstance(value, str):
            _reject_dependency_reference(field_name, value)
            return
        if isinstance(value, dict):
            for key, child in value.items():
                visit(field_name + "." + str(key), child)
            return
        if isinstance(value, (list, tuple, set)):
            for index, child in enumerate(value):
                visit(field_name + "[" + str(index) + "]", child)
    visit("params", params)


def _arguments(source_anim_bp_path, target_anim_bp_path, target_root, variant_name, speed_variable,
        create_variable_if_missing, include_direction, direction_variable, create_direction_variable_if_missing,
        include_is_in_air, is_in_air_variable, create_is_in_air_variable_if_missing, include_acceleration,
        acceleration_variable, create_acceleration_variable_if_missing, include_is_accelerating,
        is_accelerating_variable, create_is_accelerating_variable_if_missing, is_accelerating_threshold,
        guard_pawn_owner, replace_existing_links, allow_existing_target_reuse, overwrite_existing,
        compile, save_asset, dry_run, validate_only, *, _legacy_command_params=None):
    values = dict(zip(PUBLIC_FIELDS, (source_anim_bp_path, target_anim_bp_path, target_root, variant_name,
        speed_variable, create_variable_if_missing, include_direction, direction_variable,
        create_direction_variable_if_missing, include_is_in_air, is_in_air_variable,
        create_is_in_air_variable_if_missing, include_acceleration, acceleration_variable,
        create_acceleration_variable_if_missing, include_is_accelerating, is_accelerating_variable,
        create_is_accelerating_variable_if_missing, is_accelerating_threshold, guard_pawn_owner,
        replace_existing_links, allow_existing_target_reuse, overwrite_existing, compile,
        save_asset, dry_run, validate_only)))
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("anim_bp_locomotion_driver_input_invalid",
                field + " must be a string; Native owns path/name validation")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("anim_bp_locomotion_driver_input_invalid", field + " must be a boolean")
    threshold = values["is_accelerating_threshold"]
    if type(threshold) not in (int, float):
        raise OperationError("anim_bp_locomotion_driver_input_invalid", "is_accelerating_threshold must be a finite number")
    try:
        threshold = float(threshold)
    except (ValueError, OverflowError) as error:
        raise OperationError("anim_bp_locomotion_driver_input_invalid", "is_accelerating_threshold exceeds Native double") from error
    if not math.isfinite(threshold):
        raise OperationError("anim_bp_locomotion_driver_input_invalid", "is_accelerating_threshold must be finite")
    values["is_accelerating_threshold"] = threshold
    api._carrier(api._private_context(_legacy_command_params))
    try:
        return copy.deepcopy(values), copy.deepcopy({} if _legacy_command_params is None else _legacy_command_params)
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("anim_bp_locomotion_driver_input_invalid", "Arguments could not be copied before dispatch") from error


def _context(result, arguments, legacy):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        legacy_command_params=copy.deepcopy(legacy), legacy_command_keys=list(legacy),
        native_private_context=copy.deepcopy(api._private_context(legacy)),
        original_twenty_seven_public_parameter_order_preserved=True,
        public_input_count=27, hidden_Native_input_count=28,
        Native_limited_target_alias_context1_only=True,
        target_alias_raw_presence_and_normalized_fallback_Native_authoritative=True,
        public_optional_create_flags_default_true_preserved=True,
        hidden_omitted_create_flags_global_inheritance_owned_by_Batch=True,
        legacy_source_and_save_alias_normalization_owned_by_Batch=True,
        threshold_finite_double_Native_clamp_authoritative=True,
        native_source_target_duplicate_variables_graph_links_compile_save_partial_authoritative=True,
        adapter_world_or_asset_queries=0, adapter_duplicate_delete_calls=0,
        adapter_variable_node_pin_or_link_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        editor_idle_admission_owned_by_native_and_registration=True,
        source_total_unchanged_proof=False, dry_run_is_total_SDK_purity_proof=False,
        actual_named_wire_verified=False, disk_reload_verified=False,
        runtime_locomotion_pose_verified=False, packaged_runtime_verified=False,
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
        compiled=native["compiled"], status="native_anim_bp_locomotion_driver_compile_receipt",
        completion_scope="Native_explicit_receipt; opaque_SDK_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_anim_bp_locomotion_driver_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_anim_bp_locomotion_driver_completion",
        native_postcondition_passed=native["postcondition_passed"],
        full_native_variables_nodes_links_warnings_errors_compile_save_readback_partial_prefix_preserved=True,
        save_failure_not_reinterpreted_as_success=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        disk_reload="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_load_duplicate_variables_graph_compile_or_save_inspect_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_anim_bp_locomotion_driver_operation"


def _run_action(result, arguments, legacy, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments, legacy)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, change_state_known=True, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_anim_bp_locomotion_driver_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, change_state_known=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_targets": {field: arguments[field] for field in TEXT_FIELDS},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=arguments["compile"], attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=arguments["save_asset"], attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_anim_bp_locomotion_driver_operation"
    try:
        native = api.ensure_anim_bp_event_graph_locomotion_driver(*(arguments[field] for field in PUBLIC_FIELDS), _legacy_command_params=legacy)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "native_call_dispatched", None) is False:
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                change_state_known=True, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=arguments["save_asset"], attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_anim_bp_locomotion_driver_operation"
        callback(result)
        raise
    _adopt(result, arguments, legacy, native)
    callback(result)  # Keep the full actual prefix before terminal error criteria.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_bp_locomotion_driver_native_failed",
            native.get("error") or "Native locomotion EventGraph driver failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("anim_bp_locomotion_driver_result_unverified", "Native driver postcondition is unverified; no replay")
    result.update(success=True, stage="complete")
    return result


def _adopt_rejection(result, error):
    state.fail(result, error)
    result["stage"] = "anim_bp_locomotion_driver_input_validation"
    result["changes"] = []
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
        change_state_known=True, mutation_state_unknown=False, journal_created=False)
    return result


def _rejected(error):
    return _adopt_rejection(state.new_result(), error)


def ensure_anim_bp_event_graph_locomotion_driver(source_anim_bp_path: str, target_anim_bp_path: str="", target_root: str="/Game/_MCP_Sample/AnimStudy", variant_name: str="LocomotionEventGraph", speed_variable: str="Speed", create_variable_if_missing: bool=True, include_direction: bool=False, direction_variable: str="Direction", create_direction_variable_if_missing: bool=True, include_is_in_air: bool=False, is_in_air_variable: str="IsInAir", create_is_in_air_variable_if_missing: bool=True, include_acceleration: bool=False, acceleration_variable: str="Acceleration", create_acceleration_variable_if_missing: bool=True, include_is_accelerating: bool=False, is_accelerating_variable: str="IsAccelerating", create_is_accelerating_variable_if_missing: bool=True, is_accelerating_threshold: float=3.0, guard_pawn_owner: bool=True, replace_existing_links: bool=False, allow_existing_target_reuse: bool=True, overwrite_existing: bool=False, compile: bool=True, save_asset: bool=True, dry_run: bool=False, validate_only: bool=False) -> Dict[str, Any]:
    try:
        _reject_public_dependency_references(locals())
        arguments, legacy = _arguments(source_anim_bp_path, target_anim_bp_path, target_root, variant_name, speed_variable, create_variable_if_missing, include_direction, direction_variable, create_direction_variable_if_missing, include_is_in_air, is_in_air_variable, create_is_in_air_variable_if_missing, include_acceleration, acceleration_variable, create_acceleration_variable_if_missing, include_is_accelerating, is_accelerating_variable, create_is_accelerating_variable_if_missing, is_accelerating_threshold, guard_pawn_owner, replace_existing_links, allow_existing_target_reuse, overwrite_existing, compile, save_asset, dry_run, validate_only)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments, legacy))


def ensure_anim_bp_event_graph_locomotion_driver_action(result, source_anim_bp_path: str, target_anim_bp_path: str="", target_root: str="/Game/_MCP_Sample/AnimStudy", variant_name: str="LocomotionEventGraph", speed_variable: str="Speed", create_variable_if_missing: bool=True, include_direction: bool=False, direction_variable: str="Direction", create_direction_variable_if_missing: bool=True, include_is_in_air: bool=False, is_in_air_variable: str="IsInAir", create_is_in_air_variable_if_missing: bool=True, include_acceleration: bool=False, acceleration_variable: str="Acceleration", create_acceleration_variable_if_missing: bool=True, include_is_accelerating: bool=False, is_accelerating_variable: str="IsAccelerating", create_is_accelerating_variable_if_missing: bool=True, is_accelerating_threshold: float=3.0, guard_pawn_owner: bool=True, replace_existing_links: bool=False, allow_existing_target_reuse: bool=True, overwrite_existing: bool=False, compile: bool=True, save_asset: bool=True, dry_run: bool=False, validate_only: bool=False, checkpoint_callback=None, *, _legacy_command_params=None):
    try:
        arguments, legacy = _arguments(source_anim_bp_path, target_anim_bp_path, target_root, variant_name, speed_variable, create_variable_if_missing, include_direction, direction_variable, create_direction_variable_if_missing, include_is_in_air, is_in_air_variable, create_is_in_air_variable_if_missing, include_acceleration, acceleration_variable, create_acceleration_variable_if_missing, include_is_accelerating, is_accelerating_variable, create_is_accelerating_variable_if_missing, is_accelerating_threshold, guard_pawn_owner, replace_existing_links, allow_existing_target_reuse, overwrite_existing, compile, save_asset, dry_run, validate_only, _legacy_command_params=_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result, error)
        result["data"]["parent_journal_only"] = True
        (checkpoint_callback or state.checkpoint)(result)
        raise
    return _run_action(result, arguments, legacy, checkpoint_callback)
