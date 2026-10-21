"""Native-command checkbox adapter and original viewport class readiness via exact Editor-only bindings."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessUMGUtilityLibrary"
BINDINGS = ('add_checkbox_to_widget', 'add_widget_to_viewport')
HIDDEN_ARGUMENTS_BY_METHOD = {'add_checkbox_to_widget': ('params_json',), 'add_widget_to_viewport': ('widget_name', 'z_order')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("umg_utility_method_invalid", "Only exact checkbox/viewport native methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("umg_utility_native_api_unavailable", "Exact UMG native binding unavailable; no dispatch occurred")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_umg_utility_result", "Returned operation differs; actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def add_checkbox_to_widget(params_json):
    return _invoke("add_checkbox_to_widget", (params_json,))


def add_widget_to_viewport(widget_name, z_order):
    return _invoke("add_widget_to_viewport", (widget_name, z_order))
