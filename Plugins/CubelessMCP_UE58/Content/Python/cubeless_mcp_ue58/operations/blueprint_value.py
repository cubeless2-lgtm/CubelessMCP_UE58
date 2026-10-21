"""Original Compare8/Literal8 through one named or existing parent journal.

Public literal Value remains whole JSON; Native converts only after allocation
and retains actual default refusal/Destroy prefixes. No public pin-set/delete
composition, compiler/save, Python cleanup, double substitute or node replay.
"""
import copy
import json
import math

from cubeless_mcp_ue58.compatibility.ue58 import blueprint_value as api
from . import state
from .state import OperationError

COMPARE, LITERAL = api.BINDINGS


def _json_value(value, label):
    count = 0
    def visit(item, depth):
        nonlocal count
        count += 1
        if depth > 8 or count > 1024:
            raise OperationError("blueprint_value_input_invalid", label + " exceeds bounded JSON depth/value scope")
        if type(item) is dict:
            for key, child in item.items():
                if type(key) is not str:
                    raise OperationError("blueprint_value_input_invalid", "JSON object keys must be strings")
                visit(key, depth + 1)
                visit(child, depth + 1)
        elif type(item) is list:
            for child in item:
                visit(child, depth + 1)
        elif type(item) is str:
            if any(0xD800 <= ord(c) <= 0xDFFF for c in item):
                raise OperationError("blueprint_value_input_invalid", "Unpaired surrogate strings are outside the adapter")
        elif type(item) in (int, float):
            try:
                finite = math.isfinite(item)
            except OverflowError:
                finite = False
            if not finite:
                raise OperationError("blueprint_value_input_invalid", "JSON numbers must be finite in the native double representation")
        elif item is not None and type(item) is not bool:
            raise OperationError("blueprint_value_input_invalid", "Only JSON shapes are admitted for public " + label)
    visit(value, 0)
    text = json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":"))
    if len(text) > 65536:
        raise OperationError("blueprint_value_input_invalid", label + " exceeds bounded private JSON carrier")
    return text


def _arguments(method, blueprint_name, selector, value, node_position, graph_name, graph_id, graph_type, create_graph_if_missing):
    if method not in api.BINDINGS or any(type(v) is not str for v in (blueprint_name, selector, graph_name, graph_id, graph_type)):
        raise OperationError("blueprint_value_input_invalid", "Original required Blueprint/type/operation and selectors require strings")
    if type(create_graph_if_missing) is not bool:
        raise OperationError("blueprint_value_input_invalid", "Original graph-create flag requires a boolean")
    common = {"blueprint_name": blueprint_name, "node_position": copy.deepcopy(node_position), "graph_name": graph_name,
        "graph_id": graph_id, "graph_type": graph_type, "create_graph_if_missing": create_graph_if_missing}
    hidden = {"blueprint_name": blueprint_name, "position_json": _json_value([0, 0] if node_position is None else node_position, "position"),
        "graph_name": graph_name, "graph_id": graph_id, "graph_type": graph_type, "create_graph_if_missing": create_graph_if_missing}
    normalizations = ["node_position_None_to_origin_pair"] if node_position is None else []
    if method == COMPARE:
        if type(value) is not str:
            raise OperationError("blueprint_value_input_invalid", "Original Compare value_type requires a string")
        public = {**common, "operation": selector, "value_type": value}
        hidden.update(operation=selector, value_type=value)
    else:
        public = {**common, "literal_type": selector, "value": copy.deepcopy(value)}
        hidden.update(literal_type=selector, value_json=_json_value(value, "literal value"))
        if value is None:
            normalizations.append("literal_value_None_means_original_Value_omitted")
    # No original dependency guard or alias normalization is inserted. Scalar,
    # object/array stringify/default conversion and late refusal remain native.
    return {"method": method, "public": public, "hidden": hidden, "normalizations": normalizations}


