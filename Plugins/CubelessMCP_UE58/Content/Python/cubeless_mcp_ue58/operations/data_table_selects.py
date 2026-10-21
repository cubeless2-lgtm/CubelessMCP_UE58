"""Exact PresetLoader DataTable Bool Select authoring; no runtime selection."""

from cubeless_mcp_ue58.compatibility.ue58 import data_table_selects as api
from . import functions
from .state import OperationError, fail, mutate, new_result


_OPERATION = "add_blueprint_data_table_bool_select_node"


def _arguments(blueprint_name, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    create = functions._bool(create_graph_if_missing, "create_graph_if_missing")
    if create:
        raise OperationError("datatable_select_graph_creation_unsupported", "Resolve the exact existing PresetLoader graph separately")
    position = functions._json(node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
    if (type(position) is not list or len(position) != 2
            or any(type(value) is not int or abs(value) > 1000000 for value in position)):
        raise OperationError("datatable_select_position_invalid", "Use two integer coordinates in -1000000..1000000")
    values = functions._selectors(blueprint_name, graph_name, graph_id, graph_type)
    if values["graph_type"] not in ("", "any", "function"):
        raise OperationError("datatable_select_graph_type_unsupported", "Only an existing exact PresetLoader BFL function is admitted")
    return {**values, "node_position_json": functions.variables._json_text(position), "create_graph_if_missing": False}


def _run(result, values, checkpoint_callback=None):
    result["data"].update(node_only_contract=True, implicit_data_wiring=False,
        stock_datatable_select_requested=True, runtime_selection_verified=False,
        original_select_node_topology_parity=False,
        requested_scope="exact PresetLoader native DataTable Bool Select; initial wildcard is not a completed typed producer")
    try:
        functions._run_action(result, _OPERATION, values, checkpoint_callback, operation_api=api)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True,
                native_raw_result=getattr(error, "native_raw_result", None))
        raise
    data = result["data"]
    pins = data.get("select_pins")
    if (data.get("native_datatable_bool_select") is not True or data.get("node_only_verified") is not True
            or data.get("specialization_pending") is not True or data.get("index_typed") is not False
            or data.get("value_typed") is not False or data.get("runtime_selection_verified") is not False
            or data.get("node_type") != "K2Node_Select" or type(data.get("node_id")) is not str or not data["node_id"]
            or type(pins) is not list or len(pins) != 4
            or any(type(pin) is not dict or type(pin.get("name")) is not str or type(pin.get("direction")) is not str for pin in pins)
            or {(pin.get("name"),pin.get("direction")) for pin in pins} != {
                ("Option 0","input"),("Option 1","input"),("Index","input"),("ReturnValue","output")}):
        result["stage"] = "verification"
        data["mutation_state_unknown"] = True
        raise OperationError("datatable_select_result_unverified", "Actual native allocation prefix retained; pending whole four-pin Select receipt is incomplete")


def add_blueprint_data_table_bool_select_node(blueprint_name, node_position_json="[0,0]",
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, operation_id=None):
    try:
        values = _arguments(blueprint_name, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate(_OPERATION, values, operation_id, lambda result: _run(result, values))


def add_blueprint_data_table_bool_select_node_action(result, blueprint_name, node_position_json="[0,0]",
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, _arguments(blueprint_name, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing),
        checkpoint_callback)
