"""GU3 holds the existing journal slot until actual Sprite-preset Native terminal.

Only the original Sprite-preset Named MCP entry uses this continuation. Native
owns exact preset/compile completion and conditional save order. Existing state storage and CU completion
are reused; no synchronous pending finalization, blocking wait, asset retry or
shared Sprite preset-compiler cancellation is introduced.
"""
import copy
import hashlib
import json
import os
import time
import uuid
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import niagara_sprite_preset_async as api
from cubeless_mcp_ue58.registration.async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.registration.results import reflected_result
from . import state

METHOD='create_sprite_niagara_preset_system'
_ACTIVE = False
_TICKER_HANDLE = None
_SHUTDOWN_HANDLE = None
_PENDING = {}
_QUARANTINED = {}
_LAST_TERMINAL = {}
_LIFECYCLE_ERRORS = []


def _arguments(method,public):
    if method!=METHOD:raise state.OperationError('niagara_sprite_preset_async_method_invalid','Only the dedicated Sprite-preset construction job is admitted')
    from . import niagara_sprite_preset_authoring
    args,original,legacy=niagara_sprite_preset_authoring._arguments(method,public,None,True)
    return dict(arguments=copy.deepcopy(public),args=args,original=original)

def _context(result,method,public):
    from . import niagara_sprite_preset_authoring
    niagara_sprite_preset_authoring._context(result,method,public['args'],public['original'],None)
    result['data'].update(async_native_job=True,async_entry='Named_official_MCP_only',direct_Python_dict_return_shape_preserved=True,direct_RawBatch_execution="Native_pre_writer_unsupported_requires_Root_forwarder",Raw_Batch_async_continuation_implemented=False,mandatory_wait_not_bypassed=True,
        no_Python_asset_mutation_compile_or_save=True,global_compiler_cancel_attempted=False,actual_official_async_wire_verified=False)

class _SpritePresetJournal:
    def __init__(self, method, public, run_id):
        self.result = state.new_result(run_id)
        _context(self.result, method, public)
        self.result["stage"] = "before_native_niagara_sprite_preset_async_start"
        self.result["data"].update(native_start_dispatched=False, native_start_returned=False,
            native_snapshot_returned=False, mutation_performed=False, mutation_state_unknown=False,
            pending_native_intent={"method": method, "arguments": copy.deepcopy(public),
                "scope": "future native phases unverified until returned snapshots"})
        self.path = state._journal_path(run_id)
        self.started = time.perf_counter()
        self.owned = False
        if self.path.exists():
            raise state.OperationError("operation_incomplete", "Sprite preset IDs are not replayed; inspect the journal")
        if state._active_operation is not None:
            raise state.OperationError("mutation_busy", "Another operation owns the project slot")
        fingerprint = hashlib.sha256(json.dumps({"name": method, "arguments": public},
            sort_keys=True, allow_nan=False).encode("utf-8")).hexdigest()
        record = {"schema_version": 1, "operation_id": run_id, "operation": method,
            "arguments": copy.deepcopy(public), "fingerprint": fingerprint, "state": "started",
            "result": self.result, "engine_version": unreal.SystemLibrary.get_engine_version(), "process_id": os.getpid()}
        encoded = json.dumps(record, ensure_ascii=False, sort_keys=True, allow_nan=False)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("x", encoding="utf-8") as stream:
            stream.write(encoded); stream.flush(); os.fsync(stream.fileno())
        state._active_operation = run_id
        self.owned = True

    def checkpoint(self):
        if not self.owned or state._active_operation != self.result["operation_id"]:
            raise state.OperationError("journal_checkpoint_not_owned", "Sprite preset ownership changed; no replay")
        state.checkpoint(self.result)

    def finish(self, recovery):
        if not self.owned or state._active_operation != self.result["operation_id"]:
            raise state.OperationError("journal_finalize_not_owned", "Sprite preset ownership changed before finalization")
        with self.path.open("r", encoding="utf-8") as stream:
            record = json.load(stream)
        if (record.get("operation_id") != self.result["operation_id"] or record.get("state") != "started"
                or record.get("process_id") != os.getpid()):
            raise state.OperationError("journal_finalize_not_owned", "Sprite preset started journal ownership differs")
        record["result"] = copy.deepcopy(self.result)
        record["state"] = "started" if recovery else "complete"
        state._write_record(self.path, record)
        if not recovery:
            state._active_operation = None
            self.owned = False


