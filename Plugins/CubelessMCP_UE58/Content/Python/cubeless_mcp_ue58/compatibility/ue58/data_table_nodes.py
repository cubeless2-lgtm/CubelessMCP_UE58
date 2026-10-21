"""MH native row/UDS Break leaves. No asset/UObject/pin wrapper is returned here."""

import unreal

from cubeless_mcp_ue58.operations.state import OperationError
from .functions import _decode_result


def _invoke(method, blueprint_path, row_struct_path, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    function = getattr(getattr(unreal, "CubelessDataTableLibrary", None), method, None)
    if not callable(function):
        # _run_action's existing pre-dispatch absence category retains changes[].
        raise OperationError("native_function_api_unavailable", "Build/load the exact CubelessDataTableLibrary graph leaf; no fallback executed")
    try:
        raw = function(blueprint_path, row_struct_path, node_position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing)
    finally:
        function = None
    return _decode_result(raw)


def add_blueprint_get_data_table_row_node(blueprint_path, row_struct_path, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke("add_blueprint_get_data_table_row_node", blueprint_path, row_struct_path, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing)


def add_blueprint_break_string_struct_node(blueprint_path, row_struct_path, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke("add_blueprint_break_string_struct_node", blueprint_path, row_struct_path, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing)
