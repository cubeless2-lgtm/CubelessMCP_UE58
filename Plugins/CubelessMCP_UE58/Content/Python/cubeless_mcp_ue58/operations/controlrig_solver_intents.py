"""Solver-intent policy in Python; one Named journal or an existing Batch parent.

The hidden Native helper supplies scalar text semantics only. It does not
execute the validator, query an asset/world, load, mutate, compile, save or solve.
"""
import copy
from typing import Any, Dict

from cubeless_mcp_ue58.compatibility.ue58 import controlrig_solver_intents as api
from . import state
from .state import OperationError

METHOD = "validate_controlrig_solver_intents"
PUBLIC_FIELDS = ("request",)


def _arguments(request):
    # Match the original wrapper: parse strings to an object, then recursively
    # reject MCP dependency references before entering the operation/journal.
    return {"request": api.prepare_request(request)}


def _context(result, arguments):
    result["data"].update(operation=METHOD, requested_arguments=copy.deepcopy(arguments),
        original_public_request_object_or_string_contract=True,
        request_normalized_to_object_before_validation=True,
        execution_backend="python_validation_with_UE58_text_semantics_batch",
        validator_registration_identifier=api.VALIDATOR,
        pure_python_validator=True, pure_python_validator_scope="solver_policy_and_issue_report",
        native_operation_library_required=False, actual_native_operation_dispatch=False,
        native_dispatched=False, native_result_returned=False,
        text_semantics_helper_library=api.TEXT_LIBRARY,
        native_text_semantics_required=True,
        legacy_adapter_identifier="unrealmcp_native",
        legacy_adapter_is_actual_native_execution_evidence=False,
        legacy_scope_is_current_external_server_dependency=False,
        asset_query_calls=0, asset_load_calls=0, graph_mutation_calls=0,
        compile_calls=0, save_calls=0, runtime_execute_calls=0, runtime_solve_calls=0,
        mutation_attempted=False, mutation_performed=False, observed_state_changed=False,
        mutation_state_unknown=False, change_state_known=True,
        change_state_scope="validator_asset_graph_compile_save_execute_solve_work",
        total_Engine_global_text_cache_purity_claimed=False,
        journal_framework_metadata_queries="existing_state_module_project_saved_dir_and_engine_version; not_validator_asset_queries",
        atomic_rollback=False, automatic_action_retry=False,
        actual_named_object_string_wire_verified=False,
        runtime_pose_verified=False, playback_verified=False, unique_external_pipeline_complete=False)
    result["changes"] = []
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested_pure_validation")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested_pure_validation")
    result["retry"].update(automatic_retry=False, duplicate_risk="none_observed", atomic_rollback=False)


def _adopt(result, arguments, report):
    legacy = report["legacy_result"]
    result["data"].update(copy.deepcopy(legacy))
    result["data"].update(legacy_result=copy.deepcopy(legacy),
        text_semantics_receipt=copy.deepcopy(report["text_semantics"]),
        native_text_dispatched=True, native_text_result_returned=True,
        actual_text_helper_calls=1, validation_completed=True,
        issue_order_and_intent_partial_subset_preserved=True)
    _context(result, arguments)
    result["validation"].update(status="pass" if legacy["success"] else "fail",
        diagnostic_completed=True, validation_only=True, sample_live_proof="not_run",
        issue_count=legacy["issue_count"], validated_intent_count=len(legacy["validated_intents"]),
        warning_count=len(legacy["warnings"]), build_allowed=False, asset_mutation_allowed=False,
        solver_units_added=False, total_change_state_known=True,
        runtime_pose="not_run", playback="not_run", unique_external_pipeline="not_run")
    result["stage"] = "returned_controlrig_solver_intents_validation"


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    _context(result, arguments)
    result["data"].update(validation_completed=False, native_text_dispatched=False,
        native_text_result_returned=False, actual_text_helper_calls=0,
        pending_operation=copy.deepcopy(arguments))
    result["stage"] = "before_controlrig_solver_intents_checkpoint"
    callback(result)
    result["stage"] = "python_controlrig_solver_intents_validation"
    # Asset mutation remains known-false even if a text-only receipt fails. Do
    # not imply that an uncertain helper dispatch was absent, or retry it.
    result["data"].update(native_text_dispatched=None, native_text_result_returned=None,
        actual_text_helper_calls=None)
    try:
        report = api.validate_prepared_request(arguments["request"])
    except Exception as error:
        result["data"].update(legacy_result=api.legacy_exception_result(error),
            validation_completed=False, input_or_text_error_code=getattr(error, "code", "python_validation_failed"),
            native_text_dispatched=False if getattr(error, "code", "") == "controlrig_solver_text_api_unavailable" else None,
            native_text_result_returned=None, actual_text_helper_calls=None)
        result["validation"].update(status="error", diagnostic_completed=False,
            total_change_state_known=True, sample_live_proof="not_run")
        result["stage"] = "returned_controlrig_solver_intents_validation"
        callback(result)
        raise OperationError("controlrig_solver_intent_validation_failed", str(error)) from error
    _adopt(result, arguments, report)
    callback(result)  # Full original issues/warnings/partial intents precede error.
    if not report["legacy_result"]["success"]:
        raise OperationError("controlrig_solver_intent_validation_failed",
            "Solver intent validation reported " + str(report["legacy_result"]["issue_count"])
            + " issue(s); inspect the ordered legacy result and validated_intents subset")
    result.update(success=True, stage="complete")
    return result


def _adopt_rejection(result, error):
    failure = error if isinstance(error, OperationError) else OperationError(
        "controlrig_solver_intent_validation_failed", str(error))
    result["changes"] = []
    result = state.fail(result, failure)
    result["stage"] = "controlrig_solver_intent_input_validation"
    result["data"].update(operation=METHOD, admission_rejected=True,
        legacy_result=api.legacy_exception_result(error),
        input_error_code=getattr(error, "code", "controlrig_solver_intent_validation_failed"),
        execution_backend="python_validation_with_UE58_text_semantics_batch",
        native_operation_library_required=False, actual_native_operation_dispatch=False,
        native_dispatched=False, native_result_returned=False,
        native_text_dispatched=False, native_text_result_returned=False, actual_text_helper_calls=0,
        mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
        change_state_known=True, journal_created=False,
        asset_query_calls=0, asset_load_calls=0, compile_calls=0, save_calls=0)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested_input_rejected")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested_input_rejected")
    result["validation"].update(status="error", diagnostic_completed=False, total_change_state_known=True)
    return result


def _rejected(error):
    return _adopt_rejection(state.new_result(), error)

def validate_controlrig_solver_intents(request: Dict[str, Any] | str) -> Dict[str, Any]:
    try:
        arguments = _arguments(request)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None, lambda result: _run_action(result, arguments))


def validate_controlrig_solver_intents_action(result, request: Dict[str, Any] | str, checkpoint_callback=None):
    try:
        arguments = _arguments(request)
    except Exception as error:
        # Reuse the Named rejection payload in the existing parent-owned result.
        # Do not create a new operation_id, Native call or child journal.
        _adopt_rejection(result, error)
        result["data"]["parent_journal_only"] = True
        result["stage"] = "returned_controlrig_solver_intents_validation"
        callback = checkpoint_callback or state.checkpoint
        callback(result)
        raise OperationError(result["error_code"], result["error"]) from error
    return _run_action(result, arguments, checkpoint_callback)
