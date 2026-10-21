"""Original component reference7 fixed Editor node authoring; actual Variable result prefix."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintComponentReferenceLibrary"
BINDINGS = ('add_blueprint_get_self_component_reference',)
HIDDEN_ARGUMENTS_BY_METHOD = {'add_blueprint_get_self_component_reference': ('blueprint_name', 'component_name', 'position_json', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')}




def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}



def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_component_reference_method_invalid", "Only exact original static component reference methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_component_reference_native_api_unavailable", "Exact native static component reference binding unavailable; no Native component reference dispatch occurred")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_component_reference_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value



def add_blueprint_get_self_component_reference(blueprint_name, component_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke('add_blueprint_get_self_component_reference', (blueprint_name, component_name, position_json, graph_name, graph_id, graph_type, create_graph_if_missing))