def _adopt(entry,native):
    from . import niagara_sprite_preset_authoring
    result=entry['journal'].result;was_saved=result['save'].get('saved') is True
    job_id=native['data'].get('native_job_id');prior_id=entry.get('native_job_id')
    if prior_id is not None and job_id!=prior_id:raise state.OperationError('niagara_sprite_preset_job_identity_changed','Snapshot belongs to another job; do not repeat start')
    entry['native_job_id']=job_id
    niagara_sprite_preset_authoring._adopt(result,METHOD,entry['public']['args'],entry['public']['original'],None,native)
    result['save']['saved']=was_saved or native['saved']
    _context(result,METHOD,entry['public'])
    result['data'].update(native_snapshot_returned=True,native_terminal_observed=native['data']['native_job_terminal'],adapter_snapshot_calls=entry['snapshot_calls'],adapter_tick_callbacks=entry['tick_callbacks'],command_may_still_be_in_flight=not native['data']['native_job_terminal'],native_job_id=job_id)
    result['data']['deferred_terminal_wait_completion_unverified']=not (native['data']['native_job_terminal'] and native['success'] and native['postcondition_passed'])
    result['stage']=native['stage']
    result['validation'].update(native_terminal_observed=native['data']['native_job_terminal'],compile_preset_save_proof_native_authoritative=True)
    result['retry'].update(automatic_retry=False,duplicate_risk='held_sprite_preset_job_and_journal_no_replay')
    # Snapshot decoding returns a new plain JSON tree. _adopt copies values
    # into the journal; neither path mutates this returned tree afterwards.
    entry['last_native']=native
    _canonical_stage_views(result['data'])

def _checkpoint_native(entry, native, force=False):
    revision = native["data"].get("native_checkpoint_revision")
    if type(revision) not in (int,float) or revision<0 or revision!=int(revision):
        raise state.OperationError("niagara_sprite_preset_async_revision_invalid", "Native meaningful snapshot revision required")
    previous = entry.get("last_checkpoint_revision")
    if previous is not None and revision < previous:
        raise state.OperationError("niagara_sprite_preset_async_revision_regressed", "Native snapshot revision regressed; no replay")
    if force or previous != revision:
        entry["native_checkpoint_attempts"] = entry.get("native_checkpoint_attempts", 0) + 1
        entry["journal"].result["data"]["adapter_native_checkpoint_attempts"] = entry["native_checkpoint_attempts"]
        entry["journal"].checkpoint()
        entry["last_checkpoint_revision"] = revision


def _delivery_error(entry, message):
    return (str(message)[:700] + "; operation_id=" + entry["run_id"]
        + "; stage=" + str(entry["journal"].result["stage"])
        + "; journal=" + str(entry["journal"].path))


def _cancel_once(entry, reason):
    result = entry["journal"].result
    if not entry.get("job_reference_verified"):
        result["data"]["owned_cancel_not_attempted_unverified_job_reference"] = True
        return None
    if not entry.get("cancel_attempted"):
        entry["cancel_attempted"] = True
        result["data"]["adapter_owned_cancel_attempt_count"] = 1
        try:
            acknowledgement = entry["job"].cancel_job(str(reason)[:1024])
            result["data"].update(cancel_call_returned=True,
                cancel_acknowledgement=acknowledgement if type(acknowledgement) is bool else None,
                cancel_acknowledgement_is_not_shared_compiler_stop=True)
        except Exception as error:
            result["data"]["owned_cancel_error"] = str(error)[:700]
    # One best-effort read remains safe even if cancellation acknowledgement failed.
    entry["snapshot_calls"] += 1
    try:
        native = api.snapshot(entry["job"])
        _adopt(entry, native)
        return native
    except Exception as error:
        if getattr(error, "native_snapshot_returned", False):
            result["data"]["cancel_snapshot_undecoded_raw_result"] = getattr(error, "native_raw_result", None)
        _preserve_snapshot_error(entry,error)
        result["data"]["cancel_snapshot_error"] = str(error)[:700]
        return None


