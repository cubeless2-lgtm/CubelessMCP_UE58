"""Original AnimGraph Settings9/ProtectedTopology15 via exact Editor bindings."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessAnimGraphInspectionLibrary"
BINDINGS = ('inspect_anim_graph_node_settings', 'inspect_anim_graph_protected_topology')
HIDDEN_ARGUMENTS_BY_METHOD = {'inspect_anim_graph_node_settings': ('blueprint_name', 'node_id', 'node_type', 'title_contains', 'include_pins', 'max_depth', 'graph_name', 'graph_id', 'graph_type'), 'inspect_anim_graph_protected_topology': ('blueprint_name', 'node_id', 'node_type', 'title_contains', 'include_pins', 'include_pose_pins', 'include_links', 'include_non_pose_links', 'include_settings', 'max_depth', 'max_nodes', 'max_links', 'graph_name', 'graph_id', 'graph_type')}




def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}



def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("anim_graph_inspection_method_invalid", "Only exact original static AnimGraph inspection methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("anim_graph_inspection_native_api_unavailable", "Exact native static AnimGraph inspection binding unavailable; no Native AnimGraph inspection dispatch occurred")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_anim_graph_inspection_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value



def inspect_anim_graph_node_settings(blueprint_name, node_id, node_type, title_contains, include_pins, max_depth, graph_name, graph_id, graph_type):
    return _invoke('inspect_anim_graph_node_settings', (blueprint_name, node_id, node_type, title_contains, include_pins, max_depth, graph_name, graph_id, graph_type))

def inspect_anim_graph_protected_topology(blueprint_name, node_id, node_type, title_contains, include_pins, include_pose_pins, include_links, include_non_pose_links, include_settings, max_depth, max_nodes, max_links, graph_name, graph_id, graph_type):
    return _invoke('inspect_anim_graph_protected_topology', (blueprint_name, node_id, node_type, title_contains, include_pins, include_pose_pins, include_links, include_non_pose_links, include_settings, max_depth, max_nodes, max_links, graph_name, graph_id, graph_type))