def _context(result, arguments):
    result["data"].update(operation=arguments["method"], raw_public_arguments=copy.deepcopy(arguments["public"]),
        requested_arguments=copy.deepcopy(arguments["hidden"]), original_default_normalizations=list(arguments["normalizations"]),
        domain_omission_vs_explicit_None_not_distinguished=True,
        public_value_and_node_position_schema="JSON Any/defaultNull through same fixed RAW expression owner",
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, actual_official_raw_wire_verified=False,
        actual_value_node_family="Engine KismetMathLibrary Compare, KismetSystemLibrary MakeLiteral*, or missing-MakeLiteralFloat adaptation via KismetMathLibrary WeightedMovingAverage_Float UK2Node_CallFunction; actual CurrentSample/PreviousSample/Weight float inputs differ from original Value",
        native_late_literal_conversion_cleanup_authoritative=True,
        literal_float_single_remapped_to_double=False,
        node_retirement_is_atomic_rollback_or_zero_effect=False,
        native_core_and_implicit_structural_work="Full opaque actual signature/default/current graph/source/created/retired/counter prefix",
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
    result["validation"].update(status="native_blueprint_value_postcondition", native_postcondition_passed=native["postcondition_passed"],
        disk_reload="not_run", runtime="not_run", implicit_skeleton_or_cache_work="not_collapsed_into_explicit_compile_count")
    result["stage"] = "returned_blueprint_value_operation"


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["compile"].update(requested=False, attempted=False, compiled=None, status="not_requested")
    result["save"].update(requested=False, attempted=False, saved=False, status="not_requested")
    result["data"].update(native_dispatched=False, native_result_returned=False, native_mutation_attempted=False,
        mutation_performed=False, mutation_state_unknown=False, pending_mutation=copy.deepcopy(arguments))
    callback = checkpoint_callback or state.checkpoint
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["stage"] = "native_blueprint_value_operation"
    result["data"].update(native_dispatched=True, native_mutation_attempted=None, mutation_performed=None, mutation_state_unknown=True)
    result["compile"].update(attempted=None, compiled=None, status="native_dispatch_outcome_unverified")
    result["save"].update(attempted=None, saved=None, status="native_dispatch_outcome_unverified")
    result["changes"] = [{"kind": arguments["method"], "asset_path": arguments["public"]["blueprint_name"],
        "attempted": None, "changed": None, "change_state_known": False}]
    try:
        if arguments["method"] == COMPARE:
            native = api.add_blueprint_compare_node(**arguments["hidden"])
        else:
            native = api.add_blueprint_literal_node(**arguments["hidden"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_blueprint_value_operation"
            callback(result)
        if getattr(error, "code", "") == "blueprint_value_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["compile"].update(attempted=False, compiled=None, status="native_unavailable_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_unavailable_not_dispatched")
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    callback(result)
    if native["compile_attempted"] or native["save_attempted"]:
        raise OperationError("blueprint_value_unexpected_compile_save", "Actual unrequested compiler/save prefix retained; no second dispatch")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "blueprint_value_native_failed", native.get("error") or "Native value/default/cleanup/function-core prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("blueprint_value_result_unverified", "Native success lacks known current postconditions; no replay")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "inspect_actual_value_created_default_retired_signature_and_journal_no_node_replay"


def _named(arguments):
    return state.mutate(arguments["method"], arguments, None, lambda result: _run_action(result, arguments))


def _rejected(method, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=method, admission_rejected=True, native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    return result




def add_blueprint_compare_node(blueprint_name, operation, value_type="double", node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False):
    try:
        args = _arguments(COMPARE, blueprint_name, operation, value_type, node_position, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return _rejected(COMPARE, error)
    return _named(args)


def add_blueprint_literal_node(blueprint_name, literal_type, value=None, node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False):
    try:
        args = _arguments(LITERAL, blueprint_name, literal_type, value, node_position, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return _rejected(LITERAL, error)
    return _named(args)


def add_blueprint_compare_node_action(result, blueprint_name, operation, value_type="double", node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments(COMPARE, blueprint_name, operation, value_type, node_position,
        graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)


def add_blueprint_literal_node_action(result, blueprint_name, literal_type, value=None, node_position=None,
        graph_name="", graph_id="", graph_type="", create_graph_if_missing=False, checkpoint_callback=None):
    _run_action(result, _arguments(LITERAL, blueprint_name, literal_type, value, node_position,
        graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
