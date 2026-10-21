"""UE58 stock loop constructor binding; domain code owns no transport."""
import unreal
from cubeless_mcp_ue58.operations.state import OperationError
from .functions import _decode_result


def add_blueprint_loop_node(blueprint_path, loop_type, first_index_json, last_index_json,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing,
        element_type="", type_object="", enum_type=""):
    method = getattr(getattr(unreal, "CubelessLoopLibrary", None), "add_blueprint_loop_node", None)
    if not callable(method):
        raise OperationError("native_function_api_unavailable", "Build/load the CubelessLoopLibrary constructor; no fallback dispatched")
    if create_graph_if_missing:
        try:
            raw = method(blueprint_path, loop_type, first_index_json, last_index_json,
                node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing,
                element_type, type_object, enum_type)
        finally:
            method = None  # Release this composite graph/node call before decode or traceback allocation.
        return _decode_result(raw)
    typed_for_each = str(loop_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {"foreach", "foreachloop"}
    element = str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "")
    finite_new_element = ((element == "name" and not type_object and not enum_type)
        or (element == "object" and type_object == "/Script/Engine.SkeletalMeshComponent" and not enum_type)
        or (element == "struct" and type_object == "/Script/CoreUObject.AssetData" and not enum_type))
    if typed_for_each and finite_new_element:
        try:
            raw = method(blueprint_path, loop_type, first_index_json, last_index_json,
                node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing,
                element_type, type_object, enum_type)
        finally:
            method = None  # Release only the new typed cohorts before decode/traceback allocation.
        return _decode_result(raw)
    return _decode_result(method(blueprint_path, loop_type, first_index_json, last_index_json,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing,
        element_type, type_object, enum_type))
