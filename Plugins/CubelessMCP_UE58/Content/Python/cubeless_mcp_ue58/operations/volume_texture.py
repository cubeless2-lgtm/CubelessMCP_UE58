"""Held DX ownership until actual Native texture-job terminal observation.

Only the original sheet/raw workflows are admitted. Native owns file/asset work,
completion and conditional save order. Existing state storage and CU completion
are reused; no synchronous pending finalization, blocking wait, asset retry or
shared texture-compiler cancellation is introduced.
"""
import copy
import hashlib
import json
import os
import time
import uuid
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import volume_texture as api
from cubeless_mcp_ue58.registration.async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.registration.results import reflected_result
from . import state

PUBLIC_FIELDS_BY_METHOD = {'create_volume_texture_from_2d_sheet_mcp': ('source_texture_path', 'target_volume_texture_path', 'tile_size_x', 'tile_size_y', 'dry_run', 'overwrite_existing', 'save_asset', 'srgb', 'compression_settings', 'mip_gen_settings', 'never_stream', 'virtual_texture_streaming', 'address_mode'), 'create_volume_texture_from_raw_rgba16_sheet_mcp': ('raw_file_path', 'source_texture_path', 'target_volume_texture_path', 'sheet_size_x', 'sheet_size_y', 'tile_size_x', 'tile_size_y', 'dry_run', 'overwrite_existing', 'save_assets', 'srgb', 'source_compression_settings', 'volume_compression_settings', 'source_mip_gen_settings', 'volume_mip_gen_settings', 'never_stream_volume', 'virtual_texture_streaming', 'address_mode')}
DEFAULTS_BY_METHOD = {'create_volume_texture_from_2d_sheet_mcp': {'tile_size_x': 128, 'tile_size_y': 128, 'dry_run': False, 'overwrite_existing': False, 'save_asset': True, 'srgb': None, 'compression_settings': 'TC_VectorDisplacementmap', 'mip_gen_settings': 'TMGS_NoMipmaps', 'never_stream': True, 'virtual_texture_streaming': False, 'address_mode': 'TA_Wrap'}, 'create_volume_texture_from_raw_rgba16_sheet_mcp': {'sheet_size_x': 2048, 'sheet_size_y': 1024, 'tile_size_x': 128, 'tile_size_y': 128, 'dry_run': False, 'overwrite_existing': False, 'save_assets': True, 'srgb': False, 'source_compression_settings': 'TC_Default', 'volume_compression_settings': 'TC_VectorDisplacementmap', 'source_mip_gen_settings': 'TMGS_FromTextureGroup', 'volume_mip_gen_settings': 'TMGS_NoMipmaps', 'never_stream_volume': True, 'virtual_texture_streaming': False, 'address_mode': 'TA_Wrap'}}
FIELD_KINDS_BY_METHOD = {'create_volume_texture_from_2d_sheet_mcp': {'source_texture_path': 'string', 'target_volume_texture_path': 'string', 'tile_size_x': 'integer', 'tile_size_y': 'integer', 'dry_run': 'boolean', 'overwrite_existing': 'boolean', 'save_asset': 'boolean', 'srgb': 'nullable_boolean', 'compression_settings': 'string', 'mip_gen_settings': 'string', 'never_stream': 'boolean', 'virtual_texture_streaming': 'boolean', 'address_mode': 'string'}, 'create_volume_texture_from_raw_rgba16_sheet_mcp': {'raw_file_path': 'string', 'source_texture_path': 'string', 'target_volume_texture_path': 'string', 'sheet_size_x': 'integer', 'sheet_size_y': 'integer', 'tile_size_x': 'integer', 'tile_size_y': 'integer', 'dry_run': 'boolean', 'overwrite_existing': 'boolean', 'save_assets': 'boolean', 'srgb': 'boolean', 'source_compression_settings': 'string', 'volume_compression_settings': 'string', 'source_mip_gen_settings': 'string', 'volume_mip_gen_settings': 'string', 'never_stream_volume': 'boolean', 'virtual_texture_streaming': 'boolean', 'address_mode': 'string'}}
_ACTIVE = False
_TICKER_HANDLE = None
_SHUTDOWN_HANDLE = None
_PENDING = {}
_QUARANTINED = {}
_LAST_TERMINAL = {}
_LIFECYCLE_ERRORS = []


