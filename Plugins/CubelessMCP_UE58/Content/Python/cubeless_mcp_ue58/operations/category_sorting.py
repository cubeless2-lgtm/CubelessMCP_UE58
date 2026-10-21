"""Original category display order on one named or parent Batch journal.

CategorySorting is separate from member declaration order. Native owns original
trim/root-category/FName normalization, changed-only compile and matching-dirty
save work. Actual returned phases/counts/save never follow intent alone.
"""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import category_sorting as api
from . import variables, state
from .state import OperationError

IDENTITY = api.BINDINGS[0]


def _arguments(blueprint_name, category_order, category_to_front, append_existing, save, compile):
    if category_order is not None and (type(category_order) is not list or len(category_order)>4096
            or any(type(value) is not str for value in category_order)):
        raise OperationError("category_sorting_input_invalid","category_order requires an optional bounded JSON string array")
    # Original Python omitted None and empty arrays. Front text is forwarded;
    # root delimiter/whitespace/FName uniqueness belong to the native handler.
    order = None if not category_order else category_order
    encoded = variables._json_text(order)
    variables._json_value(encoded,"category_order")
    return {"blueprint_path":variables._path(blueprint_name,write=True),"category_order_json":encoded,
        "category_to_front":variables._text(category_to_front,"category_to_front",empty=True),
        "append_existing":variables._boolean(append_existing,"append_existing"),
        "save":variables._boolean(save,"save"),"compile":variables._boolean(compile,"compile")}


def _context(result, arguments):
    result["data"].update(operation=IDENTITY,requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        automatic_action_retry=False,full_original_category_sorting_parity=False,
        category_display_order_is_not_member_declaration_order=True,
        dirty_matching_save_may_be_real_work_without_category_change=True,
        internal_RPC_category_alias_mapped=False,
        actual_work_counts_authority="returned_native_counters_phases_and_ordered_save_receipts",
        implicit_compiler_cache_callbacks_exhaustively_instrumented=False,
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
    result["validation"].update(status="native_category_sorting_postcondition" if native["postcondition_passed"] else
        "native_category_sorting_postcondition_failed",native_postcondition_passed=native["postcondition_passed"],disk_reload="not_run",runtime="not_run")
    result["stage"] = "returned_category_sorting_operation"


def set_blueprint_category_sorting_action(result, blueprint_name, category_order=None, category_to_front="",
        append_existing=True, save=True, compile=False, checkpoint_callback=None):
    arguments = _arguments(blueprint_name,category_order,category_to_front,append_existing,save,compile)
    _context(result,arguments)
    result["compile"].update(requested=compile,attempted=False,compiled=None,status="not_dispatched")
    result["save"].update(requested=save,attempted=False,saved=False,status="not_dispatched")
    result["data"]["pending_mutation"] = {"operation":IDENTITY,**arguments,"dispatch_state_at_checkpoint":"not_yet_dispatched"}
    result["stage"] = "before_mutation_checkpoint"
    callback = checkpoint_callback or state.checkpoint
    callback(result)
    result["data"].update(native_dispatched=True,native_result_returned=False,mutation_state_unknown=True,mutation_performed=None)
    result["stage"] = "native_category_sorting_operation"
    result["changes"] = [{"kind":IDENTITY,"attempted":None,"changed":None,"change_state_known":False,"outcome":"unknown_until_native_response"}]
    result["compile"].update(attempted=None,compiled=None,status="unknown_until_native_returns")
    result["save"].update(attempted=None,saved=None,status="unknown_until_native_returns")
    try:
        native = api.set_blueprint_category_sorting(**arguments)
    except Exception as error:
        if getattr(error,"native_call_returned",False):
            result["data"].update(native_result_returned=True,native_raw_result=getattr(error,"native_raw_result",None))
            result["stage"] = "returned_category_sorting_operation"
            callback(result)
        if getattr(error,"code","") == "native_category_sorting_api_unavailable":
            result["data"].update(native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False,compiled=None,status="native_not_dispatched")
            result["save"].update(attempted=False,saved=False,status="native_not_dispatched")
        raise
    _apply_native(result,arguments,native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_category_sorting_failed",native.get("error") or "Actual native prefix retained; no replay")
    if not (native["change_state_known"] and native["postcondition_passed"]):
        raise OperationError("category_sorting_result_unverified","Native success lacks known complete postconditions; actual prefix retained")
    result.update(success=True,stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_category_order_dirty_compile_save_and_journal_no_writer_replay"


def set_blueprint_category_sorting(blueprint_name, category_order=None, category_to_front="",
        append_existing=True, save=True, compile=False):
    try:
        arguments = _arguments(blueprint_name,category_order,category_to_front,append_existing,save,compile)
    except Exception as error:
        result = state.fail(state.new_result(),error)
        result["data"].update(operation=IDENTITY,admission_rejected=True,native_dispatched=False,
            native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
        return result
    return state.mutate(IDENTITY,arguments,None,lambda result:set_blueprint_category_sorting_action(result,
        blueprint_name,category_order,category_to_front,append_existing,save,compile))
