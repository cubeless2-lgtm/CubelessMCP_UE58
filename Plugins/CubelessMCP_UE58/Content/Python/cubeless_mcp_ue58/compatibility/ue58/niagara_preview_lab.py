"""UE5.8 dedicated Niagara Preview Lab Native1 x4. No transport, fallback, readiness or hidden progress."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraPreviewLabLibrary'
BINDINGS=["get_niagara_preview_lab_state", "cleanup_niagara_preview_lab", "capture_niagara_preview_lab_view", "preview_niagara_system_in_preview_lab"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"get_niagara_preview_lab_state": [], "cleanup_niagara_preview_lab": [], "capture_niagara_preview_lab_view": ["filepath", "view", "auto_frame", "camera_location", "camera_rotation", "camera_fov", "allow_current_map_preview", "view_location", "view_rotation", "view_fov"], "preview_niagara_system_in_preview_lab": ["system_path", "filepath", "view", "label", "warmup_time", "warmup_tick_delta", "cleanup_before", "cleanup_after", "location", "scale", "auto_frame", "camera_location", "camera_rotation", "camera_fov", "user_parameter_overrides", "preserve_override_parameters", "allow_current_map_preview", "view_location", "view_rotation", "view_fov"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_preview_lab_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_preview_lab_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_preview_lab_native_api_unavailable','Build/load the exact NiagaraPreviewLab Native API; no fallback')
        error.native_call_dispatched=False;raise error
    if method=='preview_niagara_system_in_preview_lab':
        try:raw=fn(*args)
        finally:fn=None
    else:raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_preview_lab_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def get_niagara_preview_lab_state(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("get_niagara_preview_lab_state",text)

def cleanup_niagara_preview_lab(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("cleanup_niagara_preview_lab",text)

def capture_niagara_preview_lab_view(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("capture_niagara_preview_lab_view",text)

def preview_niagara_system_in_preview_lab(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("preview_niagara_system_in_preview_lab",text)

