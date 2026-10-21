"""Original Math7/Boolean7 through exact hidden Editor-native functions."""
import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintExpressionLibrary"
BINDINGS = ("add_blueprint_math_node", "add_blueprint_boolean_node")
HIDDEN_ARGUMENTS = ("blueprint_name", "operation", "position_json", "graph_name", "graph_id", "graph_type", "create_graph_if_missing")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_expression_method_invalid", "Only exact original Math/Boolean methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_expression_native_api_unavailable", "Exact native expression binding unavailable; no node dispatched")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_expression_result", "Returned operation differs; actual native prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def add_blueprint_math_node(blueprint_name, operation, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke(BINDINGS[0], (blueprint_name, operation, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))


def add_blueprint_boolean_node(blueprint_name, operation, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke(BINDINGS[1], (blueprint_name, operation, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))
