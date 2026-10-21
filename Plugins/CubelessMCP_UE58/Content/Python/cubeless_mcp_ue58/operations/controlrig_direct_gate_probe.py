"""Direct gate probe8 with one Named journal or an existing Batch parent.

Only the two transient ControlRig probe operations share the helpers below.
Native owns resolution, fresh rigs per case, Initialize/Execute, input reapply,
case/sample diagnostics, lifetime/dirty checks and actual partial outcomes.
"""
import copy
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import controlrig_direct_gate_probe as api
from . import state
from .state import OperationError

METHOD = "controlrig_direct_gate_probe"
STAGE_PREFIX = "controlrig_direct_gate_probe"
PUBLIC_FIELDS = ("control_rig_path", "control_rig_class", "cases", "sample_elements",
    "control_values", "execute_events", "should_trace_property", "interaction_location_property")
TEXT_FIELDS = ("control_rig_path", "control_rig_class", "should_trace_property", "interaction_location_property")


def _prepare_probe_arguments(values, array_fields, object_fields):
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:
            raise OperationError("controlrig_transient_probe_input_invalid", field + " must be a string; None is not an empty path or property")
    for field in array_fields:
        if values[field] is not None and type(values[field]) is not list:
            raise OperationError("controlrig_transient_probe_input_invalid", field + " must be None or a JSON array")
    for field in object_fields:
        if values[field] is not None and type(values[field]) is not dict:
            raise OperationError("controlrig_transient_probe_input_invalid", field + " must be None or a JSON object")
    events = values["execute_events"]
    if events is not None and any(type(event) is not str for event in events):
        raise OperationError("controlrig_transient_probe_input_invalid", "execute_events must contain strings")
    # None/[]/{} remain distinct. Native alone selects default cases/events,
    # validates rows, names, curves/controls and interprets aliases or values.
    for field in array_fields + object_fields:
        api._carrier(values[field])
    return copy.deepcopy(values)


def _arguments(control_rig_path, control_rig_class, cases, sample_elements, control_values,
        execute_events, should_trace_property, interaction_location_property):
    values = dict(control_rig_path=control_rig_path, control_rig_class=control_rig_class,
        cases=cases, sample_elements=sample_elements, control_values=control_values,
        execute_events=execute_events, should_trace_property=should_trace_property,
        interaction_location_property=interaction_location_property)
    return _prepare_probe_arguments(values, ("cases", "sample_elements", "execute_events"), ("control_values",))


def _probe_context(result, arguments, method):
    result["data"].update(operation=method, requested_arguments=copy.deepcopy(arguments),
        public_parameter_count=len(arguments), nullable_none_empty_list_and_empty_object_preserved=True,
        prepost_None_input_and_curve_defaults_normalized_only_when_None=(method == "sample_controlrig_pre_post_runtime_pose"),
        native_resolution_case_event_input_selection_and_sampling_authoritative=True,
        native_case_error_warning_partial_samples_and_baseline_rules_authoritative=True,
        native_transient_creation_Initialize_Execute_and_reapply_counters_preserved=True,
        adapter_asset_load_calls=0, adapter_Initialize_calls=0, adapter_Execute_calls=0,
        adapter_property_or_hierarchy_write_calls=0, adapter_compile_calls=0, adapter_save_calls=0,
        transient_read_only_label_is_total_SDK_purity_proof=False,
        unchanged_asset_dirty_is_total_SDK_purity_proof=False,
        compiled_AnimGraph_prepost_instrumented=False,
        execution_warning_or_invalid_sample_alone_promoted_to_operation_error=False,
        actual_named_nullable_wire_verified=False, automatic_action_retry=False, atomic_rollback=False,
        unique_external_pipeline_complete=False)


def _adopt_probe_result(result, arguments, native, method, stage_prefix):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native["changed"] if known else None,
        observed_state_changed=native["changed"] if known else None)
    _probe_context(result, arguments, method)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_transient_probe_compile_receipt",
        completion_scope="Native_explicit_receipt; implicit_load_Initialize_Execute_and_callback_effects_not_inferred")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
        saved=native["saved"], status="native_transient_probe_save_receipt")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": method,
        "requested_paths": {field: arguments[field] for field in ("control_rig_path", "control_rig_class")},
        "attempted": native["mutation_attempted"], "changed": native["changed"] if known else None,
        "change_state_known": known, "observed_explicit_change": native["data"].get("observed_explicit_change"),
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_transient_ControlRig_probe_completion",
        native_postcondition_passed=native["postcondition_passed"],
        case_count=native["data"].get("case_count"),
        native_cases_errors_warnings_echoes_samples_and_deltas_preserved=True,
        success_not_inferred_from_warning_counts_or_sample_presence=True,
        total_change_state_known=known, all_implicit_SDK_effects_observed=False,
        compiled_AnimGraph_prepost="not_instrumented", unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False, atomic_rollback=False,
        duplicate_risk="unknown_transient_Initialize_Execute_loader_and_callback_effects_inspect_prefix_no_replay" if not known
            else "observed_transient_or_asset_changes_inspect_prefix_before_new_call" if native["mutation_attempted"] or native["changed"] else "none_observed")
    result["stage"] = "returned_" + stage_prefix + "_operation"


