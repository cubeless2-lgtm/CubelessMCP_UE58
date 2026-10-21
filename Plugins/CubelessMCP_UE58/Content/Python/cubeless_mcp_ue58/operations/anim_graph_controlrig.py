"""Original ControlRig12/forced15 potential writers.

One named journal or a Batch parent retains Native current and partial results.
Native owns visibility/defaults/links/curve arrays and SDK reconstruction effects,
including possible transient rig construction/evaluation and asset preload.
The consumer does no asset work, compiler/save, cleanup, retry or rollback.
"""
import copy
import json
from typing import Any, Dict, List, Optional
from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_controlrig as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'set_anim_graph_controlrig_input_defaults': ('blueprint_name', 'input_defaults', 'expose_input_pins', 'graph_name', 'graph_id', 'graph_type', 'node_id', 'node_name', 'title_contains', 'control_rig_class', 'disconnect_existing_links', 'allow_non_sample'), 'ensure_controlrig_forced_driver_animbp': ('blueprint_name', 'curve_values', 'input_defaults', 'alpha', 'apply_mode', 'graph_name', 'graph_id', 'graph_type', 'node_id', 'node_name', 'title_contains', 'control_rig_class', 'replace_existing', 'disconnect_existing_links', 'allow_non_sample')}
_FIELD_KINDS = {'set_anim_graph_controlrig_input_defaults': {'blueprint_name': 'string', 'input_defaults': 'nullable_any_map', 'expose_input_pins': 'nullable_string_list', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'node_id': 'string', 'node_name': 'string', 'title_contains': 'string', 'control_rig_class': 'string', 'disconnect_existing_links': 'boolean', 'allow_non_sample': 'boolean'}, 'ensure_controlrig_forced_driver_animbp': {'blueprint_name': 'string', 'curve_values': 'nullable_number_map', 'input_defaults': 'nullable_any_map', 'alpha': 'number', 'apply_mode': 'string', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'node_id': 'string', 'node_name': 'string', 'title_contains': 'string', 'control_rig_class': 'string', 'replace_existing': 'boolean', 'disconnect_existing_links': 'boolean', 'allow_non_sample': 'boolean'}}
_DEFAULT_INPUTS = {'ShouldDoIKTrace': True, 'InteractionWorldLocation': [80, -40, 80]}
_DEFAULT_CURVES = {'IK_blend_interact': 1.0, 'IKBlend_l': 1.0}


def _json(value):
    try:
        return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("anim_graph_controlrig_input_invalid", "Original collections and numbers require finite whole JSON without coercion") from error


