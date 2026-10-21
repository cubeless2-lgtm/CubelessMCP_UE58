"""Original subsystem-from-controller7/construct-object7 through exact Editor native methods."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintConstructionLibrary"
BINDINGS = ('add_blueprint_get_subsystem_from_pc_node', 'add_blueprint_construct_object_node')
HIDDEN_ARGUMENTS_BY_METHOD = {'add_blueprint_get_subsystem_from_pc_node': ('blueprint_name', 'subsystem_class', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing'), 'add_blueprint_construct_object_node': ('blueprint_name', 'object_class', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_construction_method_invalid", "Only exact original subsystem-from-controller/construct-object methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_construction_native_api_unavailable", "Exact native construction binding unavailable; no node/default/cleanup dispatched")
    try:
        raw = function(*arguments)
    finally:
        function = None
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_construction_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def add_blueprint_get_subsystem_from_pc_node(blueprint_name, subsystem_class, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke('add_blueprint_get_subsystem_from_pc_node', (blueprint_name, subsystem_class, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))


def add_blueprint_construct_object_node(blueprint_name, object_class, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke('add_blueprint_construct_object_node', (blueprint_name, object_class, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))
