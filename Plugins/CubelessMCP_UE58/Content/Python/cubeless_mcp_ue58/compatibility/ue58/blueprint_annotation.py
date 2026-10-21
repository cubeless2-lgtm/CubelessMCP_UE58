"""Original annotation AddBox15/NodeComment8/FunctionDescription5 through exact Editor native methods."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintAnnotationLibrary"
BINDINGS = ('add_blueprint_comment_box', 'set_blueprint_node_comment', 'set_blueprint_function_description')
HIDDEN_ARGUMENTS_BY_METHOD = {'add_blueprint_comment_box': ('blueprint_name', 'comment_text', 'position_json', 'size_json', 'color_json', 'font_size', 'move_mode', 'comment_bubble_visible', 'color_comment_bubble', 'node_details', 'node_ids_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'set_blueprint_node_comment': ('blueprint_name', 'node_id', 'node_comment', 'comment_bubble_visible', 'comment_bubble_pinned', 'graph_name', 'graph_id', 'graph_type'), 'set_blueprint_function_description': ('blueprint_name', 'description', 'graph_name', 'graph_id', 'graph_type')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_annotation_method_invalid", "Only exact original annotation methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_annotation_native_api_unavailable", "Exact native annotation binding unavailable; no node/default/cleanup dispatched")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_annotation_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value






def add_blueprint_comment_box(blueprint_name, comment_text, position_json, size_json, color_json, font_size, move_mode, comment_bubble_visible, color_comment_bubble, node_details, node_ids_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke('add_blueprint_comment_box', (blueprint_name, comment_text, position_json, size_json, color_json, font_size, move_mode, comment_bubble_visible, color_comment_bubble, node_details, node_ids_json, graph_name, graph_id, graph_type, create_graph_if_missing))


def set_blueprint_node_comment(blueprint_name, node_id, node_comment, comment_bubble_visible, comment_bubble_pinned, graph_name, graph_id, graph_type):
    return _invoke('set_blueprint_node_comment', (blueprint_name, node_id, node_comment, comment_bubble_visible, comment_bubble_pinned, graph_name, graph_id, graph_type))


def set_blueprint_function_description(blueprint_name, description, graph_name, graph_id, graph_type):
    return _invoke('set_blueprint_function_description', (blueprint_name, description, graph_name, graph_id, graph_type))
