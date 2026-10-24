"""HK/HO dedicated cleanup Native job adapter. No journal, transport or Engine asset calls."""
import json
import math
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

METHODS=["remove_niagara_modules", "remove_niagara_parameter_map_set_nodes", "remove_niagara_orphan_parameter_map_get_nodes", "remove_niagara_simulation_stage", "remove_niagara_scratch_pad_script"]
START_METHODS={"remove_niagara_modules": "start_remove_niagara_modules_job", "remove_niagara_parameter_map_set_nodes": "start_remove_niagara_parameter_map_set_nodes_job", "remove_niagara_orphan_parameter_map_get_nodes": "start_remove_niagara_orphan_parameter_map_get_nodes_job", "remove_niagara_simulation_stage": "start_remove_niagara_simulation_stage_job", "remove_niagara_scratch_pad_script": "start_remove_niagara_scratch_pad_script_job"}
LIBRARY='CubelessNiagaraGraphCleanupAsyncLibrary'
JOB_CLASS_PATH='/Script/CubelessMCPUE58Editor.CubelessNiagaraGraphCleanupJob'
MAX_CARRIER_UTF16_UNITS=1048576

def binding_status():
    library=getattr(unreal,LIBRARY,None);job=getattr(unreal,'CubelessNiagaraGraphCleanupJob',None)
    starts={method:callable(getattr(library,name,None)) for method,name in START_METHODS.items()}
    return dict(starts,job_class_available=job is not None,job_snapshot_callable=callable(getattr(job,'get_snapshot_json',None)),job_cancel_callable=callable(getattr(job,'cancel_job',None)))


def _carrier(params):
    if type(params) is not dict:raise OperationError('niagara_graph_cleanup_job_input_invalid','Native raw params object required')
    try:text=json.dumps(params,ensure_ascii=False,allow_nan=False,separators=(',',':'));units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:raise OperationError('niagara_graph_cleanup_job_input_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_graph_cleanup_job_input_bound','Native carrier exceeds 1Mi UTF16 or contains embedded NUL')
    return text

def start_job(method,params,current_callback=None):
    try:
        if method not in METHODS:raise OperationError('niagara_graph_cleanup_job_method_invalid','Only the original cleanup5 Native starts are admitted')
        text=_carrier(params);function=getattr(getattr(unreal,LIBRARY,None),START_METHODS[method],None)
        if not callable(function):raise OperationError('niagara_graph_cleanup_job_native_unavailable','Dedicated Native start is unavailable; no fallback or start dispatched')
    except Exception as error:
        error.native_start_dispatched=False;raise
    if current_callback is not None:
        try:current_callback()
        except Exception as error:error.native_start_dispatched=False;raise
    job=function(text)
    try:
        if current_callback is not None:current_callback()
        cls=getattr(unreal,'CubelessNiagaraGraphCleanupJob',None)
        if current_callback is None:
            # Preserve the existing Named validation order and error unchanged.
            if cls is None or not isinstance(job,cls) or job.get_class().get_path_name()!=JOB_CLASS_PATH or not callable(getattr(job,'get_snapshot_json',None)) or not callable(getattr(job,'cancel_job',None)):
                raise OperationError('niagara_graph_cleanup_job_identity_invalid','Returned job differs; dispatched start must not be repeated')
        else:
            if cls is None or not isinstance(job,cls) or not callable(getattr(job,'get_snapshot_json',None)) or not callable(getattr(job,'cancel_job',None)):
                raise OperationError('niagara_graph_cleanup_job_identity_invalid','Returned job differs; dispatched start must not be repeated')
            actual_class=job.get_class()
            current_callback()
            actual_path=actual_class.get_path_name()
            current_callback()
            if actual_path!=JOB_CLASS_PATH:
                raise OperationError('niagara_graph_cleanup_job_identity_invalid','Returned job differs; dispatched start must not be repeated')
    except Exception as error:error.native_start_returned=True;error.native_returned_job=job;raise
    return job


def snapshot(job,method,current_callback=None):
    if current_callback is not None:current_callback()
    raw=job.get_snapshot_json();value=None
    try:
        if current_callback is not None:current_callback()
        value=_decode_result(raw);data=value['data'];revision=data.get('native_checkpoint_revision')
        if (method not in METHODS or value.get('operation')!=method or type(data.get('native_job_id')) is not str or not data['native_job_id'] or type(data.get('native_job_terminal')) is not bool or type(data.get('operation_complete')) is not bool or data['operation_complete']!=data['native_job_terminal'] or type(revision) not in (int,float) or not math.isfinite(revision) or revision<0 or revision!=int(revision) or revision>9007199254740991):
            raise OperationError('niagara_graph_cleanup_job_snapshot_invalid','Exact operation/job/revision/terminal Native receipt required')
        if not data['native_job_terminal'] and (value['success'] or value['postcondition_passed']):
            raise OperationError('niagara_graph_cleanup_job_pending_health_invalid','Pending is not completed success or a passed final postcondition')
        if data.get('unsafe_job_publication_quarantined') is True:
            error=OperationError('niagara_graph_cleanup_job_publication_quarantined','Native stopped or quarantined before safe public terminal publication; no further Job work')
            error.native_unsafe_publication_quarantined=True;raise error
        if current_callback is not None:current_callback()
        public_terminal=job.get_editor_property('is_terminal')
        if current_callback is not None:current_callback()
        if type(public_terminal) is not bool or public_terminal is not data['native_job_terminal']:
            raise OperationError('niagara_graph_cleanup_job_publication_mismatch','Job terminal publication differs from returned snapshot; no replay')
    except Exception as error:
        error.native_snapshot_returned=True;error.native_raw_result=raw
        if value is not None:error.native_decoded_result=value
        raise
    value['_raw_native_json']=raw;return value
