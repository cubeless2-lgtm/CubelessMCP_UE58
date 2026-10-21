"""Original Select7/EnumSwitch8 through one named or existing parent journal.

Select delegates to the actual MathLibrary Select* call constructor, never a
different Select node. SwitchEnum retains current enum/pin/count/cleanup rows.
Public JSON position is preserved; only original None becomes origin.
"""
import copy
import json
import math

from cubeless_mcp_ue58.compatibility.ue58 import blueprint_selection as api
from . import state
from .state import OperationError

SELECT, SWITCH = api.BINDINGS
_DEPENDENCY_TOKENS = ("/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal",
    "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor")


def _position_json(value):
    count = 0
    def visit(item, depth):
        nonlocal count
        count += 1
        if depth > 8 or count > 1024:
            raise OperationError("blueprint_selection_input_invalid", "Position exceeds bounded JSON depth/value scope")
        if type(item) is dict:
            for key, child in item.items():
                if type(key) is not str:
                    raise OperationError("blueprint_selection_input_invalid", "JSON object keys must be strings")
                visit(key, depth + 1)
                visit(child, depth + 1)
        elif type(item) is list:
            for child in item:
                visit(child, depth + 1)
        elif type(item) is str:
            if any(0xD800 <= ord(c) <= 0xDFFF for c in item):
                raise OperationError("blueprint_selection_input_invalid", "Unpaired surrogate strings are outside the adapter")
        elif type(item) in (int, float):
            try:
                finite = math.isfinite(item)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("blueprint_selection_input_invalid", "JSON numbers must be finite in the native double representation")
        elif item is not None and type(item) is not bool:
            raise OperationError("blueprint_selection_input_invalid", "Only JSON shapes are admitted for public position")
    visit(value, 0)
    text = json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":"))
    if len(text) > 65536:
        raise OperationError("blueprint_selection_input_invalid", "Position exceeds bounded private JSON carrier")
    return text


def _arguments(method, blueprint_name, selector, node_position, graph_name, graph_id, graph_type, create_graph_if_missing, has_default_pin=False):
    if method not in api.BINDINGS or any(type(v) is not str for v in (blueprint_name, selector, graph_name, graph_id, graph_type)):
        raise OperationError("blueprint_selection_input_invalid", "Original required names/type and graph selectors require literal strings")
    if type(create_graph_if_missing) is not bool or type(has_default_pin) is not bool:
        raise OperationError("blueprint_selection_input_invalid", "Original create/default flags require exact booleans")
    if method == SWITCH:
        # Exact original pre-RPC scalar enum guard. No new guard is inserted
        # into the original unguarded Select value_type or position inputs.
        candidate = selector.strip().strip("\"'").replace("\\", "/").lower()
        if any(token in candidate for token in _DEPENDENCY_TOKENS):
            raise OperationError("blueprint_selection_dependency_reference", "Original enum_type guard refuses an MCP-only runtime reference")
    public = {"blueprint_name": blueprint_name, "value_type" if method == SELECT else "enum_type": selector,
        "node_position": copy.deepcopy(node_position), "graph_name": graph_name, "graph_id": graph_id,
        "graph_type": graph_type, "create_graph_if_missing": create_graph_if_missing}
    if method == SWITCH:
        public["has_default_pin"] = has_default_pin
    hidden = {"blueprint_name": blueprint_name, "value_type" if method == SELECT else "enum_type": selector,
        "position_json": _position_json([0, 0] if node_position is None else node_position),
        "graph_name": graph_name, "graph_id": graph_id, "graph_type": graph_type, "create_graph_if_missing": create_graph_if_missing}
    if method == SWITCH:
        hidden["has_default_pin"] = has_default_pin
    return {"method": method, "public": public, "hidden": hidden,
        "normalizations": ["node_position_None_to_origin_pair"] if node_position is None else []}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], raw_public_arguments=copy.deepcopy(arguments["public"]),
        requested_arguments=copy.deepcopy(arguments["hidden"]), original_default_normalizations=list(arguments["normalizations"]),
        domain_omission_vs_explicit_None_not_distinguished=True, public_node_position_schema="JSON Any/defaultNull via fixed named RAW method",
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        select_actual_node="Engine KismetMathLibrary.Select* UK2Node_CallFunction, not UK2Node_Select",
        switch_actual_node="Engine UK2Node_SwitchEnum, original visible-case/default/count cleanup",
        native_core_and_implicit_structural_work="Full opaque returned shared-core/lifecycle/enum snapshots; no fabricated total or v1",
        full_original_variant_runtime_cold_or_mixed_pipeline_verified=False)


def _apply_native(result, arguments, native):
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_raw_result=native["_raw_native_json"],
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native["changed"] if known else None)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status="native_returned_not_attempted" if not native["compile_attempted"] else "unexpected_native_compile_attempt")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_attempted_not_saved" if native["save_attempted"] else "native_returned_not_attempted")
    no_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_work else [{"kind": arguments["method"], "asset_path": arguments["public"]["blueprint_name"],
        "attempted": native["mutation_attempted"], "change_state_known": known, "changed": native["changed"] if known else None,
        "postcondition_passed": native["postcondition_passed"]}]
    result["validation"].update(status="native_blueprint_selection_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", implicit_skeleton_or_cache_work="not_collapsed_into_explicit_compile_count")
    result["stage"] = "returned_blueprint_selection_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_mutation=copy.deepcopy(arguments))
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_selection_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"]["blueprint_name"],
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        if arguments["method"] == SELECT:
            native = api.add_blueprint_select_node(**arguments["hidden"])
        else:
            native = api.add_blueprint_switch_enum_node(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_blueprint_selection_operation"
            callback(result)
        if getattr(error, "code", "") == "blueprint_selection_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("blueprint_selection_unexpected_compile_save", "Actual unrequested compiler/save prefix retained; no second dispatch")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_selection_native_failed", native.get("error") or "Native node/enum/core/cleanup prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_selection_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_node_enum_and_journal_no_node_replay"


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result


def add_blueprint_select_node(blueprint_name, value_type="int", node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False):
    try:
        args = _arguments(SELECT, blueprint_name, value_type, node_position, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return _rejected(SELECT, error)
    return _named(args)


def add_blueprint_switch_enum_node(blueprint_name, enum_type, has_default_pin=False, node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False):
    try:
        args = _arguments(SWITCH, blueprint_name, enum_type, node_position, graph_name, graph_id, graph_type, create_graph_if_missing, has_default_pin)
    except Exception as error:
        return _rejected(SWITCH, error)
    return _named(args)


def add_blueprint_select_node_action(result, blueprint_name, value_type="int", node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments(SELECT, blueprint_name, value_type, node_position,
        graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)


def add_blueprint_switch_enum_node_action(result, blueprint_name, enum_type, has_default_pin=False, node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments(SWITCH, blueprint_name, enum_type, node_position,
        graph_name, graph_id, graph_type, create_graph_if_missing, has_default_pin), checkpoint_callback)
