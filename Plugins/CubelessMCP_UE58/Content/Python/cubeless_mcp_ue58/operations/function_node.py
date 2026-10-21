"""Original function-node9 whole JSON params and finite positions, one named or parent journal.

Native adapts original rounding/ignored keys/float32 positions and calls the
unchanged validated core. Complete actual prefix is durable before any error.
"""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import function_node as api
from . import state, variables
from .state import OperationError

IDENTITY = "add_blueprint_function_node"


def _json(value, field):
    count = 0
    def visit(item, depth):
        nonlocal count
        count += 1
        if depth > 8 or count > 1024:
            raise OperationError("function_node_input_invalid", field + " exceeds finite JSON depth/value budget")
        if type(item) is dict:
            for key, child in item.items():
                if type(key) is not str:
                    raise OperationError("function_node_input_invalid", "JSON object keys must be strings")
                visit(key, depth + 1); visit(child, depth + 1)
        elif type(item) is list:
            for child in item: visit(child, depth + 1)
        elif type(item) is str:
            if any(0xD800 <= ord(c) <= 0xDFFF for c in item):
                raise OperationError("function_node_input_invalid", "Unpaired surrogate strings are outside the finite adapter")
        elif type(item) in (int, float):
            try:
                finite = math.isfinite(item)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("function_node_input_invalid", "JSON numbers must be finite in the native double representation")
        elif item is not None and type(item) is not bool:
            raise OperationError("function_node_input_invalid", "Only JSON object/array/scalar/null values are admitted")
    visit(value, 0)
    # Preserve object iteration/spelling and unknown values. No new dependency
    # rejection is inserted into the original wrapper's unguarded params path.
    text = json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":"))
    if len(text) > 65536:
        raise OperationError("function_node_input_invalid", field + " exceeds native bounded JSON carrier")
    return text


def _arguments(blueprint_name, target, function_name, params, node_position,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    if node_position is not None and (type(node_position) is not list or len(node_position) not in (0, 2)):
        raise OperationError("function_node_position_unsupported", "Use nullable/empty origin or exactly two integral int32 coordinates")
    if node_position:
        for item in node_position:
            if (type(item) not in (int, float) or not math.isfinite(item)
                    or item != math.trunc(item) or not -(2**31) <= item <= 2**31 - 1):
                raise OperationError("function_node_position_unsupported", "Position requires integral signed-int32 JSON numbers")
    arguments = {"blueprint_path": variables._path(blueprint_name, write=True),
        "target": variables._text(target, "target", empty=True, maximum=128),
        "function_name": variables._text(function_name, "function_name", maximum=128),
        "params_json": _json({} if params is None else params, "params"),
        "position_json": _json([0, 0] if node_position is None else node_position, "node_position"),
        "graph_name": variables._text(graph_name, "graph_name", empty=True),
        "graph_id": variables._text(graph_id, "graph_id", empty=True),
        "graph_type": variables._text(graph_type, "graph_type", empty=True),
        "create_graph_if_missing": variables._boolean(create_graph_if_missing, "create_graph_if_missing")}
    arguments["_public_arguments"] = copy.deepcopy({"blueprint_name": blueprint_name, "target": target,
        "function_name": function_name, "params": params, "node_position": node_position,
        "graph_name": graph_name, "graph_id": graph_id, "graph_type": graph_type,
        "create_graph_if_missing": create_graph_if_missing})
    arguments["_default_normalizations"] = (["params_None_to_empty_object"] if params is None else []) + (
        ["node_position_None_to_origin_pair"] if node_position is None else [])
    return arguments


def _native_arguments(arguments):
    return {key: arguments[key] for key in api.HIDDEN_ARGUMENTS}


def _context(result, arguments):
    result["data"].update(operation=IDENTITY, requested_arguments=copy.deepcopy(_native_arguments(arguments)),
        raw_public_arguments=copy.deepcopy(arguments["_public_arguments"]),
        original_default_normalizations=list(arguments["_default_normalizations"]),
        domain_omission_vs_explicit_None_not_distinguished=True,
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, full_original_function_node_parity=False,
        public_params_schema_route="official_named_raw_toolset_full_JSON_Any",
        actual_official_raw_toolset_wire_verified=False, native_core_effects_exhaustively_instrumented=False,
        runtime_player_controller_resolution_verified=False, cold_reload_verified=False,
        actual_work_counts_authority="native_original_function_node_adapter_and_complete_reused_core_receipt")


def _apply_native(result, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native), native_raw_result=native["_raw_native_json"],
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"], status="native_not_attempted" if not native["compile_attempted"] else "unexpected_native_compile_attempt")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"], status="native_returned_saved" if native["saved"] else "native_returned_failed" if native["save_attempted"] else "native_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": IDENTITY, "asset_path": arguments["blueprint_path"],
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "outcome": "native_scoped_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_function_node_postcondition" if native["postcondition_passed"] else "native_function_node_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run", runtime="not_run")
    result["stage"] = "returned_function_node_operation"


def _action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested")
    result["data"]["pending_mutation"] = {"operation": IDENTITY, **arguments, "dispatch_state_at_checkpoint": "not_yet_dispatched"}
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"; callback(result)
    result["data"].update(native_dispatched=True, native_result_returned=False, mutation_state_unknown=True, mutation_performed=None)
    result["stage"] = "native_function_node_operation"
    result["changes"] = [{"kind": IDENTITY, "asset_path": arguments["blueprint_path"], "attempted": None,
        "changed": None, "change_state_known": False, "outcome": "unknown_until_native_response"}]
    try:
        native = api.add_blueprint_function_node(**_native_arguments(arguments))
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_function_node_operation"; callback(result)
        if getattr(error, "code", "") == "native_function_node_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, arguments, native); callback(result)
    if native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("unexpected_function_node_compile_save", "Native prefix reports unrequested compile/save; no second action dispatched")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_function_node_failed", native.get("error") or "Actual node/default/core prefix retained; no retry")
    if not (native["change_state_known"] and native["postcondition_passed"]):
        raise OperationError("function_node_result_unverified", "Native success lacks complete known postconditions; raw prefix retained")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_node_and_journal_no_function_node_replay"


def add_blueprint_function_node(blueprint_name, target, function_name, params=None, node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False):
    try:
        arguments = _arguments(blueprint_name, target, function_name, params, node_position, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        result["data"].update(operation=IDENTITY, admission_rejected=True, native_dispatched=False,
            native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
        return result
    return state.mutate(IDENTITY, arguments, None, lambda result: _action(result, arguments))


def add_blueprint_function_node_action(result, blueprint_name, target, function_name, params=None, node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False, checkpoint_callback=None):
    _action(result, _arguments(blueprint_name, target, function_name, params, node_position,
        graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
