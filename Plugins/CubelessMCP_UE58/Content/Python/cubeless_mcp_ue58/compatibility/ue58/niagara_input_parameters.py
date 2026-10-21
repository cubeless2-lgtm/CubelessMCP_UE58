"""UE5.8 dedicated NiagaraInputParameter Native1 x7. No transport or fallback."""
import copy
import json
import math
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraInputParameterLibrary'
BINDINGS=["inspect_niagara_rapid_iteration_parameters","set_niagara_rapid_iteration_parameter","set_niagara_module_static_switch","inspect_niagara_module_inputs","inspect_niagara_data_interface_overrides","set_niagara_module_input_value","create_niagara_module_input_override"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"inspect_niagara_rapid_iteration_parameters":["system_path","contains","max_parameters_per_script"],"set_niagara_rapid_iteration_parameter":["system_path","parameter_name","value","owner_kind","owner_name","script_usage","emitter_index","allow_source_edit","save","request_compile"],"set_niagara_module_static_switch":["system_path","input_name","value","emitter_index","emitter_name","module_index","module_name","module_node_guid","expected_current_value","allow_source_edit","save","request_compile","wait_for_completion","timeout_seconds","poll_interval_seconds","require_compile_gate_for_save","target_scope","scope"],"inspect_niagara_module_inputs":["system_path","include_linked_sources","include_resolved_stack_inputs","max_modules","max_candidates_per_module","max_resolved_inputs_per_module","max_top_candidates"],"inspect_niagara_data_interface_overrides":["system_path","input_name","emitter_index","emitter_name","module_index","module_name","module_node_guid","max_modules","max_inputs_per_module","include_data_interface_properties","max_data_interface_properties","max_data_interface_property_value_length"],"set_niagara_module_input_value":["system_path","input_name","value","emitter_index","emitter_name","module_index","module_name","module_node_guid","allow_source_edit","save","request_compile","wait_for_completion","timeout_seconds","poll_interval_seconds"],"create_niagara_module_input_override":["system_path","input_name","value","emitter_index","emitter_name","module_index","module_name","module_node_guid","overwrite_existing","allow_source_edit","save","request_compile","wait_for_completion","timeout_seconds","poll_interval_seconds"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_input_parameters_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_input_parameters_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_input_parameters_native_api_unavailable','Build/load the exact NiagaraInputParameter Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_input_parameters_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def inspect_niagara_rapid_iteration_parameters(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_rapid_iteration_parameters",text)

def set_niagara_rapid_iteration_parameter(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_rapid_iteration_parameter",text)

def set_niagara_module_static_switch(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_module_static_switch",text)

def inspect_niagara_module_inputs(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_module_inputs",text)

def inspect_niagara_data_interface_overrides(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_data_interface_overrides",text)

def set_niagara_module_input_value(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_module_input_value",text)

def create_niagara_module_input_override(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("create_niagara_module_input_override",text)


ASYNC_METHODS=["set_niagara_module_static_switch","set_niagara_module_input_value","create_niagara_module_input_override"]
START_ASYNC_METHODS={"set_niagara_module_static_switch":"start_set_niagara_module_static_switch_job","set_niagara_module_input_value":"start_set_niagara_module_input_value_job","create_niagara_module_input_override":"start_create_niagara_module_input_override_job"}
ASYNC_LIBRARY="CubelessNiagaraInputParameterAsyncLibrary"
JOB_CLASS_PATH="/Script/CubelessMCPUE58Editor.CubelessNiagaraInputParameterJob"

def async_binding_status():
    library=getattr(unreal,ASYNC_LIBRARY,None);job=getattr(unreal,"CubelessNiagaraInputParameterJob",None)
    starts={method:callable(getattr(library,name,None)) for method,name in START_ASYNC_METHODS.items()}
    return dict(starts,job_class_available=job is not None,job_snapshot_callable=callable(getattr(job,"get_snapshot_json",None)),job_cancel_callable=callable(getattr(job,"cancel_job",None)))

def start_job(method,params):
    try:
        if method not in ASYNC_METHODS:raise OperationError('niagara_input_parameters_job_method_invalid','Only the original input writer3 Native starts are admitted')
        text=_carrier(params);function=getattr(getattr(unreal,ASYNC_LIBRARY,None),START_ASYNC_METHODS[method],None)
        if not callable(function):raise OperationError('niagara_input_parameters_job_native_unavailable','Dedicated Native start is unavailable; no fallback or start dispatched')
    except Exception as error:
        error.native_start_dispatched=False;raise
    job=function(text)
    try:
        cls=getattr(unreal,'CubelessNiagaraInputParameterJob',None)
        if cls is None or not isinstance(job,cls) or job.get_class().get_path_name()!=JOB_CLASS_PATH or not callable(getattr(job,'get_snapshot_json',None)) or not callable(getattr(job,'cancel_job',None)):
            raise OperationError('niagara_input_parameters_job_identity_invalid','Returned job differs; dispatched start must not be repeated')
    except Exception as error:error.native_start_returned=True;error.native_returned_job=job;raise
    return job


def snapshot(job,method):
    raw=job.get_snapshot_json();value=None
    try:
        value=_decode_result(raw);data=value['data'];revision=data.get('native_checkpoint_revision')
        if (method not in ASYNC_METHODS or value.get('operation')!=method or type(data.get('native_job_id')) is not str or not data['native_job_id'] or type(data.get('native_job_terminal')) is not bool or type(data.get('operation_complete')) is not bool or data['operation_complete']!=data['native_job_terminal'] or type(revision) not in (int,float) or not math.isfinite(revision) or revision<0 or revision!=int(revision) or revision>9007199254740991):
            raise OperationError('niagara_input_parameters_job_snapshot_invalid','Exact operation/job/revision/terminal Native receipt required')
        if not data['native_job_terminal'] and (value['success'] or value['postcondition_passed']):
            raise OperationError('niagara_input_parameters_job_pending_health_invalid','Pending is not completed success or a passed final postcondition')
        if data.get('unsafe_job_publication_quarantined') is True:
            error=OperationError('niagara_input_parameters_job_publication_quarantined','Native stopped or quarantined before safe public terminal publication; no further Job work')
            error.native_unsafe_publication_quarantined=True;raise error
        public_terminal=job.get_editor_property('is_terminal')
        if type(public_terminal) is not bool or public_terminal is not data['native_job_terminal']:
            raise OperationError('niagara_input_parameters_job_publication_mismatch','Job terminal publication differs from returned snapshot; no replay')
    except Exception as error:
        error.native_snapshot_returned=True;error.native_raw_result=raw
        if value is not None:error.native_decoded_result=value
        raise
    value['_raw_native_json']=raw;return value
