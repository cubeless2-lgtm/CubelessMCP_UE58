"""One AnimGraph dry resolver or four runtime modes, with one owning journal.

Native owns selection, mapping, watches/debug state, forced ticks, temporary
clones/compilers, sampling and requested cleanup. Python keeps the full partial
receipt before terminal criteria and adds no engine query, tick, save or retry.
Root debug pointer restoration does not prove linked debug state restoration;
tick0 and a reused watch do not prove fresh capture. Separate-component sampling
is not same-instance internal pose instrumentation. Cleanup is non-atomic.
"""
import copy
import math
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import anim_node_pre_post_runtime_pose as api
from . import state
from .state import OperationError

METHOD = "sample_anim_node_pre_post_runtime_pose"
PUBLIC_FIELDS = ('blueprint_name', 'anim_blueprint', 'graph_name', 'graph_id', 'graph_type', 'node_id', 'node_type', 'title_contains', 'sample_bones', 'sample_sockets', 'mode', 'skeletal_mesh', 'temp_root', 'cleanup', 'overwrite_existing_temp_assets', 'run_id', 'actor_label', 'actor_name', 'actor_path', 'component_name', 'anim_instance_source', 'prefer_pie_world', 'require_pie_world', 'tick_count', 'tick_delta_time', 'settle_tick_count', 'refresh_bone_transforms', 'allow_missing_bones', 'pose_link_max_depth', 'input_pose_mode', 'input_pose_field_path', 'input_pose_index', 'include_pins', 'max_depth', 'dry_run')
TEXT_FIELDS = ('blueprint_name', 'anim_blueprint', 'graph_name', 'graph_id', 'graph_type', 'node_id', 'node_type', 'title_contains', 'mode', 'skeletal_mesh', 'temp_root', 'run_id', 'actor_label', 'actor_name', 'actor_path', 'component_name', 'anim_instance_source', 'input_pose_mode', 'input_pose_field_path')
BOOL_FIELDS = ('cleanup', 'overwrite_existing_temp_assets', 'prefer_pie_world', 'require_pie_world', 'refresh_bone_transforms', 'allow_missing_bones', 'include_pins', 'dry_run')
INT_FIELDS = ('tick_count', 'settle_tick_count', 'pose_link_max_depth', 'input_pose_index', 'max_depth')
SAMPLE_FIELDS = ("sample_bones", "sample_sockets")
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
                error = OperationError("anim_node_pre_post_runtime_pose_dependency_reference",
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


def _arguments(blueprint_name, anim_blueprint, graph_name, graph_id, graph_type, node_id, node_type, title_contains, sample_bones, sample_sockets, mode, skeletal_mesh, temp_root, cleanup, overwrite_existing_temp_assets, run_id, actor_label, actor_name, actor_path, component_name, anim_instance_source, prefer_pie_world, require_pie_world, tick_count, tick_delta_time, settle_tick_count, refresh_bone_transforms, allow_missing_bones, pose_link_max_depth, input_pose_mode, input_pose_field_path, input_pose_index, include_pins, max_depth, dry_run, *, _legacy_command_params=None):
    values = dict(zip(PUBLIC_FIELDS, (blueprint_name, anim_blueprint, graph_name, graph_id, graph_type, node_id, node_type, title_contains, sample_bones, sample_sockets, mode, skeletal_mesh, temp_root, cleanup, overwrite_existing_temp_assets, run_id, actor_label, actor_name, actor_path, component_name, anim_instance_source, prefer_pie_world, require_pie_world, tick_count, tick_delta_time, settle_tick_count, refresh_bone_transforms, allow_missing_bones, pose_link_max_depth, input_pose_mode, input_pose_field_path, input_pose_index, include_pins, max_depth, dry_run)))
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("anim_node_pre_post_runtime_pose_input_invalid",
                field + " must be a string; Native owns path/name/mode meaning")
    for field in BOOL_FIELDS:
        if type(values[field]) is not bool:
            raise OperationError("anim_node_pre_post_runtime_pose_input_invalid", field + " must be a boolean")
    for field in INT_FIELDS:
        value = values[field]
        if type(value) is not int or not -(1 << 63) <= value < (1 << 63):
            raise OperationError("anim_node_pre_post_runtime_pose_input_invalid", field + " must be a signed int64")
    for field in SAMPLE_FIELDS:
        value = values[field]
        if value is not None and (type(value) is not list or any(type(item) is not str for item in value)):
            raise OperationError("anim_node_pre_post_runtime_pose_input_invalid", field + " must be None or list[str]")
    delta = values["tick_delta_time"]
    if type(delta) not in (int, float):
        raise OperationError("anim_node_pre_post_runtime_pose_input_invalid", "tick_delta_time must be a finite number")
    try:
        delta = float(delta)
    except (ValueError, OverflowError) as error:
        raise OperationError("anim_node_pre_post_runtime_pose_input_invalid", "tick_delta_time exceeds Native double") from error
    if not math.isfinite(delta):
        raise OperationError("anim_node_pre_post_runtime_pose_input_invalid", "tick_delta_time must be finite")
    values["tick_delta_time"] = delta
    legacy = api._legacy_params(_legacy_command_params)
    api._payloads(values["sample_bones"], values["sample_sockets"], legacy)
    try:
        # None means Named, while a raw empty dict means graph keys were absent.
        return copy.deepcopy(values), copy.deepcopy(legacy)
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("anim_node_pre_post_runtime_pose_input_invalid", "Arguments could not be copied before dispatch") from error


def _context(result, arguments, legacy):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        legacy_command_params=copy.deepcopy(legacy), legacy_command_keys=[] if legacy is None else list(legacy),
        native_private_context=copy.deepcopy(api._private_context(legacy)),
        public_input_count=35, hidden_Native_input_count=36,
        original_thirty_five_public_parameter_order_preserved=True,
        limited_raw11_context5_only=True, private_empty_dict_distinct_from_Named_none=True,
        sample_fallback_graph_presence_input_selection_and_mode_policy_Native_authoritative=True,
        original_Named_dependency_guard_present=False,
        original_public_Batch_recursive_values_guard_owned_by_Batch=True,
        native_resolve_mapping_watch_tick_clone_compile_cleanup_partial_authoritative=True,
        native_compile_flags_include_only_observed_mode_internal_compilers=True,
        adapter_world_asset_or_pose_queries=0, adapter_debug_or_watch_calls=0,
        adapter_tick_refresh_initialize_calls=0, adapter_duplicate_delete_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        native_explicit_save_contract_zero=True, implicit_SDK_file_effects_observed=False,
        PIE_admission_owned_by_native_and_registration=True,
        dry_run_is_total_SDK_purity_proof=False, atomic_pose_snapshot_verified=False,
        tick0_fresh_capture_verified=False, linked_debug_state_fully_restored_verified=False,
        actual_named_wire_verified=False, packaged_runtime_verified=False,
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
        compiled=native["compiled"], status="native_anim_node_pre_post_runtime_pose_compile_receipt",
        completion_scope="Native_observed_mode_internal_compilers; opaque_SDK_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_anim_node_pre_post_runtime_pose_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD,
        "requested_mode": arguments["mode"], "dry_run": arguments["dry_run"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_anim_node_pre_post_runtime_pose_completion",
        native_postcondition_passed=native["postcondition_passed"],
        full_native_legacy_partial_mapping_sampling_tick_compile_cleanup_prefix_preserved=True,
        legacy_success_does_not_override_standard_cleanup_failure=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        disk_reload="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_mapping_debug_tick_clone_compile_cleanup_inspect_prefix_no_replay" if not known
            else "observed_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_anim_node_pre_post_runtime_pose_operation"


def _run_action(result, arguments, legacy, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments, legacy)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, change_state_known=True, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="explicit_save_unrequested")
    result["stage"] = "before_anim_node_pre_post_runtime_pose_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, change_state_known=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "requested_mode": arguments["mode"], "dry_run": arguments["dry_run"],
        "attempted": None, "changed": None, "change_state_known": False}]
    # The selected mode and preflight determine internal compile need in Native.
    # A dispatched exception must not be reported as zero compiler attempts.
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_anim_node_pre_post_runtime_pose_operation"
    try:
        native = api.sample_anim_node_pre_post_runtime_pose(*(arguments[field] for field in PUBLIC_FIELDS), _legacy_command_params=legacy)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "native_call_dispatched", None) is False:
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                change_state_known=True, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
        result["stage"] = "returned_anim_node_pre_post_runtime_pose_operation"
        callback(result)
        raise
    _adopt(result, arguments, legacy, native)
    callback(result)  # Persist full actual partial/cleanup receipt before errors.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_node_pre_post_runtime_pose_native_failed",
            native.get("error") or "Native AnimGraph pre/post operation failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("anim_node_pre_post_runtime_pose_result_unverified", "Native postcondition is unverified; no replay")
    result.update(success=True, stage="complete")
    return result