def _run_probe_action(result, arguments, invoke, method, stage_prefix, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _probe_context(result, arguments, method)
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_operation=copy.deepcopy(arguments))
    result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
    result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
    result["stage"] = "before_" + stage_prefix + "_operation"
    callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False,
        native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["changes"] = [{"kind": method,
        "requested_paths": {field: arguments[field] for field in ("control_rig_path", "control_rig_class")},
        "attempted": None, "changed": None, "change_state_known": False}]
    result["compile"].update(requested=None, attempted=None, compiled=None, status="native_dispatch_outcome_unknown")
    result["save"].update(requested=None, attempted=None, saved=None, status="native_dispatch_outcome_unknown")
    result["stage"] = "native_" + stage_prefix + "_operation"
    try:
        native = invoke(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "controlrig_transient_probe_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(requested=False, attempted=False, compiled=None, status="native_not_dispatched")
            result["save"].update(requested=False, attempted=False, saved=False, status="native_not_dispatched")
        result["stage"] = "returned_" + stage_prefix + "_operation"
        callback(result)
        raise
    _adopt_probe_result(result, arguments, native, method, stage_prefix)
    callback(result)  # Entire Native actual/partial/raw/compile-save/unknown prefix precedes failure.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "controlrig_transient_probe_native_failed",
            native.get("error") or "Native transient probe failed; actual partial prefix retained")
    if not native["postcondition_passed"]:
        raise OperationError("controlrig_transient_probe_result_unverified", "Native transient probe completion is unverified; no replay")
    result.update(success=True, stage="complete")
    return result


def _adopt_probe_rejection(result, error, method, stage_prefix):
    result = state.fail(result, error)
    result["stage"] = stage_prefix + "_input_validation"
    result["data"].update(operation=method, admission_rejected=True,
        native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, journal_created=False)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="input_rejected_no_dispatch")
    result["save"].update(requested=False, attempted=False, saved=False, status="input_rejected_no_dispatch")
    result["validation"].update(status="error", diagnostic_completed=False, total_change_state_known=True)
    return result


def _rejected(error):
    return _adopt_probe_rejection(state.new_result(), error, METHOD, STAGE_PREFIX)


def controlrig_direct_gate_probe(control_rig_path: str = "", control_rig_class: str = "",
        cases: Optional[List[Dict[str, Any]]] = None, sample_elements: Optional[List[Any]] = None,
        control_values: Optional[Dict[str, Any]] = None, execute_events: Optional[List[str]] = None,
        should_trace_property: str = "ShouldDoIKTrace",
        interaction_location_property: str = "InteractionWorldLocation") -> Dict[str, Any]:
    try:
        arguments = _arguments(control_rig_path, control_rig_class, cases, sample_elements,
            control_values, execute_events, should_trace_property, interaction_location_property)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None,
        lambda result: _run_probe_action(result, arguments, api.controlrig_direct_gate_probe, METHOD, STAGE_PREFIX))


def controlrig_direct_gate_probe_action(result, control_rig_path: str = "", control_rig_class: str = "",
        cases: Optional[List[Dict[str, Any]]] = None, sample_elements: Optional[List[Any]] = None,
        control_values: Optional[Dict[str, Any]] = None, execute_events: Optional[List[str]] = None,
        should_trace_property: str = "ShouldDoIKTrace",
        interaction_location_property: str = "InteractionWorldLocation", checkpoint_callback=None):
    try:
        arguments = _arguments(control_rig_path, control_rig_class, cases, sample_elements,
            control_values, execute_events, should_trace_property, interaction_location_property)
    except Exception as error:
        _adopt_probe_rejection(result, error, METHOD, STAGE_PREFIX)
        result["data"]["parent_journal_only"] = True
        result["stage"] = "returned_" + STAGE_PREFIX + "_operation"
        (checkpoint_callback or state.checkpoint)(result)
        raise OperationError(result["error_code"], result["error"]) from error
    return _run_probe_action(result, arguments, api.controlrig_direct_gate_probe, METHOD, STAGE_PREFIX, checkpoint_callback)
