"""Exact StateTree list4/inspect2 bindings; Native owns all engine work."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessStateTreeReadLibrary"
BINDINGS = ("list_state_trees", "inspect_state_tree")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {method: callable(getattr(library, method, None)) for method in BINDINGS}


def _function(method):
    if method not in BINDINGS:
        raise OperationError("state_tree_read_native_api_unavailable", "Only original StateTree list/inspect bindings admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("state_tree_read_native_api_unavailable", "Exact StateTree Native API unavailable; no load or fallback dispatched")
    return function


def _returned(method, raw):
    try:
        native = _decode_result(raw)
        if (native.get("operation") != method or "changed" not in native
                or native["changed"] is not None and type(native["changed"]) is not bool):
            raise OperationError("invalid_state_tree_read_native_result", "Native operation or changed receipt differs; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def list_state_trees(directory, recursive, max_results, offset):
    method = "list_state_trees"
    raw = _function(method)(directory, recursive, max_results, offset)
    return _returned(method, raw)


def inspect_state_tree(asset_path, max_states):
    method = "inspect_state_tree"
    raw = _function(method)(asset_path, max_states)
    return _returned(method, raw)