def _adopt_rejection(result, error):
    state.fail(result, error)
    result["stage"] = "anim_node_pre_post_runtime_pose_input_validation"
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


def sample_anim_node_pre_post_runtime_pose(blueprint_name: str = '', anim_blueprint: str = '', graph_name: str = 'AnimGraph', graph_id: str = '', graph_type: str = 'function', node_id: str = '', node_type: str = '', title_contains: str = '', sample_bones: Optional[List[str]] = None, sample_sockets: Optional[List[str]] = None, mode: str = 'active_component_tick_delta', skeletal_mesh: str = '', temp_root: str = '/Game/_MCP_Temp/AnimNodePrePost', cleanup: bool = True, overwrite_existing_temp_assets: bool = True, run_id: str = '', actor_label: str = '', actor_name: str = '', actor_path: str = '', component_name: str = '', anim_instance_source: str = 'main', prefer_pie_world: bool = True, require_pie_world: bool = False, tick_count: int = 1, tick_delta_time: float = 1.0 / 30.0, settle_tick_count: int = 0, refresh_bone_transforms: bool = True, allow_missing_bones: bool = False, pose_link_max_depth: int = 4, input_pose_mode: str = 'first', input_pose_field_path: str = '', input_pose_index: int = -1, include_pins: bool = True, max_depth: int = 3, dry_run: bool = True) -> Dict[str, Any]:
    try:
        arguments, legacy = _arguments(blueprint_name, anim_blueprint, graph_name, graph_id, graph_type, node_id, node_type, title_contains, sample_bones, sample_sockets, mode, skeletal_mesh, temp_root, cleanup, overwrite_existing_temp_assets, run_id, actor_label, actor_name, actor_path, component_name, anim_instance_source, prefer_pie_world, require_pie_world, tick_count, tick_delta_time, settle_tick_count, refresh_bone_transforms, allow_missing_bones, pose_link_max_depth, input_pose_mode, input_pose_field_path, input_pose_index, include_pins, max_depth, dry_run)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments, legacy))


