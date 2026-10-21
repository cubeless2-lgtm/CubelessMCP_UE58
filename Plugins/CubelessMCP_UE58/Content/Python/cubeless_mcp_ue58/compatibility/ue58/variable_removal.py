"""Original RemoveVariable2 on the existing exact native Variable library."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessVariableLibrary"
BINDINGS = ("remove_blueprint_variable",)


def binding_status():
    library = getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}


def remove_blueprint_variable(blueprint_path, variable_name):
    function = getattr(getattr(unreal,LIBRARY,None),BINDINGS[0],None)
    if not callable(function):
        raise OperationError("native_variable_removal_api_unavailable","Exact native removal binding unavailable; no fallback/load/writer dispatched")
    try:
        raw = function(blueprint_path,variable_name)
    finally:
        # Release the bound native callable before decode or exception retention.
        function = None
    try:
        native = _decode_result(raw)
        if (native.get("operation") != BINDINGS[0] or "changed" not in native
                or native["changed"] is not None and type(native["changed"]) is not bool):
            raise OperationError("invalid_native_variable_removal_result","Wrong returned operation/changed type; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
