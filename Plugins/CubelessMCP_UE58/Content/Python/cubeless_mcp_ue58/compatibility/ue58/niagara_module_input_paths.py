"""UE5.8 dedicated NiagaraModuleInputPath Native1 x3. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraModuleInputPathLibrary'
BINDINGS=["remove_niagara_unused_module_input_path", "set_niagara_module_input_user_parameter", "set_niagara_array_data_interface_module_input_user_parameter"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"remove_niagara_unused_module_input_path": ["system_path", "scratch_pad_name", "module_name", "module_node_guid", "input_name", "linked_user_parameter_name", "custom_hlsl_signature_name", "expected_hlsl_identifier_absent", "custom_hlsl_input_name", "scratch_pad_script_path", "expected_hlsl_body_hash", "expected_custom_hlsl_node_guid", "expected_override_node_guid", "expected_input_type", "expected_user_source_node_guid", "expected_scratch_map_get_node_guids", "dry_run", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"], "set_niagara_module_input_user_parameter": ["system_path", "input_name", "user_parameter_name", "default_value", "emitter_index", "emitter_name", "module_index", "module_name", "module_node_guid", "overwrite_existing", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds", "require_array_data_interface"], "set_niagara_array_data_interface_module_input_user_parameter": ["system_path", "input_name", "user_parameter_name", "emitter_index", "emitter_name", "module_index", "module_name", "module_node_guid", "overwrite_existing", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds", "default_value", "require_array_data_interface"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_module_input_paths_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_module_input_paths_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_module_input_paths_native_api_unavailable','Build/load the exact NiagaraModuleInputPath Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_module_input_paths_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def remove_niagara_unused_module_input_path(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("remove_niagara_unused_module_input_path",text)

def set_niagara_module_input_user_parameter(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_module_input_user_parameter",text)

def set_niagara_array_data_interface_module_input_user_parameter(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_array_data_interface_module_input_user_parameter",text)



def async_binding_status():
    from . import niagara_scratch_user_recipes as shared
    status=shared.binding_status()
    return {name:status.get(name) is True for name in ('remove_niagara_unused_module_input_path','set_niagara_module_input_user_parameter','set_niagara_array_data_interface_module_input_user_parameter')}
