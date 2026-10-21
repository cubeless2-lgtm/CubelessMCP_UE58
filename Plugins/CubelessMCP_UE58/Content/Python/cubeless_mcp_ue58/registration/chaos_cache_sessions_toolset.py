"""Original logical cache session15/9 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import chaos_cache_sessions as domain

@unreal.uclass()
class CubelessChaosCacheSessionTools(unreal.ToolsetDefinition):
    'Bounded native Chaos cache record/validation sessions. Begin acceptance is not whole-session completion. Preserve Named nullable/default/timeout behavior and raw presence/hidden record8/validate2, actual current request versus cumulative session save/unknown/restore receipts. Both operations mutate live session state and use only the existing runtime-write admission. Native ticker does not update returned Python journals. No automatic retry, atomic rollback, forced wait timeout, simulation/render/MRQ/pipeline completion claim.'
    @toolset_registry.tool_call
    @staticmethod
    def record_chaos_cache(operation:str,cache_manager_actor_name:str,geometry_collection_actor_names:str,cache_collection_path:str,cache_names:str='null',solver_actor_name:str='""',duration_seconds:str='2.0',frame_rate:str='30.0',timeout_seconds:str='10.0',initial_trigger_mode:str='"strain_velocity"',motion_velocity:str='null',per_advance_breaks_allowed:str='8',per_advance_breaks_reschedule_limit:str='64',particle_release_throttling_min_count:str='64',particle_release_throttling_max_count:str='128')->CubelessPythonOperationResult:
        'Begin, sample, finish or abort the native cache recording session. Named always requires its original four fields and defaults cache names/motion only for None; raw operation-only/hidden controls remain separate. Fresh PIE and generation/identity/binding/solver/physics/record/save/restore checks are Native-owned. Scheduling and accumulated callback receipts are distinct from current request effects.'
        rejected,report,raw=_guard_admission('record_chaos_cache')
        if rejected is not None:return reflected_result(rejected)
        try:values={'operation':_decode_value(operation),'cache_manager_actor_name':_decode_value(cache_manager_actor_name),'geometry_collection_actor_names':_decode_value(geometry_collection_actor_names),'cache_collection_path':_decode_value(cache_collection_path),'cache_names':_decode_value(cache_names),'solver_actor_name':_decode_value(solver_actor_name),'duration_seconds':_decode_value(duration_seconds),'frame_rate':_decode_value(frame_rate),'timeout_seconds':_decode_value(timeout_seconds),'initial_trigger_mode':_decode_value(initial_trigger_mode),'motion_velocity':_decode_value(motion_velocity),'per_advance_breaks_allowed':_decode_value(per_advance_breaks_allowed),'per_advance_breaks_reschedule_limit':_decode_value(per_advance_breaks_reschedule_limit),'particle_release_throttling_min_count':_decode_value(particle_release_throttling_min_count),'particle_release_throttling_max_count':_decode_value(particle_release_throttling_max_count)}
        except Exception as error:return reflected_result(_carrier_rejected('record_chaos_cache',error,report,raw))
        return reflected_result(domain._named_mcp('record_chaos_cache',values))

    @toolset_registry.tool_call
    @staticmethod
    def validate_chaos_cache(operation:str,cache_manager_actor_name:str,cache_collection_path:str,sequence_path:str,geometry_collection_actor_names:str='null',sample_times:str='null',scrub_frames:str='null',repeat_count:str='2',timeout_seconds:str='10.0')->CubelessPythonOperationResult:
        'Begin, sample, finish or abort native repeated/reverse cache validation. This is a mutable manager/player/ticker session, not a read-only query. Named None defaults and forced validate_forward_reverse=True remain original; raw sample_frame_rate/forward_reverse presence is Native-owned. Current-request save fields do not become true merely because an earlier callback saved.'
        rejected,report,raw=_guard_admission('validate_chaos_cache')
        if rejected is not None:return reflected_result(rejected)
        try:values={'operation':_decode_value(operation),'cache_manager_actor_name':_decode_value(cache_manager_actor_name),'cache_collection_path':_decode_value(cache_collection_path),'sequence_path':_decode_value(sequence_path),'geometry_collection_actor_names':_decode_value(geometry_collection_actor_names),'sample_times':_decode_value(sample_times),'scrub_frames':_decode_value(scrub_frames),'repeat_count':_decode_value(repeat_count),'timeout_seconds':_decode_value(timeout_seconds)}
        except Exception as error:return reflected_result(_carrier_rejected('validate_chaos_cache',error,report,raw))
        return reflected_result(domain._named_mcp('validate_chaos_cache',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessChaosCacheSessionAttachmentLibrary', None), 'verify_chaos_cache_sessions_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ChaosCacheSession adapter raw-input admission helper is unavailable')
        raw = helper(CubelessChaosCacheSessionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_chaos_cache_sessions_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ChaosCacheSession adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('chaos_cache_sessions_raw_guard_admission_missing', message))
    result['stage'] = 'chaos_cache_sessions_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('chaos_cache_sessions_registration_carrier_invalid',str(error)))
    result['stage']='chaos_cache_sessions_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
