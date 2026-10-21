"""One hidden native stock DataTable Select leaf; no UObject value roundtrip."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


def add_blueprint_data_table_bool_select_node(blueprint_path, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    function = getattr(getattr(unreal, "CubelessFunctionLibrary", None),
        "add_blueprint_data_table_bool_select_node", None)
    if not callable(function):
        raise OperationError("native_function_api_unavailable", "Exact native DataTable Select leaf is unavailable; no fallback dispatched")
    try:
        raw = function(blueprint_path, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
    finally:
        function = None
    try:
        value = _decode_result(raw)
        if value.get("operation") != "add_blueprint_data_table_bool_select_node":
            raise OperationError("invalid_native_datatable_select_result", "Native operation differs; actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    return value
