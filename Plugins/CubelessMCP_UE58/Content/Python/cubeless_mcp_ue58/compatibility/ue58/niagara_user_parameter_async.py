"""GM dedicated User-parameter Native job adapter. No journal, transport or Engine asset calls."""
import json
import math
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

METHOD='remove_niagara_user_parameters'
LIBRARY='CubelessNiagaraUserParameterAsyncLibrary'
JOB_CLASS_PATH='/Script/CubelessMCPUE58Editor.CubelessNiagaraUserParameterJob'
MAX_CARRIER_UTF16_UNITS=1048576

def binding_status():
    library=getattr(unreal,LIBRARY,None);job=getattr(unreal,'CubelessNiagaraUserParameterJob',None)
    return dict(start_callable=callable(getattr(library,'start_remove_user_parameters_job',None)),job_class_available=job is not None,job_snapshot_callable=callable(getattr(job,'get_snapshot_json',None)),job_cancel_callable=callable(getattr(job,'cancel_job',None)))

def _carrier(params):
    if type(params) is not dict:raise OperationError('niagara_user_parameter_job_input_invalid','Native raw params object required')
    try:text=json.dumps(params,ensure_ascii=False,allow_nan=False,separators=(',',':'));units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:raise OperationError('niagara_user_parameter_job_input_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_user_parameter_job_input_bound','Native carrier exceeds 1Mi UTF16 or contains embedded NUL')
    return text

def start_job(params):
    text=_carrier(params);function=getattr(getattr(unreal,LIBRARY,None),'start_remove_user_parameters_job',None)
    if not callable(function):
        error=OperationError('niagara_user_parameter_job_native_unavailable','Dedicated Native start is unavailable; no fallback or start dispatched');error.native_start_dispatched=False;raise error
    job=function(text)
    try:
        cls=getattr(unreal,'CubelessNiagaraUserParameterJob',None)
        if cls is None or not isinstance(job,cls) or job.get_class().get_path_name()!=JOB_CLASS_PATH or not callable(getattr(job,'get_snapshot_json',None)) or not callable(getattr(job,'cancel_job',None)):
            raise OperationError('niagara_user_parameter_job_identity_invalid','Returned job differs; dispatched start must not be repeated')
    except Exception as error:error.native_start_returned=True;error.native_returned_job=job;raise
    return job

def snapshot(job):
    raw=job.get_snapshot_json();value=None
    try:
        value=_decode_result(raw);data=value['data'];revision=data.get('native_checkpoint_revision')
        if (value.get('operation')!=METHOD or type(data.get('native_job_id')) is not str or not data['native_job_id'] or type(data.get('native_job_terminal')) is not bool or type(data.get('operation_complete')) is not bool or data['operation_complete']!=data['native_job_terminal'] or type(revision) not in (int,float) or not math.isfinite(revision) or revision<0 or revision!=int(revision) or revision>9007199254740991):
            raise OperationError('niagara_user_parameter_job_snapshot_invalid','Exact operation/job/revision/terminal Native receipt required')
        public_terminal=job.get_editor_property('is_terminal')
        if type(public_terminal) is not bool or public_terminal is not data['native_job_terminal']:
            raise OperationError('niagara_user_parameter_job_publication_mismatch','Job terminal publication differs from returned snapshot; no replay')
    except Exception as error:
        error.native_snapshot_returned=True;error.native_raw_result=raw
        if value is not None:error.native_decoded_result=value
        raise
    value['_raw_native_json']=raw;return value
