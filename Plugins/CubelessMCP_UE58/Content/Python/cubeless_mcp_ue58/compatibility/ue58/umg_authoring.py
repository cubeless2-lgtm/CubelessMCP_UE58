"""Original UMG create/text/button via exact Editor-only native bindings."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessUMGAuthoringLibrary"
BINDINGS = ('create_umg_widget_blueprint', 'add_text_block_to_widget', 'add_button_to_widget')
HIDDEN_ARGUMENTS_BY_METHOD = {'create_umg_widget_blueprint': ('widget_name', 'parent_class', 'path'), 'add_text_block_to_widget': ('widget_name', 'text_block_name', 'text', 'position_json', 'size_json', 'font_size', 'color_json'), 'add_button_to_widget': ('widget_name', 'button_name', 'text', 'position_json', 'size_json', 'font_size', 'color_json', 'background_color_json')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("umg_authoring_method_invalid", "Only original UMG create/text/button methods admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("umg_authoring_native_api_unavailable", "Exact UMG native binding unavailable; no dispatch occurred")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_umg_authoring_result", "Returned operation differs; actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def create_umg_widget_blueprint(widget_name, parent_class, path):
    return _invoke('create_umg_widget_blueprint', (widget_name, parent_class, path))

def add_text_block_to_widget(widget_name, text_block_name, text, position_json, size_json, font_size, color_json):
    return _invoke('add_text_block_to_widget', (widget_name, text_block_name, text, position_json, size_json, font_size, color_json))

def add_button_to_widget(widget_name, button_name, text, position_json, size_json, font_size, color_json, background_color_json):
    return _invoke('add_button_to_widget', (widget_name, button_name, text, position_json, size_json, font_size, color_json, background_color_json))
