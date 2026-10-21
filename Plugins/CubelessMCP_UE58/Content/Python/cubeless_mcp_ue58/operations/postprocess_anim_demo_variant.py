"""Compound variant creation/reuse, graph setup, compile, class assignment and save.

The specific Native operation owns both asset lifetimes and all real changes.
Python records the full actual/partial/unknown receipt before failure. It never
loads, duplicates, deletes, authors a graph, compiles, saves, rolls back or retries.
"""
import copy
from typing import Any, Dict
from cubeless_mcp_ue58.compatibility.ue58 import postprocess_anim_demo_variant as api
from . import state
from .state import OperationError

METHOD = "ensure_postprocess_anim_demo_variant"
PUBLIC_FIELDS = ('source_blueprint_name', 'source_skeletal_mesh', 'variant_name', 'target_root', 'target_blueprint_name', 'target_skeletal_mesh', 'bone_name', 'rotation', 'graph_name', 'graph_id', 'graph_type', 'replace_existing', 'overwrite_existing', 'compile', 'save', 'dry_run', 'allow_non_sample')
TEXT_FIELDS = ('source_blueprint_name', 'source_skeletal_mesh', 'variant_name', 'target_root', 'target_blueprint_name', 'target_skeletal_mesh', 'bone_name', 'graph_name', 'graph_id', 'graph_type')
BOOL_FIELDS = ('replace_existing', 'overwrite_existing', 'compile', 'save', 'dry_run', 'allow_non_sample')


def _arguments(source_blueprint_name, source_skeletal_mesh, variant_name, target_root, target_blueprint_name, target_skeletal_mesh, bone_name, rotation, graph_name, graph_id, graph_type, replace_existing, overwrite_existing, compile, save, dry_run, allow_non_sample):
    arguments = dict(zip(PUBLIC_FIELDS, (source_blueprint_name, source_skeletal_mesh, variant_name, target_root, target_blueprint_name, target_skeletal_mesh, bone_name, rotation, graph_name, graph_id, graph_type, replace_existing, overwrite_existing, compile, save, dry_run, allow_non_sample)))
    for field in TEXT_FIELDS:
        if type(arguments[field]) is not str:
            raise OperationError("postprocess_anim_demo_variant_input_invalid", field + " must be a string; None is not an empty selector")
    for field in BOOL_FIELDS:
        if type(arguments[field]) is not bool:
            raise OperationError("postprocess_anim_demo_variant_input_invalid", field + " must be a boolean")
    # The original wrapper normalizes only None. No Euler/vector coercion occurs here.
    api._carrier([0,0,6] if rotation is None else rotation)
    return copy.deepcopy(arguments)


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_seventeen_public_parameters_preserved=True,
        private_presence_keys=tuple(sorted(api.LEGACY_PARAM_KEYS)),
        public_rotation_None_uses_original_default=[0,0,6],
        public_empty_target_omission_and_private_explicit_empty_preserved=True,
        public_empty_graph_default_and_private_raw_empty_preserved=True,
        native_source_target_path_sanitization_and_identity_guards_authoritative=True,
        native_duplicate_reuse_delete_graph_and_asset2_partial_prefix_authoritative=True,
        native_optional_compile_or_forced_missing_generated_class_authoritative=True,
        native_mesh_postprocess_class_assignment_and_two_save_results_authoritative=True,
        native_game_thread_editor_idle_current_lifetimes_and_callback_guards_authoritative=True,
        adapter_world_or_asset_queries=0, adapter_asset_duplicate_delete_calls=0, adapter_graph_authoring_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        source_assets_unchanged_is_proven_by_wrapper=False, dry_run_is_total_SDK_purity=False,
        atomic_rollback=False, automatic_action_retry=False,
        actual_named_raw_wire_verified=False, runtime_pose_verified=False, playback_verified=False,
        unique_external_pipeline_complete=False, packaged_runtime_verified=False)