def _seal(entry, recovery):
    """Prepare all result data before the one durable finalization attempt."""
    if entry.get("finalization_attempted"):
        raise state.OperationError("niagara_sprite_preset_async_finalize_already_attempted", "Finalization is not replayed")
    result = entry["journal"].result
    # Preserve the actual returned Native text, not a reconstruction of the
    # adopted/compacted public view. This is one terminal diagnostic write.
    raw=entry.get('last_native',{}).get('_raw_native_json')
    if type(raw) is str:
        payload=raw.encode('utf-8')
        path=entry['journal'].path.with_suffix('.native.json')
        with path.open('xb') as stream:
            stream.write(payload);stream.flush();os.fsync(stream.fileno())
        result['data']['native_terminal_diagnostic']={'path':str(path),'bytes':len(payload),
            'sha256':hashlib.sha256(payload).hexdigest(),'representation':'actual_returned_native_JSON_UTF8'}
    result["data"]["elapsed_seconds"] = round(time.perf_counter() - entry["journal"].started, 6)
    entry["sealed_result"] = copy.deepcopy(result)
    entry["finalization_attempted"] = True
    entry["journal"].finish(recovery)
    entry["journal_finalized"] = True


def _delivery_failed(entry, error):
    # A finalization/write may already have committed. Do not mutate the sealed
    # result/journal or observe/cancel/restart the Job after this boundary.
    entry["delivery_error"] = str(error)[:1024]
    entry["observation_only_quarantine"] = False
    _QUARANTINED[entry["run_id"]] = entry
    _PENDING.pop(entry["run_id"], None)
    if (not entry.get("cu_completed") and not entry.get("cu_error_attempted")
            and not entry.get("cu_completion_call_entered")):
        entry["cu_error_attempted"] = True
        try:
            entry["result_object"].set_error(_delivery_error(entry, "Final receipt/durable delivery is uncertain; no replay: " + str(error)))
        except Exception as delivery_error:
            entry["delivery_error"] = str(delivery_error)[:1024]


def _canonical_stage_views(data):
    """Deduplicate identical diagnostic mirrors without losing stage evidence."""
    canonical=data.get('stage_results')
    refs=dict(data.get('wire_diagnostic_references',{}))
    if type(canonical) is list:
        mirror=data.get('legacy_Named_result');legacy=data.get('legacy_result')
        same_mirror=type(mirror) is dict and mirror==legacy
        for key in ('session_execution','legacy_result'):
            view=data.get(key)
            if type(view) is dict and view.get('stage_results')==canonical:
                view.pop('stage_results');view['stage_results_reference']='data.stage_results'
                refs['data.'+key+'.stage_results']='data.stage_results'
        # This adapter-generated mirror is the same public legacy projection.
        if same_mirror:
            data.pop('legacy_Named_result');data['legacy_Named_result_reference']='data.legacy_result'
            refs['data.legacy_Named_result']='data.legacy_result'
    data['wire_diagnostic_references']=refs
    data['wire_diagnostic_reference_contract']='Canonical data.stage_results; diagnostic mirrors use explicit references. Actual original Native terminal text is preserved separately.'
    return refs


