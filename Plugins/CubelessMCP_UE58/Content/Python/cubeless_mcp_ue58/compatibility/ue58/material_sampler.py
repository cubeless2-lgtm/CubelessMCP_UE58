"""Exact original sampler-type operation6 through one hidden native binding."""
import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessMaterialSamplerLibrary"
BINDINGS = ("set_material_texture_sample_sampler_type_mcp",)


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def set_material_texture_sample_sampler_type_mcp(material_path, texture_path, sampler_type, dry_run, save, compile):
    method = BINDINGS[0]
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("material_sampler_native_api_unavailable",
            "Exact native sampler binding unavailable; no sampler edit, compile or save dispatched")
    raw = function(material_path, texture_path, sampler_type, dry_run, save, compile)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_material_sampler_result",
                "Native result lacks the original operation; returned raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value
