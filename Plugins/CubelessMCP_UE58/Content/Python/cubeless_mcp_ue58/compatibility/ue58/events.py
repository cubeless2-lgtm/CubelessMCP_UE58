"""Exact loaded event graph adapter; native helpers own mutation/readback."""

import unreal

from . import functions, graph
from cubeless_mcp_ue58.operations.state import OperationError


def _invoke(method, blueprint_path, graph_name, graph_id, graph_type, selector, x, y, *options):
    function = getattr(getattr(unreal, "CubelessEventLibrary", None), method, None)
    if not callable(function):
        raise OperationError("native_event_api_unavailable", "Build/load CubelessEventLibrary; no fallback executed")
    editor, selected = graph.sequence_editor(blueprint_path, graph_name, graph_id, graph_type)
    if selected["graph_type"] != "event":
        raise OperationError("event_graph_type_unsupported", "Event tools require an existing owned event graph")
    if method == "add_blueprint_parent_function_call":
        selector = graph.node_id(graph.find_node(editor, selector))
    result = functions._decode_result(function(editor.get_graph().get_path_name(), selector, x, y, *options))
    result["data"]["graph_selection"] = selected
    return result


def add_blueprint_event_node(blueprint_path, event_name, graph_name, graph_id, graph_type, x, y):
    if event_name == "ReceiveParticleData":
        from . import particle_receiver
        return particle_receiver.add_particle_data_event_node(blueprint_path, graph_name, graph_id, graph_type, x, y)
    return _invoke("add_blueprint_event_node", blueprint_path, graph_name, graph_id, graph_type, event_name, x, y)


def add_blueprint_parent_function_call(blueprint_path, event_node_id, graph_name, graph_id, graph_type, x, y):
    return _invoke("add_blueprint_parent_function_call", blueprint_path, graph_name, graph_id, graph_type, event_node_id, x, y)


def add_blueprint_custom_event_node(blueprint_path, custom_event_name, graph_name, graph_id, graph_type, x, y,
        call_in_editor, has_call_in_editor):
    return _invoke("add_blueprint_custom_event_node", blueprint_path, graph_name, graph_id, graph_type,
        custom_event_name, x, y, call_in_editor, has_call_in_editor)


def add_blueprint_input_key_node(blueprint_path, key, graph_name, graph_id, graph_type, x, y,
        control, alt, shift, command, consume_input, execute_when_paused, override_parent_binding):
    return _invoke("add_blueprint_input_key_node", blueprint_path, graph_name, graph_id, graph_type,
        key, x, y, control, alt, shift, command, consume_input, execute_when_paused, override_parent_binding)
