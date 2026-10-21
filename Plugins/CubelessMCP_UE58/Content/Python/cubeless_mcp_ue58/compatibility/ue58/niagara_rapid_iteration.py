"""Exact find-only current RI observation; no writer or fallback."""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessNiagaraModuleLibrary"
BINDINGS = ("inspect_niagara_module_rapid_iteration",)


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def inspect_niagara_module_rapid_iteration(system_path):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_niagara_rapid_iteration_read_unavailable", "Read binding unavailable; no load/compile/save/writer dispatched")
    raw = function(system_path)
    try:
        native = _decode_result(raw)
        if (native.get("operation") != BINDINGS[0] or "changed" not in native
                or native["changed"] is not None and type(native["changed"]) is not bool):
            raise OperationError("invalid_native_niagara_rapid_iteration_read", "Actual read operation/changed type differs; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