def _wire_projection(entry):
    """Keep canonical observations once; full sealed journal stays unchanged."""
    result=copy.deepcopy(entry['sealed_result']);data=result['data']
    refs=_canonical_stage_views(data)
    payload=entry['journal'].path.read_bytes()
    data['full_result_journal']={'path':str(entry['journal'].path),'bytes':len(payload),'sha256':hashlib.sha256(payload).hexdigest()}
    # Leave room for the reflected standard11 JSON escaping below the unchanged
    # 16MiB boundary. Oversized diagnostic observations remain in that journal;
    # original outcome, actual mutation/compile/save and retry fields stay live.
    def wire_bytes():
        fields={k+'_json':json.dumps(result[k],ensure_ascii=False,allow_nan=False) for k in ('changes','compile','save','validation','retry','data')}
        return len(json.dumps(fields,ensure_ascii=False,allow_nan=False).encode('utf-8'))
    if wire_bytes()>15*1024*1024:
        for key in ('graph_observation_evidence','stage_results','legacy_result','last_child_result','session_execution'):
            if key in data:
                data.pop(key)
                data.setdefault('externalized_diagnostic_fields',{})[key]='full_result_journal.result.data.'+key
        # Every mirror resolves to its original field in the sealed journal,
        # including the original legacy field (which still has stage_results).
        data['wire_diagnostic_references']={source:'full_result_journal.result.'+target for source,target in refs.items()}
        if 'legacy_Named_result_reference' in data:
            data['legacy_Named_result_reference']='full_result_journal.result.data.legacy_result'
        for key in ('session_execution','legacy_result'):
            if type(data.get(key)) is dict and 'stage_results_reference' in data[key]:
                data[key]['stage_results_reference']='full_result_journal.result.data.stage_results'
        data['wire_diagnostic_reference_contract']='Large diagnostics are stored unchanged in full_result_journal; this is a terminal outcome projection, not a complete inline legacy result.'
    if wire_bytes()>15*1024*1024:
        # Exceptional undecoded/transport diagnostic blobs can occur outside the
        # regular graph fields. Keep the original outcome, not a fabricated
        # engine failure, and point all diagnostic detail to its durable copy.
        result['data']={key:copy.deepcopy(data[key]) for key in (
            'full_result_journal','native_terminal_diagnostic','native_outcome',
            'operation_complete','native_job_terminal','native_job_id','native_terminal_observed',
            'mutation_performed','mutation_state_unknown','change_state_known',
            'native_mutation_attempted','execution_mutation_attempted','workflow_recovery_required') if key in data}
        result['data']['wire_diagnostics_externalized']=True
        result['data']['wire_diagnostic_reference_contract']='All diagnostic details are in full_result_journal.result.data. Original outcome/change/compile/save/retry fields are retained.'
    if wire_bytes()>15*1024*1024:
        raise state.OperationError('niagara_sprite_preset_delivery_oversized',
            'Outcome is sealed in '+str(entry['journal'].path)+'; diagnostic projection still exceeds transport budget; engine request is not retried')
    return result


def _deliver(entry):
    if entry.get("cu_delivery_attempted"):
        raise state.OperationError("niagara_sprite_preset_async_delivery_already_attempted", "CU delivery already attempted; no second final")
    if not entry.get("journal_finalized") or type(entry.get("sealed_result")) is not dict:
        raise state.OperationError("niagara_sprite_preset_async_final_not_sealed", "Durable final result is required before typed final delivery")
    entry["cu_delivery_attempted"] = True
    entry["result_object"].set_editor_property("value", reflected_result(_wire_projection(entry)))
    # CompleteValue may itself call SetError and return False. Once entered,
    # no fallback completion is safe, even when its return/publication is uncertain.
    entry["cu_completion_call_entered"] = True
    if entry["result_object"].complete_value() is not True:
        raise state.OperationError("niagara_sprite_preset_async_delivery_failed", "CU completion refused; no retry or result mutation")
    entry["cu_completed"] = True


def _terminal(entry, native):
    global _LAST_TERMINAL
    if entry.get("finalization_attempted"):
        return
    result = entry["journal"].result
    _adopt(entry, native)
    _checkpoint_native(entry, native, force=True)
    result.update(success=native["success"], error_code=native.get("error_code", ""), error=native.get("error", ""))
    if native["success"] and (not native["change_state_known"] or not native["postcondition_passed"] or not native["data"]["operation_complete"]):
        state.fail(result, state.OperationError("niagara_sprite_preset_async_result_unverified", "Native success lacks terminal known preset/compile/save postconditions"))
    recovery = not native["change_state_known"]
    result["data"].update(native_terminal_observed=True, workflow_recovery_required=recovery,
        cancel_acknowledgement_is_not_shared_compiler_stop=True,
        final_receipt_sealed_before_delivery=True)
    try:
        _seal(entry, recovery)
        if recovery:
            _QUARANTINED[entry["run_id"]] = entry
        _deliver(entry)
    except Exception as error:
        _delivery_failed(entry, error)
    _LAST_TERMINAL = {"operation_id": entry["run_id"], "success": result["success"],
        "native_terminal_observed": True, "recovery_required": recovery,
        "delivery_error": entry.get("delivery_error", "")}
    _PENDING.pop(entry["run_id"], None)


