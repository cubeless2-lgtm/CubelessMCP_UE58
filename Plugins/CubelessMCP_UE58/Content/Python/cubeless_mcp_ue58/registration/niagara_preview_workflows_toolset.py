"""GZ public4 value carriers: same-job async final for open/swap/sample, synchronous Player metadata state."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_preview_workflows as domain

@unreal.uclass()
class CubelessNiagaraPreviewWorkflowTools(unreal.ToolsetDefinition):
    "Owned Preview Player UI, exact borrowed-component swap/restore and ordered Lab sample workflows."
    @toolset_registry.tool_call
    @staticmethod
    def open_niagara_preview_player(system_path:str="\"\"")->CubelessPythonAsyncOperationResult:
        "Original open_niagara_preview_player contract. Native owns actual UI, warmup, restore and partial results; source-only registration is not runtime proof."
        rejected,report,raw=_guard_admission("open_niagara_preview_player")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={"system_path":_decode_value(system_path)}
        except Exception as error:return domain.completed_result(_carrier_rejected("open_niagara_preview_player",error,report,raw))
        return domain.begin("open_niagara_preview_player",arguments)

    @toolset_registry.tool_call
    @staticmethod
    def get_niagara_preview_player_state()->CubelessPythonOperationResult:
        "Original get_niagara_preview_player_state contract. Native owns actual UI, warmup, restore and partial results; source-only registration is not runtime proof."
        rejected,report,raw=_guard_admission("get_niagara_preview_player_state")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={}
        except Exception as error:return reflected_result(_carrier_rejected("get_niagara_preview_player_state",error,report,raw))
        return reflected_result(domain._named_mcp("get_niagara_preview_player_state",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def preview_niagara_component_asset_swap(replacement_system_path:str,actor_name:str="\"\"",actor_label:str="\"\"",component_name:str="\"\"",system_path:str="\"\"",parameter_name_contains:str="\"\"",restore_original_asset:str="true",preserve_override_parameters:str="true",activate_after_swap:str="true",advance_time:str="0.35",advance_tick_delta:str="0.03333333333333333",max_parameters:str="256",max_emitter_instances:str="128",include_parameter_values:str="true",include_raw_bytes:str="false",max_raw_bytes:str="64",include_parameter_object_properties:str="false",include_emitter_runtime:str="true",max_object_properties:str="80",max_array_elements:str="16",max_property_value_chars:str="512")->CubelessPythonAsyncOperationResult:
        "Original preview_niagara_component_asset_swap contract. Native owns actual UI, warmup, restore and partial results; source-only registration is not runtime proof."
        rejected,report,raw=_guard_admission("preview_niagara_component_asset_swap")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={"replacement_system_path":_decode_value(replacement_system_path),"actor_name":_decode_value(actor_name),"actor_label":_decode_value(actor_label),"component_name":_decode_value(component_name),"system_path":_decode_value(system_path),"parameter_name_contains":_decode_value(parameter_name_contains),"restore_original_asset":_decode_value(restore_original_asset),"preserve_override_parameters":_decode_value(preserve_override_parameters),"activate_after_swap":_decode_value(activate_after_swap),"advance_time":_decode_value(advance_time),"advance_tick_delta":_decode_value(advance_tick_delta),"max_parameters":_decode_value(max_parameters),"max_emitter_instances":_decode_value(max_emitter_instances),"include_parameter_values":_decode_value(include_parameter_values),"include_raw_bytes":_decode_value(include_raw_bytes),"max_raw_bytes":_decode_value(max_raw_bytes),"include_parameter_object_properties":_decode_value(include_parameter_object_properties),"include_emitter_runtime":_decode_value(include_emitter_runtime),"max_object_properties":_decode_value(max_object_properties),"max_array_elements":_decode_value(max_array_elements),"max_property_value_chars":_decode_value(max_property_value_chars)}
        except Exception as error:return domain.completed_result(_carrier_rejected("preview_niagara_component_asset_swap",error,report,raw))
        return domain.begin("preview_niagara_component_asset_swap",arguments)

    @toolset_registry.tool_call
    @staticmethod
    def sample_niagara_system_in_preview_lab(system_path:str,output_dir:str="\"\"",label:str="\"\"",warmup_times:str="null",views:str="null",warmup_tick_delta:str="0.03333333333333333",cleanup_before:str="true",cleanup_after_each:str="true",cleanup_after_all:str="true",location:str="null",scale:str="null",auto_frame:str="true",camera_location:str="null",camera_rotation:str="null",camera_fov:str="null",user_parameter_overrides:str="null",preserve_override_parameters:str="true",allow_current_map_preview:str="false")->CubelessPythonAsyncOperationResult:
        "Original sample_niagara_system_in_preview_lab contract. Native owns actual UI, warmup, restore and partial results; source-only registration is not runtime proof."
        rejected,report,raw=_guard_admission("sample_niagara_system_in_preview_lab")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={"system_path":_decode_value(system_path),"output_dir":_decode_value(output_dir),"label":_decode_value(label),"warmup_times":_decode_value(warmup_times),"views":_decode_value(views),"warmup_tick_delta":_decode_value(warmup_tick_delta),"cleanup_before":_decode_value(cleanup_before),"cleanup_after_each":_decode_value(cleanup_after_each),"cleanup_after_all":_decode_value(cleanup_after_all),"location":_decode_value(location),"scale":_decode_value(scale),"auto_frame":_decode_value(auto_frame),"camera_location":_decode_value(camera_location),"camera_rotation":_decode_value(camera_rotation),"camera_fov":_decode_value(camera_fov),"user_parameter_overrides":_decode_value(user_parameter_overrides),"preserve_override_parameters":_decode_value(preserve_override_parameters),"allow_current_map_preview":_decode_value(allow_current_map_preview)}
        except Exception as error:return domain.completed_result(_carrier_rejected("sample_niagara_system_in_preview_lab",error,report,raw))
        return domain.begin("sample_niagara_system_in_preview_lab",arguments)

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraPreviewWorkflowAttachmentLibrary', None), 'verify_niagara_preview_workflows_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraPreviewWorkflow adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraPreviewWorkflowTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_preview_workflows_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraPreviewWorkflow adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_preview_workflows_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_preview_workflows_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_preview_workflows_registration_carrier_invalid',str(error)))
    result['stage']='niagara_preview_workflows_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
