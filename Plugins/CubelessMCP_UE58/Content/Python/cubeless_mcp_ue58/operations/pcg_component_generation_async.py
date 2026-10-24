"""HD Named PCG refresh: one initial operation and GT query-only continuation.
Uses existing CU typed future and the one operation journal; no Native job,
worker, shared task cancellation, sleep, flush, pump or mutation replay.
"""
import copy
import hashlib
import json
import os
import time
import uuid
import unreal
from cubeless_mcp_ue58.registration.async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.registration.results import reflected_result
from . import state
from . import pcg_component_generation as domain

METHOD='refresh_pcg_components'
_ACTIVE=False
_TICKER=None
_SHUTDOWN=None
_PENDING={}
_QUARANTINED={}
_LAST_TERMINAL={}
_LIFECYCLE_ERRORS=[]


class _RefreshJournal:
    def __init__(self,arguments,prepared,run_id):
        self.result=state.new_result(run_id);self.started=time.perf_counter();self.owned=False
        domain._context(self.result,METHOD,prepared['args'],prepared['original'],None)
        self.result['data'].update(Named_async_refresh=True,async_external_wait_not_raw_single_frame=True,
            initial_request_replayed=False,shared_PCG_task_cancelled=False)
        self.result['stage']='before_pcg_component_generation_checkpoint'
        self.path=state._journal_path(run_id)
        if self.path.exists():raise state.OperationError('operation_incomplete','PCG operation IDs are never restarted')
        if state._active_operation is not None:raise state.OperationError('mutation_busy','Another parent owns the project slot')
        record=dict(schema_version=1,operation_id=run_id,operation=METHOD,arguments=copy.deepcopy(arguments),
            fingerprint=hashlib.sha256(json.dumps({'name':METHOD,'arguments':arguments},sort_keys=True,allow_nan=False).encode('utf-8')).hexdigest(),
            state='started',result=self.result,engine_version=unreal.SystemLibrary.get_engine_version(),process_id=os.getpid())
        encoded=json.dumps(record,ensure_ascii=False,sort_keys=True,allow_nan=False)
        self.path.parent.mkdir(parents=True,exist_ok=True)
        with self.path.open('x',encoding='utf-8') as stream:stream.write(encoded);stream.flush();os.fsync(stream.fileno())
        state._active_operation=run_id;self.owned=True

    def checkpoint(self,result=None):
        if not self.owned or state._active_operation!=self.result['operation_id']:
            raise state.OperationError('journal_checkpoint_not_owned','PCG parent ownership changed; no replay')
        state.checkpoint(self.result)

    def finish(self,sealed,recovery):
        if not self.owned or state._active_operation!=self.result['operation_id']:
            raise state.OperationError('journal_finalize_not_owned','PCG parent ownership changed before finalization')
        with self.path.open('r',encoding='utf-8') as stream:record=json.load(stream)
        if record.get('state')!='started' or record.get('operation_id')!=self.result['operation_id'] or record.get('process_id')!=os.getpid():
            raise state.OperationError('journal_finalize_not_owned','Started PCG record differs')
        record.update(result=copy.deepcopy(sealed),state='started' if recovery else 'complete')
        state._write_record(self.path,record)
        if not recovery:state._active_operation=None;self.owned=False


def _message(entry,message):
    return str(message)[:700]+'; operation_id='+entry['id']+'; journal='+str(entry['journal'].path)


def _set_error_once(entry,error):
    if entry.get('cu_error_attempted') or entry.get('cu_completion_entered'):return
    entry['cu_error_attempted']=True
    try:entry['object'].set_error(_message(entry,error))
    except Exception as secondary:entry['delivery_error']=str(secondary)[:700]


def _delivery_failed(entry,error):
    # No change to sealed result/journal and no Engine observation after seal.
    entry['delivery_error']=str(error)[:700]
    _PENDING.pop(entry['id'],None);_QUARANTINED[entry['id']]=entry
    _set_error_once(entry,error)


def _seal_deliver(entry,recovery=False):
    global _LAST_TERMINAL
    if entry.get('finalization_attempted'):return
    result=entry['journal'].result
    result['data'].update(elapsed_seconds=round(time.perf_counter()-entry['journal'].started,6),
        final_receipt_sealed_before_delivery=True,initial_request_replayed=False,deferred_completion_pending=False,
        shared_PCG_task_cancelled=False)
    entry['sealed']=copy.deepcopy(result);entry['finalization_attempted']=True
    try:
        entry['journal'].finish(entry['sealed'],recovery)
        entry['journal_finalized']=True
        entry['object'].set_editor_property('value',reflected_result(copy.deepcopy(entry['sealed'])))
        entry['cu_completion_entered']=True
        if entry['object'].complete_value() is not True:
            raise state.OperationError('pcg_refresh_async_delivery_failed','CU completion refused; no second completion, journal replay or DTO mutation')
        entry['cu_completed']=True
    except Exception as error:
        _delivery_failed(entry,error)
    _PENDING.pop(entry['id'],None)
    if recovery:_QUARANTINED[entry['id']]=entry
    _LAST_TERMINAL=dict(operation_id=entry['id'],success=result['success'],recovery_required=recovery,delivery_error=entry.get('delivery_error',''))


