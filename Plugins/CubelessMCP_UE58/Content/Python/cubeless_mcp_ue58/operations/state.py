"""Durable mutation journal. Interrupted requests are blocked, never retried."""

import copy
import hashlib
import json
import os
from pathlib import Path
import re
import time
import uuid

import unreal

_active_operation = None
_ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]{0,95}$")


class OperationError(RuntimeError):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def new_result(operation_id=""):
    return {"success": False, "operation_id": operation_id or str(uuid.uuid4()),
            "stage": "preflight", "error_code": "", "error": "", "changes": [],
            "compile": {"requested": False, "attempted": False, "status": "not_requested"},
            "save": {"requested": False, "attempted": False, "saved": False},
            "validation": {"status": "not_run", "disk_reload": "not_run",
                           "complete_dependency_scan": "not_run", "packaged_runtime": "not_run"},
            "retry": {"automatic_retry": False, "duplicate_risk": "none_observed", "atomic_rollback": False,
                      "policy": "Inspect the recorded result before issuing another mutation.", "replayed": False},
            "data": {}}


def fail(result, error):
    result["success"] = False
    result["error_code"] = getattr(error, "code", "python_operation_failed")
    result["error"] = str(error)
    if result["changes"]:
        result["retry"]["duplicate_risk"] = "partial_or_attempted_changes"
    return result


def _journal_path(operation_id):
    if not _ID_PATTERN.fullmatch(operation_id):
        raise OperationError("invalid_operation_id", "operation_id must be 1-96 letters, digits, underscore, dash or dot, starting with a letter/digit")
    return Path(unreal.Paths.project_saved_dir()).resolve() / "CubelessMCP_UE58" / "Operations" / (operation_id + ".json")


def get_operation(operation_id):
    path = _journal_path(operation_id)
    if not path.is_file():
        return {"operation_id": operation_id, "state": "unknown", "automatic_retry": False,
                "reason": "No journal in this project; inspect assets and connection target before retry."}
    with path.open("r", encoding="utf-8") as stream:
        record = json.load(stream)
    if record.get("state") == "started":
        # A stored stage describes only that checkpoint, never the actual later
        # failure/completion stage. Old journals have just the initial snapshot.
        checkpoint_info = record.get("checkpoint", {})
        has_checkpoint = isinstance(checkpoint_info, dict) and checkpoint_info.get("sequence", 0) > 0
        record["journal_observation"] = {
            "recorded_result_scope": "last_durable_checkpoint" if has_checkpoint else "last_initial_checkpoint",
            "checkpoint_stage": record.get("result", {}).get("stage"),
            "checkpoint_sequence": checkpoint_info.get("sequence", 0) if has_checkpoint else 0,
            "recorded_stage_is_actual_failure_stage": False,
            "actual_failure_stage": "unknown",
            "actual_changes": "unknown",
            "actual_completion": "unknown",
            "automatic_retry": False,
            "meaning": "The request started without a durable completion record. The stored result describes only the last durable checkpoint, not the actual failure stage or a claim that no later changes occurred. Inspect the editor, assets and available evidence; the operation may still be running or may have stopped.",
        }
    return record


def _write_record(path, record):
    # Encode before touching a file. This uses json's fast complete encoder and
    # avoids thousands of small text writes without changing bytes/durability.
    encoded = json.dumps(record, ensure_ascii=False, sort_keys=True)
    temporary = path.with_name(path.name + "." + str(uuid.uuid4()) + ".tmp")
    with temporary.open("x", encoding="utf-8") as stream:
        stream.write(encoded)
        stream.flush()
        os.fsync(stream.fileno())
    # Retry only Windows sharing/access contention for this already flushed
    # temp file. The engine action, JSON payload and temp creation are never
    # repeated. Exhaustion retains the prior journal and the same temp evidence.
    retry_delays = (0.01, 0.02, 0.04, 0.08, 0.16)  # 0.31 s total sleep, six calls.
    for attempt in range(len(retry_delays) + 1):
        try:
            os.replace(temporary, path)
            return
        except OSError as error:
            if getattr(error, "winerror", None) not in (5, 32) or attempt == len(retry_delays):
                raise
            time.sleep(retry_delays[attempt])


