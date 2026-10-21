"""Exact original stage/event inspection2 and event writer1 JSON carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_event_stage_authoring as domain

@unreal.uclass()
class CubelessNiagaraEventStageTools(unreal.ToolsetDefinition):
    "Asset observation does not prove live GPU execution; actual Native mutation/compile/save/partial receipts remain authoritative."
    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_simulation_stages(system_path:str,include_compile_data:str="true",include_script_compile_status:str="true",max_stages:str="128")->CubelessPythonOperationResult:
        "\n        Inspect Niagara SimulationStage settings and compiled stage data.\n\n        Args:\n            system_path: Niagara System package path or object path\n            include_compile_data: Include FillCompilationData output for generic stages\n            include_script_compile_status: Include per-stage script compile status\n            max_stages: Maximum SimulationStages to include across all emitters\n         Required actual compile wait/save completion is not provided by source registration."
        rejected,report,raw=_guard_admission("inspect_niagara_simulation_stages")
        if rejected is not None:return reflected_result(rejected)
        try:values={"system_path":_decode_value(system_path),"include_compile_data":_decode_value(include_compile_data),"include_script_compile_status":_decode_value(include_script_compile_status),"max_stages":_decode_value(max_stages)}
        except Exception as error:return reflected_result(_carrier_rejected("inspect_niagara_simulation_stages",error,report,raw))
        return reflected_result(domain._named_mcp("inspect_niagara_simulation_stages",values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_event_handlers(system_path:str,emitter_index:str="null",emitter_name:str="\"\"",max_handlers:str="256")->CubelessPythonOperationResult:
        "Read-only inspect of Niagara Event Handler topology and settings.\n\n        Returns each matching emitter's handler usage ID, source emitter/event,\n        execution mode, spawn limits, script graph, and output-node readback.\n         Required actual compile wait/save completion is not provided by source registration."
        rejected,report,raw=_guard_admission("inspect_niagara_event_handlers")
        if rejected is not None:return reflected_result(rejected)
        try:values={"system_path":_decode_value(system_path),"emitter_index":_decode_value(emitter_index),"emitter_name":_decode_value(emitter_name),"max_handlers":_decode_value(max_handlers)}
        except Exception as error:return reflected_result(_carrier_rejected("inspect_niagara_event_handlers",error,report,raw))
        return reflected_result(domain._named_mcp("inspect_niagara_event_handlers",values))

    @toolset_registry.tool_call
    @staticmethod
    def add_niagara_event_handler(system_path:str,emitter_index:str="null",emitter_name:str="\"\"",source_emitter_index:str="null",source_emitter_name:str="\"\"",source_emitter_id:str="\"\"",source_event_name:str="\"CollisionEvent\"",execution_mode:str="\"SpawnedParticles\"",spawn_number:str="1",max_events_per_frame:str="64",random_spawn_number:str="false",min_spawn_number:str="0",update_attribute_initial_values:str="true",skip_if_duplicate:str="true",allow_source_edit:str="false",save:str="true",request_compile:str="true",wait_for_completion:str="true",timeout_seconds:str="60.0",poll_interval_seconds:str="0.1")->CubelessPythonAsyncOperationResult:
        "Create one CPU Niagara Event Handler output in a target emitter.\n\n        The returned target_usage_id can be passed to\n        add_niagara_stock_module_to_stack(target_usage=\"ParticleEventScript\")\n        to populate the handler. Mutation is temp-only by default, and\n        duplicate source-event consumers fail closed.\n         Required actual compile wait/save completion is not provided by source registration."
        rejected,report,raw=_guard_admission("add_niagara_event_handler")
        if rejected is not None:return domain.completed_result(rejected)
        try:values={"system_path":_decode_value(system_path),"emitter_index":_decode_value(emitter_index),"emitter_name":_decode_value(emitter_name),"source_emitter_index":_decode_value(source_emitter_index),"source_emitter_name":_decode_value(source_emitter_name),"source_emitter_id":_decode_value(source_emitter_id),"source_event_name":_decode_value(source_event_name),"execution_mode":_decode_value(execution_mode),"spawn_number":_decode_value(spawn_number),"max_events_per_frame":_decode_value(max_events_per_frame),"random_spawn_number":_decode_value(random_spawn_number),"min_spawn_number":_decode_value(min_spawn_number),"update_attribute_initial_values":_decode_value(update_attribute_initial_values),"skip_if_duplicate":_decode_value(skip_if_duplicate),"allow_source_edit":_decode_value(allow_source_edit),"save":_decode_value(save),"request_compile":_decode_value(request_compile),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("add_niagara_event_handler",error,report,raw))
        return domain._named_mcp("add_niagara_event_handler",values)

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraEventStageAttachmentLibrary', None), 'verify_niagara_event_stage_authoring_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraEventStage adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraEventStageTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_event_stage_authoring_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraEventStage adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_event_stage_authoring_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_event_stage_authoring_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_event_stage_authoring_registration_carrier_invalid',str(error)))
    result['stage']='niagara_event_stage_authoring_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
