"""Original ControlRig sample builders13/20, with one named journal or Batch parent.

Collection members are opaque original JSON. Native owns path/source/target,
hierarchy/units/graph/lifecycle/current proofs and requested compiler/save.
Dry-run can load/construct transient rigs before target admission; no replay.
"""
import copy
import json
from typing import Any, Dict, List, Optional
from cubeless_mcp_ue58.compatibility.ue58 import controlrig_builders as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'ensure_anim_graph_controlrig_node_sample': ('source_anim_bp_path', 'control_rig_asset_path', 'control_rig_class_path', 'target_anim_bp_path', 'target_root', 'variant_name', 'allow_existing_target_reuse', 'overwrite_existing', 'replace_existing_links', 'compile', 'save_asset', 'dry_run', 'validate_only'), 'ensure_controlrig_sample_asset': ('target_control_rig_path', 'skeletal_mesh_path', 'skeleton_path', 'preview_skeletal_mesh_path', 'setup_mode', 'curves', 'nulls', 'controls', 'unit_nodes', 'max_direct_curves', 'max_direct_nulls', 'max_direct_controls', 'max_unit_nodes', 'allow_existing_target_reuse', 'overwrite_existing', 'modular_rig', 'compile', 'save_asset', 'dry_run', 'validate_only')}
_FIELD_KINDS = {'ensure_anim_graph_controlrig_node_sample': {'source_anim_bp_path': 'string', 'control_rig_asset_path': 'string', 'control_rig_class_path': 'string', 'target_anim_bp_path': 'string', 'target_root': 'string', 'variant_name': 'string', 'allow_existing_target_reuse': 'boolean', 'overwrite_existing': 'boolean', 'replace_existing_links': 'boolean', 'compile': 'boolean', 'save_asset': 'boolean', 'dry_run': 'boolean', 'validate_only': 'boolean'}, 'ensure_controlrig_sample_asset': {'target_control_rig_path': 'string', 'skeletal_mesh_path': 'string', 'skeleton_path': 'string', 'preview_skeletal_mesh_path': 'string', 'setup_mode': 'string', 'curves': 'nullable_any_list', 'nulls': 'nullable_any_list', 'controls': 'nullable_any_list', 'unit_nodes': 'nullable_any_list', 'max_direct_curves': 'integer', 'max_direct_nulls': 'integer', 'max_direct_controls': 'integer', 'max_unit_nodes': 'integer', 'allow_existing_target_reuse': 'boolean', 'overwrite_existing': 'boolean', 'modular_rig': 'boolean', 'compile': 'boolean', 'save_asset': 'boolean', 'dry_run': 'boolean', 'validate_only': 'boolean'}}

def _json(value):
    try:
        return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("controlrig_builders_input_invalid", "Original public JSON must be finite; no descriptor coercion") from error

def _arguments(method, public):
    if method not in api.BINDINGS or tuple(public) != _PUBLIC_FIELDS[method]:
        raise OperationError("controlrig_builders_input_invalid", "Only ordered original thirteen/twenty public fields admitted")
    for field, kind in _FIELD_KINDS[method].items():
        value = public[field]
        valid = value is None or type(value) is list if kind == "nullable_any_list" else type(value) is (bool if kind == "boolean" else int if kind == "integer" else str)
        if not valid:
            raise OperationError("controlrig_builders_input_invalid", "Original public type differs: " + field)
        if kind == "integer" and not -(1 << 63) <= value < (1 << 63):
            raise OperationError("controlrig_builders_input_invalid", "Safe private int64 carrier required; original limit clamp stays Native: " + field)
    _json(public)
    forwarded = copy.deepcopy(public)
    if method == "ensure_anim_graph_controlrig_node_sample":
        for field in ("control_rig_asset_path", "control_rig_class_path", "target_anim_bp_path"):
            if not forwarded[field]:
                del forwarded[field]
    else:
        for field in ("curves", "nulls", "controls", "unit_nodes"):
            if forwarded[field] is None:
                del forwarded[field]
    hidden = {}
    for field, kind in _FIELD_KINDS[method].items():
        value = public[field]
        if kind == "nullable_any_list":
            hidden[field + "_json"] = "" if value is None else _json(value)
        else:
            hidden[field] = copy.deepcopy(value)
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden, "original_command_parameters": forwarded}

