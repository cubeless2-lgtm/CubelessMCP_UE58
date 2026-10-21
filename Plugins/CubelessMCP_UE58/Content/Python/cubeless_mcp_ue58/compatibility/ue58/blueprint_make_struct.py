"""Exact original MakeStruct7 hidden bridge; no loading or generic fallback."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessFunctionLibrary"
BINDINGS = ("add_blueprint_make_struct_node",)
PREPARATION = "prepare_blueprint_make_struct_toolset"


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def add_blueprint_make_struct_node(blueprint_path, struct_type, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_function_api_unavailable", "Exact MakeStruct native binding unavailable; no fallback/load/writer dispatched")
    try:
        raw = function(blueprint_path, struct_type, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
    finally:
        function = None
    try:
        value = _decode_result(raw)
        if value.get("operation") != BINDINGS[0]:
            raise OperationError("invalid_native_function_result", "Returned native operation differs; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True; error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value