def _preserve_snapshot_error(entry,error):
    result=entry['journal'].result
    if getattr(error,'native_snapshot_returned',False):result['data'].update(native_snapshot_returned=True,undecoded_native_raw_result=getattr(error,'native_raw_result',None))
    native=getattr(error,'native_decoded_result',None)
    if type(native) is dict and native.get('operation')==METHOD and native.get('data',{}).get('native_job_id')==entry.get('native_job_id'):
        # A typed same-job receipt may prove a save prefix even if its public terminal publication failed.
        # Keep the receipt as evidence, without treating it as a verified terminal or clearing quarantine.
        result['data']['same_job_snapshot_at_adapter_failure']=copy.deepcopy(native)
        result['save']['saved']=result['save'].get('saved') is True or native.get('saved') is True
        result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'])
        result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'])


def _uncertain(entry, error):
    if entry.get("finalization_attempted"):
        _delivery_failed(entry, error)
        return
    result = entry["journal"].result
    failure_stage = result["stage"]
    from . import niagara_sprite_preset_authoring
    result["data"]["legacy_Named_result"] = niagara_sprite_preset_authoring._legacy_exception(METHOD,error)
    _preserve_snapshot_error(entry,error)
    if getattr(error, "native_snapshot_returned", False):
        result["data"].update(native_snapshot_returned=True, undecoded_native_raw_result=getattr(error, "native_raw_result", None))
    last = entry.get("last_native")
    terminal = last is not None and last["data"]["native_job_terminal"]
    if not terminal and entry.get("job_reference_verified"):
        native = _cancel_once(entry, "Stop owned next writers after observer/journal failure: " + str(error))
        if native is not None:
            last = native
            terminal = native["data"]["native_job_terminal"]
    state.fail(result, error)  # Preserve the triggering failure; never promote late success.
    no_dispatch = not entry.get("native_start_call_entered")
    recovery = not no_dispatch and (not terminal or not last["change_state_known"])
    result["data"].update(adapter_failure_stage_before_cancel=failure_stage,
        native_terminal_observed=terminal, workflow_recovery_required=recovery,
        last_returned_native_prefix_retained=True)
    if no_dispatch:
        result["data"].update(native_start_dispatched=False, mutation_state_unknown=False, mutation_performed=False)
        result["compile"].update(requested=False, attempted=False, compiled=None)
        result["save"].update(requested=False, attempted=False, saved=False)
    elif not terminal:
        result["data"].update(mutation_state_unknown=True, mutation_performed=None)
        for change in result["changes"]:
            change.update(change_state_known=False, changed=None)
    try:
        entry["journal"].checkpoint()
        if no_dispatch or terminal:
            _seal(entry, recovery)
    except Exception as checkpoint_error:
        if entry.get("finalization_attempted"):
            _delivery_failed(entry, checkpoint_error)
            return
        result["data"].update(checkpoint_error=str(checkpoint_error)[:700], workflow_recovery_required=True)
        recovery = True
    if recovery:
        entry["observation_only_quarantine"] = entry.get("job_reference_verified") and not terminal
        _QUARANTINED[entry["run_id"]] = entry
    _PENDING.pop(entry["run_id"], None)
    try:
        if no_dispatch or terminal:_deliver(entry)
        else:
            entry["cu_delivery_attempted"]=True
            entry["cu_error_attempted"]=True
            entry["result_object"].set_error(_delivery_error(entry,"Native terminal is unverified; started journal and ownership retained, no retry"))
    except Exception as delivery_error:
        _delivery_failed(entry, delivery_error)


def _observe_quarantined(entry):
    if entry.get("finalization_attempted"):
        return
    # If the one owned cancellation could not certify terminal, retain only read
    # observation. No second cancel, writers, saves or second CU delivery occurs.
    try:
        entry["snapshot_calls"] += 1
        native = api.snapshot(entry["job"])
        failure = {k: entry["journal"].result[k] for k in ("success", "error_code", "error")}
        _adopt(entry, native)
        entry["journal"].result.update(failure)
        entry["journal"].result["data"].update(workflow_recovery_required=True,
            quarantine_later_native_terminal=native["data"]["native_job_terminal"])
        revision = native["data"].get("native_checkpoint_revision")
        if ("quarantine_checkpoint_attempt_revision" not in entry
                or entry["quarantine_checkpoint_attempt_revision"] != revision):
            # A failed durable write is not retried each frame for the same
            # observed revision. A later actual phase/terminal prefix may be recorded.
            entry["quarantine_checkpoint_attempt_revision"] = revision
            _checkpoint_native(entry, native)
        if native["data"]["native_job_terminal"]:
            entry["observation_only_quarantine"] = False
    except Exception as error:
        _preserve_snapshot_error(entry,error)
        entry["last_quarantine_observation_error"] = str(error)[:700]