def checkpoint(result):
    """Persist an active action's snapshot before continuing; failure aborts it.

    This records observations at one instant. It does not make mutation atomic or
    permit retry after an interrupted request, even when changes was still empty.
    """
    operation_id = result.get("operation_id")
    if not operation_id or _active_operation != operation_id:
        raise OperationError("journal_checkpoint_not_owned", "Checkpoint requires the currently active mutation's operation ID")
    path = _journal_path(operation_id)
    try:
        with path.open("r", encoding="utf-8") as stream:
            record = json.load(stream)
        if record.get("operation_id") != operation_id or record.get("state") != "started" or record.get("process_id") != os.getpid():
            raise OperationError("journal_checkpoint_not_owned", "Active journal ownership/state changed; mutation must not continue")
        previous = record.get("checkpoint", {})
        sequence = previous.get("sequence", 0) + 1
        record["checkpoint"] = {"sequence": sequence, "captured_ns": time.time_ns(), "stage": result.get("stage"),
                                "scope": "observations_at_checkpoint_only", "later_outcome": "unknown_until_durable_completion"}
        record["result"] = copy.deepcopy(result)
        _write_record(path, record)
        return sequence
    except OperationError:
        raise
    except Exception as error:
        raise OperationError("journal_checkpoint_failed", "Checkpoint could not be persisted; action must stop before its next mutation: " + str(error)) from error


def mutate(name, arguments, operation_id, action):
    """Run one bounded game-thread mutation. No worker thread, retry or rollback."""
    global _active_operation
    result = new_result(operation_id)
    path = None
    record = None
    owns_journal = False
    owns_active_slot = False
    started = time.perf_counter()
    try:
        path = _journal_path(result["operation_id"])
        fingerprint = hashlib.sha256(json.dumps({"name": name, "arguments": arguments}, sort_keys=True).encode("utf-8")).hexdigest()
        if path.is_file():
            existing = get_operation(result["operation_id"])
            if existing.get("fingerprint") != fingerprint:
                raise OperationError("operation_id_conflict", "This operation_id already belongs to different arguments")
            if existing.get("state") == "complete":
                replay = copy.deepcopy(existing["result"])
                replay["retry"]["replayed"] = True
                return replay
            raise OperationError("operation_incomplete", "A previous request may have changed assets; inspect its journal and assets. No automatic retry was performed")
        if _active_operation is not None:
            raise OperationError("mutation_busy", f"Another Cubeless Python mutation is active: {_active_operation}")
        path.parent.mkdir(parents=True, exist_ok=True)
        record = {"schema_version": 1, "operation_id": result["operation_id"], "operation": name,
                  "arguments": arguments, "fingerprint": fingerprint, "state": "started", "result": result,
                  "engine_version": unreal.SystemLibrary.get_engine_version(), "process_id": os.getpid()}
        # Serialization failure must precede exclusive creation and mutation.
        # The exclusive open still decides ownership if another process races.
        encoded = json.dumps(record, ensure_ascii=False, sort_keys=True)
        # Exclusive create prevents two editor processes from executing the same request ID.
        with path.open("x", encoding="utf-8") as stream:
            stream.write(encoded)
            stream.flush()
            os.fsync(stream.fileno())
        owns_journal = True
        _active_operation = result["operation_id"]
        owns_active_slot = True
        action(result)
    except Exception as error:
        fail(result, error)
    finally:
        if owns_active_slot:
            _active_operation = None
    if record is not None and owns_journal:
        result["data"]["elapsed_seconds"] = round(time.perf_counter() - started, 6)
        record["state"] = "complete"
        record["result"] = result
        try:
            _write_record(path, record)
        except Exception as error:
            # An uncertain outcome cannot be made replayable by reporting success.
            fail(result, OperationError("journal_finalize_failed", f"Editor operation ran but journal completion failed: {error}"))
            result["retry"]["duplicate_risk"] = "uncertain_journal_completion"
    return result
