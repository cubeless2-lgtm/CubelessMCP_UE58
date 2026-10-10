"""Legacy event-node tool and a separately identified parent-call extension."""

from cubeless_mcp_ue58.compatibility.ue58 import events as api
from . import functions, state
from .state import OperationError


def _arguments(blueprint_name, selector_name, selector, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    if functions._bool(create_graph_if_missing, "create_graph_if_missing"):
        raise OperationError("event_graph_creation_unsupported", "Resolve an existing event graph; implicit graph creation is not implemented")
    position = functions._json(node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
    if type(position) is not list or len(position) != 2 or any(type(v) is not int or abs(v) > 1000000 for v in position):
        raise OperationError("event_position_invalid", "node_position_json requires two integers from -1000000 to 1000000")
    return {**functions._selectors(blueprint_name, graph_name, graph_id, graph_type),
        selector_name: functions._text(selector, selector_name), "x": position[0], "y": position[1]}


def _run(result, identity, arguments, checkpoint_callback=None):
    result["data"].update(full_legacy_event_parity=False,
        legacy_public_tool=identity == "add_blueprint_event_node",
        parent_call_is_new_extension=identity == "add_blueprint_parent_function_call")
    functions._run_action(result, identity, arguments, checkpoint_callback, operation_api=api)


def _prepared(identity, arguments, operation_id):
    try:
        values = arguments()
    except Exception as error:
        return state.fail(state.new_result(operation_id), error)
    return state.mutate(identity, values, operation_id, lambda result: _run(result, identity, values))


def add_blueprint_event_node(blueprint_name, event_name, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, create_graph_if_missing=False, operation_id=None):
    return _prepared("add_blueprint_event_node", lambda: _arguments(blueprint_name, "event_name", event_name,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), operation_id)


def add_blueprint_event_node_action(result, blueprint_name, event_name, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, "add_blueprint_event_node", _arguments(blueprint_name, "event_name", event_name,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)


def add_blueprint_parent_function_call(blueprint_name, event_node_id, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, create_graph_if_missing=False, operation_id=None):
    return _prepared("add_blueprint_parent_function_call", lambda: _arguments(blueprint_name, "event_node_id", event_node_id,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), operation_id)


def add_blueprint_parent_function_call_action(result, blueprint_name, event_node_id, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, "add_blueprint_parent_function_call", _arguments(blueprint_name, "event_node_id", event_node_id,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
