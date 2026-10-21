"""Exact stock Is Valid exec macro leaf; no reflected UObject roundtrip."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


def add_blueprint_is_valid_exec_macro_node(blueprint_path, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    function = getattr(getattr(unreal, "CubelessFunctionLibrary", None),
        "add_blueprint_is_valid_exec_macro_node", None)
    if not callable(function):
        raise OperationError("native_function_api_unavailable", "Exact native Is Valid exec macro leaf is unavailable; no fallback dispatched")
    try:
        raw = function(blueprint_path, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
    finally:
        function = None
    try:
        value = _decode_result(raw)
        if value.get("operation") != "add_blueprint_is_valid_exec_macro_node":
            raise OperationError("invalid_native_validity_macro_result", "Native operation differs; actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    return value
