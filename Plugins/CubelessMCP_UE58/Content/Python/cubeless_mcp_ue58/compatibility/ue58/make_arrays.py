"""One finite Engine MakeArray constructor binding, without RPC."""
import unreal
from cubeless_mcp_ue58.operations.state import OperationError
from .functions import _decode_result


def add_blueprint_make_array_node(blueprint_path, element_type, input_count_json,
        values_json, type_object, enum_type, node_position_json, graph_name,
        graph_id, graph_type, create_graph_if_missing):
    method = getattr(getattr(unreal, "CubelessMakeArrayLibrary", None), "add_blueprint_make_array_node", None)
    if not callable(method):
        raise OperationError("native_function_api_unavailable", "Build/load the Editor-only CubelessMakeArrayLibrary; no fallback dispatched")
    if str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {
            "int64", "integer64", "float", "single", "double", "real", "text",
            "object", "class", "rotator", "transform", "struct", "scriptstruct"}:
        try:
            raw = method(blueprint_path, element_type, input_count_json,
                values_json, type_object, enum_type, node_position_json, graph_name,
                graph_id, graph_type, create_graph_if_missing)
        finally:
            method = None  # Release only the finite new callable before decode/traceback allocation.
        return _decode_result(raw)
    if str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") == "name":
        try:
            raw = method(blueprint_path, element_type, input_count_json,
                values_json, type_object, enum_type, node_position_json, graph_name,
                graph_id, graph_type, create_graph_if_missing)
        finally:
            method = None  # Release this finite Name callable before decode/traceback allocation.
        return _decode_result(raw)
    return _decode_result(method(blueprint_path, element_type, input_count_json,
        values_json, type_object, enum_type, node_position_json, graph_name,
        graph_id, graph_type, create_graph_if_missing))
