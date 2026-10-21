"""Original StateTree creator3 binding; Native owns all engine work."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessStateTreeCreateLibrary"
METHOD = "create_state_tree"
BINDINGS = (METHOD,)


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {METHOD: callable(getattr(library, METHOD, None))}


def create_state_tree(name, package_path, schema_type):
    function = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(function):
        raise OperationError("state_tree_create_native_api_unavailable", "Exact StateTree creator unavailable; no fallback/load dispatched")
    raw = function(name, package_path, schema_type)
    try:
        native = _decode_result(raw)
        if (native.get("operation") != METHOD or "changed" not in native
                or native["changed"] is not None and type(native["changed"]) is not bool):
            raise OperationError("invalid_state_tree_create_native_result", "Native creator operation/current result differs; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
