"""Named UE58 node-only array binding; native construction is separately verified."""

import unreal

from cubeless_mcp_ue58.operations.state import OperationError
from .functions import _decode_result


def add_blueprint_array_function_node(blueprint_path, operation, element_type, param_defaults_json,
        type_object, enum_type, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    function = getattr(getattr(unreal, "CubelessArrayLibrary", None), "add_blueprint_array_function_node", None)
    if not callable(function):
        # Existing shared mutation core treats this exact code as a known
        # missing binding before dispatch, rather than an uncertain mutation.
        raise OperationError("native_function_api_unavailable", "Build/load CubelessArrayLibrary.add_blueprint_array_function_node; no fallback executed")
    if create_graph_if_missing:
        try:
            raw = function(blueprint_path, operation, element_type, param_defaults_json,
                "" if type_object is None else type_object, "" if enum_type is None else enum_type,
                node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
        finally:
            function = None  # Release this composite graph/node call before decode or traceback allocation.
        return _decode_result(raw)
    extended_primitive = str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {
        "int64", "integer64", "float", "single", "double", "real", "text"}
    if extended_primitive:
        try:
            raw = function(blueprint_path, operation, element_type, param_defaults_json,
                "" if type_object is None else type_object, "" if enum_type is None else enum_type,
                node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
        finally:
            function = None  # Release only this finite bound leaf before JSON/traceback allocations.
        return _decode_result(raw)
    if str(element_type).casefold().replace("_", "").replace("-", "").replace(" ", "") == "struct" and type_object == "/Script/CoreUObject.Vector4":
        try:
            raw = function(blueprint_path, operation, element_type, param_defaults_json,
                type_object, "" if enum_type is None else enum_type,
                node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
        finally:
            function = None  # Release this bound callable by normal RC before decode/traceback allocation.
        return _decode_result(raw)
    skeletal_array = (str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") == "object"
        and type_object == "/Script/Engine.SkeletalMeshComponent"
        and str(operation).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {"length", "num", "count", "size", "get", "set", "setarrayelem", "add", "clear", "empty", "reset", "contains", "containsitem", "addunique", "unique", "insert", "insertitem", "remove", "removeitem"})
    new_component_array = (str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") == "object"
        and type_object in {"/Script/Engine.ActorComponent", "/Script/Engine.ChildActorComponent"}
        and str(operation).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {"set", "setarrayelem", "addunique", "unique", "insert", "insertitem", "contains", "containsitem", "remove", "removeitem"})
    descriptor_length = (str(operation).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {"length", "num", "count", "size", "get", "set", "setarrayelem", "add", "clear", "empty", "reset", "contains", "containsitem", "addunique", "unique", "insert", "insertitem", "remove", "removeitem"}
        and (enum_type is None or enum_type == "")
        and ((str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") == "class"
                and type_object in {None, "", "/Script/CoreUObject.Object", "/Script/Engine.Actor", "/Script/Engine.ActorComponent", "/Script/Engine.ChildActorComponent", "/Script/Engine.SkeletalMeshComponent"})
            or (str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") == "object"
                and type_object in {None, "", "/Script/CoreUObject.Object", "/Script/Engine.Actor"})))
    value_struct_length = (str(operation).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {"length", "num", "count", "size", "get", "set", "setarrayelem", "add", "clear", "empty", "reset", "contains", "containsitem", "addunique", "unique", "insert", "insertitem", "remove", "removeitem"}
        and str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {"rotator", "transform"})
    uds_length = (str(operation).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {"length", "num", "count", "size", "get", "set", "setarrayelem", "add", "clear", "empty", "reset", "contains", "containsitem", "addunique", "unique", "insert", "insertitem", "remove", "removeitem"}
        and str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {"struct", "scriptstruct"}
        and not (str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") == "struct" and type_object == "/Script/CoreUObject.Vector4"))
    if skeletal_array or new_component_array or descriptor_length or value_struct_length or uds_length:
        try:
            raw = function(blueprint_path, operation, element_type, param_defaults_json,
                "" if (descriptor_length or value_struct_length or uds_length) and type_object is None else type_object, "" if enum_type is None else enum_type,
                node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
        finally:
            function = None  # Release only this new finite binding before decode/traceback allocation.
        return _decode_result(raw)
    name_length = (str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") == "name"
        and str(operation).strip().casefold().replace("_", "").replace("-", "").replace(" ", "") in {"length", "num", "count", "size"})
    if name_length:
        try:
            raw = function(blueprint_path, operation, element_type, param_defaults_json,
                "" if type_object is None else type_object, "" if enum_type is None else enum_type,
                node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing)
        finally:
            function = None  # The same Name length callable is released before JSON decoding.
        return _decode_result(raw)
    return _decode_result(function(blueprint_path, operation, element_type, param_defaults_json,
        "" if type_object is None else type_object, "" if enum_type is None else enum_type,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing))
