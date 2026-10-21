"""Original nineteen-field Trail component-space demo potential writer.

Native owns all projection, callbacks, node creation, pose schema side effects,
and late failure prefixes. Each named call holds one journal; Batch borrows its
parent. No replay, cleanup, compiler/save calls or runtime validation here.
"""
import copy
import json
from typing import Any, Dict, Optional
from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_trail_demo as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'ensure_anim_graph_trail_demo': ('blueprint_name', 'trail_bone', 'base_joint', 'chain_length', 'chain_bone_axis', 'alpha', 'fake_velocity', 'graph_name', 'graph_id', 'graph_type', 'replace_existing', 'allow_non_sample', 'invert_chain_bone_axis', 'reorient_parent_to_child', 'actor_space_fake_velocity', 'relaxation_speed_scale', 'limit_stretch', 'stretch_limit', 'max_delta_time')}
_FIELD_KINDS = {'ensure_anim_graph_trail_demo': {'blueprint_name': 'string', 'trail_bone': 'string', 'base_joint': 'string', 'chain_length': 'integer', 'chain_bone_axis': 'string', 'alpha': 'number', 'fake_velocity': 'any', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'replace_existing': 'boolean', 'allow_non_sample': 'boolean', 'invert_chain_bone_axis': 'boolean', 'reorient_parent_to_child': 'boolean', 'actor_space_fake_velocity': 'boolean', 'relaxation_speed_scale': 'number', 'limit_stretch': 'boolean', 'stretch_limit': 'number', 'max_delta_time': 'number'}}


def _json(value):
    try:
        return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("anim_graph_trail_demo_input_invalid", "Original Any vector and public numbers require finite whole JSON without coercion") from error


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("anim_graph_trail_demo_input_invalid", "Only original nineteen method fields admitted")
    for field, kind in _FIELD_KINDS[method].items():
        value = public[field]
        if kind == "any":
            continue
        valid = type(value) in (int, float) if kind == "number" else type(value) is (bool if kind == "boolean" else int if kind == "integer" else str)
        if not valid:
            raise OperationError("anim_graph_trail_demo_input_invalid", "Original public type differs: " + field)
        if kind == "integer" and not -(1 << 63) <= value < (1 << 63):
            raise OperationError("anim_graph_trail_demo_input_invalid", "Safe int64 Native carrier required: " + field)
    _json(public)
    hidden = copy.deepcopy(public)
    velocity = hidden.pop("fake_velocity")
    hidden["fake_velocity_json"] = _json([0, 0, 0] if velocity is None else velocity)
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]),
        original_fake_velocity_None_normalized=[0, 0, 0],
        Native_owns_vector_float32_chain_minimum_axis_bones_and_loaded_SDK_health_admission=True,
        Native_root_position_write_separate_from_legacy_graph_changed=True,
        accepted_node_chain_array_setting_default_or_removed_link_prefix_not_rolled_back=True,
        no_Python_vector_shape_bone_axis_guard_numeric_coercion_or_field_clamp=True,
        public_chain_int64_carrier_bound_only_no_added_minimum_or_upper_clamp=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        Native_reflection_loader_cache_node_schema_effects_not_total_zero=True,
        loaded_unsafe_SDK_branch_and_original_partial_chain_authority=True,
        full_original_runtime_Trail_preview_cold_or_unique_pipeline_verified=False)


def _apply_native(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_raw_result=native["_raw_native_json"],
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, observed_state_changed=native["changed"] if known else None,
        mutation_performed=(native["changed"] if known else None) if native["mutation_attempted"] else False)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status="native_returned_not_attempted" if not native["compile_attempted"] else "native_returned_compiled" if native["compiled"] is True else "native_returned_compile_failed" if native["compiled"] is False else "native_returned_compile_unverified")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"]["blueprint_name"],
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_anim_graph_trail_demo_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
        implicit_graph_node_schema_structural_cache_work="not_collapsed_into_explicit_compile_count")
    result["stage"] = "returned_anim_graph_authoring_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["compile"].update(requested=arguments["public"].get("compile", False), attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=arguments["public"].get("save", False), attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_anim_graph_trail_demo_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"]["blueprint_name"],
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = getattr(api, arguments["method"])(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_anim_graph_authoring_operation"
            callback(result)
        if getattr(error, "code", "") == "anim_graph_trail_demo_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_graph_trail_demo_native_failed", native.get("error") or "Native AnimGraph authoring/settings/node/link/current prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("anim_graph_trail_demo_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_AnimGraph_setting_node_pose_link_prefix_and_journal_no_replay"



def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result



def ensure_anim_graph_trail_demo(blueprint_name: str, trail_bone: str='VB VBHead', base_joint: str='head', chain_length: int=2, chain_bone_axis: str='X', alpha: float=1.0, fake_velocity=None, graph_name: str='AnimGraph', graph_id: str='', graph_type: str='function', replace_existing: bool=False, allow_non_sample: bool=False, invert_chain_bone_axis: bool=False, reorient_parent_to_child: bool=True, actor_space_fake_velocity: bool=False, relaxation_speed_scale: float=1.0, limit_stretch: bool=False, stretch_limit: float=0.0, max_delta_time: float=0.0) -> Dict[str, Any]:
    try:
        arguments = _arguments('ensure_anim_graph_trail_demo', dict(blueprint_name=blueprint_name, trail_bone=trail_bone, base_joint=base_joint, chain_length=chain_length, chain_bone_axis=chain_bone_axis, alpha=alpha, fake_velocity=fake_velocity, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, replace_existing=replace_existing, allow_non_sample=allow_non_sample, invert_chain_bone_axis=invert_chain_bone_axis, reorient_parent_to_child=reorient_parent_to_child, actor_space_fake_velocity=actor_space_fake_velocity, relaxation_speed_scale=relaxation_speed_scale, limit_stretch=limit_stretch, stretch_limit=stretch_limit, max_delta_time=max_delta_time))
    except Exception as error:
        return _rejected('ensure_anim_graph_trail_demo', error)
    return _named(arguments)


def ensure_anim_graph_trail_demo_action(result, blueprint_name: str, trail_bone: str='VB VBHead', base_joint: str='head', chain_length: int=2, chain_bone_axis: str='X', alpha: float=1.0, fake_velocity=None, graph_name: str='AnimGraph', graph_id: str='', graph_type: str='function', replace_existing: bool=False, allow_non_sample: bool=False, invert_chain_bone_axis: bool=False, reorient_parent_to_child: bool=True, actor_space_fake_velocity: bool=False, relaxation_speed_scale: float=1.0, limit_stretch: bool=False, stretch_limit: float=0.0, max_delta_time: float=0.0, checkpoint_callback=None):
    _run_action(result, _arguments('ensure_anim_graph_trail_demo', dict(blueprint_name=blueprint_name, trail_bone=trail_bone, base_joint=base_joint, chain_length=chain_length, chain_bone_axis=chain_bone_axis, alpha=alpha, fake_velocity=fake_velocity, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, replace_existing=replace_existing, allow_non_sample=allow_non_sample, invert_chain_bone_axis=invert_chain_bone_axis, reorient_parent_to_child=reorient_parent_to_child, actor_space_fake_velocity=actor_space_fake_velocity, relaxation_speed_scale=relaxation_speed_scale, limit_stretch=limit_stretch, stretch_limit=stretch_limit, max_delta_time=max_delta_time)), checkpoint_callback)
