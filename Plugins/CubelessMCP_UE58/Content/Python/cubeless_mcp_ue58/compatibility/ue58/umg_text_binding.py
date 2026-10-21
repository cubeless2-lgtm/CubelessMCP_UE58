"""Original Text binding public4 through the selected native property-path repair."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessUMGTextBindingLibrary"
BINDINGS = ('set_text_block_binding',)
HIDDEN_ARGUMENTS_BY_METHOD = {'set_text_block_binding': ('widget_name', 'text_block_name', 'binding_property', 'binding_type')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("umg_text_binding_method_invalid", "Only original Text binding method admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("umg_text_binding_native_api_unavailable", "Exact UMG native binding unavailable; no dispatch occurred")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_umg_text_binding_result", "Returned operation differs; actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def set_text_block_binding(widget_name, text_block_name, binding_property, binding_type):
    return _invoke("set_text_block_binding", (widget_name, text_block_name, binding_property, binding_type))
