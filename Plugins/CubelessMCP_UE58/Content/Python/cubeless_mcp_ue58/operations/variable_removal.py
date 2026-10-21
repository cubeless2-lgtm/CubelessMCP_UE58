"""Original selected variable removal on one named or parent Batch journal.

Native owns descriptor deletion and public Editor structural/cache callbacks.
The current generated property can remain stale; no fullcompile/save/layout
absence, runtime, cold or original mutation_result_v1 is inferred here.
"""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import variable_removal as api
from . import variables, state
from .datatable import _reference
from .state import OperationError

IDENTITY = "remove_blueprint_variable"


def _arguments(blueprint_name, variable_name):
    name = variables._text(variable_name,"variable_name")
    try:
        _reference(name,"variable_name")
    except OperationError as error:
        raise OperationError("variable_reference_unsupported",str(error)) from error
    return {"blueprint_path":variables._path(blueprint_name,write=True),"variable_name":name}


def _context(result, arguments):
    result["data"].update(operation=IDENTITY,requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        automatic_action_retry=False,full_original_variable_removal_parity=False,
        actual_work_counts_authority="returned_native_counters_and_observation_prefix",
        native_helpers_may_request_structural_refresh_or_change_caches=True,
        implicit_skeleton_compiler_and_callback_work_exhaustively_instrumented=False,
        descriptor_removal_is_not_generated_runtime_layout_absence=True,
        simple_name_resolution_scope="Existing variables._path maps a simple leaf to Game/Blueprints; wider original lookup is not claimed",
        runtime_property_application_verified=False,cold_reload_verified=False)


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native),native_raw_result=native["_raw_native_json"],
        native_outcome={key:copy.deepcopy(value) for key,value in native.items() if key not in ("data","_raw_native_json")},
        native_result_returned=True,native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known,mutation_performed=native["changed"] if known else None)
    _context(result,arguments)
    result["compile"].update(requested=native["compile_requested"],attempted=native["compile_attempted"],
        compiled=native["compiled"],status="native_returned_compiled" if native["compiled"] is True else
        "native_returned_failed" if native["compile_attempted"] else "native_not_attempted")
    result["save"].update(requested=native["save_requested"],attempted=native["save_attempted"],saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_failed" if native["save_attempted"] else "native_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind":IDENTITY,"asset_path":arguments["blueprint_path"],
        "attempted":native["mutation_attempted"],"change_state_known":known,"changed":native["changed"] if known else None,
        "outcome":"native_scoped_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_variable_removal_postcondition" if native["postcondition_passed"] else
        "native_variable_removal_postcondition_failed",native_postcondition_passed=native["postcondition_passed"],disk_reload="not_run",runtime="not_run")
    result["stage"] = "returned_variable_removal_operation"


def remove_blueprint_variable_action(result, blueprint_name, variable_name, checkpoint_callback=None):
    arguments = _arguments(blueprint_name,variable_name)
    _context(result,arguments)
    result["compile"].update(requested=False,attempted=False,compiled=None,status="not_dispatched")
    result["save"].update(requested=False,attempted=False,saved=False,status="not_dispatched")
    result["data"]["pending_mutation"] = {"operation":IDENTITY,**arguments,"dispatch_state_at_checkpoint":"not_yet_dispatched"}
    result["stage"] = "before_mutation_checkpoint"
    callback = checkpoint_callback or state.checkpoint
    callback(result)
    result["data"].update(native_dispatched=True,native_result_returned=False,mutation_state_unknown=True,mutation_performed=None)
    result["stage"] = "native_variable_removal_operation"
    result["changes"] = [{"kind":IDENTITY,"attempted":None,"changed":None,"change_state_known":False,"outcome":"unknown_until_native_response"}]
    result["compile"].update(attempted=None,compiled=None,status="unknown_until_native_returns")
    result["save"].update(attempted=None,saved=None,status="unknown_until_native_returns")
    try:
        native = api.remove_blueprint_variable(**arguments)
    except Exception as error:
        if getattr(error,"native_call_returned",False):
            result["data"].update(native_result_returned=True,native_raw_result=getattr(error,"native_raw_result",None))
            result["stage"] = "returned_variable_removal_operation"
            callback(result)
        if getattr(error,"code","") == "native_variable_removal_api_unavailable":
            result["data"].update(native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False,compiled=None,status="native_not_dispatched")
            result["save"].update(attempted=False,saved=False,status="native_not_dispatched")
        raise
    _apply_native(result,arguments,native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_variable_removal_failed",native.get("error") or "Actual removal prefix retained; no replay")
    if not (native["change_state_known"] and native["postcondition_passed"] and native["mutation_attempted"]
            and native["changed"] is True and native["data"].get("declaration_removed") is True):
        raise OperationError("variable_removal_result_unverified","Native success lacks known removal proof; actual prefix retained")
    if (native["compile_requested"] or native["compile_attempted"] or native["compiled"] is not None
            or native["save_requested"] or native["save_attempted"] or native["saved"]):
        raise OperationError("variable_removal_unexpected_compile_save","Unexpected actual prefix retained; original Remove2 has no explicit compile/save")
    result.update(success=True,stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_declaration_current_generated_property_and_journal_no_removal_replay"


def remove_blueprint_variable(blueprint_name, variable_name):
    try:
        arguments = _arguments(blueprint_name,variable_name)
    except Exception as error:
        result = state.fail(state.new_result(),error)
        result["data"].update(operation=IDENTITY,admission_rejected=True,native_dispatched=False,
            native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
        return result
    return state.mutate(IDENTITY,arguments,None,lambda result:remove_blueprint_variable_action(result,blueprint_name,variable_name))
