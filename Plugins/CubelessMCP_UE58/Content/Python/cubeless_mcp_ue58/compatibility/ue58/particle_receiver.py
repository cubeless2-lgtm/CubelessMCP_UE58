"""Finite UE5.8 native particle callback authoring; no transport or wildcard fallback."""

import unreal

from . import functions, graph
from cubeless_mcp_ue58.operations.state import OperationError

INTERFACE_PATH = "/Script/Niagara.NiagaraParticleCallbackHandler"


def _invoke(method, blueprint_path, graph_name, graph_id, graph_type, *arguments, event_node_id=None):
    function = getattr(getattr(unreal, "CubelessParticleReceiverLibrary", None), method, None)
    if not callable(function):
        # Shared action accounting recognizes this as a pre-resolution absence,
        # with no dispatched native mutation or asset loading.
        raise OperationError("native_function_api_unavailable", "Build/load CubelessParticleReceiverLibrary; no fallback executed")
    editor, selected = graph.sequence_editor(blueprint_path, graph_name, graph_id, graph_type)
    if selected["graph_type"] != "event":
        raise OperationError("particle_event_graph_required", "Particle receiver tools require the existing directly owned EventGraph")
    if event_node_id is not None:
        arguments = (graph.node_id(graph.find_node(editor, event_node_id)), *arguments)
    result = functions._decode_result(function(editor.get_graph().get_path_name(), *arguments))
    result["data"]["graph_selection"] = selected
    return result


def inspect_particle_receiver(blueprint_path, graph_name, graph_id, graph_type):
    return _invoke("read_particle_receiver_plan", blueprint_path, graph_name, graph_id, graph_type)


def add_blueprint_interface(blueprint_path, interface_class):
    if interface_class == INTERFACE_PATH:
        return _invoke("implement_particle_callback_interface", blueprint_path, "", "", "event")
    function = getattr(getattr(unreal, "CubelessParticleReceiverLibrary", None), "implement_blueprint_interface", None)
    if not callable(function):
        raise OperationError("native_function_api_unavailable", "Build/load the native interface implementation binding; no fallback executed")
    try:
        raw = function(blueprint_path, interface_class)
    finally:
        function = None
    return functions._decode_result(raw)


def add_particle_data_event_node(blueprint_path, graph_name, graph_id, graph_type, x, y):
    return _invoke("add_particle_data_event_node", blueprint_path, graph_name, graph_id, graph_type, x, y)


def add_particle_data_array_length_and_wire(blueprint_path, event_node_id, graph_name, graph_id, graph_type, x, y):
    return _invoke("add_particle_data_array_length_node", blueprint_path, graph_name, graph_id, graph_type,
        x, y, event_node_id=event_node_id)


def add_particle_data_array_get_copy_and_wire(blueprint_path, event_node_id, graph_name, graph_id, graph_type, x, y):
    return _invoke("add_particle_data_array_get_copy_node", blueprint_path, graph_name, graph_id, graph_type,
        x, y, event_node_id=event_node_id)


def add_basic_particle_data_break_node(blueprint_path, graph_name, graph_id, graph_type, x, y):
    return _invoke("add_basic_particle_data_break_node", blueprint_path, graph_name, graph_id, graph_type, x, y)
