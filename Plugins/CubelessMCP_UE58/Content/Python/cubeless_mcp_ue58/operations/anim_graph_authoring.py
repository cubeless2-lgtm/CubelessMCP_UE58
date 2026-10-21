"""Original RigidBody settings and LinkedInputPose passthrough potential writers.

Native owns all projection, callbacks, node creation, pose schema side effects,
and late failure prefixes. Each named call holds one journal; Batch borrows its
parent. No replay, cleanup, compiler/save calls or runtime validation here.
"""
import copy
import json
from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_authoring as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'set_anim_graph_rigidbody_settings': ('blueprint_name', 'node_id', 'alpha', 'external_force', 'simulation_space', 'enable_world_geometry', 'allow_non_sample', 'graph_name', 'graph_id', 'graph_type'), 'ensure_anim_graph_input_pose_passthrough': ('blueprint_name', 'graph_name', 'graph_id', 'graph_type', 'replace_existing', 'input_node_position', 'allow_non_sample')}
_FIELD_KINDS = {'set_anim_graph_rigidbody_settings': {'blueprint_name': 'string', 'node_id': 'string', 'alpha': 'any', 'external_force': 'any', 'simulation_space': 'string', 'enable_world_geometry': 'any', 'allow_non_sample': 'boolean', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string'}, 'ensure_anim_graph_input_pose_passthrough': {'blueprint_name': 'string', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'replace_existing': 'boolean', 'input_node_position': 'any', 'allow_non_sample': 'boolean'}}


def _json(value):
    try:
        return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("anim_graph_authoring_input_invalid", "Original Any value requires a whole JSON carrier") from error


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("anim_graph_authoring_input_invalid", "Only original method fields admitted")
    for field, kind in _FIELD_KINDS[method].items():
        if kind != "any" and type(public[field]) is not (bool if kind == "boolean" else str):
            raise OperationError("anim_graph_authoring_input_invalid", "Original public type differs: " + field)
    hidden = copy.deepcopy(public)
    if method == "set_anim_graph_rigidbody_settings":
        for field in ("alpha", "external_force", "enable_world_geometry"):
            value = hidden.pop(field)
            hidden[field + "_json"] = "" if value is None else _json(value)
    else:
        value = hidden.pop("input_node_position")
        hidden["input_node_position_json"] = _json([-320, 0] if value is None else value)
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]),
        optional_Any_None_omitted_private_empty_string=True,
        private_bare_JSON_null_is_supplied_and_distinct=True,
        original_input_pose_None_position_normalized=[-320, 0],
        Native_owns_setting_projection_callbacks_reconstruction_and_schema_link_effects=True,
        late_failed_setting_node_or_removed_link_prefix_not_rolled_back=True,
        no_Python_geometry_setting_alias_or_sample_scope_reinterpretation=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        Native_reflection_loader_cache_structural_effects_not_total_zero=True,
        full_original_runtime_cold_or_unique_pipeline_verified=False)


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
    result["validation"].update(status="native_anim_graph_authoring_postcondition", native_postcondition_passed=native["postcondition_passed"],
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
    result["stage"] = "native_anim_graph_authoring_operation"
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
        if getattr(error, "code", "") == "anim_graph_authoring_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_graph_authoring_native_failed", native.get("error") or "Native AnimGraph authoring/settings/node/link/current prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("anim_graph_authoring_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_AnimGraph_setting_node_pose_link_prefix_and_journal_no_replay"



def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result



def set_anim_graph_rigidbody_settings(blueprint_name, node_id='', alpha=None, external_force=None, simulation_space='', enable_world_geometry=None, allow_non_sample=False, graph_name='AnimGraph', graph_id='', graph_type='function'):
    try:
        arguments = _arguments('set_anim_graph_rigidbody_settings', dict(blueprint_name=blueprint_name, node_id=node_id, alpha=alpha, external_force=external_force, simulation_space=simulation_space, enable_world_geometry=enable_world_geometry, allow_non_sample=allow_non_sample, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type))
    except Exception as error:
        return _rejected('set_anim_graph_rigidbody_settings', error)
    return _named(arguments)


def set_anim_graph_rigidbody_settings_action(result, blueprint_name, node_id='', alpha=None, external_force=None, simulation_space='', enable_world_geometry=None, allow_non_sample=False, graph_name='AnimGraph', graph_id='', graph_type='function', checkpoint_callback=None):
    _run_action(result, _arguments('set_anim_graph_rigidbody_settings', dict(blueprint_name=blueprint_name, node_id=node_id, alpha=alpha, external_force=external_force, simulation_space=simulation_space, enable_world_geometry=enable_world_geometry, allow_non_sample=allow_non_sample, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type)), checkpoint_callback)


def ensure_anim_graph_input_pose_passthrough(blueprint_name, graph_name='AnimGraph', graph_id='', graph_type='function', replace_existing=False, input_node_position=None, allow_non_sample=False):
    try:
        arguments = _arguments('ensure_anim_graph_input_pose_passthrough', dict(blueprint_name=blueprint_name, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, replace_existing=replace_existing, input_node_position=input_node_position, allow_non_sample=allow_non_sample))
    except Exception as error:
        return _rejected('ensure_anim_graph_input_pose_passthrough', error)
    return _named(arguments)


def ensure_anim_graph_input_pose_passthrough_action(result, blueprint_name, graph_name='AnimGraph', graph_id='', graph_type='function', replace_existing=False, input_node_position=None, allow_non_sample=False, checkpoint_callback=None):
    _run_action(result, _arguments('ensure_anim_graph_input_pose_passthrough', dict(blueprint_name=blueprint_name, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, replace_existing=replace_existing, input_node_position=input_node_position, allow_non_sample=allow_non_sample)), checkpoint_callback)
