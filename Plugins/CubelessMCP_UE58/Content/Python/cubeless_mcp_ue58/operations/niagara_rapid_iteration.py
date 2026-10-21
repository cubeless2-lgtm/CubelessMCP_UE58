"""One current read of RI stores/graphs/status, never a dummy edit or save."""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import niagara_rapid_iteration as api
from . import state
from .state import OperationError

IDENTITY = "inspect_niagara_module_rapid_iteration"


def inspect_niagara_module_rapid_iteration(system_path):
    result = state.new_result()
    result["data"].update(operation=IDENTITY, read_only=True, native_dispatched=False,
        native_result_returned=False, native_mutation_attempted=False,
        mutation_state_unknown=False, mutation_performed=False, adapter_explicit_load_calls=0,
        adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        runtime_or_cold_verified=False)
    try:
        if type(system_path) is not str:
            raise OperationError("niagara_rapid_iteration_read_input_invalid", "Literal already-loaded system path required")
        result["data"].update(requested_system_path=system_path, native_dispatched=True,
            mutation_state_unknown=True, mutation_performed=None)
        result["stage"] = "native_niagara_rapid_iteration_read"
        native = api.inspect_niagara_module_rapid_iteration(system_path)
        result["data"].update(copy.deepcopy(native["data"]))
        known = native["change_state_known"]
        result["data"].update(native_raw_result=native["_raw_native_json"],
            native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
            native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
            mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
        result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
            compiled=native["compiled"], kind="niagara_observation", status="native_current_status_observed")
        result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"],
            saved=native["saved"], status="native_returned_saved" if native["saved"] else "native_not_attempted")
        result["changes"] = [] if known and native["changed"] is False else [{"kind": "read_scope_drift",
            "asset_path": system_path, "attempted": native["mutation_attempted"], "change_state_known": known,
            "changed": native["changed"] if known else None, "outcome": "actual_read_scope_observation"}]
        result["validation"].update(status="native_rapid_iteration_read_postcondition" if native["postcondition_passed"] else "native_rapid_iteration_read_failed",
            native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run", runtime="not_run")
        result["stage"] = "returned_niagara_rapid_iteration_read"
        if native["mutation_attempted"] or native["compile_attempted"] or native["save_attempted"]:
            raise OperationError("unexpected_niagara_rapid_iteration_read_work", "Actual unexpected dispatch prefix retained; no second action")
        if not native["success"]:
            raise OperationError(native.get("error_code") or "niagara_rapid_iteration_read_failed", native.get("error") or "Actual read prefix retained")
        if not known or not native["postcondition_passed"]:
            raise OperationError("niagara_rapid_iteration_read_unverified", "Actual read lacks complete scoped proof")
        result.update(success=True, stage="complete")
        result["retry"]["duplicate_risk"] = "read_only_no_automatic_retry"
        return result
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "code", "") == "native_niagara_rapid_iteration_read_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
        return state.fail(result, error)
