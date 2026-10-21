"""Original Material expansion7 with one named or existing parent journal.

Native owns ordered passes, skips, composite changes, rollback and direct saves.
Completed report success is distinct from expansion/error/partial/save fields.
No Python compiler, second save, wait, graph rollback or automatic replay.
"""
import copy

from cubeless_mcp_ue58.compatibility.ue58 import material_function_expansion as api
from . import state
from .state import OperationError


def _arguments(material_path, node_id, recursive, exclude_engine_functions, max_passes, save, allow_partial_save):
    if type(material_path) is not str or type(node_id) is not str:
        raise OperationError("material_function_expansion_input_invalid", "Original material_path/node_id require strings")
    if any(type(v) is not bool for v in (recursive, exclude_engine_functions, save, allow_partial_save)):
        raise OperationError("material_function_expansion_input_invalid", "Original recursion/exclusion/save flags require exact booleans")
    if type(max_passes) is not int or not -(2 ** 31) <= max_passes < 2 ** 31:
        raise OperationError("material_function_expansion_input_invalid", "max_passes requires an exact reflected int32; native owns the original clamp")
    # Preserve literals. Native owns original path/node selection and limits;
    # the original public function inserts no dependency-reference guard.
    return {"material_path": material_path, "node_id": node_id, "recursive": recursive,
        "exclude_engine_functions": exclude_engine_functions, "max_passes": max_passes,
        "save": save, "allow_partial_save": allow_partial_save}


def _context(result, arguments):
    result["data"].update(operation=api.BINDINGS[0], requested_arguments=copy.deepcopy(arguments),
        caller_save_requested=arguments.get("save"),
        public_input_representation="original7 strings/bools/int; empty node_id selects all calls; no public compile or operation_id",
        Python_explicit_asset_load_calls=0, Python_explicit_expansion_calls=0,
        Python_explicit_compile_calls=0, Python_explicit_save_calls=0,
        native_phase_authority="complete passes/skips/errors/composite/rollback/source/actual-save receipt before terminal predicates",
        completed_report_success_is_all_nodes_expanded=False,
        explicit_compiler_attempt_counts_do_not_measure_implicit_PostEdit_work=True,
        automatic_action_retry=False, full_original_expansion_variants_verified=False,
        render_cold_runtime_completion_verified=False)


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None,
        mutation_observation_scope="actual selected graph/source/composite/rollback/dirty/disk receipt; no full compiler/world effect proof")
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_returned_compiled_observation" if native["compiled"] is True else (
            "native_returned_failed_compile_observation" if native["compiled"] is False else "native_returned_compile_observation_unverified"))
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else (
            "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted"))
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": api.BINDINGS[0],
        "asset_path": native["data"].get("material", arguments["material_path"]),
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None, "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="returned_native_material_function_expansion",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        expansion_success="inspect_actual_legacy_result_errors_skips_passes_and_rollback",
        compilation_completion="nullable_actual_observation_no_explicit_compiler_added",
        persistence_completion="actual_save_attempt_and_bool_not_inferred_from_report_success")
    result["stage"] = "returned_material_function_expansion_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    callback = checkpoint_callback or state.checkpoint
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="no_explicit_compiler_intent")
    result["save"].update(requested=arguments["save"], attempted=False, saved=False, status="intent_prepared_not_dispatched")
    result["stage"] = "before_mutation_checkpoint"
    result["data"]["pending_mutation"] = {"operation": api.BINDINGS[0], **copy.deepcopy(arguments),
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "inspect_actual_composite_rollback_and_save_prefix_do_not_replay"}
    callback(result)
    result["stage"] = "native_material_function_expansion"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": api.BINDINGS[0], "asset_path": arguments["material_path"],
        "attempted": None, "change_state_known": False, "changed": None}]
    try:
        native = api.expand_material_function_calls(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["compile"]["status"] = "native_returned_unparsed"
            result["save"]["status"] = "native_returned_unparsed"
            result["stage"] = "returned_material_function_expansion_operation"
            callback(result)
        if getattr(error, "code", "") == "material_function_expansion_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_material_function_expansion_failed",
            native.get("error") or "Native selected expansion/source/rollback/save prefix failed; retained without replay")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("material_function_expansion_postcondition_unverified",
            "Native completed report lacks known bounded graph/source proof; no retry")
    # Original structured errors, skipped/rolled-back expansions and savedFalse
    # remain data. Do not invent all-expanded, shader or persistence success.
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "composite_allocation_retirement_callbacks_rollback_save_would_repeat_inspect_prefix"


def expand_material_function_calls(material_path, node_id="", recursive=True, exclude_engine_functions=True,
        max_passes=8, save=True, allow_partial_save=False):
    try:
        arguments = _arguments(material_path, node_id, recursive, exclude_engine_functions, max_passes, save, allow_partial_save)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        result["data"].update(operation=api.BINDINGS[0], admission_rejected=True, native_dispatched=False,
            native_result_returned=False, native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
        return result
    return state.mutate(api.BINDINGS[0], arguments, None, lambda result: _run_action(result, arguments))


def expand_material_function_calls_action(result, material_path, node_id="", recursive=True, exclude_engine_functions=True,
        max_passes=8, save=True, allow_partial_save=False, checkpoint_callback=None):
    _run_action(result, _arguments(material_path, node_id, recursive, exclude_engine_functions,
        max_passes, save, allow_partial_save), checkpoint_callback)