def _tick_all(delta_seconds):
    if not _ACTIVE:
        return False
    for entry in tuple(_PENDING.values()):
        try:
            entry["tick_callbacks"] += 1
            entry["snapshot_calls"] += 1
            native = api.snapshot(entry["job"])
            if native["data"]["native_job_terminal"]:
                _terminal(entry, native)
            else:
                _adopt(entry, native)
                _checkpoint_native(entry, native)
        except Exception as error:
            _uncertain(entry, error)
    for entry in tuple(_QUARANTINED.values()):
        if entry.get("observation_only_quarantine"):
            _observe_quarantined(entry)
    return True


def _shutdown():
    # Native module owns teardown. No late asset/job/save/cleanup method here.
    global _ACTIVE
    _ACTIVE = False
    for entry in tuple(_PENDING.values()):
        if entry.get("finalization_attempted"):
            continue
        result = entry["journal"].result
        state.fail(result, state.OperationError("niagara_sprite_preset_async_shutdown_unknown", "Shutdown before verified Native terminal; no late work attempted"))
        result["data"].update(mutation_state_unknown=True, mutation_performed=None,
            native_terminal_observed=False, workflow_recovery_required=True)
        try:
            entry["journal"].checkpoint()
        except Exception:
            pass  # Started journal remains; no repeated write or Engine work.
        _QUARANTINED[entry["run_id"]] = entry
        _PENDING.pop(entry["run_id"], None)


def binding_status():
    return {"active": _ACTIVE, "pending_count": len(_PENDING), "quarantined_count": len(_QUARANTINED),
        "ticker_handle_present": _TICKER_HANDLE is not None, "shutdown_handle_present": _SHUTDOWN_HANDLE is not None,
        "last_terminal": copy.deepcopy(_LAST_TERMINAL), "lifecycle_errors": list(_LIFECYCLE_ERRORS),
        "quarantined_operation_ids": sorted(_QUARANTINED),
        "quarantined_journal_paths": {run_id: str(entry["journal"].path) for run_id, entry in _QUARANTINED.items()},
        "observation_only_quarantine_count": sum(bool(entry.get("observation_only_quarantine")) for entry in _QUARANTINED.values()),
        "native_handle_membership_verified": False, "ordinary_async_Batch_integration_verified": False, "Native_bindings": api.binding_status()}


def activate():
    global _ACTIVE, _TICKER_HANDLE, _SHUTDOWN_HANDLE
    status=api.binding_status()
    if not all(value is True for value in status.values()):raise state.OperationError("niagara_sprite_preset_async_native_jobs_unavailable","Exact dedicated Native start/Job bindings are required")
    if not _ACTIVE:
        if _PENDING or _QUARANTINED or _TICKER_HANDLE is not None or _SHUTDOWN_HANDLE is not None:
            raise state.OperationError("niagara_sprite_preset_async_ownership_retained", "Prior retained ownership requires Root reconciliation")
        _SHUTDOWN_HANDLE = unreal.register_python_shutdown_callback(_shutdown)
        try:
            _TICKER_HANDLE = unreal.register_ticker_callback(_tick_all, delay=0.0)
        except Exception:
            unreal.unregister_python_shutdown_callback(_SHUTDOWN_HANDLE)
            _SHUTDOWN_HANDLE = None
            raise
        _ACTIVE = True
    return binding_status()


def deactivate(reason="Sprite preset toolset deactivated"):
    global _ACTIVE, _TICKER_HANDLE, _SHUTDOWN_HANDLE
    _ACTIVE = False
    for name, handle, remover in (("ticker", _TICKER_HANDLE, unreal.unregister_ticker_callback),
            ("shutdown", _SHUTDOWN_HANDLE, unreal.unregister_python_shutdown_callback)):
        if handle is not None:
            try:
                remover(handle)
                if name == "ticker":
                    _TICKER_HANDLE = None
                else:
                    _SHUTDOWN_HANDLE = None
            except Exception as error:
                _LIFECYCLE_ERRORS.append({"handle": name, "error": str(error)[:700]})
    for entry in tuple(_PENDING.values()):
        native = _cancel_once(entry, reason)
        if native is not None and native["data"]["native_job_terminal"]:
            try:
                _terminal(entry, native)
            except Exception as error:
                _uncertain(entry, error)
        else:
            _uncertain(entry, state.OperationError("niagara_sprite_preset_async_cancel_terminal_unverified", "Owned cancellation not terminal verified; no repeat"))
    return binding_status()


