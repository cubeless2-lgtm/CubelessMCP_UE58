"""Finite legacy Break helpers through the existing UE58 call-node binding."""

import unreal

from cubeless_mcp_ue58.operations.state import OperationError
from . import functions


_HELPERS = {"vector": "BreakVector", "rotator": "BreakRotator", "transform": "BreakTransform"}


def _asset_data(blueprint_path, struct_type, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    function = getattr(getattr(unreal, "CubelessFunctionLibrary", None), "add_blueprint_break_asset_data_node", None)
    if not callable(function):
        raise OperationError("native_function_api_unavailable", "Build/load the exact native AssetData Break leaf; no fallback executed")
    try:
        raw = function(blueprint_path, struct_type, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
    finally:
        function = None
    return functions._decode_result(raw)


def add_blueprint_break_struct_node(blueprint_path, struct_type, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    if type(struct_type) is str and struct_type == "/Script/CoreUObject.AssetData":
        return _asset_data(blueprint_path, struct_type, node_position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing)
    if type(struct_type) is str and struct_type.startswith("/"):
        from . import data_table_nodes
        return data_table_nodes.add_blueprint_break_string_struct_node(blueprint_path, struct_type, node_position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing)
    # Only these original three helper mappings; no arbitrary type/function lookup.
    canonical = struct_type.lower().strip().replace(" ", "").replace("_", "").replace("-", "")
    helper = _HELPERS.get(canonical)
    if helper is None:
        raise OperationError("struct_break_type_unsupported", "Legacy Break helper supports vector, rotator or transform only")
    binding = getattr(getattr(unreal, "CubelessFunctionLibrary", None), "add_blueprint_call_function_node", None)
    if not callable(binding):
        raise OperationError("native_function_api_unavailable", "Build/load the existing CubelessFunctionLibrary.add_blueprint_call_function_node; no fallback executed")
    binding = None
    result = functions.add_blueprint_call_function_node(blueprint_path, "/Script/Engine.KismetMathLibrary", helper,
        "{}", node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing,
        False, False, False, False, False)
    result["data"].update(legacy_node_kind="break_struct", requested_struct_type=struct_type, struct_type=canonical,
        requested_function_class="/Script/Engine.KismetMathLibrary", requested_function_name=helper)
    return result
