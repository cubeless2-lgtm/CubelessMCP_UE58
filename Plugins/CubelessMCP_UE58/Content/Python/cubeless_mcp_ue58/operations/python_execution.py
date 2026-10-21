"""UP original Python execution caller5 and one journal-free Batch child action.

The existing Native core owns execution, accident guards and transaction cleanup.
Policy none is a trusted no-import declaration, never a sandbox or purity proof.
Standalone import/deferred dispatch belongs to the Raw tool, not this sync seam.
"""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import python_execution as api
from . import state
from .state import OperationError


def execute_unreal_python(code, mode="ExecuteFile", defer_to_ticker=False,
                          allow_unsafe_editor_scripting_during_pie=False,
                          interchange_policy="auto"):
    arguments = dict(code=code, mode=mode, defer_to_ticker=defer_to_ticker,
                     allow_unsafe_editor_scripting_during_pie=allow_unsafe_editor_scripting_during_pie,
                     interchange_policy=interchange_policy)
    return state.mutate("execute_unreal_python", arguments, None,
                        lambda result: execute_unreal_python_action(result, **arguments, checkpoint_callback=state.checkpoint))


def execute_unreal_python_action(result, code, mode="ExecuteFile", defer_to_ticker=False,
                                 allow_unsafe_editor_scripting_during_pie=False,
                                 interchange_policy="auto", checkpoint_callback=None):
    return _execute_action(result, code, mode, defer_to_ticker, allow_unsafe_editor_scripting_during_pie,
                           interchange_policy, "command", False, checkpoint_callback, "execute_unreal_python")


def execute_python_action(result, code, mode="ExecuteStatement", defer_to_ticker=False,
                          allow_unsafe_editor_scripting_during_pie=False,
                          interchange_policy="auto", transaction_policy="command",
                          allow_unsafe_pcg_selector_python=False, checkpoint_callback=None):
    return _execute_action(result, code, mode, defer_to_ticker, allow_unsafe_editor_scripting_during_pie,
                           interchange_policy, transaction_policy, allow_unsafe_pcg_selector_python,
                           checkpoint_callback, "execute_python")


