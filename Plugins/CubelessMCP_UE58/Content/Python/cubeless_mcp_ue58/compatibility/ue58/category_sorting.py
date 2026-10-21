"""Original category-sorting6 on the exact existing native Variable library."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessVariableLibrary"
BINDINGS = ("set_blueprint_category_sorting",)


def binding_status():
    library = getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}


def set_blueprint_category_sorting(blueprint_path, category_order_json, category_to_front, append_existing, save, compile):
    function = getattr(getattr(unreal,LIBRARY,None),BINDINGS[0],None)
    if not callable(function):
        raise OperationError("native_category_sorting_api_unavailable","Exact native category sorting binding unavailable; no fallback/load/writer dispatched")
    raw = function(blueprint_path,category_order_json,category_to_front,append_existing,save,compile)
    try:
        native = _decode_result(raw)
        if (native.get("operation") != BINDINGS[0] or "changed" not in native
                or native["changed"] is not None and type(native["changed"]) is not bool):
            raise OperationError("invalid_native_category_sorting_result","Wrong returned operation/changed type; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
