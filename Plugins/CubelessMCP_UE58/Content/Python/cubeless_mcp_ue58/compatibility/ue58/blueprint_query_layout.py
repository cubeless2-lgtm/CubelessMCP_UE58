"""Original Find6/Positions8 through exact Editor native query/layout methods.

Producer uses immutable Variable encoder so actual compiler/save flags survive.
The shared scalar bool/null decoder does not hardcode node-operation zero flags.
"""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintQueryLayoutLibrary"
BINDINGS = ('find_blueprint_nodes', 'set_blueprint_node_positions')
HIDDEN_ARGUMENTS_BY_METHOD = {'find_blueprint_nodes': ('blueprint_name', 'node_type_json', 'event_type_json', 'graph_name', 'graph_id', 'graph_type'), 'set_blueprint_node_positions': ('blueprint_name', 'graph_name', 'updates_json', 'graph_id', 'graph_type', 'dry_run', 'compile', 'save')}



def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}



def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_query_layout_method_invalid", "Only exact original query/layout methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_query_layout_native_api_unavailable", "Exact native query/layout binding unavailable; no import/repair/compiler/save dispatched")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_query_layout_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value



def find_blueprint_nodes(blueprint_name, node_type_json, event_type_json, graph_name, graph_id, graph_type):
    return _invoke('find_blueprint_nodes', (blueprint_name, node_type_json, event_type_json, graph_name, graph_id, graph_type))

def set_blueprint_node_positions(blueprint_name, graph_name, updates_json, graph_id, graph_type, dry_run, compile, save):
    return _invoke('set_blueprint_node_positions', (blueprint_name, graph_name, updates_json, graph_id, graph_type, dry_run, compile, save))
