"""UE5.8 dedicated EditorLevelSession Native1/1/1/1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessEditorLevelSessionLibrary'
BINDINGS=["open_editor_level","get_editor_level_open_status","clear_dirty_package_flags","safe_new_preview_map"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"open_editor_level":["level_path","dry_run","allow_dirty_packages","load_as_template","show_progress","deferred_load_delay_seconds","map_path"],"get_editor_level_open_status":["request_id"],"clear_dirty_package_flags":["package_paths","package_path","clear_all_dirty","dry_run","allow_non_temp_path","required_root"],"safe_new_preview_map":["target_path","map_path","dry_run","allow_dirty_packages","overwrite_existing","allow_non_temp_path","required_root","is_partitioned_world","level_path"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('editor_level_session_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('editor_level_session_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('editor_level_session_native_api_unavailable','Build/load the exact EditorLevelSession Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('editor_level_session_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def open_editor_level(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("open_editor_level",text)


def get_editor_level_open_status(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("get_editor_level_open_status",text)


def clear_dirty_package_flags(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("clear_dirty_package_flags",text)


def safe_new_preview_map(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("safe_new_preview_map",text)
