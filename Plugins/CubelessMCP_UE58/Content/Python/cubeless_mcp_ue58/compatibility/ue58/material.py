"""UE 5.8.3 Material native bindings; no registry or external transport here."""

import json
import unreal

from cubeless_mcp_ue58.operations.state import OperationError

MAX_GRAPH_NODES = 2048


def _invoke(method, *arguments):
    library = getattr(unreal, "CubelessMaterialLibrary", None)
    function = getattr(library, method, None) if library is not None else None
    if function is None:
        raise OperationError("native_material_api_unavailable", "Build/load the UE58 CubelessMaterialLibrary before using Material tools; no fallback executed")
    raw = function(*arguments)
    try:
        result = json.loads(raw)
    except (TypeError, ValueError) as error:
        raise OperationError("invalid_native_material_result", "Native Material result was not valid JSON") from error
    if not isinstance(result, dict) or type(result.get("success")) is not bool or not isinstance(result.get("data"), dict):
        raise OperationError("invalid_native_material_result", "Native Material result lacks its success/data envelope")
    return result


def resolve_material_graph(material_path, graph_type):
    return _invoke("resolve_material_graph", material_path, graph_type, MAX_GRAPH_NODES)


def list_material_nodes(material_path, graph_type, node_type, desc_contains, include_pins):
    return _invoke("list_material_nodes", material_path, graph_type, node_type, desc_contains, include_pins, MAX_GRAPH_NODES)


def analyze_material_graph(material_path, graph_type, include_nodes, include_usage, max_referencers):
    return _invoke("analyze_material_graph", material_path, graph_type, include_nodes, include_usage, max_referencers, MAX_GRAPH_NODES)


def list_material_collection_parameter_nodes(material_path, graph_type):
    return _invoke("list_material_collection_parameter_nodes", material_path, graph_type, MAX_GRAPH_NODES)


def get_material_parameter_collection_values(collection_path, parameter_names, include_asset_defaults, include_runtime):
    return _invoke("get_material_parameter_collection_values", collection_path, parameter_names or [], include_asset_defaults, include_runtime)


def connect_material_nodes(material_path, from_node_id, to_node_id, to_input, from_output, graph_type):
    return _invoke("connect_material_nodes", material_path, from_node_id, to_node_id, to_input, from_output, graph_type, MAX_GRAPH_NODES)


def connect_material_property(material_path, from_node_id, material_property, from_output):
    return _invoke("connect_material_property", material_path, from_node_id, material_property, from_output, MAX_GRAPH_NODES)


def delete_material_node(material_path, node_id, graph_type):
    return _invoke("delete_material_node", material_path, node_id, graph_type, MAX_GRAPH_NODES)


def layout_material_nodes(material_path, graph_type):
    return _invoke("layout_material_nodes", material_path, graph_type, MAX_GRAPH_NODES)


def add_material_node(material_path, expression_class, graph_type, node_position, selected_asset, properties_json):
    return _invoke("add_material_node", material_path, expression_class, graph_type,
                   node_position[0], node_position[1], selected_asset, properties_json, MAX_GRAPH_NODES)


def set_material_node_property(material_path, node_id, property_name, value_json, graph_type):
    return _invoke("set_material_node_property", material_path, node_id, property_name, value_json, graph_type, MAX_GRAPH_NODES)


def add_custom_material_node(material_path, code, output_type, inputs_json, description, graph_type,
                             node_position, additional_defines_json, include_file_paths_json):
    return _invoke("add_custom_material_node", material_path, code, output_type, inputs_json, description, graph_type,
                   node_position[0], node_position[1], additional_defines_json, include_file_paths_json, MAX_GRAPH_NODES)
