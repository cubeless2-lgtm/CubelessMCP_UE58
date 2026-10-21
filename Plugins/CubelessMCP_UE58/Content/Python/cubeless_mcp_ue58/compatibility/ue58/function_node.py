"""Original function-node9 finite route; no asset load or fallback in this adapter."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessFunctionNodeLibrary"
BINDINGS = ("add_blueprint_function_node",)
HIDDEN_ARGUMENTS = ("blueprint_path", "target", "function_name", "params_json", "position_json",
    "graph_name", "graph_id", "graph_type", "create_graph_if_missing")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def add_blueprint_function_node(blueprint_path, target, function_name, params_json, position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_function_node_api_unavailable", "Exact original function-node binding unavailable; no load/fallback/writer dispatched")
    try:
        raw = function(blueprint_path, target, function_name, params_json, position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing)
    finally:
        function = None
    try:
        native = _decode_result(raw)
        if (native.get("operation") != BINDINGS[0] or "changed" not in native
                or native["changed"] is not None and type(native["changed"]) is not bool):
            raise OperationError("invalid_native_function_node_result", "Returned operation/changed type differs; complete raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
