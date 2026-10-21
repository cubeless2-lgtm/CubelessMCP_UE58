"""UE5.8 dedicated NiagaraScratchStackAuthoring Native1 x4. No transport or fallback."""
import copy
import json
import math
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraScratchStackAuthoringLibrary'
BINDINGS=["add_niagara_scratch_pad_module_input", "add_niagara_scratch_pad_assignment_module_to_stack", "wrap_niagara_scratch_pad_output_with_stack_context"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"add_niagara_scratch_pad_module_input": ["system_path", "allow_source_edit", "input_name", "module_input_name", "input_type", "parameter_type", "default_value", "value", "consumer_pin_name", "connect_to_parameter_name", "target_pin_name", "require_consumer_link", "overwrite_existing_link", "save", "request_compile", "scratch_pad_script_path", "scratch_pad_owner_kind", "scratch_pad_name", "scratch_pad_script_name", "scratch_pad_script_index", "scratch_pad_emitter_index", "scratch_pad_emitter_name", "wait_for_completion", "timeout_seconds", "poll_interval_seconds"], "add_niagara_scratch_pad_assignment_module_to_stack": ["allow_assignment_module_route", "target_system_path", "system_path", "allow_source_edit", "parameter_name", "parameter_type", "default_value", "value", "target_usage", "target_usage_id", "target_emitter_index", "target_emitter_name", "target_index", "save", "request_compile", "skip_if_duplicate"], "wrap_niagara_scratch_pad_output_with_stack_context": ["system_path", "allow_source_edit", "target_pin_name", "input_pin_name", "local_value_pin_name", "previous_stack_value_pin_name", "expression", "skip_if_already_wrapped", "replace_existing_custom", "save", "request_compile", "scratch_pad_script_path", "scratch_pad_owner_kind", "scratch_pad_name", "scratch_pad_script_name", "scratch_pad_script_index", "scratch_pad_emitter_index", "scratch_pad_emitter_name", "target_node_guid", "parameter_map_set_node_guid", "target_node_index", "parameter_map_set_node_index", "parameter_map_set_index"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_scratch_stack_authoring_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_scratch_stack_authoring_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_scratch_stack_authoring_native_api_unavailable','Build/load the exact NiagaraScratchStackAuthoring Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_scratch_stack_authoring_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def add_niagara_scratch_pad_module_input(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("add_niagara_scratch_pad_module_input",text)

def add_niagara_scratch_pad_assignment_module_to_stack(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("add_niagara_scratch_pad_assignment_module_to_stack",text)

def wrap_niagara_scratch_pad_output_with_stack_context(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("wrap_niagara_scratch_pad_output_with_stack_context",text)

ASYNC_METHODS=['add_niagara_scratch_pad_module_input','add_niagara_scratch_pad_assignment_module_to_stack']
MODULE_ASYNC_LIBRARY='CubelessNiagaraScratchStackAuthoringAsyncLibrary'
MODULE_JOB_CLASS='CubelessNiagaraScratchStackAuthoringJob'
STATUS_ASYNC_LIBRARY='CubelessNiagaraScratchUserRecipeAsyncLibrary'
STATUS_JOB_CLASS='CubelessNiagaraScratchUserRecipeJob'

def native_job_operation(method):
    if method==ASYNC_METHODS[0]:return method
    if method==ASYNC_METHODS[1]:return 'inspect_niagara_compile_status'
    raise OperationError('niagara_scratch_stack_authoring_job_method_invalid','Only the original ModuleInput/Assignment Named continuations are admitted')

def _job_binding(method):
    if method==ASYNC_METHODS[0]:return MODULE_ASYNC_LIBRARY,'start_add_niagara_scratch_pad_module_input_job',MODULE_JOB_CLASS
    if method==ASYNC_METHODS[1]:return STATUS_ASYNC_LIBRARY,'start_compile_status_job',STATUS_JOB_CLASS
    native_job_operation(method)

def async_binding_status():
    status={}
    for method in ASYNC_METHODS:
        library,start,job_class=_job_binding(method);job=getattr(unreal,job_class,None)
        status[method]=callable(getattr(getattr(unreal,library,None),start,None))
        status[method+'_job_available']=job is not None
        status[method+'_snapshot_callable']=callable(getattr(job,'get_snapshot_json',None))
        status[method+'_cancel_callable']=callable(getattr(job,'cancel_job',None))
    return status

def start_job(method,params):
    try:
        library,start,job_class=_job_binding(method);text=_carrier(params)
        function=getattr(getattr(unreal,library,None),start,None)
        if not callable(function):raise OperationError('niagara_scratch_stack_authoring_job_native_unavailable','Exact Native start is unavailable; no fallback or dispatch')
    except Exception as error:error.native_start_dispatched=False;raise
    job=function(text)
    try:
        cls=getattr(unreal,job_class,None)
        if cls is None or not isinstance(job,cls) or job.get_class().get_path_name()!='/Script/CubelessMCPUE58Editor.'+job_class or not callable(getattr(job,'get_snapshot_json',None)) or not callable(getattr(job,'cancel_job',None)):
            raise OperationError('niagara_scratch_stack_authoring_job_identity_invalid','Returned exact Job differs; dispatched start must not be repeated')
    except Exception as error:error.native_start_returned=True;error.native_returned_job=job;raise
    return job

def snapshot(job,method):
    raw=job.get_snapshot_json();value=None
    try:
        value=_decode_result(raw);data=value['data'];revision=data.get('native_checkpoint_revision')
        if (value.get('operation')!=native_job_operation(method) or type(data.get('native_job_id')) is not str or not data['native_job_id'] or type(data.get('native_job_terminal')) is not bool or type(data.get('operation_complete')) is not bool or data['operation_complete']!=data['native_job_terminal'] or type(revision) not in (int,float) or not math.isfinite(revision) or revision<0 or revision!=int(revision) or revision>9007199254740991):
            raise OperationError('niagara_scratch_stack_authoring_job_snapshot_invalid','Exact operation/job/revision/terminal Native receipt required')
        if not data['native_job_terminal'] and (value['success'] or value['postcondition_passed']):raise OperationError('niagara_scratch_stack_authoring_job_pending_health_invalid','Pending is not completed success or passed final postcondition')
        if data.get('unsafe_job_publication_quarantined') is True:
            error=OperationError('niagara_scratch_stack_authoring_job_publication_quarantined','Native stopped or quarantined before safe terminal publication; no further Job work')
            error.native_unsafe_publication_quarantined=True;raise error
        public_terminal=job.get_editor_property('is_terminal')
        if type(public_terminal) is not bool or public_terminal is not data['native_job_terminal']:raise OperationError('niagara_scratch_stack_authoring_job_publication_mismatch','Job terminal publication differs from returned snapshot; no replay')
    except Exception as error:
        error.native_snapshot_returned=True;error.native_raw_result=raw
        if value is not None:error.native_decoded_result=value
        raise
    value['_raw_native_json']=raw;return value
