"""Installed UE58 Editor enum APIs; no Toolset/server calls in domain work."""
import json
import unreal
from ...operations.state import OperationError


def binding_status():
    lib = getattr(unreal, "CubelessUserDefinedEnumLibrary", None)
    return {name: callable(getattr(lib, name, None)) for name in
            ("inspect_user_defined_enum", "edit_user_defined_enum")}


def invoke(method, *args):
    fn = getattr(getattr(unreal, "CubelessUserDefinedEnumLibrary", None), method, None)
    if not callable(fn):
        raise OperationError("enum_editing_native_unavailable", "Required UE58 Editor binding unavailable; no fallback executed")
    raw = fn(*args)
    try:
        value = json.loads(raw)
        if type(value) is not dict or type(value.get("success")) is not bool or type(value.get("data")) is not dict:
            raise ValueError("Expected typed native enum receipt")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value