def begin(public):
    method=METHOD
    result_object = CubelessPythonAsyncOperationResult()
    run_id = uuid.uuid4().hex
    journal = None
    entry = None
    try:
        arguments = _arguments(method, public)
        if not _ACTIVE:
            raise state.OperationError("niagara_sprite_preset_async_domain_inactive", "Root registration has not activated the Sprite preset domain")
        if _QUARANTINED:
            raise state.OperationError("niagara_sprite_preset_async_workflow_quarantined", "Retained prior Sprite preset outcome requires Root reconciliation")
        journal = _SpritePresetJournal(method, arguments, run_id)
        entry = {"run_id": run_id, "method": method, "public": arguments, "journal": journal,
            "result_object": result_object, "job": None, "tick_callbacks": 0, "snapshot_calls": 0,
            "native_start_call_entered": False, "job_reference_verified": False,
            "cancel_attempted": False, "last_checkpoint_revision": None}
        _PENDING[run_id] = entry
        journal.result["data"].update(native_start_dispatched=True, native_mutation_attempted=None,
            mutation_performed=None, mutation_state_unknown=True)
        journal.result["compile"].update(requested=None, attempted=None, compiled=None, status="native_start_outcome_unverified")
        journal.result["save"].update(requested=None, attempted=None, saved=None, status="native_start_outcome_unverified")
        journal.checkpoint()  # Covers all future Native phases BEFORE start.
        entry["native_start_call_entered"] = True
        entry["job"] = api.start_job(arguments["args"]["params"])
        entry["job_reference_verified"] = True
        journal.result["data"].update(native_start_returned=True)
        entry["snapshot_calls"] += 1
        native = api.snapshot(entry["job"])
        if native["data"]["native_job_terminal"]:
            _terminal(entry, native)
        else:
            _adopt(entry, native)
            _checkpoint_native(entry, native)
    except Exception as error:
        if entry is not None:
            if getattr(error, "native_start_returned", False):
                entry["job"] = getattr(error, "native_returned_job", None)
                journal.result["data"]["native_start_returned"] = True
            if getattr(error, "native_start_dispatched", None) is False:
                entry["native_start_call_entered"] = False
                journal.result["data"].update(native_start_dispatched=False, mutation_state_unknown=False, mutation_performed=False)
                journal.result["compile"].update(requested=False, attempted=False, compiled=None)
                journal.result["save"].update(requested=False, attempted=False, saved=False)
                from . import niagara_sprite_preset_authoring
                journal.result["data"]["legacy_Named_result"] = niagara_sprite_preset_authoring._legacy_exception(METHOD,error)
                state.fail(journal.result, error)
                try:
                    _seal(entry, False)
                    _deliver(entry)
                except Exception as delivery_error:
                    _uncertain(entry, delivery_error)
                _PENDING.pop(run_id, None)
            else:
                _uncertain(entry, error)
        else:
            from . import niagara_sprite_preset_authoring
            record = niagara_sprite_preset_authoring._rejected(METHOD,error);record["operation_id"]=run_id
            record["data"]["legacy_Named_result"] = copy.deepcopy(getattr(error,"legacy_result",None if hasattr(error,"public_validation_errors") else niagara_sprite_preset_authoring._legacy_exception(METHOD,error)))
            record["data"].update(operation=method, admission_rejected=True, native_start_dispatched=False,
                native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result_object.set_editor_property("value", reflected_result(record))
            if result_object.complete_value() is not True:
                result_object.set_error("Sprite preset admission failed and CU completion refused; operation_id=" + run_id
                    + "; stage=" + str(record["stage"]) + "; journal=" + str(state._journal_path(run_id)))
    return result_object


def completed_result(record):
    """Typed async final for pre-dispatch admission rejection only; no journal or Native job."""
    value=CubelessPythonAsyncOperationResult();value.set_editor_property('value',reflected_result(copy.deepcopy(record)))
    if value.complete_value() is not True:value.set_error('GU3 pre-dispatch completion refused; no Native job was dispatched')
    return value
