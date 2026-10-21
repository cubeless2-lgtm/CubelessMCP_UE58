"""Original AnimBlueprintSample10 writer: one named journal or Batch parent.

Native owns target overwrite/delete/factory creation, source identity/current
proof and original explicit compile followed by optional forced save even
after compile failure. Factory implicit compile remains separate SDK work.
"""
import copy
import json
from typing import Any, Dict
from cubeless_mcp_ue58.compatibility.ue58 import anim_blueprint_sample as api
from . import state
from .state import OperationError

_PUBLIC_FIELDS = {'create_anim_blueprint_sample': ('target_anim_bp_path', 'skeleton_path', 'skeletal_mesh_path', 'preview_skeletal_mesh_path', 'parent_class_path', 'overwrite_existing', 'compile', 'save_asset', 'dry_run', 'validate_only')}
_FIELD_KINDS = {'create_anim_blueprint_sample': {'target_anim_bp_path': 'string', 'skeleton_path': 'string', 'skeletal_mesh_path': 'string', 'preview_skeletal_mesh_path': 'string', 'parent_class_path': 'string', 'overwrite_existing': 'boolean', 'compile': 'boolean', 'save_asset': 'boolean', 'dry_run': 'boolean', 'validate_only': 'boolean'}}

def _arguments(method, public):
    if method not in api.BINDINGS or tuple(public) != _PUBLIC_FIELDS[method]:
        raise OperationError("anim_blueprint_sample_input_invalid", "Only ordered original ten public fields admitted")
    for field, kind in _FIELD_KINDS[method].items():
        if type(public[field]) is not (bool if kind == "boolean" else str):
            raise OperationError("anim_blueprint_sample_input_invalid", "Original public field type differs: " + field)
    try:
        json.dumps(public, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except (TypeError, ValueError, RecursionError) as error:
        raise OperationError("anim_blueprint_sample_input_invalid", "Original public JSON arguments required without coercion") from error
    return {"method": method, "public": copy.deepcopy(public), "hidden": copy.deepcopy(public)}

def _context(result, arguments):
    result["data"].update(operation=arguments["method"], adapter_public_arguments=copy.deepcopy(arguments["public"]),
        adapter_native_arguments=copy.deepcopy(arguments["hidden"]), original_all_ten_parameters_forwarded_unchanged=True,
        Native_original_target_sample_scope_parent_source_identity_delete_create_authoritative=True,
        Native_factory_implicit_Blueprint_compile_separate_from_explicit_full_compiler=True,
        Native_original_requested_save_after_compile_failure_order_retained=True,
        Native_source_target_current_partial_compile_save_prefix_adopted_before_failure=True,
        Native_dry_validate_source_load_cache_work_not_total_zero=True,
        no_Python_path_parent_name_guard_normalization_or_new_fields=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        full_original_runtime_cold_unique_pipeline_verified=False)


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
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"]["target_anim_bp_path"],
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_anim_blueprint_sample_postcondition", native_postcondition_passed=native["postcondition_passed"],
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
    result["stage"] = "native_anim_blueprint_sample_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"]["target_anim_bp_path"],
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = getattr(api, arguments["method"])(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_anim_graph_authoring_operation"
            callback(result)
        if getattr(error, "code", "") == "anim_blueprint_sample_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "anim_blueprint_sample_native_failed", native.get("error") or "Native target delete/create/factory/source/current/compiler/save prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("anim_blueprint_sample_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_target_delete_create_factory_compile_save_prefix_and_journal_no_replay"


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result






def create_anim_blueprint_sample(target_anim_bp_path: str, skeleton_path: str='', skeletal_mesh_path: str='', preview_skeletal_mesh_path: str='', parent_class_path: str='/Script/Engine.AnimInstance', overwrite_existing: bool=False, compile: bool=True, save_asset: bool=False, dry_run: bool=False, validate_only: bool=False) -> Dict[str, Any]:
    try:
        arguments = _arguments('create_anim_blueprint_sample', dict(target_anim_bp_path=target_anim_bp_path, skeleton_path=skeleton_path, skeletal_mesh_path=skeletal_mesh_path, preview_skeletal_mesh_path=preview_skeletal_mesh_path, parent_class_path=parent_class_path, overwrite_existing=overwrite_existing, compile=compile, save_asset=save_asset, dry_run=dry_run, validate_only=validate_only))
    except Exception as error:
        return _rejected('create_anim_blueprint_sample', error)
    return _named(arguments)


def create_anim_blueprint_sample_action(result, target_anim_bp_path: str, skeleton_path: str='', skeletal_mesh_path: str='', preview_skeletal_mesh_path: str='', parent_class_path: str='/Script/Engine.AnimInstance', overwrite_existing: bool=False, compile: bool=True, save_asset: bool=False, dry_run: bool=False, validate_only: bool=False, checkpoint_callback=None):
    _run_action(result, _arguments('create_anim_blueprint_sample', dict(target_anim_bp_path=target_anim_bp_path, skeleton_path=skeleton_path, skeletal_mesh_path=skeletal_mesh_path, preview_skeletal_mesh_path=preview_skeletal_mesh_path, parent_class_path=parent_class_path, overwrite_existing=overwrite_existing, compile=compile, save_asset=save_asset, dry_run=dry_run, validate_only=validate_only)), checkpoint_callback)
