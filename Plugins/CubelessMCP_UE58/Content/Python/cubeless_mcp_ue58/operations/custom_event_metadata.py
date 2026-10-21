"""Original CustomEventMetadata10 on one named or parent batch journal.

Native owns reconstruction/structural work, optional full compile, generated
UFunction metadata and forced save. Full returned prefix precedes any error.
"""
import copy
import json
from cubeless_mcp_ue58.compatibility.ue58 import custom_event_metadata as api
from . import state, variables
from .datatable import _reference
from .state import OperationError

IDENTITY = "set_blueprint_custom_event_metadata"


def _arguments(blueprint_name, custom_event_name, description, node_id, event_name,
        tooltip, call_in_editor, set_node_comment, compile, save):
    texts = {name: variables._text(value, name, empty=True,
        maximum=4096 if name in ("description", "tooltip") else 1023,
        multiline=name in ("description", "tooltip")) for name, value in (
            ("custom_event_name", custom_event_name), ("description", description),
            ("node_id", node_id), ("event_name", event_name), ("tooltip", tooltip))}
    # Preserve the original Python truthy alias precedence and dependency guard.
    selected_name = texts["custom_event_name"] or texts["event_name"]
    selected_description = texts["description"] or texts["tooltip"]
    if not selected_name and not texts["node_id"]:
        raise OperationError("missing_custom_event_identifier", "custom_event_name or node_id is required")
    if selected_description:
        _reference(selected_description, "description")
    if call_in_editor is not None and type(call_in_editor) is not bool:
        raise OperationError("custom_event_metadata_input_invalid", "call_in_editor requires boolean/null; False remains meaningful")
    return {"blueprint_path": variables._path(blueprint_name, write=True), **texts,
        "call_in_editor_json": json.dumps(call_in_editor, allow_nan=False),
        "set_node_comment": variables._boolean(set_node_comment, "set_node_comment"),
        "compile": variables._boolean(compile, "compile"), "save": variables._boolean(save, "save")}


def _context(result, arguments):
    result["data"].update(operation=IDENTITY, requested_arguments=copy.deepcopy(arguments),
        effective_public_event_name=arguments["custom_event_name"] or arguments["event_name"],
        effective_public_description=arguments["description"] or arguments["tooltip"],
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, full_original_custom_event_metadata_parity=False,
        actual_work_counts_authority="returned_native_counters_and_ordered_observation_compile_save_prefix",
        implicit_skeleton_compiler_callback_work_exhaustively_instrumented=False,
        runtime_custom_event_execution_verified=False, cold_reload_verified=False)


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native), native_raw_result=native["_raw_native_json"],
        native_outcome={key:copy.deepcopy(value) for key,value in native.items() if key not in ("data","_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status="native_returned_compiled" if native["compiled"] is True else "native_returned_unverified" if native["compiled"] is None and native["compile_attempted"] else "native_returned_failed" if native["compile_attempted"] else "native_not_attempted")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_failed" if native["save_attempted"] else "native_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind":IDENTITY,"asset_path":arguments["blueprint_path"],
        "attempted":native["mutation_attempted"],"change_state_known":known,"changed":native["changed"] if known else None,
        "outcome":"native_scoped_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_custom_event_metadata_postcondition" if native["postcondition_passed"] else "native_custom_event_metadata_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",runtime="not_run")
    result["stage"] = "returned_custom_event_metadata_operation"


def _action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=arguments["compile"], attempted=False, compiled=None, status="intent_prepared_not_dispatched")
    result["save"].update(requested=arguments["save"], attempted=False, saved=False, status="intent_prepared_not_dispatched")
    result["data"]["pending_mutation"] = {"operation":IDENTITY,**arguments,"dispatch_state_at_checkpoint":"not_yet_dispatched"}
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"; callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False, mutation_state_unknown=True, mutation_performed=None)
    result["stage"] = "native_custom_event_metadata_operation"
    result["changes"] = [{"kind":IDENTITY,"asset_path":arguments["blueprint_path"],"attempted":None,"changed":None,"change_state_known":False,"outcome":"unknown_until_native_response"}]
    result["compile"].update(attempted=None, compiled=None, status="unknown_until_native_returns")
    result["save"].update(attempted=None, saved=None, status="unknown_until_native_returns")
    try:
        native = api.set_blueprint_custom_event_metadata(**arguments)
    except Exception as error:
        if getattr(error,"native_call_returned",False):
            result["data"].update(native_result_returned=True,native_raw_result=getattr(error,"native_raw_result",None))
            result["stage"] = "returned_custom_event_metadata_operation"; callback(result)
        if getattr(error,"code","") == "native_custom_event_metadata_api_unavailable":
            result["data"].update(native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False,compiled=None,status="native_not_dispatched")
            result["save"].update(attempted=False,saved=False,status="native_not_dispatched")
        raise
    _apply_native(result, arguments, native); callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_custom_event_metadata_failed",native.get("error") or "Actual metadata/compile/generated-function/save prefix retained; no retry")
    if not (native["change_state_known"] and native["postcondition_passed"]):
        raise OperationError("custom_event_metadata_result_unverified","Native success lacks complete known postconditions; actual prefix retained")
    result.update(success=True,stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_custom_event_generated_function_and_journal_no_metadata_replay"


def set_blueprint_custom_event_metadata(blueprint_name, custom_event_name="", description="", node_id="", event_name="", tooltip="",
        call_in_editor=None, set_node_comment=True, compile=True, save=False):
    try:
        arguments = _arguments(blueprint_name,custom_event_name,description,node_id,event_name,tooltip,call_in_editor,set_node_comment,compile,save)
    except Exception as error:
        result = state.fail(state.new_result(),error)
        result["data"].update(operation=IDENTITY,admission_rejected=True,native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
        return result
    return state.mutate(IDENTITY,arguments,None,lambda result:_action(result,arguments))


def set_blueprint_custom_event_metadata_action(result, blueprint_name, custom_event_name="", description="", node_id="", event_name="", tooltip="",
        call_in_editor=None, set_node_comment=True, compile=True, save=False, checkpoint_callback=None):
    _action(result,_arguments(blueprint_name,custom_event_name,description,node_id,event_name,tooltip,call_in_editor,set_node_comment,compile,save),checkpoint_callback)
