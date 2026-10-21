"""Original metadata6/order5, exact existing native Variable library only."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessVariableLibrary"
BINDINGS = ("set_blueprint_variable_metadata", "set_blueprint_variable_order")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}


def _invoke(name, *arguments):
    if name not in BINDINGS:
        raise OperationError("variable_lifecycle_operation_invalid", "Only original metadata and order names are admitted")
    function = getattr(getattr(unreal, LIBRARY, None), name, None)
    if not callable(function):
        raise OperationError("native_variable_lifecycle_api_unavailable", "Exact native Variable lifecycle binding is unavailable; no fallback/load/writer dispatched")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if (value.get("operation") != name or "changed" not in value
                or value["changed"] is not None and type(value["changed"]) is not bool):
            raise OperationError("invalid_native_variable_lifecycle_result", "Wrong returned native operation/changed type; full raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def set_blueprint_variable_metadata(blueprint_path, variable_name, metadata_json,
        is_editable_json, instance_editable_json, expose_on_spawn_json):
    return _invoke(BINDINGS[0], blueprint_path, variable_name, metadata_json,
        is_editable_json, instance_editable_json, expose_on_spawn_json)


def set_blueprint_variable_order(blueprint_path, variable_order_json, strict, compile, save):
    return _invoke(BINDINGS[1], blueprint_path, variable_order_json, strict, compile, save)
