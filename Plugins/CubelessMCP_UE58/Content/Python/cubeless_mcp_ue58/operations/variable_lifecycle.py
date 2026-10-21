"""Original Blueprint metadata6/order5 on one shared writer journal.

Native owns descriptor/metadata helper lifecycle and changed-only compile/save.
Optional alias precedence and source-specific outputs remain opaque; no v1
metadata is fabricated for these original methods, which emitted none.
"""
import copy
import json
from cubeless_mcp_ue58.compatibility.ue58 import variable_lifecycle as api
from . import variables, state
from .state import OperationError

METADATA, ORDER = api.BINDINGS


def _optional_bool(value, field):
    if value is not None:
        variables._boolean(value, field)
    return variables._json_text(value)


def _metadata_arguments(blueprint_name, variable_name, metadata, is_editable, instance_editable, expose_on_spawn):
    if metadata is not None and type(metadata) is not dict:
        raise OperationError("variable_metadata_invalid", "metadata requires an optional JSON object, not public JSON text")
    # The original Python wrapper omits an empty object and forwards unknown,
    # null/non-scalar values otherwise; native owns skip/stringification/error.
    value = None if metadata is None or not metadata else metadata
    encoded = variables._json_text(value)
    variables._json_value(encoded, "metadata")
    return {"blueprint_path":variables._path(blueprint_name,write=True),
        "variable_name":variables._text(variable_name,"variable_name"), "metadata_json":encoded,
        "is_editable_json":_optional_bool(is_editable,"is_editable"),
        "instance_editable_json":_optional_bool(instance_editable,"instance_editable"),
        "expose_on_spawn_json":_optional_bool(expose_on_spawn,"expose_on_spawn")}


def _order_arguments(blueprint_name, variable_order, strict, compile, save):
    if type(variable_order) is not list or len(variable_order) > 4096 or any(type(name) is not str for name in variable_order):
        raise OperationError("variable_order_invalid", "variable_order requires a bounded original JSON string array")
    encoded = variables._json_text(variable_order)
    variables._json_value(encoded,"variable_order")
    return {"blueprint_path":variables._path(blueprint_name,write=True), "variable_order_json":encoded,
        "strict":variables._boolean(strict,"strict"), "compile":variables._boolean(compile,"compile"),
        "save":variables._boolean(save,"save")}


def _context(result, identity, arguments):
    result["data"].update(operation=identity, requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, full_original_variable_lifecycle_parity=False,
        native_helpers_may_request_structural_refresh_or_change_caches=True,
        callback_and_implicit_skeleton_compiler_work_exhaustively_instrumented=False,
        runtime_property_application_verified=False, cold_reload_verified=False,
        simple_name_resolution_scope="Existing variables._path maps a simple leaf to Game/Blueprints; original wider search is not claimed",
        actual_work_counts_authority="returned_native_counters_phases_and_ordered_save_receipts")


def _apply_native(result, identity, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native), native_raw_result=native["_raw_native_json"],
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ("data","_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
    _context(result,identity,arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="native_returned_compiled" if native["compiled"] is True else
            "native_returned_failed" if native["compile_attempted"] else "native_not_attempted")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_failed" if native["save_attempted"] else "native_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind":identity, "asset_path":arguments["blueprint_path"],
        "attempted":native["mutation_attempted"], "change_state_known":known, "changed":native["changed"] if known else None,
        "outcome":"native_scoped_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_variable_lifecycle_postcondition" if native["postcondition_passed"] else
        "native_variable_lifecycle_postcondition_failed", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run")
    result["stage"] = "returned_variable_lifecycle_operation"


def _action(result, identity, arguments, checkpoint_callback=None):
    _context(result,identity,arguments)
    result["compile"].update(requested=arguments.get("compile",False), attempted=False, compiled=None, status="not_dispatched")
    result["save"].update(requested=arguments.get("save",False), attempted=False, saved=False, status="not_dispatched")
    result["data"]["pending_mutation"] = {"operation":identity, **arguments,"dispatch_state_at_checkpoint":"not_yet_dispatched"}
    result["stage"] = "before_mutation_checkpoint"
    callback = checkpoint_callback or state.checkpoint
    callback(result)
    result["data"].update(native_dispatched=True,native_result_returned=False,mutation_state_unknown=True,mutation_performed=None)
    result["stage"] = "native_variable_lifecycle_operation"
    result["changes"] = [{"kind":identity,"attempted":None,"changed":None,"change_state_known":False,"outcome":"unknown_until_native_response"}]
    result["compile"].update(attempted=None,compiled=None,status="unknown_until_native_returns")
    result["save"].update(attempted=None,saved=None,status="unknown_until_native_returns")
    try:
        native = getattr(api,identity)(**arguments)
    except Exception as error:
        if getattr(error,"native_call_returned",False):
            result["data"].update(native_result_returned=True,native_raw_result=getattr(error,"native_raw_result",None))
            result["stage"] = "returned_variable_lifecycle_operation"
            callback(result)
        if getattr(error,"code","") == "native_variable_lifecycle_api_unavailable":
            result["data"].update(native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False,compiled=None,status="native_not_dispatched")
            result["save"].update(attempted=False,saved=False,status="native_not_dispatched")
        raise
    _apply_native(result,identity,arguments,native)
    callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_variable_lifecycle_failed",native.get("error") or "Actual native prefix retained; no replay")
    if not known_success(native):
        raise OperationError("variable_lifecycle_result_unverified","Native success lacks known complete postconditions; no retry")
    if identity == METADATA and (native["compile_requested"] or native["compile_attempted"] or native["compiled"] is not None
            or native["save_requested"] or native["save_attempted"] or native["saved"]):
        raise OperationError("variable_metadata_unexpected_compile_save","Actual unexpected prefix retained; metadata has no explicit compile/save")
    result.update(success=True,stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_variable_metadata_order_and_journal_no_writer_replay"


def known_success(native):
    # Matching order may retain caller compile/save intent but do neither.
    # Actual native verification alone decides; intent never creates counts.
    return native["change_state_known"] and native["postcondition_passed"]


def _named(identity, prepare):
    try:
        arguments = prepare()
    except Exception as error:
        result = state.fail(state.new_result(),error)
        result["data"].update(operation=identity,admission_rejected=True,native_dispatched=False,
            native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
        return result
    return state.mutate(identity,arguments,None,lambda result:_action(result,identity,arguments))


def set_blueprint_variable_metadata(blueprint_name, variable_name, metadata=None,
        is_editable=None, instance_editable=None, expose_on_spawn=None):
    return _named(METADATA,lambda:_metadata_arguments(blueprint_name,variable_name,metadata,is_editable,instance_editable,expose_on_spawn))


def set_blueprint_variable_metadata_action(result, blueprint_name, variable_name, metadata=None,
        is_editable=None, instance_editable=None, expose_on_spawn=None, checkpoint_callback=None):
    _action(result,METADATA,_metadata_arguments(blueprint_name,variable_name,metadata,is_editable,instance_editable,expose_on_spawn),checkpoint_callback)


def set_blueprint_variable_order(blueprint_name, variable_order, strict=False, compile=False, save=False):
    return _named(ORDER,lambda:_order_arguments(blueprint_name,variable_order,strict,compile,save))


def set_blueprint_variable_order_action(result, blueprint_name, variable_order, strict=False, compile=False, save=False, checkpoint_callback=None):
    _action(result,ORDER,_order_arguments(blueprint_name,variable_order,strict,compile,save),checkpoint_callback)