def _valid(kind, value):
    if kind == "nullable_any_map":
        return value is None or type(value) is dict and all(type(key) is str for key in value)
    if kind == "nullable_number_map":
        return value is None or type(value) is dict and all(type(key) is str and type(member) in (int, float) for key, member in value.items())
    if kind == "nullable_string_list":
        return value is None or type(value) is list and all(type(member) is str for member in value)
    if kind == "number":
        return type(value) in (int, float)
    return type(value) is (bool if kind == "boolean" else str)


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(_PUBLIC_FIELDS[method]):
        raise OperationError("anim_graph_controlrig_input_invalid", "Only original ControlRig12/forced15 fields admitted")
    for field, kind in _FIELD_KINDS[method].items():
        if not _valid(kind, public[field]):
            raise OperationError("anim_graph_controlrig_input_invalid", "Original public type differs: " + field)
    _json(public)
    projected = copy.deepcopy(public)
    if method == "set_anim_graph_controlrig_input_defaults":
        if projected["input_defaults"] is None and not projected["expose_input_pins"]:
            projected["input_defaults"] = copy.deepcopy(_DEFAULT_INPUTS)
    elif method == "ensure_controlrig_forced_driver_animbp":
        if projected["curve_values"] is None:
            projected["curve_values"] = copy.deepcopy(_DEFAULT_CURVES)
        if projected["input_defaults"] is None:
            projected["input_defaults"] = copy.deepcopy(_DEFAULT_INPUTS)
    hidden = {}
    for field in _PUBLIC_FIELDS[method]:
        value = projected[field]
        if _FIELD_KINDS[method][field].startswith("nullable_"):
            hidden[field + "_json"] = "" if value is None else _json(value)
        else:
            hidden[field] = value
    return {"method": method, "public": copy.deepcopy(public), "hidden": hidden}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]),
        original_Set12_None_and_falsey_expose_conditional_defaults_preserved=True,
        original_forced15_None_maps_default2_and_explicit_empty_maps_preserved=True,
        private_empty_collection_omitted_distinct_from_supplied_JSONnull=True,
        Native_ControlRig_asset_reference_class_path_UE58_compatibility_repair_authoritative=True,
        Native_visibility_reconstruction_pin_default_link_and_ModifyCurve_order_authoritative=True,
        Native_original_filters_and_late_partial_prefix_preserved=True,
        Native_Reconstruct_may_construct_initialize_evaluate_transient_rig_and_preload_modular_assets=True,
        no_CDO_only_or_no_indirect_evaluation_assumption=True,
        no_Python_FName_pin_geometry_alpha_clamp_numeric_coercion_or_field_order_rewrite=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        full_original_ControlRig_runtime_preview_cold_or_unique_pipeline_verified=False)


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
    result["validation"].update(status="native_anim_graph_controlrig_postcondition", native_postcondition_passed=native["postcondition_passed"],
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
    result["stage"] = "native_anim_graph_controlrig_operation"
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
        if getattr(error, "code", "") == "anim_graph_controlrig_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_graph_controlrig_native_failed", native.get("error") or "Native AnimGraph authoring/settings/node/link/current prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("anim_graph_controlrig_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_AnimGraph_setting_node_pose_link_prefix_and_journal_no_replay"


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def set_anim_graph_controlrig_input_defaults(blueprint_name: str, input_defaults: Optional[Dict[str, Any]]=None, expose_input_pins: Optional[List[str]]=None, graph_name: str='AnimGraph', graph_id: str='', graph_type: str='function', node_id: str='', node_name: str='', title_contains: str='', control_rig_class: str='', disconnect_existing_links: bool=False, allow_non_sample: bool=False) -> Dict[str, Any]:
    try:
        arguments = _arguments('set_anim_graph_controlrig_input_defaults', dict(blueprint_name=blueprint_name, input_defaults=input_defaults, expose_input_pins=expose_input_pins, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, node_id=node_id, node_name=node_name, title_contains=title_contains, control_rig_class=control_rig_class, disconnect_existing_links=disconnect_existing_links, allow_non_sample=allow_non_sample))
    except Exception as error:
        return _rejected('set_anim_graph_controlrig_input_defaults', error)
    return _named(arguments)


def set_anim_graph_controlrig_input_defaults_action(result, blueprint_name: str, input_defaults: Optional[Dict[str, Any]]=None, expose_input_pins: Optional[List[str]]=None, graph_name: str='AnimGraph', graph_id: str='', graph_type: str='function', node_id: str='', node_name: str='', title_contains: str='', control_rig_class: str='', disconnect_existing_links: bool=False, allow_non_sample: bool=False, checkpoint_callback=None):
    _run_action(result, _arguments('set_anim_graph_controlrig_input_defaults', dict(blueprint_name=blueprint_name, input_defaults=input_defaults, expose_input_pins=expose_input_pins, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, node_id=node_id, node_name=node_name, title_contains=title_contains, control_rig_class=control_rig_class, disconnect_existing_links=disconnect_existing_links, allow_non_sample=allow_non_sample)), checkpoint_callback)


def ensure_controlrig_forced_driver_animbp(blueprint_name: str, curve_values: Optional[Dict[str, float]]=None, input_defaults: Optional[Dict[str, Any]]=None, alpha: float=1.0, apply_mode: str='Add', graph_name: str='AnimGraph', graph_id: str='', graph_type: str='function', node_id: str='', node_name: str='', title_contains: str='', control_rig_class: str='', replace_existing: bool=True, disconnect_existing_links: bool=True, allow_non_sample: bool=False) -> Dict[str, Any]:
    try:
        arguments = _arguments('ensure_controlrig_forced_driver_animbp', dict(blueprint_name=blueprint_name, curve_values=curve_values, input_defaults=input_defaults, alpha=alpha, apply_mode=apply_mode, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, node_id=node_id, node_name=node_name, title_contains=title_contains, control_rig_class=control_rig_class, replace_existing=replace_existing, disconnect_existing_links=disconnect_existing_links, allow_non_sample=allow_non_sample))
    except Exception as error:
        return _rejected('ensure_controlrig_forced_driver_animbp', error)
    return _named(arguments)


def ensure_controlrig_forced_driver_animbp_action(result, blueprint_name: str, curve_values: Optional[Dict[str, float]]=None, input_defaults: Optional[Dict[str, Any]]=None, alpha: float=1.0, apply_mode: str='Add', graph_name: str='AnimGraph', graph_id: str='', graph_type: str='function', node_id: str='', node_name: str='', title_contains: str='', control_rig_class: str='', replace_existing: bool=True, disconnect_existing_links: bool=True, allow_non_sample: bool=False, checkpoint_callback=None):
    _run_action(result, _arguments('ensure_controlrig_forced_driver_animbp', dict(blueprint_name=blueprint_name, curve_values=curve_values, input_defaults=input_defaults, alpha=alpha, apply_mode=apply_mode, graph_name=graph_name, graph_id=graph_id, graph_type=graph_type, node_id=node_id, node_name=node_name, title_contains=title_contains, control_rig_class=control_rig_class, replace_existing=replace_existing, disconnect_existing_links=disconnect_existing_links, allow_non_sample=allow_non_sample)), checkpoint_callback)


