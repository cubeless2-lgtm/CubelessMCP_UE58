"""UE58 direct loaded-instance packing binding; no transport or preview-world fallback."""
import json
import unreal
from ...operations.state import OperationError


def invoke(request):
    fn = getattr(getattr(unreal, "CubelessLoadedLevelInstancePackingLibrary", None),
                 "execute_packing_request", None)
    try:
        text = json.dumps(request, ensure_ascii=False, allow_nan=False)
        if len(text.encode("utf-16-le")) // 2 > 16384:
            raise OperationError("packing_request_bound", "Request exceeds 16384 UTF16 units")
        if not callable(fn):
            raise OperationError("packing_native_unavailable", "Build/load the Editor packing binding; no fallback executed")
    except Exception as error:
        error.native_call_dispatched = False
        raise
    raw = fn(text)
    try:
        native = json.loads(raw)
        if type(native) is not dict or type(native.get("success")) is not bool:
            raise ValueError("Expected a native JSON object with boolean success")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