def _arguments(method, public):
    if method not in api.BINDINGS or set(public) != set(PUBLIC_FIELDS_BY_METHOD[method]):
        raise state.OperationError("volume_texture_input_invalid", "Exact original public13/18 fields required")
    ordered = {name: copy.deepcopy(public[name]) for name in PUBLIC_FIELDS_BY_METHOD[method]}
    for name, value in ordered.items():
        kind = FIELD_KINDS_BY_METHOD[method][name]
        valid = (type(value) is str if kind == "string" else type(value) is int if kind == "integer"
            else type(value) is bool if kind == "boolean" else value is None or type(value) is bool)
        if not valid:
            raise state.OperationError("volume_texture_input_invalid", "Original public field type differs: " + name)
    return ordered  # Native owns path/enum/dimension/file/texture admission.


def _context(result, method, public):
    result["data"].update(operation=method, original_public_arguments=copy.deepcopy(public),
        original_public_input_count=len(PUBLIC_FIELDS_BY_METHOD[method]),
        public_srgb_None_is_omission=method == api.BINDINGS[0] and public["srgb"] is None,
        async_native_job=True, original_conditional_save_order_native_authoritative=True,
        no_Python_raw_texture_file_reads_asset_edits_build_or_package_save_calls=True,
        automatic_action_retry=False, global_texture_compiler_cancel_attempted=False,
        client_disconnect_cancellation_verified=False, actual_official_async_wire_verified=False,
        full_volume_sampling_cold_cook_or_reactive_fog_pipeline_verified=False)


class _TextureJournal:
    def __init__(self, method, public, run_id):
        self.result = state.new_result(run_id)
        _context(self.result, method, public)
        self.result["stage"] = "before_native_volume_texture_start"
        self.result["data"].update(native_start_dispatched=False, native_start_returned=False,
            native_snapshot_returned=False, mutation_performed=False, mutation_state_unknown=False,
            pending_native_intent={"method": method, "arguments": copy.deepcopy(public),
                "scope": "future native phases unverified until returned snapshots"})
        self.path = state._journal_path(run_id)
        self.started = time.perf_counter()
        self.owned = False
        if self.path.exists():
            raise state.OperationError("operation_incomplete", "Texture IDs are not replayed; inspect the journal")
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
            raise state.OperationError("journal_checkpoint_not_owned", "Texture ownership changed; no replay")
        state.checkpoint(self.result)

    def finish(self, recovery):
        if not self.owned or state._active_operation != self.result["operation_id"]:
            raise state.OperationError("journal_finalize_not_owned", "Texture ownership changed before finalization")
        with self.path.open("r", encoding="utf-8") as stream:
            record = json.load(stream)
        if (record.get("operation_id") != self.result["operation_id"] or record.get("state") != "started"
                or record.get("process_id") != os.getpid()):
            raise state.OperationError("journal_finalize_not_owned", "Texture started journal ownership differs")
        self.result["data"]["elapsed_seconds"] = round(time.perf_counter() - self.started, 6)
        record["result"] = copy.deepcopy(self.result)
        record["state"] = "started" if recovery else "complete"
        state._write_record(self.path, record)
        if not recovery:
            state._active_operation = None
            self.owned = False


def _adopt(entry, native):
    result = entry["journal"].result
    known = native["change_state_known"]
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_raw_result=native["_raw_native_json"], native_snapshot_returned=True,
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=(native["changed"] if known else None) if native["mutation_attempted"] else False,
        observed_state_changed=native["changed"] if known else None,
        adapter_snapshot_calls=entry["snapshot_calls"], adapter_tick_callbacks=entry["tick_callbacks"])
    _context(result, entry["method"], entry["public"])
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"], status="actual_native_texture_job_prefix")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"], status="actual_native_source_volume_save_prefix")
    result["changes"] = [] if known and native["changed"] is False and not native["mutation_attempted"] else [{
        "kind": entry["method"], "source_texture_path": entry["public"]["source_texture_path"],
        "target_volume_texture_path": entry["public"]["target_volume_texture_path"],
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native["changed"] if known else None, "postcondition_passed": native["postcondition_passed"]}]
    result["stage"] = native["stage"]
    result["validation"].update(status="actual_native_texture_job_prefix", native_postcondition_passed=native["postcondition_passed"],
        native_terminal_observed=native["data"]["native_job_terminal"], disk_reload="not_run", runtime_sampling="not_run", reactive_fog_pipeline="not_run")
    result["retry"]["duplicate_risk"] = "inspect_texture_job_snapshots_and_held_journal_no_replay"
    entry["last_native"] = copy.deepcopy(native)


