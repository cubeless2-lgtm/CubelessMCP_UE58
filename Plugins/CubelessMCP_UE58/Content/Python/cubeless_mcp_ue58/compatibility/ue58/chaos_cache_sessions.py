"""UE5.8 dedicated ChaosCacheSession Native1/1/1/1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessChaosCacheSessionLibrary'
BINDINGS=('record_chaos_cache', 'validate_chaos_cache')
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={'record_chaos_cache': ('operation', 'cache_manager_actor_name', 'geometry_collection_actor_names', 'cache_collection_path', 'cache_names', 'solver_actor_name', 'duration_seconds', 'frame_rate', 'timeout_seconds', 'initial_trigger_mode', 'motion_velocity', 'per_advance_breaks_allowed', 'per_advance_breaks_reschedule_limit', 'particle_release_throttling_min_count', 'particle_release_throttling_max_count', 'cache_name_prefix', 'initial_trigger_propagation_depth', 'initial_trigger_delay_frames', 'require_saved_cache_transform_variation', 'preserve_initialization_fields_during_record', 'use_native_after_release_anchor_fallback', 'initial_trigger_radius', 'initial_trigger_root_transform_index'), 'validate_chaos_cache': ('operation', 'cache_manager_actor_name', 'cache_collection_path', 'sequence_path', 'geometry_collection_actor_names', 'sample_times', 'scrub_frames', 'repeat_count', 'timeout_seconds', 'validate_forward_reverse', 'sample_frame_rate')}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('chaos_cache_sessions_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('chaos_cache_sessions_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('chaos_cache_sessions_native_api_unavailable','Build/load the exact ChaosCacheSession Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('chaos_cache_sessions_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def record_chaos_cache(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('record_chaos_cache',text)


def validate_chaos_cache(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('validate_chaos_cache',text)


