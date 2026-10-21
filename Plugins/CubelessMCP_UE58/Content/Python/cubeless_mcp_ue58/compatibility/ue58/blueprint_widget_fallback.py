"""Original fallback analyzer public3 through the native diagnostic report publisher."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessBlueprintWidgetFallbackLibrary"
BINDINGS = ('analyze_blueprint_widget_fallbacks_mcp',)
HIDDEN_ARGUMENTS_BY_METHOD = {'analyze_blueprint_widget_fallbacks_mcp': ('source_root', 'target_root', 'suffix')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments):
    if method not in BINDINGS:
        raise OperationError("blueprint_widget_fallback_method_invalid", "Only original Blueprint/Widget fallback analyzer admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("blueprint_widget_fallback_native_api_unavailable", "Exact fallback diagnostic native binding unavailable; no dispatch occurred")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_widget_fallback_result", "Returned operation differs; actual raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def analyze_blueprint_widget_fallbacks_mcp(source_root, target_root, suffix):
    return _invoke("analyze_blueprint_widget_fallbacks_mcp", (source_root, target_root, suffix))
