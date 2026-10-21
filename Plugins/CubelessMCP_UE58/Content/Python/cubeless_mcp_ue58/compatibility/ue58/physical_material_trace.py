"""UE58 native trace5. JSON vector carriers are a private ABI detail."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessPhysicalMaterialTraceLibrary"
METHOD = "line_trace_physical_material"
BINDINGS = (METHOD,)


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def line_trace_physical_material(start, end, trace_channel="visibility", trace_complex=False,
        return_physical_material=True):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("physical_material_trace_native_api_unavailable", "UE58 physical-material trace binding is unavailable")
    start_json = json.dumps(start, allow_nan=False, separators=(",", ":"))
    end_json = json.dumps(end, allow_nan=False, separators=(",", ":"))
    raw = fn(start_json, end_json, trace_channel, trace_complex, return_physical_material)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("physical_material_trace_native_result_invalid", "Native trace operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("physical_material_trace_native_result_invalid", "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
