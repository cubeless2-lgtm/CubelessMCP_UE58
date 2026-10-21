"""Original logical viewport7/7/10/13 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import chaos_cinematic_sequence as operation

@unreal.uclass()
class CubelessChaosCinematicSequenceTools(unreal.ToolsetDefinition):
    'Chaos cinematic sequence/cache binding/timing and hierarchy, proxy, anchor, cache, Sequencer, MRQ manifest/dependency audit. Preserve original public conversions, raw hidden mapping and timing omission, partial manager/component/track/section/save receipts and SDK unknown. Inspect creates no mutation journal. Native SDK waits are synchronous; no atomic rollback, automatic retry, PNG decode, whole pipeline or packaged-runtime claim.'
    @toolset_registry.tool_call
    @staticmethod
    def create_chaos_level_sequence(output_sequence_path:str,camera_actor_name:str='""',display_rate:str='30.0',tick_resolution:str='24000',start_frame:str='0',end_frame:str='60',overwrite_existing:str='false')->CubelessPythonOperationResult:
        'Create a new native LevelSequence with original rate/tick/camera gates and forced sequence save. overwrite_existing is public but the original handler still refuses collisions. Failed camera or save stages may retain actual new package/sequence state; no cleanup/retry is invented.'
        rejected,report,raw=_guard_admission('create_chaos_level_sequence')
        if rejected is not None:return reflected_result(rejected)
        try:values={'output_sequence_path':_decode_value(output_sequence_path),'camera_actor_name':_decode_value(camera_actor_name),'display_rate':_decode_value(display_rate),'tick_resolution':_decode_value(tick_resolution),'start_frame':_decode_value(start_frame),'end_frame':_decode_value(end_frame),'overwrite_existing':_decode_value(overwrite_existing)}
        except Exception as error:return reflected_result(_carrier_rejected('create_chaos_level_sequence',error,report,raw))
        return reflected_result(operation._named_mcp('create_chaos_level_sequence',values))

    @toolset_registry.tool_call
    @staticmethod
    def bind_chaos_cache_to_sequence(sequence_path:str,cache_manager_actor_name:str,cache_collection_path:str,section_start_frame:str='0',section_end_frame:str='60',cache_play_rate:str='1.0',cache_start_offset_seconds:str='0.0')->CubelessPythonOperationResult:
        'Bind native Chaos Cache manager/components and reuse exact manager binding, cache track and timing section where possible. Hidden component/cache arrays are raw-only. Preserve EndEvaluate/physics/notifier/save-sequence/save-level prefixes and final sim suppression; current managed writer requires editor idle.'
        rejected,report,raw=_guard_admission('bind_chaos_cache_to_sequence')
        if rejected is not None:return reflected_result(rejected)
        try:values={'sequence_path':_decode_value(sequence_path),'cache_manager_actor_name':_decode_value(cache_manager_actor_name),'cache_collection_path':_decode_value(cache_collection_path),'section_start_frame':_decode_value(section_start_frame),'section_end_frame':_decode_value(section_end_frame),'cache_play_rate':_decode_value(cache_play_rate),'cache_start_offset_seconds':_decode_value(cache_start_offset_seconds)}
        except Exception as error:return reflected_result(_carrier_rejected('bind_chaos_cache_to_sequence',error,report,raw))
        return reflected_result(operation._named_mcp('bind_chaos_cache_to_sequence',values))

    @toolset_registry.tool_call
    @staticmethod
    def configure_chaos_sequence_timing(sequence_path:str,display_rate:str='30.0',tick_resolution:str='24000',start_frame:str='0',end_frame:str='60',section_start_frame:str='0',section_end_frame:str='60',cache_play_rate:str='1.0',cache_start_offset_seconds:str='0.0',reverse:str='false')->CubelessPythonOperationResult:
        'Configure playback and every ChaosCache section range/rate/offset/reverse. Named section defaults0/60 differ from raw omission. Original 30fps/two-second assertion can fail after valid edits/save; preserve actual changed/saved state before failure.'
        rejected,report,raw=_guard_admission('configure_chaos_sequence_timing')
        if rejected is not None:return reflected_result(rejected)
        try:values={'sequence_path':_decode_value(sequence_path),'display_rate':_decode_value(display_rate),'tick_resolution':_decode_value(tick_resolution),'start_frame':_decode_value(start_frame),'end_frame':_decode_value(end_frame),'section_start_frame':_decode_value(section_start_frame),'section_end_frame':_decode_value(section_end_frame),'cache_play_rate':_decode_value(cache_play_rate),'cache_start_offset_seconds':_decode_value(cache_start_offset_seconds),'reverse':_decode_value(reverse)}
        except Exception as error:return reflected_result(_carrier_rejected('configure_chaos_sequence_timing',error,report,raw))
        return reflected_result(operation._named_mcp('configure_chaos_sequence_timing',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_cinematic_pipeline(geometry_collection_path:str,sequence_path:str,cache_manager_actor_name:str,map_path:str,chaos_state_sha256:str,field_actor_name:str='"Chaos_AnchorField"',require_movie_render_pipeline:str='true',mrq_output_directory:str='""',mrq_manifest_path:str='""',expected_display_rate:str='30',expected_tick_resolution:str='24000',expected_playback_start_frame:str='0',expected_playback_end_frame:str='60')->CubelessPythonOperationResult:
        'Observe native hierarchy/proxy/anchor/cache/sequence and MRQ manifest/file SHA/dependency criteria. Loads/cache/registry/global effects can remain unknown with direct write/compile/save0. The inspector does not decode PNGs or recompute black/perceptual evidence and does not create a mutation journal.'
        rejected,report,raw=_guard_admission('inspect_chaos_cinematic_pipeline')
        if rejected is not None:return reflected_result(rejected)
        try:values={'geometry_collection_path':_decode_value(geometry_collection_path),'sequence_path':_decode_value(sequence_path),'cache_manager_actor_name':_decode_value(cache_manager_actor_name),'map_path':_decode_value(map_path),'chaos_state_sha256':_decode_value(chaos_state_sha256),'field_actor_name':_decode_value(field_actor_name),'require_movie_render_pipeline':_decode_value(require_movie_render_pipeline),'mrq_output_directory':_decode_value(mrq_output_directory),'mrq_manifest_path':_decode_value(mrq_manifest_path),'expected_display_rate':_decode_value(expected_display_rate),'expected_tick_resolution':_decode_value(expected_tick_resolution),'expected_playback_start_frame':_decode_value(expected_playback_start_frame),'expected_playback_end_frame':_decode_value(expected_playback_end_frame)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_chaos_cinematic_pipeline',error,report,raw))
        return reflected_result(operation._named_mcp('inspect_chaos_cinematic_pipeline',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessChaosCinematicSequenceAttachmentLibrary', None), 'verify_chaos_cinematic_sequence_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ChaosCinematicSequence adapter raw-input admission helper is unavailable')
        raw = helper(CubelessChaosCinematicSequenceTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_chaos_cinematic_sequence_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ChaosCinematicSequence adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('chaos_cinematic_sequence_raw_guard_admission_missing', message))
    result['stage'] = 'chaos_cinematic_sequence_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('chaos_cinematic_sequence_registration_carrier_invalid',str(error)))
    result['stage']='chaos_cinematic_sequence_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
