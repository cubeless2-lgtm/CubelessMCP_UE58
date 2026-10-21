"""UE5.8 dedicated ChaosSolverSettings Native1/1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessChaosSolverSettingsLibrary'
BINDINGS=["inspect_chaos_solver_settings","configure_chaos_solver_settings"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"inspect_chaos_solver_settings":["actor_name","actor_label","world_context","include_runtime_state","max_results"],"configure_chaos_solver_settings":["settings","actor_name","actor_label","world_context","expected_current","dry_run","save_level"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('chaos_solver_settings_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('chaos_solver_settings_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('chaos_solver_settings_native_api_unavailable','Build/load the exact ChaosSolverSettings Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('chaos_solver_settings_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result






def inspect_chaos_solver_settings(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('inspect_chaos_solver_settings',text)

def configure_chaos_solver_settings(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('configure_chaos_solver_settings',text)
