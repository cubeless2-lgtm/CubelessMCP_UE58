"""UE5.8 dedicated NiagaraEmitterConfiguration Native1 x7. No transport or fallback."""
import copy
import json
import math
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraEmitterConfigurationLibrary'
BINDINGS=["inspect_niagara_emitter_properties", "set_niagara_emitter_enabled", "set_niagara_emitter_local_space", "set_niagara_emitter_sim_target", "set_niagara_emitter_fixed_bounds", "set_niagara_emitter_persistent_ids", "remove_niagara_emitter"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"inspect_niagara_emitter_properties": ["system_path", "emitter_index", "emitter_name"], "set_niagara_emitter_enabled": ["system_path", "enabled", "emitter_index", "emitter_name", "allow_source_edit", "save", "request_compile"], "set_niagara_emitter_local_space": ["system_path", "value", "emitter_index", "emitter_name", "allow_source_edit", "save", "request_compile"], "set_niagara_emitter_sim_target": ["system_path", "sim_target", "auto_fixed_bounds", "emitter_index", "emitter_name", "allow_source_edit", "save", "request_compile"], "set_niagara_emitter_fixed_bounds": ["system_path", "min", "max", "mode", "emitter_index", "emitter_name", "allow_source_edit", "save", "request_compile"], "set_niagara_emitter_persistent_ids": ["system_path", "enabled", "emitter_index", "emitter_name", "allow_source_edit", "save", "request_compile", "wait_for_completion", "timeout_seconds", "poll_interval_seconds", "value"], "remove_niagara_emitter": ["system_path", "emitter_index", "emitter_name", "allow_source_edit", "save", "request_compile"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_emitter_configuration_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_emitter_configuration_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_emitter_configuration_native_api_unavailable','Build/load the exact NiagaraEmitterConfiguration Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_emitter_configuration_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def inspect_niagara_emitter_properties(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_niagara_emitter_properties",text)

def set_niagara_emitter_enabled(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_emitter_enabled",text)

def set_niagara_emitter_local_space(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_emitter_local_space",text)

def set_niagara_emitter_sim_target(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_emitter_sim_target",text)

def set_niagara_emitter_fixed_bounds(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_emitter_fixed_bounds",text)

def set_niagara_emitter_persistent_ids(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("set_niagara_emitter_persistent_ids",text)

def remove_niagara_emitter(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("remove_niagara_emitter",text)



ASYNC_METHODS=["set_niagara_emitter_persistent_ids"]
START_ASYNC_METHODS={"set_niagara_emitter_persistent_ids":"start_set_niagara_emitter_persistent_ids_job"}
ASYNC_LIBRARY="CubelessNiagaraEmitterEventAuthoringAsyncLibrary"
JOB_CLASS_PATH="/Script/CubelessMCPUE58Editor.CubelessNiagaraEmitterEventAuthoringJob"

def async_binding_status():
    library=getattr(unreal,ASYNC_LIBRARY,None);job=getattr(unreal,"CubelessNiagaraEmitterEventAuthoringJob",None)
    starts={method:callable(getattr(library,name,None)) for method,name in START_ASYNC_METHODS.items()}
    return dict(starts,job_class_available=job is not None,job_snapshot_callable=callable(getattr(job,"get_snapshot_json",None)),job_cancel_callable=callable(getattr(job,"cancel_job",None)))

def start_job(method,params):
    try:
        if method not in ASYNC_METHODS:raise OperationError('niagara_emitter_configuration_job_method_invalid','Only the original Persistent IDs Native start is admitted')
        text=_carrier(params);function=getattr(getattr(unreal,ASYNC_LIBRARY,None),START_ASYNC_METHODS[method],None)
        if not callable(function):raise OperationError('niagara_emitter_configuration_job_native_unavailable','Dedicated Native start is unavailable; no fallback or start dispatched')
    except Exception as error:
        error.native_start_dispatched=False;raise
    job=function(text)
    try:
        cls=getattr(unreal,'CubelessNiagaraEmitterEventAuthoringJob',None)
        if cls is None or not isinstance(job,cls) or job.get_class().get_path_name()!=JOB_CLASS_PATH or not callable(getattr(job,'get_snapshot_json',None)) or not callable(getattr(job,'cancel_job',None)):
            raise OperationError('niagara_emitter_configuration_job_identity_invalid','Returned job differs; dispatched start must not be repeated')
    except Exception as error:error.native_start_returned=True;error.native_returned_job=job;raise
    return job


def snapshot(job,method):
    raw=job.get_snapshot_json();value=None
    try:
        value=_decode_result(raw);data=value['data'];revision=data.get('native_checkpoint_revision')
        if (method not in ASYNC_METHODS or value.get('operation')!=method or type(data.get('native_job_id')) is not str or not data['native_job_id'] or type(data.get('native_job_terminal')) is not bool or type(data.get('operation_complete')) is not bool or data['operation_complete']!=data['native_job_terminal'] or type(revision) not in (int,float) or not math.isfinite(revision) or revision<0 or revision!=int(revision) or revision>9007199254740991):
            raise OperationError('niagara_emitter_configuration_job_snapshot_invalid','Exact operation/job/revision/terminal Native receipt required')
        if not data['native_job_terminal'] and (value['success'] or value['postcondition_passed']):
            raise OperationError('niagara_emitter_configuration_job_pending_health_invalid','Pending is not completed success or a passed final postcondition')
        if data.get('unsafe_job_publication_quarantined') is True:
            error=OperationError('niagara_emitter_configuration_job_publication_quarantined','Native stopped or quarantined before safe public terminal publication; no further Job work')
            error.native_unsafe_publication_quarantined=True;raise error
        public_terminal=job.get_editor_property('is_terminal')
        if type(public_terminal) is not bool or public_terminal is not data['native_job_terminal']:
            raise OperationError('niagara_emitter_configuration_job_publication_mismatch','Job terminal publication differs from returned snapshot; no replay')
    except Exception as error:
        error.native_snapshot_returned=True;error.native_raw_result=raw
        if value is not None:error.native_decoded_result=value
        raise
    value['_raw_native_json']=raw;return value
