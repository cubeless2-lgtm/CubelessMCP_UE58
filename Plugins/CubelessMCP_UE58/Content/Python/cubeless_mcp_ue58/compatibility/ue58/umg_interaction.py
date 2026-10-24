"""Original UMG slider composition/event binding via exact Editor-only native bindings."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessUMGInteractionLibrary"
BINDINGS = ('add_sliders_to_widget', 'bind_widget_event')
HIDDEN_ARGUMENTS_BY_METHOD = {'add_sliders_to_widget': ('widget_name', 'sliders_json'), 'bind_widget_event': ('widget_name', 'widget_component_name', 'event_name', 'function_name')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS and method != "bind_widget_event_raw":
        raise OperationError("umg_interaction_method_invalid", "Only original UMG slider/event methods methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("umg_interaction_native_api_unavailable", "Exact UMG native binding unavailable; no dispatch occurred")
    if method == "bind_widget_event_raw":
        try:
            raw = function(*arguments)
        finally:
            function = None
    else:
        raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != ("bind_widget_event" if method == "bind_widget_event_raw" else method):
            raise OperationError("invalid_native_umg_interaction_result", "Returned operation differs; actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def add_sliders_to_widget(widget_name, sliders_json):
    return _invoke("add_sliders_to_widget", (widget_name, sliders_json))


def bind_widget_event(widget_name, widget_component_name, event_name, function_name):
    return _invoke("bind_widget_event", (widget_name, widget_component_name, event_name, function_name))


def bind_widget_event_raw(widget_name, widget_component_name, event_name, function_name, node_position_json):
    # Deliberately absent from Named BINDINGS/readiness/public schema inventories.
    return _invoke("bind_widget_event_raw", (widget_name, widget_component_name, event_name, function_name, node_position_json))
