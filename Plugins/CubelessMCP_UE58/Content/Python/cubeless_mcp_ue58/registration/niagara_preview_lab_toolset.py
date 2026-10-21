"""GW original Preview Lab4 through bounded public value carriers; actual prefix, warmup limits and file evidence are Native owned."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_preview_lab as domain

@unreal.uclass()
class CubelessNiagaraPreviewLabTools(unreal.ToolsetDefinition):
    "Original Preview Lab state/cleanup/capture/still-preview contracts; Native owns actual partial, GPU/warmup limits and file evidence."
    @toolset_registry.tool_call
    @staticmethod
    def get_niagara_preview_lab_state()->CubelessPythonOperationResult:
        "Get the current Niagara Preview Lab map, dirty, safety, and preview actor state."
        rejected,report,raw=_guard_admission("get_niagara_preview_lab_state")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={}
        except Exception as error:return reflected_result(_carrier_rejected("get_niagara_preview_lab_state",error,report,raw))
        result=domain._named_mcp("get_niagara_preview_lab_state",arguments)
        return reflected_result(result)

    @toolset_registry.tool_call
    @staticmethod
    def cleanup_niagara_preview_lab()->CubelessPythonOperationResult:
        "Delete Niagara Preview Lab preview actors without saving or reloading the map."
        rejected,report,raw=_guard_admission("cleanup_niagara_preview_lab")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={}
        except Exception as error:return reflected_result(_carrier_rejected("cleanup_niagara_preview_lab",error,report,raw))
        result=domain._named_mcp("cleanup_niagara_preview_lab",arguments)
        return reflected_result(result)

    @toolset_registry.tool_call
    @staticmethod
    def capture_niagara_preview_lab_view(filepath:str,view:str="1",auto_frame:str="true",camera_location:str="null",camera_rotation:str="null",camera_fov:str="null",allow_current_map_preview:str="false")->CubelessPythonOperationResult:
        "Capture a clean Niagara Preview Lab screenshot.\n\n        When Preview Lab actors exist, Unreal auto-frames those temporary actors.\n        The view number is a near/mid/far distance hint and a fallback when no\n        preview actor exists.\n\n        Args:\n            filepath: PNG output path. Relative paths are resolved under Saved/MCP/NiagaraReviews.\n            view: Preview Lab view number. Use 1 first; use 2 or 3 only when the auto-framed effect is too large, clipped, or not reviewable.\n            auto_frame: Auto-frame temporary preview actors when present. Set false for manual camera captures.\n            camera_location: Optional [x, y, z] capture camera location override.\n            camera_rotation: Optional [pitch, yaw, roll] capture camera rotation override.\n            camera_fov: Optional capture camera FOV override.\n            allow_current_map_preview: Allow preview capture in the current editor map when the dedicated Preview Lab map is unavailable.\n        "
        rejected,report,raw=_guard_admission("capture_niagara_preview_lab_view")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"filepath":_decode_value(filepath),"view":_decode_value(view),"auto_frame":_decode_value(auto_frame),"camera_location":_decode_value(camera_location),"camera_rotation":_decode_value(camera_rotation),"camera_fov":_decode_value(camera_fov),"allow_current_map_preview":_decode_value(allow_current_map_preview)}
        except Exception as error:return reflected_result(_carrier_rejected("capture_niagara_preview_lab_view",error,report,raw))
        result=domain._named_mcp("capture_niagara_preview_lab_view",arguments)
        return reflected_result(result)

    @toolset_registry.tool_call
    @staticmethod
    def preview_niagara_system_in_preview_lab(system_path:str,filepath:str,view:str="1",label:str="\"\"",warmup_time:str="0.35",warmup_tick_delta:str="0.03333333333333333",cleanup_before:str="true",cleanup_after:str="true",location:str="null",scale:str="null",auto_frame:str="true",camera_location:str="null",camera_rotation:str="null",camera_fov:str="null",user_parameter_overrides:str="null",preserve_override_parameters:str="true",allow_current_map_preview:str="false")->CubelessPythonAsyncOperationResult:
        "Run a one-call Niagara Preview Lab still preview. Named positive warmup uses the existing owned async PreviewWorkflow job; zero warmup retains the sync path. Final standard11 preserves actual capture/cleanup and unknown prefixes. Ordinary Batch positive warmup continuation remains unsupported.\n\n        This optimized route loads a read-only Niagara system, deletes prior\n        Preview Lab actors if requested, spawns one transient preview actor,\n        applies optional component-only User parameter overrides, advances\n        simulation for warmup, captures with auto framing, and optionally\n        removes the actor afterward.\n\n        Args:\n            system_path: Niagara system object path or package path.\n            filepath: PNG output path. Relative paths are resolved under Saved/MCP/NiagaraReviews.\n            view: Near/mid/far distance hint, 1 to 3.\n            label: Optional preview actor label suffix.\n            warmup_time: Seconds to advance simulation before capture.\n            warmup_tick_delta: Simulation tick size for warmup.\n            cleanup_before: Delete existing Preview Lab actors before spawning.\n            cleanup_after: Delete Preview Lab actors after capture.\n            location: Optional [x, y, z] spawn location. Defaults to [0, 0, 120].\n            scale: Optional [x, y, z] actor scale. Defaults to [1, 1, 1].\n            auto_frame: Auto-frame temporary preview actors when present. Set false for manual camera captures.\n            camera_location: Optional [x, y, z] capture camera location override.\n            camera_rotation: Optional [pitch, yaw, roll] capture camera rotation override.\n            camera_fov: Optional capture camera FOV override.\n            user_parameter_overrides: Optional transient component overrides, e.g. {\"Amount\": 90000.0, \"Custom Camera Position\": [0, -3200, 650]}.\n            preserve_override_parameters: Preserve existing component override parameters when assigning the system. Keep true for Blueprint/component swap parity.\n            allow_current_map_preview: Allow preview capture in the current editor map when the dedicated Preview Lab map is unavailable.\n        "
        rejected,report,raw=_guard_admission("preview_niagara_system_in_preview_lab")
        if rejected is not None:return domain.completed_result(rejected)
        try:arguments={"system_path":_decode_value(system_path),"filepath":_decode_value(filepath),"view":_decode_value(view),"label":_decode_value(label),"warmup_time":_decode_value(warmup_time),"warmup_tick_delta":_decode_value(warmup_tick_delta),"cleanup_before":_decode_value(cleanup_before),"cleanup_after":_decode_value(cleanup_after),"location":_decode_value(location),"scale":_decode_value(scale),"auto_frame":_decode_value(auto_frame),"camera_location":_decode_value(camera_location),"camera_rotation":_decode_value(camera_rotation),"camera_fov":_decode_value(camera_fov),"user_parameter_overrides":_decode_value(user_parameter_overrides),"preserve_override_parameters":_decode_value(preserve_override_parameters),"allow_current_map_preview":_decode_value(allow_current_map_preview)}
        except Exception as error:return domain.completed_result(_carrier_rejected("preview_niagara_system_in_preview_lab",error,report,raw))
        return domain._named_mcp("preview_niagara_system_in_preview_lab",arguments)

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraPreviewLabAttachmentLibrary', None), 'verify_niagara_preview_lab_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraPreviewLab adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraPreviewLabTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_preview_lab_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraPreviewLab adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_preview_lab_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_preview_lab_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_preview_lab_registration_carrier_invalid',str(error)))
    result['stage']='niagara_preview_lab_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
