"""Exact UE58 RapidIteration batch11 binding; no load, wait or fallback."""
import json
import math
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessNiagaraModuleLibrary"
BINDINGS = ("set_niagara_module_inputs_batch",)


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def set_niagara_module_inputs_batch(system_path, edits_json, operation, overwrite_existing,
        continue_on_error, allow_source_edit, save, request_compile, wait_for_completion,
        timeout_seconds, poll_interval_seconds):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_niagara_module_batch_api_unavailable",
            "Exact native Niagara module batch11 binding unavailable; no fallback/load/writer dispatched")
    raw = function(system_path, edits_json, operation, overwrite_existing, continue_on_error,
        allow_source_edit, save, request_compile, wait_for_completion, timeout_seconds, poll_interval_seconds)
    try:
        native = _decode_result(raw)
        if (native.get("operation") != BINDINGS[0] or "changed" not in native
                or native["changed"] is not None and type(native["changed"]) is not bool):
            raise OperationError("invalid_native_niagara_module_batch_result",
                "Wrong returned operation/changed type; full raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


# Only the outer normalized11 carrier gets escape headroom; edits_json retains its old1Mi budget.
MAX_ASYNC_CARRIER_UTF16_UNITS=2*1048576+16384
def _carrier(params):
    try:
        text=json.dumps(params,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_module_batch_job_carrier_invalid','Finite valid Unicode normalized11 required') from error
    if units>MAX_ASYNC_CARRIER_UTF16_UNITS or '\x00' in text:
        raise OperationError('niagara_module_batch_job_carrier_bound','Outer normalized carrier exceeds2Mi+16Ki UTF16')
    return text

ASYNC_METHODS=[BINDINGS[0]]
START_ASYNC_METHODS={BINDINGS[0]:"start_set_niagara_module_inputs_batch_job"}
ASYNC_LIBRARY="CubelessNiagaraModuleInputsBatchAsyncLibrary"
JOB_CLASS_PATH="/Script/CubelessMCPUE58Editor.CubelessNiagaraModuleInputsBatchJob"

def async_binding_status():
    library=getattr(unreal,ASYNC_LIBRARY,None);job=getattr(unreal,"CubelessNiagaraModuleInputsBatchJob",None)
    starts={method:callable(getattr(library,name,None)) for method,name in START_ASYNC_METHODS.items()}
    return dict(starts,job_class_available=job is not None,job_snapshot_callable=callable(getattr(job,"get_snapshot_json",None)),job_cancel_callable=callable(getattr(job,"cancel_job",None)))

def start_job(method,params):
    try:
        if method not in ASYNC_METHODS:raise OperationError('niagara_module_batch_job_method_invalid','Only the original batch Native start is admitted')
        text=_carrier(params);function=getattr(getattr(unreal,ASYNC_LIBRARY,None),START_ASYNC_METHODS[method],None)
        if not callable(function):raise OperationError('niagara_module_batch_job_native_unavailable','Dedicated Native start is unavailable; no fallback or start dispatched')
    except Exception as error:
        error.native_start_dispatched=False;raise
    job=function(text)
    try:
        cls=getattr(unreal,'CubelessNiagaraModuleInputsBatchJob',None)
        if cls is None or not isinstance(job,cls) or job.get_class().get_path_name()!=JOB_CLASS_PATH or not callable(getattr(job,'get_snapshot_json',None)) or not callable(getattr(job,'cancel_job',None)):
            raise OperationError('niagara_module_batch_job_identity_invalid','Returned job differs; dispatched start must not be repeated')
    except Exception as error:error.native_start_returned=True;error.native_returned_job=job;raise
    return job


def snapshot(job,method):
    raw=job.get_snapshot_json();value=None
    try:
        value=_decode_result(raw);data=value['data'];revision=data.get('native_checkpoint_revision')
        if (method not in ASYNC_METHODS or value.get('operation')!=method or type(data.get('native_job_id')) is not str or not data['native_job_id'] or type(data.get('native_job_terminal')) is not bool or type(data.get('operation_complete')) is not bool or data['operation_complete']!=data['native_job_terminal'] or type(revision) not in (int,float) or not math.isfinite(revision) or revision<0 or revision!=int(revision) or revision>9007199254740991):
            raise OperationError('niagara_module_batch_job_snapshot_invalid','Exact operation/job/revision/terminal Native receipt required')
        if not data['native_job_terminal'] and (value['success'] or value['postcondition_passed']):
            raise OperationError('niagara_module_batch_job_pending_health_invalid','Pending is not completed success or a passed final postcondition')
        if data.get('unsafe_job_publication_quarantined') is True:
            error=OperationError('niagara_module_batch_job_publication_quarantined','Native stopped or quarantined before safe public terminal publication; no further Job work')
            error.native_unsafe_publication_quarantined=True;raise error
        public_terminal=job.get_editor_property('is_terminal')
        if type(public_terminal) is not bool or public_terminal is not data['native_job_terminal']:
            raise OperationError('niagara_module_batch_job_publication_mismatch','Job terminal publication differs from returned snapshot; no replay')
    except Exception as error:
        error.native_snapshot_returned=True;error.native_raw_result=raw
        if value is not None:error.native_decoded_result=value
        raise
    value['_raw_native_json']=raw;return value
