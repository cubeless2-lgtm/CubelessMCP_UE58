"""Original original CustomEvent12 with private optional presence13 through exact Editor native methods."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintDispatcherLifecycleLibrary"
BINDINGS = ('add_blueprint_custom_event_node',)
HIDDEN_METHOD = "add_blueprint_custom_event_signature"
HIDDEN_ARGUMENTS = ('blueprint_name', 'custom_event_name', 'position_json', 'signature_source_dispatcher_name', 'signature_source_class', 'signature_source_delegate_name', 'native_delegate_name', 'call_in_editor', 'has_call_in_editor', 'graph_name', 'graph_id', 'graph_type', 'create_graph_if_missing')

def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, HIDDEN_METHOD, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("custom_event_signatures_method_invalid", "Only exact original custom-event operation admitted")
    function = getattr(getattr(unreal, LIBRARY, None), HIDDEN_METHOD, None)
    if not callable(function):
        raise OperationError("custom_event_signatures_native_api_unavailable", "Exact native dispatcher binding unavailable; no declaration/node/cleanup dispatched")
    try:
        raw = function(*arguments)
    finally:
        function = None
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_custom_event_signatures_result", "Returned operation differs; complete actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value







def add_blueprint_custom_event_node(blueprint_name, custom_event_name, position_json, signature_source_dispatcher_name, signature_source_class, signature_source_delegate_name, native_delegate_name, call_in_editor, has_call_in_editor, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke('add_blueprint_custom_event_node', (blueprint_name, custom_event_name, position_json, signature_source_dispatcher_name, signature_source_class, signature_source_delegate_name, native_delegate_name, call_in_editor, has_call_in_editor, graph_name, graph_id, graph_type, create_graph_if_missing))
