"""UE5.8 dedicated NiagaraRenderer Native1 x7. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraRendererLibrary'
BINDINGS=["inspect_niagara_renderers", "set_niagara_renderer_material", "set_niagara_sprite_renderer_settings", "add_niagara_sprite_renderer", "add_niagara_mesh_renderer", "set_niagara_renderer_enabled", "cleanup_niagara_renderers"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"inspect_niagara_renderers": ["system_path"], "set_niagara_renderer_material": ["system_path", "material_path", "emitter_index", "emitter_name", "renderer_index", "material_slot_index", "allow_source_edit", "save"], "set_niagara_sprite_renderer_settings": ["system_path", "alignment", "facing_mode", "sub_image_size", "emitter_index", "emitter_name", "renderer_index", "allow_source_edit", "save", "request_compile"], "add_niagara_sprite_renderer": ["system_path", "material_path", "emitter_index", "emitter_name", "alignment", "facing_mode", "sub_image_size", "skip_if_existing", "allow_source_edit", "save", "request_compile"], "add_niagara_mesh_renderer": ["system_path", "emitter_index", "emitter_name", "meshes_binding_parameter", "mesh_index_binding", "mesh_paths", "skip_if_existing", "allow_source_edit", "save", "request_compile", "meshes"], "set_niagara_renderer_enabled": ["system_path", "enabled", "emitter_index", "emitter_name", "renderer_index", "allow_source_edit", "save", "request_compile"], "cleanup_niagara_renderers": ["system_path", "remove_renderer_indices", "clear_meshes_binding_renderer_indices", "expected_renderers", "expected_renderer_count", "emitter_index", "emitter_name", "dry_run", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"]}


def binding_status():
    status={name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}
    from . import niagara_scratch_user_recipes as workflow
    starts=workflow.binding_status()
    status['cleanup_niagara_renderers']=status['cleanup_niagara_renderers'] and starts.get('cleanup_niagara_renderers') is True and all(starts.get(key) is True for key in ('job_class_available','job_snapshot_callable','job_cancel_callable'))
    return status


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_renderers_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_renderers_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_renderers_native_api_unavailable','Build/load the exact NiagaraRenderer Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_renderers_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def inspect_niagara_renderers(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_renderers",text)

def set_niagara_renderer_material(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_renderer_material",text)

def set_niagara_sprite_renderer_settings(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_sprite_renderer_settings",text)

def add_niagara_sprite_renderer(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("add_niagara_sprite_renderer",text)

def add_niagara_mesh_renderer(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("add_niagara_mesh_renderer",text)

def set_niagara_renderer_enabled(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_renderer_enabled",text)

def cleanup_niagara_renderers(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("cleanup_niagara_renderers",text)