def _adopt(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key:copy.deepcopy(value) for key,value in native.items() if key not in ("data","_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], native_observed_changed=native["changed"],
        mutation_state_unknown=not known, change_state_known=known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result,arguments)
    result["compile"].update(requested=native["compile_requested"],attempted=native["compile_attempted"],
        compiled=native["compiled"],status="native_postprocess_variant_compile_receipt")
    result["save"].update(requested=native["save_requested"],attempted=native["save_attempted"],
        saved=native["saved"],status="native_postprocess_variant_save_receipt")
    no_work=known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"]=[] if no_work else [{"kind":METHOD,"requested_target_selectors":{
        "target_blueprint_name":arguments["target_blueprint_name"],"target_skeletal_mesh":arguments["target_skeletal_mesh"]},
        "attempted":native["mutation_attempted"],"change_state_known":known,"changed":native["changed"] if known else None,
        "native_observed_changed":native["changed"],"postcondition_passed":native["postcondition_passed"]}]
    result["validation"].update(status="native_postprocess_variant_completion",native_postcondition_passed=native["postcondition_passed"],
        total_change_state_known=known,full_asset2_duplicate_reuse_delete_graph_compile_assignment_save_prefix_preserved=True,
        partial_failures_not_rolled_back_or_reinterpreted=True,disk_reload="not_run",runtime_pose="not_run",unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,atomic_rollback=False,
        duplicate_risk="unknown_asset2_delete_duplicate_graph_compile_class_assignment_or_save_inspect_prefix_before_new_call" if not known
            else "partial_or_attempted_asset_changes_inspect_prefix" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"]="returned_postprocess_anim_demo_variant_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, change_state_known=True,
        pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=arguments["save"], attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_postprocess_anim_demo_variant_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True, change_state_known=False)
    result["changes"] = [{"kind": METHOD, "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_postprocess_anim_demo_variant_operation"
    try:
        native = api.ensure_postprocess_anim_demo_variant(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "native_call_dispatched", None) is False:
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False, change_state_known=True)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_postprocess_anim_demo_variant_operation"
        callback(result)  # Persist malformed raw and unknown Native asset work before error.
        raise
    _adopt(result, arguments, native)
    callback(result)  # Both assets and graph/compile/assignment/save partial rows and actual work precede failure.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "postprocess_anim_demo_variant_native_failed",
            native.get("error") or "Native PostProcess variant failed; actual partial state is retained")
    if not native["postcondition_passed"]:
        raise OperationError("postprocess_anim_demo_variant_result_unverified", "Native postcondition is unverified; inspect prefix without replay")
    result.update(success=True, stage="complete")
    return result


def _adopt_rejection(result, error):
    result["changes"] = []
    state.fail(result, error)
    result["stage"] = "postprocess_anim_demo_variant_input_validation"
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
        mutation_state_unknown=False, change_state_known=True, journal_created=False,
        journal_created_scope="new operation or child journal; existing Batch parent is retained")
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["validation"].update(status="input_rejected", total_change_state_known=True)
    return result


def _rejected(error):
    return _adopt_rejection(state.new_result(), error)


def ensure_postprocess_anim_demo_variant(source_blueprint_name: str, source_skeletal_mesh: str, variant_name: str='Variant', target_root: str='/Game/_MCP_Sample/AnimStudy', target_blueprint_name: str='', target_skeletal_mesh: str='', bone_name: str='head', rotation=None, graph_name: str='AnimGraph', graph_id: str='', graph_type: str='function', replace_existing: bool=True, overwrite_existing: bool=False, compile: bool=True, save: bool=True, dry_run: bool=False, allow_non_sample: bool=False) -> Dict[str, Any]:
    try:
        arguments = _arguments(source_blueprint_name, source_skeletal_mesh, variant_name, target_root, target_blueprint_name, target_skeletal_mesh, bone_name, rotation, graph_name, graph_id, graph_type, replace_existing, overwrite_existing, compile, save, dry_run, allow_non_sample)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD,arguments,None,lambda result:_run_action(result,arguments))


def ensure_postprocess_anim_demo_variant_action(result, source_blueprint_name: str, source_skeletal_mesh: str, variant_name: str='Variant', target_root: str='/Game/_MCP_Sample/AnimStudy', target_blueprint_name: str='', target_skeletal_mesh: str='', bone_name: str='head', rotation=None, graph_name: str='AnimGraph', graph_id: str='', graph_type: str='function', replace_existing: bool=True, overwrite_existing: bool=False, compile: bool=True, save: bool=True, dry_run: bool=False, allow_non_sample: bool=False, checkpoint_callback=None, *, _legacy_command_params=None):
    try:
        api._legacy_projection(target_blueprint_name,target_skeletal_mesh,rotation,graph_name,graph_type,_legacy_command_params)
        legacy=_legacy_command_params or {}
        # Typed placeholders preserve the public contract; raw presence remains in the private DTO.
        bp_arg="" if "target_blueprint_name" in legacy else target_blueprint_name
        mesh_arg="" if "target_skeletal_mesh" in legacy else target_skeletal_mesh
        graph_arg="" if "graph_name" in legacy else graph_name
        type_arg="" if "graph_type" in legacy else graph_type
        rotation_arg=None if "rotation" in legacy else rotation
        arguments=_arguments(source_blueprint_name,source_skeletal_mesh,variant_name,target_root,bp_arg,mesh_arg,bone_name,
            rotation_arg,graph_arg,graph_id,type_arg,replace_existing,overwrite_existing,compile,save,dry_run,allow_non_sample)
        if _legacy_command_params is not None:
            arguments["_legacy_command_params"]=copy.deepcopy(_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result,error)
        result["data"]["parent_journal_only"]=True
        result["stage"]="returned_postprocess_anim_demo_variant_operation"
        (checkpoint_callback or state.checkpoint)(result)
        raise OperationError(result["error_code"],result["error"]) from error
    return _run_action(result,arguments,checkpoint_callback)
