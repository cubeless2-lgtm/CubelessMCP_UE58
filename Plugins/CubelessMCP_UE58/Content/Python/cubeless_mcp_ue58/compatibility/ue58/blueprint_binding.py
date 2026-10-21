"""Original NativeBind8/CreateDelegate11/InterfaceMessage10 through fixed Editor APIs."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintObjectDelegateLibrary"
BINDINGS = ('add_blueprint_native_delegate_bind_node', 'add_blueprint_create_delegate_node', 'add_blueprint_interface_message_node')
HIDDEN_ARGUMENTS_BY_METHOD = {'add_blueprint_native_delegate_bind_node': ('blueprint_name', 'delegate_owner_class', 'delegate_name', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_create_delegate_node': ('blueprint_name', 'function_name', 'object_node_id', 'delegate_target_node_id', 'object_pin_name', 'delegate_target_pin_name', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_interface_message_node': ('blueprint_name', 'interface_class', 'function_name', 'target_node_id', 'target_pin_name', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_binding_method_invalid", "Only exact original native bind/create delegate/interface message methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_binding_native_api_unavailable", "Exact native binding binding unavailable; no property/node/connection/cleanup dispatched")
    try:
        raw = function(*arguments)
    finally:
        function = None
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_binding_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def add_blueprint_native_delegate_bind_node(blueprint_name, delegate_owner_class, delegate_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke('add_blueprint_native_delegate_bind_node', (blueprint_name, delegate_owner_class, delegate_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))


def add_blueprint_create_delegate_node(blueprint_name, function_name, object_node_id, delegate_target_node_id, object_pin_name, delegate_target_pin_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke('add_blueprint_create_delegate_node', (blueprint_name, function_name, object_node_id, delegate_target_node_id, object_pin_name, delegate_target_pin_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))


def add_blueprint_interface_message_node(blueprint_name, interface_class, function_name, target_node_id, target_pin_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke('add_blueprint_interface_message_node', (blueprint_name, interface_class, function_name, target_node_id, target_pin_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))
