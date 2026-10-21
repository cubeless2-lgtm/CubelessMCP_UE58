"""Original logical Niagara runtime inspection4 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import niagara_runtime_inspection as domain

@unreal.uclass()
class CubelessNiagaraRuntimeInspectionTools(unreal.ToolsetDefinition):
    'Observe Niagara component/stores, already-paused Solo or shared non-Solo InstanceParameters and actual system/emitter counters; Grid2D GT snapshots remain already-paused Solo-only, plus render-target readback. Running Solo/non-Solo internals and stateless-specific counters remain unavailable. GPU particle counts may be latent or estimated; no current GPU-frame completion is claimed. Native availability/partial receipts are authoritative; no observer Pause, ForceSolo, tick, wait, finalize, compile, save or replay.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_component_runtime_state(actor_name:str="\"\"",actor_label:str="\"\"",component_name:str="\"\"",system_path:str="\"\"",parameter_name_contains:str="\"\"",max_components:str="32",max_parameters:str="256",include_parameter_values:str="true",include_raw_bytes:str="false",max_raw_bytes:str="64",include_asset_exposed_parameters:str="false",include_instance_parameters:str="false",include_parameter_object_properties:str="false",include_emitter_runtime:str="true",max_emitter_instances:str="128",max_object_properties:str="80",max_array_elements:str="16",max_property_value_chars:str="512")->CubelessPythonOperationResult:
        "Inspect live Niagara state in PIE/SIE when active, otherwise the editor world.\n\n        This is a read-only diagnostic route for comparing Blueprint-owned\n        Niagara components after asset swaps. It serializes component metadata\n        plus the component override parameter store, and optionally the live\n        system-instance parameter store only when its existing controller owns an\n        already-paused Solo or shared non-Solo instance. Running Solo/non-Solo internals and stateless-specific counters remain unavailable; already-paused Solo or shared non-Solo exposes actual system/emitter counters. Shared access requires current Native controller/ID, paused phase, no pending finalize and simulation/System/world/component ownership; it is not general concurrent raw access. Grid2D remains Solo-only. SDK GPU particle counts may be latent or estimated; no current GPU-frame completion is claimed. It copies actual store bytes before value\n        projection; no atomic cross-store/frame or GPU completion is claimed.\n        The observer never creates, Pauses, forces Solo, ticks, waits, finalizes,\n        loads, saves, reloads, compiles, or persists content.\n\n        Args:\n            actor_name: Optional substring filter against actor name, label, or path.\n            actor_label: Optional substring filter against actor label.\n            component_name: Optional substring filter against component name or path.\n            system_path: Optional substring filter against the assigned Niagara system path.\n            parameter_name_contains: Optional substring filter for parameter names.\n            max_components: Maximum matching components to return.\n            max_parameters: Maximum parameters to return per store.\n            include_parameter_values: Include decoded primitive/vector/color parameter values.\n            include_raw_bytes: Include raw byte previews for unsupported parameter types.\n            max_raw_bytes: Maximum bytes per unsupported parameter raw preview.\n            include_asset_exposed_parameters: Also include assigned system exposed user parameter store.\n            include_instance_parameters: Also include live FNiagaraSystemInstance parameter store when available.\n            include_parameter_object_properties: Include limited UPROPERTY previews for UObject/DataInterface parameter values.\n            include_emitter_runtime: Include live FNiagaraSystemInstance/FNiagaraEmitterInstance counters.\n            max_emitter_instances: Maximum emitter runtime entries to return per component.\n            max_object_properties: Maximum UObject/DataInterface properties to return per object.\n            max_array_elements: Maximum array elements to sample per UObject/DataInterface property.\n            max_property_value_chars: Maximum export-text characters per property value.\n         Native receipt preserves actual availability, partial observations and SDK unknown; no synthetic payload or automatic retry."
        rejected,report,raw=_guard_admission("inspect_niagara_component_runtime_state")
        if rejected is not None:return reflected_result(rejected)
        try:values={"actor_name":_decode_value(actor_name),"actor_label":_decode_value(actor_label),"component_name":_decode_value(component_name),"system_path":_decode_value(system_path),"parameter_name_contains":_decode_value(parameter_name_contains),"max_components":_decode_value(max_components),"max_parameters":_decode_value(max_parameters),"include_parameter_values":_decode_value(include_parameter_values),"include_raw_bytes":_decode_value(include_raw_bytes),"max_raw_bytes":_decode_value(max_raw_bytes),"include_asset_exposed_parameters":_decode_value(include_asset_exposed_parameters),"include_instance_parameters":_decode_value(include_instance_parameters),"include_parameter_object_properties":_decode_value(include_parameter_object_properties),"include_emitter_runtime":_decode_value(include_emitter_runtime),"max_emitter_instances":_decode_value(max_emitter_instances),"max_object_properties":_decode_value(max_object_properties),"max_array_elements":_decode_value(max_array_elements),"max_property_value_chars":_decode_value(max_property_value_chars)}
        except Exception as error:return reflected_result(_carrier_rejected("inspect_niagara_component_runtime_state",error,report,raw))
        return reflected_result(domain._named_mcp("inspect_niagara_component_runtime_state",values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_component_parameter_store(actor_name:str="\"\"",actor_label:str="\"\"",component_name:str="\"\"",system_path:str="\"\"",parameter_name_contains:str="\"\"",max_components:str="8",max_parameters:str="64",include_raw_bytes:str="false",max_raw_bytes:str="64")->CubelessPythonOperationResult:
        "Compare live Niagara override and instance parameter stores.\n\n        This is a narrow wrapper around inspect_niagara_component_runtime_state\n        for runtime User-parameter propagation probes. It returns component\n        OverrideParameters and FNiagaraSystemInstance InstanceParameters with\n        decoded primitive/vector/color values only from an already-paused Solo or shared non-Solo\n        instance whose held controller/instance/simulation/System/world roles\n        remain current. Running Solo/non-Solo and absent instances are explicitly\n        unavailable/partial. The observer never Pauses, forces Solo, ticks, waits\n        or finalizes. Original forced include flags, filters and limits remain.\n\n        Args:\n            actor_name: Optional substring filter against actor name, label, or path.\n            actor_label: Optional substring filter against actor label.\n            component_name: Optional substring filter against component name or path.\n            system_path: Optional substring filter against the assigned Niagara system path.\n            parameter_name_contains: Optional substring filter for parameter names, e.g. V4_SeedEnabled.\n            max_components: Maximum matching components to return.\n            max_parameters: Maximum parameters to return per store.\n            include_raw_bytes: Include raw byte previews for unsupported value types.\n            max_raw_bytes: Maximum bytes per unsupported parameter raw preview.\n         Native receipt preserves actual availability, partial observations and SDK unknown; no synthetic payload or automatic retry."
        rejected,report,raw=_guard_admission("inspect_niagara_component_parameter_store")
        if rejected is not None:return reflected_result(rejected)
        try:values={"actor_name":_decode_value(actor_name),"actor_label":_decode_value(actor_label),"component_name":_decode_value(component_name),"system_path":_decode_value(system_path),"parameter_name_contains":_decode_value(parameter_name_contains),"max_components":_decode_value(max_components),"max_parameters":_decode_value(max_parameters),"include_raw_bytes":_decode_value(include_raw_bytes),"max_raw_bytes":_decode_value(max_raw_bytes)}
        except Exception as error:return reflected_result(_carrier_rejected("inspect_niagara_component_parameter_store",error,report,raw))
        return reflected_result(domain._named_mcp("inspect_niagara_component_parameter_store",values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_component_grid2d_runtime(actor_name:str="\"\"",actor_label:str="\"\"",component_name:str="\"\"",system_path:str="\"\"",max_components:str="16",max_grid_interfaces:str="16",include_internal:str="true")->CubelessPythonOperationResult:
        "Read live Niagara Grid2D runtime NumCells from matching components.\n\n        This is a read-only runtime probe for Grid2D smoke tests. It scans live\n        Niagara components, finds Grid2D Collection data interfaces, and returns\n        both static asset fields and game-thread runtime instance data when the\n        existing instance is already-paused Solo. The original Internal namespace\n        filter and ordered usage multiplicity are preserved. Exact native\n        Grid2DCollection GT scalar fields are copied after typed lookup and the\n        slot is reacquired after the return fence. No GPU pixels/frame freshness\n        are claimed. Running Solo/non-Solo and derived collection layouts remain\n        unavailable/partial; override metadata fallback is not a live snapshot.\n        No observer Pause, ForceSolo, Activate, tick, wait, finalize, compile or save.\n         Native receipt preserves actual availability, partial observations and SDK unknown; no synthetic payload or automatic retry."
        rejected,report,raw=_guard_admission("inspect_niagara_component_grid2d_runtime")
        if rejected is not None:return reflected_result(rejected)
        try:values={"actor_name":_decode_value(actor_name),"actor_label":_decode_value(actor_label),"component_name":_decode_value(component_name),"system_path":_decode_value(system_path),"max_components":_decode_value(max_components),"max_grid_interfaces":_decode_value(max_grid_interfaces),"include_internal":_decode_value(include_internal)}
        except Exception as error:return reflected_result(_carrier_rejected("inspect_niagara_component_grid2d_runtime",error,report,raw))
        return reflected_result(domain._named_mcp("inspect_niagara_component_grid2d_runtime",values))

    @toolset_registry.tool_call
    @staticmethod
    def sample_render_target2d_region_stats(render_target_path:str,x0:str="0",x1:str="-1",y0:str="0",y1:str="-1",channel:str="\"max\"",threshold:str="0.01",max_first_hits:str="8",probe_x:str="-1",probe_y:str="-1",probe_radius:str="8")->CubelessPythonOperationResult:
        "Read a TextureRenderTarget2D once and return region/channel statistics.\n\n        This is an authoring-only readback probe for render-target smoke tests.\n        It computes count, sum, max, threshold-hit bounds, centroid, optional\n        same-read probe stats, and first-hit samples in C++ so tests do not\n        need slow per-pixel Python loops.\n\n        Args:\n            render_target_path: RT package/object path.\n            x0, x1, y0, y1: Inclusive pixel region. Negative x1/y1 uses the RT maximum bound.\n            channel: r, g, b, a, luma, or max. Default max checks any channel.\n            threshold: Strictly-greater-than threshold for nonzero classification.\n            max_first_hits: Maximum hit samples returned for debugging.\n            probe_x, probe_y: Optional probe center. Set both to >= 0 to return same-read probe stats.\n            probe_radius: Inclusive square probe radius in pixels.\n         Native receipt preserves actual availability, partial observations and SDK unknown; no synthetic payload or automatic retry."
        rejected,report,raw=_guard_admission("sample_render_target2d_region_stats")
        if rejected is not None:return reflected_result(rejected)
        try:values={"render_target_path":_decode_value(render_target_path),"x0":_decode_value(x0),"x1":_decode_value(x1),"y0":_decode_value(y0),"y1":_decode_value(y1),"channel":_decode_value(channel),"threshold":_decode_value(threshold),"max_first_hits":_decode_value(max_first_hits),"probe_x":_decode_value(probe_x),"probe_y":_decode_value(probe_y),"probe_radius":_decode_value(probe_radius)}
        except Exception as error:return reflected_result(_carrier_rejected("sample_render_target2d_region_stats",error,report,raw))
        return reflected_result(domain._named_mcp("sample_render_target2d_region_stats",values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraRuntimeInspectionAttachmentLibrary', None), 'verify_niagara_runtime_inspection_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraRuntimeInspection adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraRuntimeInspectionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_runtime_inspection_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraRuntimeInspection adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_runtime_inspection_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_runtime_inspection_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_runtime_inspection_registration_carrier_invalid',str(error)))
    result['stage']='niagara_runtime_inspection_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
