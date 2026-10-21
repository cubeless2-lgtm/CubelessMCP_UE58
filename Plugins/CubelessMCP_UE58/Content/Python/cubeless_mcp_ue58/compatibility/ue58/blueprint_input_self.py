"""Original legacy InputAction7/current-owner Self6 through exact Editor native methods."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintInputSelfLibrary"
BINDINGS = ('add_blueprint_input_action_node', 'add_blueprint_self_reference')
HIDDEN_ARGUMENTS_BY_METHOD = {'add_blueprint_input_action_node': ('blueprint_name', 'action_name', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_self_reference': ('blueprint_name', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_input_self_method_invalid", "Only exact original legacy input-action/self-reference methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_input_self_native_api_unavailable", "Exact native input/self binding unavailable; no node/default/cleanup dispatched")
    try:
        raw = function(*arguments)
    finally:
        function = None
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_input_self_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value




def add_blueprint_input_action_node(blueprint_name, action_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke('add_blueprint_input_action_node', (blueprint_name, action_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))


def add_blueprint_self_reference(blueprint_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke('add_blueprint_self_reference', (blueprint_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))