def _execute_action(result, code, mode, defer_to_ticker, allow_unsafe_editor_scripting_during_pie,
                    interchange_policy, transaction_policy, allow_unsafe_pcg_selector_python,
                    checkpoint_callback, method):
    # Public and raw command fields stay separate at their explicit entry points.
    if any(type(value) is not str for value in (code, mode, interchange_policy, transaction_policy)):
        raise OperationError("invalid_python_argument", "Original code/mode/interchange_policy strings required")
    if any(type(value) is not bool for value in (defer_to_ticker, allow_unsafe_editor_scripting_during_pie, allow_unsafe_pcg_selector_python)):
        raise OperationError("invalid_python_argument", "Original execution flags must be Boolean")
    policy = interchange_policy.strip().lower()
    data = result["data"]
    data.update(public_method=method, original_command="execute_python",
                requested_arguments=dict(code=code, mode=mode, defer_to_ticker=defer_to_ticker,
                    allow_unsafe_editor_scripting_during_pie=allow_unsafe_editor_scripting_during_pie,
                    interchange_policy=interchange_policy),
                native_command_options=dict(transaction_policy=transaction_policy, allow_unsafe_pcg_selector_python=allow_unsafe_pcg_selector_python),
                native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
                execution_mutation_attempted=False, change_state_known=True, mutation_state_unknown=False,
                mutation_performed=False, full_effect_state_known=False, automatic_action_retry=False,
                explicit_compile_save_owned_by_script=True, script_compile_save_state_observed=False,
                synchronous_nested_entry=True, standalone_import_deferred_supported=False,
                no_import_declaration_is_not_sandbox=True, server_RPC_dependency=False,
                atomic_rollback=False, complete_phase_verified=False)
    result["compile"].update(requested=False, attempted=False, status="not_explicitly_requested")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_explicitly_requested")
    result["stage"] = "before_python_execution_checkpoint"
    if checkpoint_callback:
        checkpoint_callback(result)
    # A durable intent precedes the SDK call. A lost/invalid return must never
    # turn arbitrary code into a replay-safe, known-zero operation.
    data.update(change_state_known=False, mutation_state_unknown=True, mutation_performed=None,
                native_mutation_attempted=None, execution_mutation_attempted=None)
    result["changes"] = [dict(kind="execute_python", attempted=False, changed=None, change_state_known=False)]
    result["stage"] = "python_execution_dispatch_intent"
    if checkpoint_callback:
        checkpoint_callback(result)
    data["native_dispatched"] = True
    try:
        actual, raw = api.execute_python(code, mode, defer_to_ticker,
            allow_unsafe_editor_scripting_during_pie, policy, transaction_policy, allow_unsafe_pcg_selector_python)
    except Exception as error:
        if getattr(error, "native_call_dispatched", None) is False:
            data.update(native_dispatched=False, native_mutation_attempted=False, execution_mutation_attempted=False,
                        change_state_known=True, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        else:
            data.update(native_result_returned=getattr(error, "native_call_returned", False),
                        native_raw_result=getattr(error, "native_raw_result", None))
            result["compile"].update(attempted=None, status="script_dispatch_outcome_unknown")
            result["save"].update(attempted=None, saved=None, status="script_dispatch_outcome_unknown")
        result["stage"] = "returned_python_execution_operation"
        result["retry"].update(automatic_retry=False, duplicate_risk="inspect_arbitrary_execution_before_new_call")
        data["adapter_exception_type"] = type(error).__name__
        if checkpoint_callback:
            checkpoint_callback(result)
        if isinstance(error, OperationError):
            raise
        raise OperationError("execute_unreal_python_failed", str(error)) from error
    # Native continuation readiness is narrower than global effect knowledge.
    # A completed none command may continue even though performed remains null.
    unknown = (actual["change_state_known"] is not True or actual["mutation_state_unknown"] is True
               or actual.get("command_may_still_be_in_flight") is True)
    attempted = actual["mutation_attempted"]
    data.update(legacy_result=copy.deepcopy(actual), native_raw_result=raw, native_result_returned=True,
                native_outcome={key: copy.deepcopy(actual[key]) for key in (
                    "success", "python_executed", "mutation_attempted", "mutation_performed", "change_state_known",
                    "mutation_state_unknown", "command_may_still_be_in_flight", "full_effect_state_known",
                    "python_execution_success", "do_not_retry", "partial_mutation_possible") if key in actual},
                native_mutation_attempted=attempted, execution_mutation_attempted=attempted,
                change_state_known=not unknown, mutation_state_unknown=unknown,
                mutation_performed=actual["mutation_performed"] if not unknown else None,
                command_may_still_be_in_flight=actual.get("command_may_still_be_in_flight") is True)
    result["changes"] = ([] if not attempted and not unknown else [dict(kind="execute_python", attempted=attempted,
        changed=data["mutation_performed"], change_state_known=not unknown)])
    if actual["python_executed"]:
        result["compile"].update(attempted=None, status="script_owned_unobserved")
        result["save"].update(attempted=None, saved=None, status="script_owned_unobserved")
    result["stage"] = "returned_python_execution_operation"
    result["retry"].update(automatic_retry=False, duplicate_risk="unknown_outcome" if unknown else
        "arbitrary_code_may_have_changed_state" if attempted else "none_observed")
    result["validation"].update(status="native_synchronous_execution_receipt", continuation_state_known=not unknown,
        full_effect_state_known=False, arbitrary_code_is_not_a_security_sandbox=True)
    if checkpoint_callback:
        checkpoint_callback(result)
    if unknown:
        raise OperationError(actual.get("error_code") or "python_execution_outcome_unknown",
                             actual.get("error") or "Execution outcome unknown; inspect retained receipt before a new request")
    if actual["success"] is not True:
        raise OperationError(actual.get("error_code") or "execute_unreal_python_failed",
                             actual.get("error") or actual.get("command_result") or "Python execution failed")
    result.update(success=True, stage="complete")
    return result
