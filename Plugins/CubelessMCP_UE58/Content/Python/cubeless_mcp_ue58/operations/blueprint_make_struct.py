"""Original MakeStruct7 using existing graph selectors, journals and native AddCall.

Only vector/rotator/transform helper names are supported. Creates one unlinked
Kismet call; no explicit compile/save, runtime value evaluation or arbitrary struct.
"""
import copy
import math
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_make_struct as api
from . import functions, state, variables
from .state import OperationError


def _arguments(blueprint_name, struct_type, node_position, graph_name, graph_id, graph_type, create_graph_if_missing):
    text = functions._text(struct_type, "struct_type")
    if len(text.encode("utf-16-le")) // 2 > 128:
        raise OperationError("make_struct_type_invalid", "Bounded original struct helper type required")
    position = [0, 0] if node_position is None or (type(node_position) is list and len(node_position) == 0) else node_position
    if type(position) is not list or len(position) != 2 or any(type(x) not in (int, float) or not math.isfinite(x)
            or x != math.trunc(x) or not -(2**31) <= x < 2**31 for x in position):
        raise OperationError("function_position_invalid", "Finite supported position is null or two exact signed-int32 numeric coordinates; original loose/fractional coercions remain incomplete")
    return {**functions._selectors(blueprint_name, graph_name, graph_id, graph_type),
        "struct_type": text, "node_position_json": variables._json_text([int(x) for x in position]),
        "create_graph_if_missing": functions._bool(create_graph_if_missing, "create_graph_if_missing")}


def _run_action(result, arguments, checkpoint_callback=None):
    callback = checkpoint_callback or state.checkpoint
    result["data"].update(operation=api.BINDINGS[0], requested_arguments=copy.deepcopy(arguments),
        pending_mutation={"operation": api.BINDINGS[0], **arguments, "dispatch_state_at_checkpoint": "not_yet_dispatched"},
        adapter_explicit_asset_load_calls=0, native_dispatched=False, native_result_returned=False,
        mutation_performed=False, mutation_state_unknown=False, full_legacy_make_struct_parity=False,
        arbitrary_struct_creation_supported=False, implicit_graph_creation_supported=None if arguments["create_graph_if_missing"] else False, runtime_value_evaluated=False)
    result["stage"] = "before_mutation_checkpoint"; callback(result)
    result["stage"] = "native_make_struct_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True, mutation_performed=None)
    result["changes"] = [{"kind": api.BINDINGS[0], "asset_path": arguments["blueprint_path"], "attempted": None, "changed": None, "change_state_known": False}]
    try:
        native = api.add_blueprint_make_struct_node(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_make_struct_operation"; callback(result)
        if getattr(error, "code", "") == "native_function_api_unavailable":
            result["data"].update(native_dispatched=False, mutation_state_unknown=False, mutation_performed=False); result["changes"] = []
        raise
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native), native_raw_result=native.get("_raw_native_json"),
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native.get("changed") if known else None)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"], status="not_requested")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    result["changes"] = [] if known and native["changed"] is False and not native["mutation_attempted"] else [{
        "kind": api.BINDINGS[0], "asset_path": arguments["blueprint_path"], "attempted": native["mutation_attempted"],
        "changed": native.get("changed") if known else None, "change_state_known": known}]
    result["stage"] = "returned_make_struct_operation"; callback(result)
    if any(native[k] for k in ("compile_requested", "compile_attempted", "save_requested", "save_attempted", "saved")) or native["compiled"] is True:
        result["data"]["mutation_state_unknown"] = True
        raise OperationError("unexpected_function_compile_save", "Unexpected explicit compile/save prefix retained; no retry")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_make_struct_failed", native.get("error") or "MakeStruct failed; actual prefix retained")
    if not known or not native["postcondition_passed"]:
        raise OperationError("function_result_unverified", "Native success lacks known postconditions")
    result["validation"].update(status="native_make_struct_postcondition", disk_reload="not_run", runtime_value_evaluation="not_run")
    result.update(success=True, stage="complete"); result["retry"]["duplicate_risk"] = "make_struct_would_repeat" if native["mutation_attempted"] else "none_observed"


def add_blueprint_make_struct_node(blueprint_name, struct_type, node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False):
    try:
        arguments = _arguments(blueprint_name, struct_type, node_position, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return state.fail(state.new_result(), error)
    return state.mutate(api.BINDINGS[0], arguments, None, lambda result: _run_action(result, arguments))


def add_blueprint_make_struct_node_action(result, blueprint_name, struct_type, node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments(blueprint_name, struct_type, node_position, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
