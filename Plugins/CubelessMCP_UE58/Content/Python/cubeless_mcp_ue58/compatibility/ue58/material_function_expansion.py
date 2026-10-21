"""Original Material function expansion7 through the exact Editor native binding."""
import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessMaterialFunctionExpansionLibrary"
BINDINGS = ("expand_material_function_calls",)
PREPARATION_METHOD = "prepare_material_function_expansion_toolset"
HIDDEN_ARGUMENTS = ("material_path", "node_id", "recursive", "exclude_engine_functions", "max_passes", "save", "allow_partial_save")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def preparation_available():
    return callable(getattr(getattr(unreal, LIBRARY, None), PREPARATION_METHOD, None))


def expand_material_function_calls(material_path, node_id, recursive, exclude_engine_functions,
        max_passes, save, allow_partial_save):
    method = BINDINGS[0]
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("material_function_expansion_native_api_unavailable",
            "Exact native expansion binding unavailable; no graph/composite/rollback/save dispatched")
    raw = function(material_path, node_id, recursive, exclude_engine_functions, max_passes, save, allow_partial_save)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_material_function_expansion_result",
                "Returned native operation differs; actual raw expansion/save prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value
