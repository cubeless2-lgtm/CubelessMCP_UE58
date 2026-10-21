"""UE5.8 dedicated ChaosDestructionPerformance Native1/1/1/1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessChaosDestructionPerformanceLibrary'
BINDINGS=('run_chaos_destruction_performance_smoke',)
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={'run_chaos_destruction_performance_smoke': ('operation', 'actor_name', 'solver_actor_name', 'start', 'end', 'expected_hit', 'trace_count', 'benchmark_path', 'strain', 'radius', 'breaking_velocity', 'breaking_velocity_vector', 'item_index', 'expected_direction', 'dispatch_world_time_seconds', 'witness_actor_name', 'floor_actor_name', 'one_way_level', 'initial_witness_location', 'initial_witness_velocity', 'per_advance_breaks_allowed', 'per_advance_breaks_reschedule_limit', 'particle_release_throttling_min_count', 'particle_release_throttling_max_count', 'optimize_for_runtime_memory', 'performance_scenario', 'capture_limit_frames', 'require_play_session', 'expected_component_count', 'prepare_live_component', 'live_external_strain', 'reset_witness_location', 'ballistic_lift_velocity', 'fixed_delta_seconds', 'scenario', 'trace_complex', 'propagation_depth', 'propagation_factor')}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('chaos_destruction_performance_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('chaos_destruction_performance_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('chaos_destruction_performance_native_api_unavailable','Build/load the exact ChaosDestructionPerformance Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('chaos_destruction_performance_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def run_chaos_destruction_performance_smoke(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('run_chaos_destruction_performance_smoke',text)
