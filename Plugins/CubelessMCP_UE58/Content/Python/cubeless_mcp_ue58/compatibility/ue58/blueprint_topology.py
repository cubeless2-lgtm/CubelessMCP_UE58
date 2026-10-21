"""Original topology15 fixed Editor observer; actual Variable result prefix."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintTopologyLibrary"
BINDINGS = ('inspect_blueprint_graph_call_topology',)
HIDDEN_ARGUMENTS_BY_METHOD = {'inspect_blueprint_graph_call_topology': ('blueprint_name', 'graph_name', 'graph_id', 'graph_type', 'graph_name_contains', 'node_type', 'title_contains', 'reference_contains', 'include_pins', 'include_links', 'include_references', 'max_graphs', 'max_nodes_per_graph', 'max_links_per_graph', 'max_references_per_node')}




def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}



def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_topology_method_invalid", "Only exact original static topology methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_topology_native_api_unavailable", "Exact native static topology binding unavailable; no Native topology dispatch occurred")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_topology_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value



def inspect_blueprint_graph_call_topology(blueprint_name, graph_name, graph_id, graph_type, graph_name_contains, node_type, title_contains, reference_contains, include_pins, include_links, include_references, max_graphs, max_nodes_per_graph, max_links_per_graph, max_references_per_node):
    return _invoke('inspect_blueprint_graph_call_topology', (blueprint_name, graph_name, graph_id, graph_type, graph_name_contains, node_type, title_contains, reference_contains, include_pins, include_links, include_references, max_graphs, max_nodes_per_graph, max_links_per_graph, max_references_per_node))
