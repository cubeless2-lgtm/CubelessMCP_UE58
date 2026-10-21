"""UE5.8 dedicated ChaosCinematicSequence Native1/1/1/1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessChaosCinematicSequenceLibrary'
BINDINGS=('create_chaos_level_sequence', 'bind_chaos_cache_to_sequence', 'configure_chaos_sequence_timing', 'inspect_chaos_cinematic_pipeline')
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={'create_chaos_level_sequence': ('output_sequence_path', 'camera_actor_name', 'display_rate', 'tick_resolution', 'start_frame', 'end_frame', 'overwrite_existing'), 'bind_chaos_cache_to_sequence': ('sequence_path', 'cache_manager_actor_name', 'cache_collection_path', 'section_start_frame', 'section_end_frame', 'cache_play_rate', 'cache_start_offset_seconds', 'geometry_collection_actor_names', 'cache_names'), 'configure_chaos_sequence_timing': ('sequence_path', 'display_rate', 'tick_resolution', 'start_frame', 'end_frame', 'section_start_frame', 'section_end_frame', 'cache_play_rate', 'cache_start_offset_seconds', 'reverse'), 'inspect_chaos_cinematic_pipeline': ('geometry_collection_path', 'sequence_path', 'cache_manager_actor_name', 'map_path', 'chaos_state_sha256', 'field_actor_name', 'require_movie_render_pipeline', 'mrq_output_directory', 'mrq_manifest_path', 'expected_display_rate', 'expected_tick_resolution', 'expected_playback_start_frame', 'expected_playback_end_frame')}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('chaos_cinematic_sequence_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('chaos_cinematic_sequence_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('chaos_cinematic_sequence_native_api_unavailable','Build/load the exact ChaosCinematicSequence Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('chaos_cinematic_sequence_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def create_chaos_level_sequence(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('create_chaos_level_sequence',text)


def bind_chaos_cache_to_sequence(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('bind_chaos_cache_to_sequence',text)


def configure_chaos_sequence_timing(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('configure_chaos_sequence_timing',text)


def inspect_chaos_cinematic_pipeline(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('inspect_chaos_cinematic_pipeline',text)


