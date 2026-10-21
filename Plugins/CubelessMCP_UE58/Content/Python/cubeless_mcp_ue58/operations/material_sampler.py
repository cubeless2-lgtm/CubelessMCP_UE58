"""Original sampler6 with one named or existing parent Batch journal.

Native owns all TextureSample identity matches, dry would-change reporting,
matching skips, direct field edits and optional original compile/save dispatch.
Selected sampler success does not fabricate shader completion or save success.
"""
import copy

from cubeless_mcp_ue58.compatibility.ue58 import material_sampler as api
from . import state
from .state import OperationError


def _arguments(material_path, texture_path, sampler_type, dry_run, save, compile):
    if (type(material_path) is not str or type(texture_path) is not str or type(sampler_type) is not str
            or type(dry_run) is not bool or type(save) is not bool or type(compile) is not bool):
        raise OperationError("material_sampler_input_invalid",
            "Original material/texture/sampler fields require text; dry_run, save and compile require booleans")
    # Native owns path normalization, load order, enum aliases and finite admission.
    return {"material_path": material_path, "texture_path": texture_path, "sampler_type": sampler_type,
        "dry_run": dry_run, "save": save, "compile": compile}


def _context(result, arguments):
    result["data"].update(operation=api.BINDINGS[0], requested_arguments=copy.deepcopy(arguments),
        caller_dry_run_requested=arguments.get("dry_run"), caller_save_requested=arguments.get("save"),
        caller_compile_requested=arguments.get("compile"),
        public_input_representation="original material_path, texture_path, sampler_type='SAMPLERTYPE_LinearColor', dry_run=False, save=True, compile=True",
        Python_explicit_asset_load_calls=0, Python_explicit_sampler_write_calls=0,
        Python_explicit_compile_calls=0, Python_explicit_save_calls=0,
        native_phase_authority="actual returned identity/filter/dry/matching/edit/compiler/direct-save flags, counters and observations",
        action_success_scope="selected sampler/texture identity and bounded source observations; async compiler and returned save have independent outcomes",
        original_direct_save_not_gated_by_compile_completion=True,
        compiler_request_is_completed_compilation=False,
        legacy_success_true_saved_false_is_save_success=False,
        matching_and_dry_run_compile_save_attempts="native_authoritative; no additional Python dispatch",
        read_only_pending_observer="existing analyze_material_graph.ue58_compile_observation; never replay this sampler writer to poll",
        automatic_action_retry=False, full_original_sampler_contract_verified=False,
        whole_material_editor_compiler_fanout_verified=False, cold_or_packaged_render_verified=False)


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_raw_result=native.get("_raw_native_json"),
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native.get("changed") if known else None,
        mutation_observation_scope="returned selected sampler/material/source scope; compiler/cache/world fanout not whole-state preservation")
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_returned_current_resource_verified" if native["compiled"] is True else (
            "native_returned_current_resource_failed" if native["compiled"] is False else (
                "native_returned_completion_unverified" if native["compile_attempted"] else "native_returned_not_attempted")))
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else (
            "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted"))
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": api.BINDINGS[0],
        "asset_path": native["data"].get("material_path", arguments["material_path"]),
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native.get("changed") if known else None, "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="returned_native_material_sampler",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        compilation_completion="separate_native_nullable_observation",
        save_completion="actual_native_save_return_not_inferred_from_action_success",
        whole_editor_cache_effects="not_observed", render_or_cook="not_run")
    result["stage"] = "returned_material_sampler_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    callback = checkpoint_callback or state.checkpoint
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="intent_prepared_not_dispatched")
    result["save"].update(requested=arguments["save"], attempted=False, saved=False, status="intent_prepared_not_dispatched")
    result["stage"] = "before_mutation_checkpoint"
    result["data"]["pending_mutation"] = {"operation": api.BINDINGS[0], **copy.deepcopy(arguments),
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "inspect_actual_sampler_compile_save_prefix_do_not_replay"}
    callback(result)
    result["stage"] = "native_material_sampler"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": api.BINDINGS[0], "asset_path": arguments["material_path"],
        "attempted": None, "change_state_known": False, "changed": None}]
    try:
        native = api.set_material_texture_sample_sampler_type_mcp(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["compile"]["status"] = "native_returned_unparsed"
            result["save"]["status"] = "native_returned_unparsed"
            result["stage"] = "returned_material_sampler_operation"
            callback(result)
        if getattr(error, "code", "") == "material_sampler_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_performed=False, mutation_state_unknown=False)
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_material_sampler_failed",
            native.get("error") or "Native sampler action failed; full actual compiler/direct-save prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("material_sampler_postcondition_unverified",
            "Native success lacks a known selected sampler postcondition; no retry")
    # Original success is independent of asynchronous compiler completion and a returned false save.
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "sampler_compile_save_would_repeat_inspect_prefix"


def set_material_texture_sample_sampler_type_mcp(material_path, texture_path,
        sampler_type="SAMPLERTYPE_LinearColor", dry_run=False, save=True, compile=True):
    try:
        arguments = _arguments(material_path, texture_path, sampler_type, dry_run, save, compile)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        _context(result, {"dry_run": dry_run, "save": save, "compile": compile})
        result["data"].update(admission_rejected=True, native_dispatched=False, native_result_returned=False,
            native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
        return result
    return state.mutate(api.BINDINGS[0], arguments, None, lambda result: _run_action(result, arguments))


def set_material_texture_sample_sampler_type_mcp_action(result, material_path, texture_path,
        sampler_type="SAMPLERTYPE_LinearColor", dry_run=False, save=True, compile=True, checkpoint_callback=None):
    _run_action(result, _arguments(material_path, texture_path, sampler_type, dry_run, save, compile), checkpoint_callback)
