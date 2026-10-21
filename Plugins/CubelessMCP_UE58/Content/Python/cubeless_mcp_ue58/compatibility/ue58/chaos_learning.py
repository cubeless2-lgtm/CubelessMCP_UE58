"""UE5.8 dedicated ChaosLearning Native1/1/1/1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessChaosLearningLibrary'
BINDINGS=('create_chaos_destruction_learning_fixture', 'run_chaos_cache_smoke')
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={'create_chaos_destruction_learning_fixture': ('dry_run', 'force_rebuild', 'save_assets', 'configure_current_level', 'asset_root'), 'run_chaos_cache_smoke': ('operation', 'cache_collection_path', 'cache_manager_actor_name', 'component_actor_name', 'cache_name')}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('chaos_learning_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('chaos_learning_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('chaos_learning_native_api_unavailable','Build/load the exact ChaosLearning Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('chaos_learning_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def create_chaos_destruction_learning_fixture(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('create_chaos_destruction_learning_fixture',text)


def run_chaos_cache_smoke(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('run_chaos_cache_smoke',text)


