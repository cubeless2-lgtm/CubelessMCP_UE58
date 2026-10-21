"""Held CD local declaration12; exact hidden binding, no load or fallback."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessVariableLibrary"
BINDINGS = ("add_blueprint_local_variable",)
HIDDEN_ARGUMENTS = ("blueprint_path", "variable_name", "variable_type", "default_json",
    "category", "tooltip", "type_object", "is_array", "graph_name", "graph_id",
    "graph_type", "create_graph_if_missing")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def add_blueprint_local_variable(blueprint_path, variable_name, variable_type, default_json,
        category, tooltip, type_object, is_array, graph_name, graph_id, graph_type,
        create_graph_if_missing):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_local_variable_api_unavailable", "Exact held CD local binding unavailable; no load/fallback/writer dispatched")
    try:
        raw = function(blueprint_path, variable_name, variable_type, default_json,
            category, tooltip, type_object, is_array, graph_name, graph_id, graph_type,
            create_graph_if_missing)
    finally:
        # Release only our bound native callable before decoding or preserving an
        # exception frame. The same single dispatch/arguments and raw prefix remain.
        function = None
    try:
        native = _decode_result(raw)
        if (native.get("operation") != BINDINGS[0] or "changed" not in native
                or native["changed"] is not None and type(native["changed"]) is not bool):
            raise OperationError("invalid_native_local_variable_result", "Returned operation/changed type differs; complete raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
