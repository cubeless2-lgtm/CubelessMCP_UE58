"""Exact UE58 stock Map node bridge; no runtime map or implicit specialization."""

import unreal

from cubeless_mcp_ue58.operations.state import OperationError
from .functions import _decode_result


def add_blueprint_map_function_node(blueprint_path, operation, key_type, value_type, param_defaults_json,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    function = getattr(getattr(unreal, "CubelessMapLibrary", None), "add_blueprint_map_function_node", None)
    if not callable(function):
        raise OperationError("native_function_api_unavailable", "Build/load CubelessMapLibrary.add_blueprint_map_function_node; no fallback executed")
    try:
        raw = function(blueprint_path, operation, key_type, value_type, param_defaults_json,
            node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
    finally:
        function = None  # Normal RC before JSON decode or an exception traceback can retain this frame.
    return _decode_result(raw)