def _stop(entry,error,metadata_only=False):
    if entry.get('finalization_attempted'):return
    result=entry['journal'].result;state.fail(result,error)
    context=entry.get('descriptor',{}).get('context')
    if entry.get('descriptor') is not None:domain._wait_failure(result,entry['descriptor'],error)
    metadata_only=metadata_only or context is not None and context.lost or result['data'].get('projection_skipped_unsafe') is True
    result['data'].update(initial_request_replayed=False,shared_PCG_task_cancelled=False,
        PCG_generation_may_still_be_in_flight=getattr(error,'pcg_refresh_terminal_failure',False) is not True,do_not_retry=True)
    result['retry'].update(automatic_retry=False,duplicate_risk='PCG_initial_request_must_not_be_replayed')
    if metadata_only:
        result['data'].update(change_state_known=False,mutation_state_unknown=True,mutation_performed=None,
            operation_complete=False,workflow_recovery_required=True,unsafe_publication_quarantined=True,metadata_only_stop=True,shutdown_metadata_only=True)
        domain._refresh_changes(result)
        try:entry['journal'].checkpoint()
        except Exception as checkpoint_error:entry['checkpoint_error']=str(checkpoint_error)[:700]
        _PENDING.pop(entry['id'],None);_QUARANTINED[entry['id']]=entry
        return
    # This is a failed workflow receipt, never a claim that shared PCG tasks stopped.
    result['data']['operation_complete']=True
    if 'legacy_result' not in result['data']:
        result['data']['legacy_result']=dict(success=False,error=str(error),error_code=getattr(error,'code','pcg_tool_failed'))
    try:entry['journal'].checkpoint()
    except Exception as checkpoint_error:
        entry['checkpoint_error']=str(checkpoint_error)[:700]
        result['data'].update(change_state_known=False,mutation_state_unknown=True,mutation_performed=None)
    recovery=result['data'].get('change_state_known') is not True
    _seal_deliver(entry,recovery)


def _complete_wait(entry,leaf,elapsed):
    result=entry['journal'].result
    final=domain._wait_result(result,entry['descriptor'],leaf,elapsed,entry['iterations'])
    try:domain._finish(result,final)
    except Exception as error:state.fail(result,error)
    _seal_deliver(entry,result['data'].get('change_state_known') is not True)


def _observation_signature(leaf):
    rows=leaf.get('components',[])
    return json.dumps({'component_count':leaf.get('component_count'),'components':[
        {key:row.get(key) for key in ('component_path','generating','cleaning_up','refresh_in_progress','generated_output_data_count','managed_resource_count')}
        for row in rows if type(row) is dict]},sort_keys=True,allow_nan=False)


def _tick(delta):
    if not _ACTIVE:return False
    for entry in tuple(_PENDING.values()):
        if entry.get('finalization_attempted'):continue
        try:
            now=time.perf_counter();elapsed=now-entry['wait_started']
            if now<entry['next_poll'] and elapsed<entry['timeout']:continue
            frame=unreal.SystemLibrary.get_frame_count()
            if type(frame) is not int:raise state.OperationError('pcg_refresh_frame_unavailable','Actual GT frame counter required')
            if frame<=entry['last_frame']:continue
            entry['last_frame']=frame
            if elapsed>=entry['timeout']:
                entry['descriptor']['context'].admit()
                entry['journal'].result['data']['timeout_uses_last_completed_generation_observation']=True
                _complete_wait(entry,entry['last_leaf'],elapsed);continue
            leaf=domain._poll(entry['journal'].result,entry['descriptor'])
            entry['iterations']+=1;entry['last_leaf']=leaf
            entry['next_poll']=now+entry['interval']
            signature=_observation_signature(leaf)
            if signature!=entry['last_signature']:
                entry['journal'].checkpoint();entry['last_signature']=signature
            if domain._native_refresh_result_ready(leaf):_complete_wait(entry,leaf,time.perf_counter()-entry['wait_started'])
        except Exception as error:_stop(entry,error)
    return True


def completed_result(record):
    obj=CubelessPythonAsyncOperationResult()
    obj.set_editor_property('value',reflected_result(copy.deepcopy(record)))
    # CompleteValue itself resolves Reject through SetError. Never call it twice.
    if obj.complete_value() is not True:raise state.OperationError('pcg_refresh_async_rejection_delivery_failed','Pre-dispatch result completion refused; no Native request ran')
    return obj