def sample_anim_node_pre_post_runtime_pose_action(result, blueprint_name: str = '', anim_blueprint: str = '', graph_name: str = 'AnimGraph', graph_id: str = '', graph_type: str = 'function', node_id: str = '', node_type: str = '', title_contains: str = '', sample_bones: Optional[List[str]] = None, sample_sockets: Optional[List[str]] = None, mode: str = 'active_component_tick_delta', skeletal_mesh: str = '', temp_root: str = '/Game/_MCP_Temp/AnimNodePrePost', cleanup: bool = True, overwrite_existing_temp_assets: bool = True, run_id: str = '', actor_label: str = '', actor_name: str = '', actor_path: str = '', component_name: str = '', anim_instance_source: str = 'main', prefer_pie_world: bool = True, require_pie_world: bool = False, tick_count: int = 1, tick_delta_time: float = 1.0 / 30.0, settle_tick_count: int = 0, refresh_bone_transforms: bool = True, allow_missing_bones: bool = False, pose_link_max_depth: int = 4, input_pose_mode: str = 'first', input_pose_field_path: str = '', input_pose_index: int = -1, include_pins: bool = True, max_depth: int = 3, dry_run: bool = True, checkpoint_callback=None, *, _legacy_command_params=None):
    try:
        arguments, legacy = _arguments(blueprint_name, anim_blueprint, graph_name, graph_id, graph_type, node_id, node_type, title_contains, sample_bones, sample_sockets, mode, skeletal_mesh, temp_root, cleanup, overwrite_existing_temp_assets, run_id, actor_label, actor_name, actor_path, component_name, anim_instance_source, prefer_pie_world, require_pie_world, tick_count, tick_delta_time, settle_tick_count, refresh_bone_transforms, allow_missing_bones, pose_link_max_depth, input_pose_mode, input_pose_field_path, input_pose_index, include_pins, max_depth, dry_run, _legacy_command_params=_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result, error)
        result["data"]["parent_journal_only"] = True
        (checkpoint_callback or state.checkpoint)(result)
        raise
    return _run_action(result, arguments, legacy, checkpoint_callback)
