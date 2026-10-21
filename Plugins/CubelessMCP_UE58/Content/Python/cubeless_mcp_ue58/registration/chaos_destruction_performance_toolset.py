"""Original logical performance28 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import chaos_destruction_performance as domain

@unreal.uclass()
class CubelessChaosDestructionPerformanceTools(unreal.ToolsetDefinition):
    'Dedicated Chaos destruction performance public28/raw38: preserve all18 Native operations/7behavior groups, original eager admission/defaults, seven nullable omissions, raw hidden10 and scenario omissions. Native owns real per-frame metrics, removal timelines, one-way native contact and throttle/trace diagnostics; this connection is not their implementation or behavior proof. GT submission/enqueue differs from physics completion. Current request save/partial and historical in-flight session snapshots remain separate. No automatic retry/atomic rollback/packaged completion claim.'
    @toolset_registry.tool_call
    @staticmethod
    def run_chaos_destruction_performance_smoke(operation:str='"sample"',actor_name:str='""',solver_actor_name:str='""',start:str='null',end:str='null',expected_hit:str='true',trace_count:str='100',benchmark_path:str='"both"',strain:str='null',radius:str='100',breaking_velocity:str='600',breaking_velocity_vector:str='null',item_index:str='-1',expected_direction:str='null',dispatch_world_time_seconds:str='-1',witness_actor_name:str='""',floor_actor_name:str='"Chaos_StaticFloor"',one_way_level:str='-1',initial_witness_location:str='null',initial_witness_velocity:str='null',per_advance_breaks_allowed:str='2',per_advance_breaks_reschedule_limit:str='64',particle_release_throttling_min_count:str='64',particle_release_throttling_max_count:str='128',optimize_for_runtime_memory:str='true',performance_scenario:str='""',capture_limit_frames:str='90',require_play_session:str='true')->CubelessPythonOperationResult:
        'Run the original eighteen-operation Chaos performance/functional diagnostic on owned existing worlds. Preserve per-frame metrics and unavailable measurements, original tagged/removal/one-way/throttle/trace gates and raw-only controls. Begin acceptance is not session or five-run verification completion. Native body implementation and actual physics remain separately tracked.'
        rejected,report,raw=_guard_admission('run_chaos_destruction_performance_smoke')
        if rejected is not None:return reflected_result(rejected)
        try:values={'operation':_decode_value(operation),'actor_name':_decode_value(actor_name),'solver_actor_name':_decode_value(solver_actor_name),'start':_decode_value(start),'end':_decode_value(end),'expected_hit':_decode_value(expected_hit),'trace_count':_decode_value(trace_count),'benchmark_path':_decode_value(benchmark_path),'strain':_decode_value(strain),'radius':_decode_value(radius),'breaking_velocity':_decode_value(breaking_velocity),'breaking_velocity_vector':_decode_value(breaking_velocity_vector),'item_index':_decode_value(item_index),'expected_direction':_decode_value(expected_direction),'dispatch_world_time_seconds':_decode_value(dispatch_world_time_seconds),'witness_actor_name':_decode_value(witness_actor_name),'floor_actor_name':_decode_value(floor_actor_name),'one_way_level':_decode_value(one_way_level),'initial_witness_location':_decode_value(initial_witness_location),'initial_witness_velocity':_decode_value(initial_witness_velocity),'per_advance_breaks_allowed':_decode_value(per_advance_breaks_allowed),'per_advance_breaks_reschedule_limit':_decode_value(per_advance_breaks_reschedule_limit),'particle_release_throttling_min_count':_decode_value(particle_release_throttling_min_count),'particle_release_throttling_max_count':_decode_value(particle_release_throttling_max_count),'optimize_for_runtime_memory':_decode_value(optimize_for_runtime_memory),'performance_scenario':_decode_value(performance_scenario),'capture_limit_frames':_decode_value(capture_limit_frames),'require_play_session':_decode_value(require_play_session)}
        except Exception as error:return reflected_result(_carrier_rejected('run_chaos_destruction_performance_smoke',error,report,raw))
        return reflected_result(domain._named_mcp('run_chaos_destruction_performance_smoke',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessChaosDestructionPerformanceAttachmentLibrary', None), 'verify_chaos_destruction_performance_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ChaosDestructionPerformance adapter raw-input admission helper is unavailable')
        raw = helper(CubelessChaosDestructionPerformanceTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_chaos_destruction_performance_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ChaosDestructionPerformance adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('chaos_destruction_performance_raw_guard_admission_missing', message))
    result['stage'] = 'chaos_destruction_performance_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('chaos_destruction_performance_registration_carrier_invalid',str(error)))
    result['stage']='chaos_destruction_performance_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
