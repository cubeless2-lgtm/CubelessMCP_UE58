"""Preserved ForLoop and finite additive typed ForEach with one journal."""
from cubeless_mcp_ue58.compatibility.ue58 import loops as api
from . import functions, variables
from .state import OperationError, fail, mutate, new_result


def _arguments(blueprint_name, loop_type, first_index, last_index, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing,
        element_type=None, type_object=None, enum_type=None):
    kind = functions._text(loop_type, "loop_type")
    normalized = kind.strip().lower().replace(" ", "").replace("_", "").replace("-", "")
    for_each = normalized in ("foreach", "foreachloop")
    if normalized not in ("for", "forloop") and not for_each:
        raise OperationError("loop_type_unsupported", "Finite ForLoop or typed ForEach only; breakable/while remain incomplete")
    element = functions._text("" if element_type is None else element_type, "element_type", empty=True)
    object_name = functions._text("" if type_object is None else type_object, "type_object", empty=True)
    enum_name = functions._text("" if enum_type is None else enum_type, "enum_type", empty=True)
    if not for_each and (element or object_name or enum_name):
        raise OperationError("loop_type_selector_unsupported", "ForLoop accepts no element descriptor; its original index contract is unchanged")
    if for_each:
        if first_index is not None or last_index is not None:
            raise OperationError("loop_index_unsupported", "ForEach has no First/Last index inputs; supplied indices are not ignored")
        from .arrays import _ELEMENTS, _SKELETAL_COMPONENT_TYPE, _normalize
        skeletal_component = _normalize(element) == "object" and object_name == _SKELETAL_COMPONENT_TYPE and not enum_name
        asset_data = _normalize(element) == "struct" and object_name == "/Script/CoreUObject.AssetData" and not enum_name
        if not skeletal_component and not asset_data and (_ELEMENTS.get(_normalize(element)) is None or object_name or enum_name):
            raise OperationError("loop_element_type_unsupported", "Typed ForEach requires primitive selectors, exact SkeletalMeshComponent object or CoreUObject.AssetData struct with empty enum_type; Native admits int/bool/name and those exact types only")
    for name, value in (("first_index", first_index), ("last_index", last_index)):
        if value is not None and (type(value) is not int or not -(2**31) <= value <= 2**31-1):
            raise OperationError("loop_index_invalid", name + " must be null or an exact JSON int32 integer")
    create = functions._bool(create_graph_if_missing, "create_graph_if_missing")
    position = functions._json(node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
    if type(position) is not list or len(position) != 2 or any(type(v) is not int or abs(v) > 1000000 for v in position):
        raise OperationError("loop_position_invalid", "node_position_json must be a bounded pair of integer coordinates")
    arguments = {**functions._selectors(blueprint_name, graph_name, graph_id, graph_type),
        "loop_type": kind, "first_index_json": variables._json_text(first_index),
        "last_index_json": variables._json_text(last_index), "node_position_json": variables._json_text(position),
        "create_graph_if_missing": create}
    if for_each:
        arguments.update(element_type=element, type_object=object_name, enum_type=enum_name)
    # Existing ForLoop journal fingerprints retain exactly their original keys.
    return arguments


def _run(result, arguments, checkpoint_callback=None):
    result["data"].update(node_only_contract=not arguments["create_graph_if_missing"], implicit_data_wiring=False,
        for_each_implemented=arguments.get("element_type") is not None,
        full_legacy_loop_parity=False, loop_runtime_execution_verified=False,
        dependency_level="EngineAPI", stock_macro_dependency="/Engine/EditorBlueprintResources/StandardMacros")
    functions._run_action(result, "add_blueprint_loop_node", arguments, checkpoint_callback, operation_api=api)


def add_blueprint_loop_node(blueprint_name, loop_type="for_loop", first_index=None, last_index=None,
        node_position_json="[0,0]", graph_name=None, graph_id=None, graph_type=None,
        create_graph_if_missing=False, operation_id=None,
        element_type=None, type_object=None, enum_type=None):
    try:
        args = _arguments(blueprint_name, loop_type, first_index, last_index, node_position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing, element_type, type_object, enum_type)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("add_blueprint_loop_node", args, operation_id, lambda result: _run(result, args))


def add_blueprint_loop_node_action(result, blueprint_name, loop_type="for_loop", first_index=None, last_index=None,
        node_position_json="[0,0]", graph_name=None, graph_id=None, graph_type=None,
        create_graph_if_missing=False, checkpoint_callback=None,
        element_type=None, type_object=None, enum_type=None):
    _run(result, _arguments(blueprint_name, loop_type, first_index, last_index, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing, element_type, type_object, enum_type), checkpoint_callback)
