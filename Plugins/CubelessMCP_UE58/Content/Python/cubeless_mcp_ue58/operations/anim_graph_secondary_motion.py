"""Original SecondaryMotion19 potential writer with Native-owned compile/save.

Named calls hold one journal; Batch borrows its parent. Loaded SDK/reflection,
four pose links, compilation, current GUID/settings proof and requested save
remain Native authority. There is no consumer asset work or writer replay.
"""
import copy
import json
from typing import Any, Dict, List, Optional
from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_secondary_motion as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'ensure_anim_graph_secondary_motion_chain': ('blueprint_name', 'dynamics_type', 'root_bone', 'chain_end_bone', 'alpha', 'gravity', 'angular_limit_degrees', 'body_extents', 'linear_damping', 'angular_damping', 'kawaii_damping', 'kawaii_stiffness', 'graph_name', 'graph_id', 'graph_type', 'replace_existing', 'allow_non_sample', 'compile', 'save')}
_FIELD_KINDS = {'ensure_anim_graph_secondary_motion_chain': {'blueprint_name': 'string', 'dynamics_type': 'string', 'root_bone': 'string', 'chain_end_bone': 'string', 'alpha': 'number', 'gravity': 'nullable_number_list', 'angular_limit_degrees': 'number', 'body_extents': 'nullable_number_list', 'linear_damping': 'number', 'angular_damping': 'number', 'kawaii_damping': 'number', 'kawaii_stiffness': 'number', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'replace_existing': 'boolean', 'allow_non_sample': 'boolean', 'compile': 'boolean', 'save': 'boolean'}}


def _json(value):
    try:
        return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("anim_graph_secondary_motion_input_invalid", "Original typed lists and public numbers require finite whole JSON without coercion") from error


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("anim_graph_secondary_motion_input_invalid", "Only original nineteen method fields admitted")
    for field, kind in _FIELD_KINDS[method].items():
        value = public[field]
        if kind == "nullable_number_list":
            valid = value is None or type(value) is list and all(type(member) in (int, float) for member in value)
        elif kind == "number":
            valid = type(value) in (int, float)
        else:
            valid = type(value) is (bool if kind == "boolean" else str)
        if not valid:
            raise OperationError("anim_graph_secondary_motion_input_invalid", "Original public type differs: " + field)
    normalized_type = public["dynamics_type"].strip().lower()
    if normalized_type not in {"anim_dynamics", "animdynamics", "kawaii_physics", "kawaii"}:
        raise OperationError("secondary_motion_invalid_dynamics_type", "dynamics_type must be anim_dynamics or kawaii_physics")
    _json(public)
    hidden = {}
    for field in _PUBLIC_FIELDS[method]:
        value = public[field]
        if field == "dynamics_type":
            hidden[field] = normalized_type
        elif field == "gravity":
            hidden["gravity_json"] = _json([0.0, 0.0, -980.0] if value is None else value)
        elif field == "body_extents":
            hidden["body_extents_json"] = _json([5.0, 5.0, 30.0] if value is None else value)
        else:
            hidden[field] = copy.deepcopy(value)
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]),
        original_dynamics_type_strip_lower_aliases=True,
        original_gravity_None_normalized=[0.0, 0.0, -980.0],
        original_body_extents_None_normalized=[5.0, 5.0, 30.0],
        Native_owns_vector_shape_float32_field_clamps_bone_ancestry_and_loaded_SDK_health=True,
        Native_compiler_fresh_GUID_current_semantic_proof_and_requested_save_authoritative=True,
        Native_optional_Kawaii_branch_retained_availability_not_assumed=True,
        accepted_node_setting_default_array_broken_link_compile_or_save_prefix_not_rolled_back=True,
        no_Python_geometry_bone_axis_guard_numeric_coercion_or_field_clamp=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        Native_reflection_loader_cache_node_schema_effects_not_total_zero=True,
        full_original_secondary_motion_runtime_preview_cold_or_unique_pipeline_verified=False)


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
    result["validation"].update(status="native_anim_graph_secondary_motion_postcondition", native_postcondition_passed=native["postcondition_passed"],
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
    result["stage"] = "native_anim_graph_secondary_motion_operation"
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
        if getattr(error, "code", "") == "anim_graph_secondary_motion_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_graph_secondary_motion_native_failed", native.get("error") or "Native AnimGraph authoring/settings/node/link/current prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("anim_graph_secondary_motion_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_AnimGraph_setting_node_pose_link_prefix_and_journal_no_replay"


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def ensure_anim_graph_secondary_motion_chain(blueprint_name: str, dynamics_type: str, root_bone: str, chain_end_bone: str, alpha: float=1.0, gravity: Optional[List[float]]=None, angular_limit_degrees: float=60.0, body_extents: Optional[List[float]]=None, linear_damping: float=0.75, angular_damping: float=0.75, kawaii_damping: float=0.08, kawaii_stiffness: float=0.03, graph_name: str='AnimGraph', graph_id: str='', graph_type: str='function', replace_existing: bool=True, allow_non_sample: bool=False, compile: bool=True, save: bool=True) -> Dict[str, Any]:
    try:
        arguments = _arguments('ensure_anim_graph_secondary_motion_chain', dict(blueprint_name=blueprint_name, dynamics_type=dynamics_type, root_bone=root_bone, chain_end_bone=chain_end_bone, alpha=alpha, gravity=gravity, angular_limit_degrees=angular_limit_degrees, body_extents=body_extents, linear_damping=linear_damping, angular_damping=angular_damping, kawaii_damping=kawaii_damping, kawaii_stiffness=kawaii_stiffness, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, replace_existing=replace_existing, allow_non_sample=allow_non_sample, compile=compile, save=save))
    except Exception as error:
        return _rejected('ensure_anim_graph_secondary_motion_chain', error)
    return _named(arguments)


def ensure_anim_graph_secondary_motion_chain_action(result, blueprint_name: str, dynamics_type: str, root_bone: str, chain_end_bone: str, alpha: float=1.0, gravity: Optional[List[float]]=None, angular_limit_degrees: float=60.0, body_extents: Optional[List[float]]=None, linear_damping: float=0.75, angular_damping: float=0.75, kawaii_damping: float=0.08, kawaii_stiffness: float=0.03, graph_name: str='AnimGraph', graph_id: str='', graph_type: str='function', replace_existing: bool=True, allow_non_sample: bool=False, compile: bool=True, save: bool=True, checkpoint_callback=None):
    _run_action(result, _arguments('ensure_anim_graph_secondary_motion_chain', dict(blueprint_name=blueprint_name, dynamics_type=dynamics_type, root_bone=root_bone, chain_end_bone=chain_end_bone, alpha=alpha, gravity=gravity, angular_limit_degrees=angular_limit_degrees, body_extents=body_extents, linear_damping=linear_damping, angular_damping=angular_damping, kawaii_damping=kawaii_damping, kawaii_stiffness=kawaii_stiffness, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, replace_existing=replace_existing, allow_non_sample=allow_non_sample, compile=compile, save=save)), checkpoint_callback)