def begin(arguments):
    if not _ACTIVE:return completed_result(domain._rejected(METHOD,state.OperationError('pcg_refresh_async_inactive','Named PCG lifecycle must be active; no request dispatched')))
    if domain._ACTIVE_REQUEST:return completed_result(domain._rejected(METHOD,state.OperationError('pcg_component_generation_reentry','Reentrant start rejected before journal/SDK work')))
    try:
        args,original,legacy=domain._arguments(METHOD,arguments,None,True)
        frame=unreal.SystemLibrary.get_frame_count()
        if type(frame) is not int:raise state.OperationError('pcg_refresh_frame_unavailable','Actual GT frame counter required before any writer')
    except Exception as error:return completed_result(domain._rejected(METHOD,error))
    run_id=uuid.uuid4().hex;entry=None
    try:
        obj=CubelessPythonAsyncOperationResult()
        journal=_RefreshJournal(arguments,dict(args=args,original=original),run_id)
        entry=dict(id=run_id,object=obj,journal=journal,iterations=0,last_frame=frame)
        _PENDING[run_id]=entry
        result,descriptor=domain._start(journal.result,METHOD,args,original,legacy,journal.checkpoint,deferred=True)
        if descriptor is None:
            _seal_deliver(entry,result['data'].get('change_state_known') is not True)
            return obj
        entry['descriptor']=descriptor;entry['last_leaf']=descriptor['initial'];entry['wait_started']=time.perf_counter()
        values=descriptor['values'];entry['timeout']=max(0.0,min(float(values['timeout_seconds']),300.0))
        entry['interval']=max(0.005,min(float(values['poll_interval_seconds']),1.0))
        entry['next_poll']=entry['wait_started']+entry['interval'];entry['last_signature']=_observation_signature(entry['last_leaf'])
        result['data'].update(deferred_completion_pending=True,actual_wait_mode='deferred_GT_ticker_query_only',PCG_generation_completed=False)
        journal.checkpoint()
        if domain._native_refresh_result_ready(entry['last_leaf']) or entry['timeout']==0.0:
            _complete_wait(entry,entry['last_leaf'],0.0)
        return obj
    except Exception as error:
        if entry is not None:
            _stop(entry,error)
            return entry['object']
        return completed_result(domain._rejected(METHOD,error))


def _shutdown():
    global _ACTIVE
    _ACTIVE=False
    for entry in tuple(_PENDING.values()):
        _stop(entry,state.OperationError('pcg_refresh_async_shutdown_unknown','Shutdown before observed completion; no late PCG call/cancel/replay'),True)


def binding_status():
    return dict(active=_ACTIVE,pending_count=len(_PENDING),quarantined_count=len(_QUARANTINED),
        cleanup_required=bool(_PENDING or _QUARANTINED),class_unregister_allowed=not (_PENDING or _QUARANTINED),
        last_terminal=copy.deepcopy(_LAST_TERMINAL),lifecycle_errors=list(_LIFECYCLE_ERRORS),Native_bindings=domain.binding_status())


def activate():
    global _ACTIVE,_TICKER,_SHUTDOWN
    if _ACTIVE:return binding_status()
    if _PENDING or _QUARANTINED or _TICKER is not None or _SHUTDOWN is not None:
        raise state.OperationError('pcg_refresh_async_ownership_retained','Prior pending/quarantine ownership requires explicit reconciliation')
    if not all(value is True for value in domain.binding_status().values()):raise state.OperationError('pcg_refresh_native_unavailable','Exact HD Native bindings are required')
    _SHUTDOWN=unreal.register_python_shutdown_callback(_shutdown)
    try:_TICKER=unreal.register_ticker_callback(_tick,delay=0.0)
    except Exception:
        unreal.unregister_python_shutdown_callback(_SHUTDOWN);_SHUTDOWN=None;raise
    _ACTIVE=True
    return binding_status()


def deactivate(reason='PCG component generation tools deactivated'):
    global _ACTIVE,_TICKER,_SHUTDOWN
    _ACTIVE=False
    for kind,handle,remove in (('ticker',_TICKER,unreal.unregister_ticker_callback),('shutdown',_SHUTDOWN,unreal.unregister_python_shutdown_callback)):
        if handle is not None:
            if kind=='ticker':_TICKER=None
            else:_SHUTDOWN=None
            try:remove(handle)
            except Exception as error:_LIFECYCLE_ERRORS.append(kind+': '+str(error)[:700])
    for entry in tuple(_PENDING.values()):
        _stop(entry,state.OperationError('pcg_refresh_async_deactivated_unknown',reason+'; no shared PCG task was cancelled'),True)
    return binding_status()
