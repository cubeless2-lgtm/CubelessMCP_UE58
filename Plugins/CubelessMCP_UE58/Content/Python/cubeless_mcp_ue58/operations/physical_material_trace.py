"""One trace result in a Named journal or the existing Batch parent.

No explicit asset load, edit, compile or save occurs in Python. The original
Native actor resolution/default-label semantics may materialize SDK state, so
query completion is separate from total change-state knowledge.
"""
import copy
import math
from typing import Any, Dict

from cubeless_mcp_ue58.compatibility.ue58 import physical_material_trace as api
from . import state
from .state import OperationError

METHOD = "line_trace_physical_material"
PUBLIC_FIELDS = ("start", "end", "trace_channel", "trace_complex", "return_physical_material")


def _arguments(start, end, trace_channel, trace_complex, return_physical_material):
    vectors = {}
    for field, value in (("start", start), ("end", end)):
        if not isinstance(value, list) or len(value) != 3:
            raise OperationError("editor_invalid_vector_parameter", "Invalid " + field + " format. Must be a list of 3 float values.")
        # Preserve the original Python float(v) conversion; malformed/non-finite
        # values are rejected before Native dispatch, not repaired or defaulted.
        try:
            vector = [float(item) for item in value]
        except (TypeError, ValueError, OverflowError) as error:
            raise OperationError("editor_invalid_vector_parameter", "Invalid " + field + " coordinates: " + str(error)) from error
        if not all(math.isfinite(item) for item in vector):
            raise OperationError("editor_invalid_vector_parameter", field + " coordinates must be finite")
        vectors[field] = vector
    if type(trace_channel) is not str:
        raise OperationError("physical_material_trace_input_invalid", "trace_channel must be a string")
    if type(trace_complex) is not bool or type(return_physical_material) is not bool:
        raise OperationError("physical_material_trace_input_invalid", "trace_complex and return_physical_material must be booleans")
    return dict(start=vectors["start"], end=vectors["end"], trace_channel=trace_channel,
        trace_complex=trace_complex, return_physical_material=return_physical_material)


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_five_public_parameters_forwarded=True,
        native_world_trace_hit_and_body_fallback_authoritative=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_modify_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        query_completion_proves_total_sdk_purity=False,
        automatic_action_retry=False, atomic_rollback=False,
        actual_named_vector_wire_verified=False, unique_external_pipeline_complete=False)


def _adopt(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_trace_compile_receipt")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_trace_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": METHOD, "world_context": native["data"].get("world_context"),
        "attempted": native["mutation_attempted"], "changed": native["changed"] if known else None,
        "change_state_known": known, "observed_actor_label_initialized": native["data"].get("observed_actor_label_initialized")}]
    result["validation"].update(status="native_trace_query_and_result_postcondition",
        native_postcondition_passed=native["postcondition_passed"], blocking_hit=native["data"].get("blocking_hit"),
        query_miss_is_operation_failure=False, total_change_state_known=known,
        all_implicit_sdk_effects_observed=False, disk_reload="not_applicable_no_requested_save",
        packaged_runtime="not_run", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,
        duplicate_risk="unknown_world_actor_material_or_label_callbacks_inspect_prefix_no_replay" if not known else "none_observed")
    result["stage"] = "returned_physical_material_trace_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_physical_material_trace_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": METHOD, "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_physical_material_trace_operation"
    try:
        native = api.line_trace_physical_material(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_physical_material_trace_operation"
            callback(result)
        if getattr(error, "code", "") == "physical_material_trace_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        raise
    _adopt(result, arguments, native)
    callback(result) # Preserve actual raw/partial/unknown/compile/save prefix before errors.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "physical_material_trace_native_failed",
            native.get("error") or "Native trace failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("physical_material_trace_result_unverified", "Native trace postcondition is unverified; no replay")
    result.update(success=True, stage="complete")
    return result


def _rejected(error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=METHOD, admission_rejected=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def line_trace_physical_material(start: list[float], end: list[float], trace_channel: str = "visibility",
        trace_complex: bool = False, return_physical_material: bool = True) -> Dict[str, Any]:
    try:
        arguments = _arguments(start, end, trace_channel, trace_complex, return_physical_material)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def line_trace_physical_material_action(result, start: list[float], end: list[float], trace_channel: str = "visibility",
        trace_complex: bool = False, return_physical_material: bool = True, checkpoint_callback=None):
    return _run_action(result, _arguments(start, end, trace_channel, trace_complex, return_physical_material), checkpoint_callback)
