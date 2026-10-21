"""Exclusive asynchronous ownership for only the two fixed AI live workflows.

Reuse the existing durable journal path/write/checkpoint format and mutation
slot. Do not call synchronous mutate around a pending UObject. Engine domain
is separate and does not import this journal or any transport/registry module.
"""
import copy
import hashlib
import json
import os
import time
from . import state

NAMES = ("run_ai_dummy_patrol_chase_smoke", "run_ai_behavior_tree_execution_smoke")


class LiveJournal:
    def __init__(self, name, arguments, run_id):
        if name not in NAMES:
            raise state.OperationError("ai_live_operation_invalid", "Only the two original fixed live workflows are admitted")
        self.result = state.new_result(run_id)
        self.result["data"].update(operation=name, asynchronous=True, mutation_state_unknown=False,
            automatic_retry=False, client_disconnect_cancellation_verified=False)
        self.path = state._journal_path(run_id)
        self.owned = False
        self.started = time.perf_counter()
        self.record = None
        if self.path.exists():
            raise state.OperationError("operation_incomplete", "Live workflow IDs are never rerun; inspect the existing journal")
        if state._active_operation is not None:
            raise state.OperationError("mutation_busy", "Another mutation/live workflow owns the project slot: " + str(state._active_operation))
        fingerprint = hashlib.sha256(json.dumps({"name": name, "arguments": arguments}, sort_keys=True,
            allow_nan=False).encode("utf-8")).hexdigest()
        self.record = {"schema_version": 1, "operation_id": run_id, "operation": name,
            "arguments": copy.deepcopy(arguments), "fingerprint": fingerprint, "state": "started",
            "result": self.result, "engine_version": state.unreal.SystemLibrary.get_engine_version(),
            "process_id": os.getpid()}
        encoded = json.dumps(self.record, ensure_ascii=False, sort_keys=True, allow_nan=False)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("x", encoding="utf-8") as stream:
            stream.write(encoded); stream.flush(); os.fsync(stream.fileno())
        state._active_operation = run_id
        self.owned = True

    def observe(self, report, stage, *, intent=False, returned=False):
        if not self.owned or state._active_operation != self.result["operation_id"]:
            raise state.OperationError("journal_checkpoint_not_owned", "Live ownership changed; no further Engine mutation is permitted")
        self.result["stage"] = stage
        self.result["data"].update(legacy_report=copy.deepcopy(report), mutation_state_unknown=bool(report.get("outcome_unknown")),
            mutation_attempted=bool(report.get("mutation_attempted")), mutation_performed=report.get("mutation_performed"),
            workflow_recovery_required=bool(report.get("workflow_recovery_required")))
        self.result["changes"] = copy.deepcopy(report.get("changes", []))
        if intent:
            self.result["data"]["pending_engine_intent"] = {"stage": stage,
                "dispatch_observation": "durable_intent_before_call; later_dispatch_and_return_unknown_until_checkpoint"}
        if returned:
            self.result["data"]["last_returned_engine_stage"] = stage
            self.result["data"].pop("pending_engine_intent", None)
        # The existing checkpoint validates started state, process and the
        # active slot before writing one flushed atomic filesystem replacement.
        state.checkpoint(self.result)

    def finish(self, report, report_path):
        """No gameplay call; finalize only after terminal lifecycle observation.

        Unknown session/cleanup/journal state stays started and owns the slot.
        Public terminal failure may be delivered while that quarantine remains.
        """
        report["report_path"] = str(report_path)
        report["workflow_finished"] = True
        self.observe(report, "terminal_observed", returned=True)
        if not self.owned:
            raise state.OperationError("journal_finalize_not_owned", "Live journal was not admitted")
        recovery = bool(report.get("outcome_unknown") or report.get("workflow_recovery_required"))
        self.result.update(success=bool(report["success"]), stage="quarantined" if recovery else "complete",
            error_code=report.get("error_code", ""), error=report.get("reason", ""))
        self.result["data"].update(legacy_report=copy.deepcopy(report), elapsed_seconds=round(time.perf_counter() - self.started, 6),
            engine_terminal_observed=True, client_delivery_observed=False)
        self.result["validation"]["status"] = "fixed_live_criteria_and_lifecycle" if report["success"] else "live_failed_or_recovery_required"
        self.result["retry"]["duplicate_risk"] = "live_world_or_lifecycle_would_repeat"
        try:
            # Final report is also bounded to the selected Saved report root.
            # Existing _write_record retries only flushed-temp Windows rename
            # contention; no Engine call, JSON body or temp creation is retried.
            state._write_record(report_path, report)
            with self.path.open("r", encoding="utf-8") as stream:
                final_record = json.load(stream)
            if final_record.get("operation_id") != self.result["operation_id"] or final_record.get("state") != "started" or final_record.get("process_id") != os.getpid():
                raise state.OperationError("journal_finalize_not_owned", "Durable live journal ownership changed before completion")
            final_record["result"] = copy.deepcopy(self.result)
            final_record["state"] = "started" if recovery else "complete"
            state._write_record(self.path, final_record)
            self.record = final_record
        except Exception as error:
            report.update(success=False, status="BLOCKED", workflow_recovery_required=True,
                error_code="journal_finalize_failed", reason="Engine workflow ended but durable completion failed: " + str(error)[:700])
            state.fail(self.result, state.OperationError("journal_finalize_failed", report["reason"]))
            self.result["retry"]["duplicate_risk"] = "uncertain_journal_completion"
            # Keep ownership, prior journal and flushed temp evidence. Do not
            # repeat writes or gameplay from this exception path.
            raise
        if not recovery:
            state._active_operation = None
            self.owned = False

    def shutdown(self, report):
        report.update(success=False, status="BLOCKED", outcome_unknown=True, workflow_recovery_required=True,
            reason="Python shutdown before verified terminal lifecycle; no late world cleanup was attempted",
            error_code="ai_live_shutdown_unknown")
        try:
            self.observe(report, "python_shutdown")
        except Exception:
            pass  # Existing started journal remains authoritative; no retries.