def _checkpoint_native(entry, native, force=False):
    revision = native["data"].get("native_checkpoint_revision")
    if type(revision) is not int or revision < 0:
        raise state.OperationError("volume_texture_revision_invalid", "Native meaningful snapshot revision required")
    previous = entry.get("last_checkpoint_revision")
    if previous is not None and revision < previous:
        raise state.OperationError("volume_texture_revision_regressed", "Native snapshot revision regressed; no replay")
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
        native = api.snapshot(entry["job"], entry["method"])
        _adopt(entry, native)
        return native
    except Exception as error:
        if getattr(error, "native_snapshot_returned", False):
            result["data"]["cancel_snapshot_undecoded_raw_result"] = getattr(error, "native_raw_result", None)
        result["data"]["cancel_snapshot_error"] = str(error)[:700]
        return None


def _deliver(entry):
    if entry.get("cu_delivery_attempted"):
        raise state.OperationError("volume_texture_async_delivery_already_attempted", "CU delivery already attempted; retain journal without completing again")
    entry["cu_delivery_attempted"] = True
    result_object = entry["result_object"]
    result_object.set_editor_property("value", reflected_result(entry["journal"].result))
    if result_object.complete_value() is not True:
        raise state.OperationError("volume_texture_async_delivery_failed", "CU completion refused; retain result/journal, no new work")


def _terminal(entry, native):
    global _LAST_TERMINAL
    result = entry["journal"].result
    _adopt(entry, native)  # Actual raw/compile/save prefixes precede criteria/errors.
    _checkpoint_native(entry, native, force=True)
    result.update(success=native["success"], error_code=native.get("error_code", ""), error=native.get("error", ""))
    if native["success"] and (not native["change_state_known"] or not native["postcondition_passed"]):
        state.fail(result, state.OperationError("volume_texture_result_unverified", "Native success lacks known current postconditions"))
    recovery = not native["change_state_known"]
    result["data"].update(native_terminal_observed=True, workflow_recovery_required=recovery,
        cancel_acknowledgement_is_not_shared_compiler_stop=True)
    try:
        entry["journal"].finish(recovery)
    except Exception as error:
        state.fail(result, state.OperationError("journal_finalize_failed", "Native terminal observed but journal finalization failed: " + str(error)[:700]))
        result["data"]["workflow_recovery_required"] = True
        recovery = True
    if recovery:
        _QUARANTINED[entry["run_id"]] = entry
    try:
        _deliver(entry)
    except Exception as error:
        entry["delivery_error"] = str(error)[:1024]
        _QUARANTINED[entry["run_id"]] = entry
        entry["result_object"].set_error(_delivery_error(entry, "Native terminal observed but typed completion failed: " + str(error)))
    _LAST_TERMINAL = {"operation_id": entry["run_id"], "success": result["success"],
        "native_terminal_observed": True, "recovery_required": recovery, "delivery_error": entry.get("delivery_error", "")}
    _PENDING.pop(entry["run_id"], None)


def _uncertain(entry, error):
    result = entry["journal"].result
    failure_stage = result["stage"]
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
            entry["journal"].finish(recovery)
    except Exception as checkpoint_error:
        result["data"].update(checkpoint_error=str(checkpoint_error)[:700], workflow_recovery_required=True)
        recovery = True
    if recovery:
        entry["observation_only_quarantine"] = entry.get("job_reference_verified") and not terminal
        _QUARANTINED[entry["run_id"]] = entry
    _PENDING.pop(entry["run_id"], None)
    try:
        _deliver(entry)
    except Exception as delivery_error:
        entry["delivery_error"] = str(delivery_error)[:700]
        _QUARANTINED[entry["run_id"]] = entry
        entry["result_object"].set_error(_delivery_error(entry, "Texture outcome or durable delivery unknown; no retry"))