def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]), original_command_parameters=copy.deepcopy(arguments["original_command_parameters"]),
        original_builder_target_scope_reuse_overwrite_and_partial_failure_Native_authoritative=True,
        original_nullable_Any_list_None_omitted_and_explicit_empty_retained=True,
        Native_node_sample_source_load_and_transient_rig_resolution_precede_dry_validate=True,
        Native_hierarchy_RigVM_request_init_reconstruct_transient_lifecycle_effects_not_total_zero=True,
        Native_source_target_current_compile_save_prefix_authoritative=True,
        no_Python_member_shape_name_path_limit_coercion_or_new_aliases=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        full_original_builder_runtime_cold_unique_pipeline_verified=False)


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
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"].get("target_control_rig_path") or arguments["public"].get("target_anim_bp_path") or None,
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_controlrig_builders_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
        implicit_graph_node_schema_structural_cache_work="not_collapsed_into_explicit_compile_count")
    result["stage"] = "returned_anim_graph_authoring_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["compile"].update(requested=arguments["public"].get("compile", False), attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=arguments["public"].get("save_asset", False), attempted=False, saved=False, status="not_dispatched")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_controlrig_builders_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"].get("target_control_rig_path") or arguments["public"].get("target_anim_bp_path") or None,
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = getattr(api, arguments["method"])(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_anim_graph_authoring_operation"
            callback(result)
        if getattr(error, "code", "") == "controlrig_builders_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "controlrig_builders_native_failed", native.get("error") or "Native target/source/hierarchy/node/link/init/compiler/save/current prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("controlrig_builders_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_builder_target_hierarchy_node_link_compile_save_prefix_and_journal_no_replay"


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result




def ensure_anim_graph_controlrig_node_sample(source_anim_bp_path: str, control_rig_asset_path: str='', control_rig_class_path: str='', target_anim_bp_path: str='', target_root: str='/Game/_MCP_Sample/AnimStudy', variant_name: str='ControlRigNodeSample', allow_existing_target_reuse: bool=True, overwrite_existing: bool=False, replace_existing_links: bool=True, compile: bool=True, save_asset: bool=False, dry_run: bool=False, validate_only: bool=False) -> Dict[str, Any]:
    try:
        arguments = _arguments('ensure_anim_graph_controlrig_node_sample', dict(source_anim_bp_path=source_anim_bp_path, control_rig_asset_path=control_rig_asset_path, control_rig_class_path=control_rig_class_path, target_anim_bp_path=target_anim_bp_path, target_root=target_root, variant_name=variant_name, allow_existing_target_reuse=allow_existing_target_reuse, overwrite_existing=overwrite_existing, replace_existing_links=replace_existing_links, compile=compile, save_asset=save_asset, dry_run=dry_run, validate_only=validate_only))
    except Exception as error:
        return _rejected('ensure_anim_graph_controlrig_node_sample', error)
    return _named(arguments)


def ensure_anim_graph_controlrig_node_sample_action(result, source_anim_bp_path: str, control_rig_asset_path: str='', control_rig_class_path: str='', target_anim_bp_path: str='', target_root: str='/Game/_MCP_Sample/AnimStudy', variant_name: str='ControlRigNodeSample', allow_existing_target_reuse: bool=True, overwrite_existing: bool=False, replace_existing_links: bool=True, compile: bool=True, save_asset: bool=False, dry_run: bool=False, validate_only: bool=False, checkpoint_callback=None):
    _run_action(result, _arguments('ensure_anim_graph_controlrig_node_sample', dict(source_anim_bp_path=source_anim_bp_path, control_rig_asset_path=control_rig_asset_path, control_rig_class_path=control_rig_class_path, target_anim_bp_path=target_anim_bp_path, target_root=target_root, variant_name=variant_name, allow_existing_target_reuse=allow_existing_target_reuse, overwrite_existing=overwrite_existing, replace_existing_links=replace_existing_links, compile=compile, save_asset=save_asset, dry_run=dry_run, validate_only=validate_only)), checkpoint_callback)


def ensure_controlrig_sample_asset(target_control_rig_path: str, skeletal_mesh_path: str='', skeleton_path: str='', preview_skeletal_mesh_path: str='', setup_mode: str='empty_with_preview', curves: Optional[List[Any]]=None, nulls: Optional[List[Any]]=None, controls: Optional[List[Any]]=None, unit_nodes: Optional[List[Any]]=None, max_direct_curves: int=32, max_direct_nulls: int=32, max_direct_controls: int=32, max_unit_nodes: int=6, allow_existing_target_reuse: bool=True, overwrite_existing: bool=False, modular_rig: bool=False, compile: bool=True, save_asset: bool=False, dry_run: bool=False, validate_only: bool=False) -> Dict[str, Any]:
    try:
        arguments = _arguments('ensure_controlrig_sample_asset', dict(target_control_rig_path=target_control_rig_path, skeletal_mesh_path=skeletal_mesh_path, skeleton_path=skeleton_path, preview_skeletal_mesh_path=preview_skeletal_mesh_path, setup_mode=setup_mode, curves=curves, nulls=nulls, controls=controls, unit_nodes=unit_nodes, max_direct_curves=max_direct_curves, max_direct_nulls=max_direct_nulls, max_direct_controls=max_direct_controls, max_unit_nodes=max_unit_nodes, allow_existing_target_reuse=allow_existing_target_reuse, overwrite_existing=overwrite_existing, modular_rig=modular_rig, compile=compile, save_asset=save_asset, dry_run=dry_run, validate_only=validate_only))
    except Exception as error:
        return _rejected('ensure_controlrig_sample_asset', error)
    return _named(arguments)


def ensure_controlrig_sample_asset_action(result, target_control_rig_path: str, skeletal_mesh_path: str='', skeleton_path: str='', preview_skeletal_mesh_path: str='', setup_mode: str='empty_with_preview', curves: Optional[List[Any]]=None, nulls: Optional[List[Any]]=None, controls: Optional[List[Any]]=None, unit_nodes: Optional[List[Any]]=None, max_direct_curves: int=32, max_direct_nulls: int=32, max_direct_controls: int=32, max_unit_nodes: int=6, allow_existing_target_reuse: bool=True, overwrite_existing: bool=False, modular_rig: bool=False, compile: bool=True, save_asset: bool=False, dry_run: bool=False, validate_only: bool=False, checkpoint_callback=None):
    _run_action(result, _arguments('ensure_controlrig_sample_asset', dict(target_control_rig_path=target_control_rig_path, skeletal_mesh_path=skeletal_mesh_path, skeleton_path=skeleton_path, preview_skeletal_mesh_path=preview_skeletal_mesh_path, setup_mode=setup_mode, curves=curves, nulls=nulls, controls=controls, unit_nodes=unit_nodes, max_direct_curves=max_direct_curves, max_direct_nulls=max_direct_nulls, max_direct_controls=max_direct_controls, max_unit_nodes=max_unit_nodes, allow_existing_target_reuse=allow_existing_target_reuse, overwrite_existing=overwrite_existing, modular_rig=modular_rig, compile=compile, save_asset=save_asset, dry_run=dry_run, validate_only=validate_only)), checkpoint_callback)
