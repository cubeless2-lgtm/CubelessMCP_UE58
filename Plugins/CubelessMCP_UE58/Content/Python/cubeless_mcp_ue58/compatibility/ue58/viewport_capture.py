"""UE5.8 dedicated ViewportCapture Native0/1/1/1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessViewportCaptureLibrary'
BINDINGS=['list_viewport_bookmarks','set_active_viewport_realtime','capture_viewport_bookmark_screenshot','take_screenshot']
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={'list_viewport_bookmarks':[],'set_active_viewport_realtime':['realtime','realtime_frame_count'],'capture_viewport_bookmark_screenshot':['filepath','bookmark_index','redraw_count','clean_game_view','view_location','view_rotation','view_fov','restore_view_after_capture','fov'],'take_screenshot':['filepath']}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('viewport_capture_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('viewport_capture_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('viewport_capture_native_api_unavailable','Build/load the exact ViewportCapture Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('viewport_capture_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result


def list_viewport_bookmarks():return _invoke('list_viewport_bookmarks')


def set_active_viewport_realtime(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('set_active_viewport_realtime',text)


def capture_viewport_bookmark_screenshot(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('capture_viewport_bookmark_screenshot',text)


def take_screenshot(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('take_screenshot',text)
