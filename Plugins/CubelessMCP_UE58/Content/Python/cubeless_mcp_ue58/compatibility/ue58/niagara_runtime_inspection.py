"""UE5.8 dedicated NiagaraRuntimeInspection Native1 x4. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraRuntimeInspectionLibrary'
BINDINGS=["inspect_niagara_component_runtime_state", "inspect_niagara_component_parameter_store", "inspect_niagara_component_grid2d_runtime", "sample_render_target2d_region_stats"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"inspect_niagara_component_runtime_state": ["actor_name", "actor_label", "component_name", "system_path", "parameter_name_contains", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_asset_exposed_parameters", "include_instance_parameters", "include_parameter_object_properties", "include_emitter_runtime", "max_emitter_instances", "max_object_properties", "max_array_elements", "max_property_value_chars"], "inspect_niagara_component_parameter_store": ["actor_name", "actor_label", "component_name", "system_path", "parameter_name_contains", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_asset_exposed_parameters", "include_instance_parameters", "include_parameter_object_properties", "include_emitter_runtime", "max_emitter_instances", "max_object_properties", "max_array_elements", "max_property_value_chars"], "inspect_niagara_component_grid2d_runtime": ["actor_name", "actor_label", "component_name", "system_path", "max_components", "max_grid_interfaces", "include_internal"], "sample_render_target2d_region_stats": ["render_target_path", "x0", "x1", "y0", "y1", "channel", "threshold", "max_first_hits", "probe_x", "probe_y", "probe_radius"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_runtime_inspection_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_runtime_inspection_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_runtime_inspection_native_api_unavailable','Build/load the exact NiagaraRuntimeInspection Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_runtime_inspection_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def inspect_niagara_component_runtime_state(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_component_runtime_state",text)

def inspect_niagara_component_parameter_store(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_component_parameter_store",text)

def inspect_niagara_component_grid2d_runtime(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_component_grid2d_runtime",text)

def sample_render_target2d_region_stats(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("sample_render_target2d_region_stats",text)

