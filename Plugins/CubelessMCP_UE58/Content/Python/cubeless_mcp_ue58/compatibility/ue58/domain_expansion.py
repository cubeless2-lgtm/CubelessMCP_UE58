"""Direct UE58 Editor attachment binding for the documented domain tools; no server/transport fallback."""
import json
import unreal
from ...operations.state import OperationError


def binding_status():
    fn = getattr(getattr(unreal, "CubelessDomainExpansionLibrary", None), "execute_domain_request", None)
    return {"execute_domain_request": callable(fn)}


def invoke(request):
    try:
        text = json.dumps(request, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        if len(text.encode("utf-16-le")) // 2 > 65536:
            raise OperationError("domain_request_bound", "Native JSON request exceeds 65536 UTF16 units")
    except Exception as error:
        error.native_call_dispatched = False
        raise
    fn = getattr(getattr(unreal, "CubelessDomainExpansionLibrary", None), "execute_domain_request", None)
    if not callable(fn):
        error = OperationError("domain_native_unavailable", "Build/load CubelessDomainExpansionLibrary; no fallback executed")
        error.native_call_dispatched = False
        raise error
    try:
        raw = fn(text)
    finally:
        fn = None
    try:
        value = json.loads(raw)
        if type(value) is not dict or type(value.get("success")) is not bool:
            raise ValueError("Expected native JSON object with boolean success")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value
