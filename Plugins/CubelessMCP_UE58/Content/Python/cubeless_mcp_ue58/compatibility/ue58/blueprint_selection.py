"""Two exact hidden Select/SwitchEnum bindings; no loader or fallback."""
import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintSelectionLibrary"
BINDINGS = ("add_blueprint_select_node", "add_blueprint_switch_enum_node")
HIDDEN_ARGUMENTS_BY_METHOD = {
    BINDINGS[0]: ("blueprint_name", "value_type", "position_json", "graph_name", "graph_id", "graph_type", "create_graph_if_missing"),
    BINDINGS[1]: ("blueprint_name", "enum_type", "has_default_pin", "position_json", "graph_name", "graph_id", "graph_type", "create_graph_if_missing")}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, *arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_selection_native_api_unavailable", "Only the two exact original node bindings are admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_selection_native_api_unavailable", "Exact native binding unavailable; no asset/node dispatch")
    raw = function(*arguments)
    try:
        native = _decode_result(raw)
        if native.get("operation") != method:
            raise OperationError("blueprint_selection_native_result_invalid", "Returned operation differs; complete raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def add_blueprint_select_node(blueprint_name, value_type, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke(BINDINGS[0], blueprint_name, value_type, position_json, graph_name, graph_id, graph_type, create_graph_if_missing)


def add_blueprint_switch_enum_node(blueprint_name, enum_type, has_default_pin, position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke(BINDINGS[1], blueprint_name, enum_type, has_default_pin, position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing)
