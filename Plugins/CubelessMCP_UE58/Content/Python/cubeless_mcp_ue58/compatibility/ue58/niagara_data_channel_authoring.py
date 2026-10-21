"""UE5.8 dedicated NiagaraDataChannelAuthoring Native1 x3. No transport or fallback."""
import copy
import json
import math
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY="CubelessNiagaraDataChannelAuthoringLibrary"
BINDINGS=["configure_niagara_data_channel_asset", "create_niagara_ndc_reader_scratch_pad_module", "add_niagara_scratch_pad_di_function_call", "add_niagara_scratch_pad_di_variadic_input"]
PUBLIC_BINDINGS=["configure_niagara_data_channel_asset", "create_niagara_ndc_reader_scratch_pad_module", "add_niagara_scratch_pad_di_variadic_input"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"configure_niagara_data_channel_asset": ["asset_path", "create_if_missing", "keep_previous_frame_data", "save", "variables"], "create_niagara_ndc_reader_scratch_pad_module": ["system_path", "scratch_pad_name", "data_channel_asset_path", "stage", "allow_source_edit", "dry_run", "save", "request_compile", "read_current_frame", "position_variable", "velocity_variable", "spawn_count_variable", "event_type_variable", "position_target", "velocity_target", "spawn_count_target", "event_type_target", "read_success_target"], "add_niagara_scratch_pad_di_function_call": ["system_path", "scratch_pad_name", "function_name", "di_input_name", "di_type", "allow_source_edit", "save", "request_compile", "inputs", "capture_success"], "add_niagara_scratch_pad_di_variadic_input": ["system_path", "scratch_pad_name", "scratch_pad_script_path", "function_node_guid", "expected_function_name", "expected_di_class", "pin_name", "module_input_name", "input_type", "default_value", "expected_referencing_module_node_guid", "expected_input_map_node_guid", "expected_output_node_guid", "allow_source_edit", "save", "request_compile", "expected_existing_inputs", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_data_channel_authoring_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_data_channel_authoring_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_data_channel_authoring_native_api_unavailable','Build/load the exact NiagaraDataChannelAuthoring Native API; no fallback')
        error.native_call_dispatched=False;raise error
    if method == "add_niagara_scratch_pad_di_variadic_input":
        try:raw=fn(*args)
        finally:fn=None
    else:raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_data_channel_authoring_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def configure_niagara_data_channel_asset(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("configure_niagara_data_channel_asset",text)

def create_niagara_ndc_reader_scratch_pad_module(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("create_niagara_ndc_reader_scratch_pad_module",text)

def add_niagara_scratch_pad_di_function_call(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("add_niagara_scratch_pad_di_function_call",text)

def add_niagara_scratch_pad_di_variadic_input(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("add_niagara_scratch_pad_di_variadic_input",text)


# MT uses the existing reflected async-result lifecycle; this adapter owns no ticker.
VARIADIC_METHOD="add_niagara_scratch_pad_di_variadic_input"
VARIADIC_JOB_CLASS_PATH="/Script/CubelessMCPUE58Editor.CubelessNiagaraDataChannelVariadicJob"
VARIADIC_JOB_BINDINGS=("start_niagara_scratch_pad_di_variadic_input_job",
    "get_niagara_scratch_pad_di_variadic_input_job_snapshot",
    "cancel_niagara_scratch_pad_di_variadic_input_job")

def async_binding_status():
    library=getattr(unreal,LIBRARY,None)
    return dict({name:callable(getattr(library,name,None)) for name in VARIADIC_JOB_BINDINGS},
        job_class_available=getattr(unreal,"CubelessNiagaraDataChannelVariadicJob",None) is not None)

def start_variadic_job(params):
    try:
        text=_carrier(params)
        fn=getattr(getattr(unreal,LIBRARY,None),VARIADIC_JOB_BINDINGS[0],None)
        if not callable(fn):raise OperationError('niagara_data_channel_variadic_job_unavailable','Exact Native start is unavailable; no start or fallback dispatched')
    except Exception as error:error.native_start_dispatched=False;raise
    try:job=fn(text)
    finally:fn=None
    try:
        cls=getattr(unreal,"CubelessNiagaraDataChannelVariadicJob",None)
        if cls is None or not isinstance(job,cls) or job.get_class().get_path_name()!=VARIADIC_JOB_CLASS_PATH:
            raise OperationError('niagara_data_channel_variadic_job_identity_invalid','Returned transient job differs; start must not be repeated')
    except Exception as error:error.native_start_returned=True;error.native_returned_job=job;raise
    return job

def snapshot_variadic_job(job):
    fn=getattr(getattr(unreal,LIBRARY,None),VARIADIC_JOB_BINDINGS[1],None)
    if not callable(fn):raise OperationError('niagara_data_channel_variadic_snapshot_unavailable','Exact cached Native snapshot is unavailable; no fallback')
    try:raw=fn(job)
    finally:fn=None
    try:
        value=_decode_result(raw);data=value['data'];revision=data.get('native_checkpoint_revision')
        if (value.get('operation')!=VARIADIC_METHOD or type(data.get('native_job_id')) is not str or not data['native_job_id']
                or data.get('native_job_class')!=VARIADIC_JOB_CLASS_PATH or type(data.get('native_job_path')) is not str or not data['native_job_path']
                or type(data.get('native_job_terminal')) is not bool or type(data.get('operation_complete')) is not bool
                or data['operation_complete'] is not data['native_job_terminal']
                or type(revision) not in (int,float) or not math.isfinite(revision) or revision<0
                or revision!=int(revision) or revision>9007199254740991
                or not data['native_job_terminal'] and (value['success'] or value['postcondition_passed'])):
            raise OperationError('niagara_data_channel_variadic_snapshot_invalid','Exact operation/job/revision/terminal receipt required; pending is not success')
    except Exception as error:error.native_snapshot_returned=True;error.native_raw_result=raw;raise
    value['_raw_native_json']=raw;return value

def cancel_variadic_job(job,reason):
    fn=getattr(getattr(unreal,LIBRARY,None),VARIADIC_JOB_BINDINGS[2],None)
    if not callable(fn):raise OperationError('niagara_data_channel_variadic_cancel_unavailable','Exact Native cancellation is unavailable; compiler cessation is not established')
    try:returned=fn(job,reason)
    finally:fn=None
    if type(returned) is not bool:raise OperationError('niagara_data_channel_variadic_cancel_invalid','Cancellation requires an actual Boolean receipt')
    return returned