def _observe_quarantined(entry):
    # If the one owned cancellation could not certify terminal, retain only read
    # observation. No second cancel, writers, saves or second CU delivery occurs.
    try:
        entry["snapshot_calls"] += 1
        native = api.snapshot(entry["job"], entry["method"])
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
        entry["last_quarantine_observation_error"] = str(error)[:700]


def _tick_all(delta_seconds):
    if not _ACTIVE:
        return False
    for entry in tuple(_PENDING.values()):
        try:
            entry["tick_callbacks"] += 1
            entry["snapshot_calls"] += 1
            native = api.snapshot(entry["job"], entry["method"])
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
        result = entry["journal"].result
        state.fail(result, state.OperationError("volume_texture_shutdown_unknown", "Shutdown before verified Native terminal; no late work attempted"))
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
        "native_handle_membership_verified": False, "ordinary_async_Batch_integration_verified": False}


def activate():
    global _ACTIVE, _TICKER_HANDLE, _SHUTDOWN_HANDLE
    if not api.jobs_status()["active"]:
        raise state.OperationError("volume_texture_native_jobs_inactive", "Root Module must activate Native jobs first")
    if not _ACTIVE:
        if _PENDING or _QUARANTINED or _TICKER_HANDLE is not None or _SHUTDOWN_HANDLE is not None:
            raise state.OperationError("volume_texture_ownership_retained", "Prior retained ownership requires Root reconciliation")
        _SHUTDOWN_HANDLE = unreal.register_python_shutdown_callback(_shutdown)
        try:
            _TICKER_HANDLE = unreal.register_ticker_callback(_tick_all, delay=0.0)
        except Exception:
            unreal.unregister_python_shutdown_callback(_SHUTDOWN_HANDLE)
            _SHUTDOWN_HANDLE = None
            raise
        _ACTIVE = True
    return binding_status()


def deactivate(reason="Texture toolset deactivated"):
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
            _uncertain(entry, state.OperationError("volume_texture_cancel_terminal_unverified", "Owned cancellation not terminal verified; no repeat"))
    return binding_status()


def begin(method, public):
    result_object = CubelessPythonAsyncOperationResult()
    run_id = uuid.uuid4().hex
    journal = None
    entry = None
    try:
        arguments = _arguments(method, public)
        if not _ACTIVE:
            raise state.OperationError("volume_texture_domain_inactive", "Root registration has not activated the texture domain")
        if _QUARANTINED:
            raise state.OperationError("volume_texture_workflow_quarantined", "Retained prior texture outcome requires Root reconciliation")
        journal = _TextureJournal(method, arguments, run_id)
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
        entry["job"] = api.start_job(method, arguments)
        entry["job_reference_verified"] = True
        journal.result["data"].update(native_start_returned=True)
        entry["snapshot_calls"] += 1
        native = api.snapshot(entry["job"], method)
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
            if getattr(error, "code", "") == "volume_texture_native_api_unavailable":
                entry["native_start_call_entered"] = False
                journal.result["data"].update(native_start_dispatched=False, mutation_state_unknown=False, mutation_performed=False)
                journal.result["compile"].update(requested=False, attempted=False, compiled=None)
                journal.result["save"].update(requested=False, attempted=False, saved=False)
                state.fail(journal.result, error)
                try:
                    journal.finish(False)
                    _deliver(entry)
                except Exception as delivery_error:
                    _uncertain(entry, delivery_error)
                _PENDING.pop(run_id, None)
            else:
                _uncertain(entry, error)
        else:
            record = state.fail(state.new_result(run_id), error)
            record["data"].update(operation=method, admission_rejected=True, native_start_dispatched=False,
                native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            result_object.set_editor_property("value", reflected_result(record))
            if result_object.complete_value() is not True:
                result_object.set_error("Texture admission failed and CU completion refused; operation_id=" + run_id
                    + "; stage=" + str(record["stage"]) + "; journal=" + str(state._journal_path(run_id)))
    return result_object
