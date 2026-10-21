"""Original StateTree Add6/Compile2 bindings; Native owns composite work."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessStateTreeMutationLibrary"
BINDINGS = ("add_state_tree_state", "compile_state_tree")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {method: callable(getattr(library, method, None)) for method in BINDINGS}


def _function(method):
    if method not in BINDINGS:
        raise OperationError("state_tree_mutators_native_api_unavailable", "Only original StateTree Add/Compile bindings admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("state_tree_mutators_native_api_unavailable", "Exact StateTree Native mutation API unavailable; no fallback/load dispatched")
    return function


def _returned(method, raw):
    try:
        native = _decode_result(raw)
        if (native.get("operation") != method or "changed" not in native
                or native["changed"] is not None and type(native["changed"]) is not bool):
            raise OperationError("invalid_state_tree_mutators_native_result", "Native operation/current result differs; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def add_state_tree_state(asset_path, state_name, parent_path, state_type, selection_behavior, dry_run):
    method = "add_state_tree_state"
    raw = _function(method)(asset_path, state_name, parent_path, state_type, selection_behavior, dry_run)
    return _returned(method, raw)


def compile_state_tree(asset_path, save):
    method = "compile_state_tree"
    raw = _function(method)(asset_path, save)
    return _returned(method, raw)
