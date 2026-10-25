"""Offline, opt-in receipt storage for RecordedCalls; this module makes no requests.

The compact head is not a legacy report. Use materialize_report() when reading it.
Returned MCP documents (not HTTP wire bytes) and decoded values are frozen before
observers run. Checkpoints never serialize historical graph/result payloads.
"""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import time


STORAGE_SCHEMA = "cubeless_recorded_calls_storage_v2"
_SUMMARY_KEYS = (
    "schema", "status", "stage", "failed_stage", "error", "outcome_unknown",
    "started_ns", "finished_ns", "actual_counts", "current_run_counts",
    "lineage_counts", "performance", "engine_calls", "compile_attempts",
    "save_attempts", "explicit_compile_attempts", "explicit_save_attempts",
)


class StorageError(RuntimeError):
    """Persistence failure. A caller must stop dispatching, never retry a request."""
    def __init__(self, message, *, outcome_unknown=False):
        super().__init__(message)
        self.outcome_unknown = outcome_unknown


def _encode(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


def require_fresh_receipt_paths(output):
    """lstat sees occupied names even when a link's target does not exist."""
    output = Path(output)
    for path in (output, output.with_suffix(output.suffix + ".partial"),
                 output.with_name(output.name + ".evidence")):
        try:
            path.lstat()
        except FileNotFoundError:
            continue
        raise StorageError("Fresh receipt path required; existing entry refused: " + str(path))


def _atomic_head(path, value):
    temporary = path.with_suffix(path.suffix + ".partial")
    # Exclusive creation also rejects a dangling link inserted after admission.
    # Never truncate/follow an existing temporary entry or remove it on failure.
    with temporary.open("xb") as stream:
        stream.write(_encode(value))
        stream.flush()
        os.fsync(stream.fileno())
    delays = (0.025, 0.05, 0.1, 0.1, 0.1)
    for attempt in range(len(delays) + 1):
        try:
            temporary.replace(path)
            return
        except PermissionError as error:
            if (os.name != "nt" or getattr(error, "winerror", None) not in (5, 32)
                    or attempt == len(delays)):
                raise
            time.sleep(delays[attempt])


class ReceiptStorage:
    """One fresh run, exclusive immutable evidence directory, atomic small head.

    Active materialization contains the initial application snapshot, latest
    summary and all durable call receipts. Arbitrary derived top-level fields
    become authoritative only at finish(). In-memory reports are not bounded.
    """
    def __init__(self, output, report):
        self.output = Path(output)
        self.root = self.output.with_name(self.output.name + ".evidence")
        require_fresh_receipt_paths(self.output)
        if report.get("stages", []):
            raise StorageError("Storage v2 requires an initially empty stages list")
        self.output.parent.mkdir(parents=True, exist_ok=True)
        self.root.mkdir()
        (self.root / "blobs").mkdir()
        (self.root / "receipts").mkdir()
        self.entries = []
        self.terminal = None
        self.failed = False
        self.failed_outcome_unknown = False
        self._durable_responses = set()
        self._written = set()
        self.initial = self._blob(dict(report))
        self.schemas = None

    def _ready(self):
        if self.failed:
            raise StorageError("Receipt storage failed; further dispatch is blocked",
                               outcome_unknown=self.failed_outcome_unknown)

    def _file(self, relative, data):
        path = self.root / relative
        with path.open("xb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        return {"path": relative, "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}

    def _blob(self, value):
        data = _encode(value)
        digest = hashlib.sha256(data).hexdigest()
        relative = "blobs/" + digest + ".json"
        if digest not in self._written:
            reference = self._file(relative, data)
            self._written.add(digest)
        else:
            reference = {"path": relative, "sha256": digest, "bytes": len(data)}
        return reference

    def _receipt(self, index, kind, value):
        return self._file("receipts/%06d.%s.json" % (index, kind), _encode(value))

    def _guarded(self, action, *, unknown=False):
        self._ready()
        try:
            return action()
        except Exception as error:
            self.failed = True
            self.failed_outcome_unknown = unknown
            raise StorageError("Receipt persistence failed: " + str(error),
                               outcome_unknown=unknown) from error

    def checkpoint(self, report):
        def write():
            summary, summary_blobs = {}, {}
            for key in _SUMMARY_KEYS:
                if key in report:
                    if len(_encode(report[key])) > 16384:
                        summary_blobs[key] = self._blob(report[key])
                    else:
                        summary[key] = report[key]
            # Only fixed small summary fields and receipt references are encoded.
            # Large schemas, graphs, node children and raw responses are blobs.
            pending = any(entry["mutation"] and "response" not in entry for entry in self.entries)
            summary["outcome_unknown"] = bool(report.get("outcome_unknown", False) or pending)
            head = {"storage_schema": STORAGE_SCHEMA, "version": 2,
                    "evidence_directory": self.root.name, "initial": self.initial,
                    "schemas": self.schemas, "summary": summary,
                    "summary_blobs": summary_blobs,
                    "receipts": self.entries, "terminal": self.terminal,
                    "application_snapshot_scope": "terminal" if self.terminal else "initial_plus_summary"}
            _atomic_head(self.output, head)
            self._durable_responses = {entry["index"] for entry in self.entries if "response" in entry}
        uncertain = bool(report.get("outcome_unknown", False)) or any(
            entry["mutation"] and entry["index"] not in self._durable_responses for entry in self.entries)
        return self._guarded(write, unknown=uncertain)

    def record_schemas(self, schemas):
        self.schemas = self._guarded(lambda: self._blob(schemas))

    def before_dispatch(self, row, report):
        def write():
            index = len(self.entries) + 1
            reference = self._receipt(index, "request", dict(row))
            self.entries.append({"index": index, "mutation": row["mutation"], "request": reference})
        self._guarded(write)
        self.checkpoint(report)

    def raw_returned(self, raw, report):
        self._guarded(lambda: self.entries[-1].update(raw=self._blob(raw)),
                      unknown=self.entries[-1]["mutation"])
        self.checkpoint(report)

    def decoded_returned(self, row, report):
        def write():
            entry = self.entries[-1]
            decoded = self._blob(row["result"])
            entry["response"] = self._receipt(entry["index"], "response", {
                "raw": entry["raw"], "decoded": decoded, "state": "responded"})
        self._guarded(write, unknown=self.entries[-1]["mutation"])
        self.checkpoint(report)

    def completed(self, row, report):
        def write():
            entry = self.entries[-1]
            fields = {key: value for key, value in row.items() if key not in ("raw", "result")}
            entry["completion"] = self._receipt(entry["index"], "completion", {"fields": self._blob(fields)})
        self._guarded(write, unknown=self.entries[-1]["mutation"] and
                      self.entries[-1]["index"] not in self._durable_responses)
        self.checkpoint(report)

    def finish(self, report):
        def write():
            if len(report["stages"]) != len(self.entries):
                raise StorageError("Terminal stages must match this recorder's dispatch receipts")
            # One terminal pass preserves caller edits to mutable returned values.
            # Content hashes reuse unchanged blobs; immutable dispatch receipts
            # remain the evidence of what actually returned before assertions.
            views = []
            for row in report["stages"]:
                view = {"fields": self._blob({key: value for key, value in row.items()
                                              if key not in ("raw", "result")})}
                for key in ("raw", "result"):
                    if key in row:
                        view[key] = self._blob(row[key])
                views.append(view)
            application = self._blob({key: value for key, value in report.items() if key != "stages"})
            self.terminal = self._blob({"application": application, "stage_views": views})
        self._guarded(write, unknown=bool(report.get("outcome_unknown", False)))
        self.checkpoint(report)


def _read_reference(root, reference):
    if not isinstance(reference, dict) or set(reference) != {"path", "sha256", "bytes"}:
        raise StorageError("Invalid evidence reference")
    relative, digest, size = reference["path"], reference["sha256"], reference["bytes"]
    if (not isinstance(relative, str) or "\\" in relative or ":" in relative
            or not isinstance(digest, str) or len(digest) != 64
            or any(char not in "0123456789abcdef" for char in digest)
            or type(size) is not int or size < 0):
        raise StorageError("Invalid evidence path/hash/size")
    parts = PurePosixPath(relative)
    if (parts.is_absolute() or parts.as_posix() != relative
            or any(part in (".", "..") for part in parts.parts) or len(parts.parts) != 2):
        raise StorageError("Evidence path escapes its owned directory")
    if parts.parts[0] not in ("blobs", "receipts"):
        raise StorageError("Unexpected evidence directory")
    path = root.joinpath(*parts.parts).resolve()
    if not path.is_relative_to(root.resolve()):
        raise StorageError("Evidence symlink escapes its owned directory")
    data = path.read_bytes()
    if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
        raise StorageError("Evidence hash/size mismatch: " + relative)
    return json.loads(data)


def materialize_report(path):
    """Read legacy or v2 evidence offline, validating every referenced payload.

    An active v2 report is a partial application projection, not a fabricated
    terminal report. Missing decoded mutation responses remain unknown.
    """
    path = Path(path)
    head = json.loads(path.read_text(encoding="utf-8"))
    if head.get("storage_schema") != STORAGE_SCHEMA:
        return head
    if head.get("version") != 2 or head.get("evidence_directory") != path.name + ".evidence":
        raise StorageError("Unsupported storage version/evidence directory")
    root = path.with_name(head["evidence_directory"])
    if root.resolve().parent != path.parent.resolve():
        raise StorageError("Evidence directory escapes the report parent")
    read = lambda reference: _read_reference(root, reference)
    report = read(head["initial"])
    rows, pending = [], False
    terminal_reference = head.get("terminal")
    for index, entry in enumerate(head["receipts"], 1):
        if entry.get("index") != index or type(entry.get("mutation")) is not bool:
            raise StorageError("Receipt ordering/mutation flag mismatch")
        row = read(entry["request"])
        if row.get("mutation") is not entry["mutation"]:
            raise StorageError("Receipt request mutation mismatch")
        if "raw" in entry:
            raw = read(entry["raw"])
            if not terminal_reference:
                row["raw"] = raw
            del raw
        if "response" in entry:
            response = read(entry["response"])
            if response.get("state") != "responded" or response.get("raw") != entry.get("raw"):
                raise StorageError("Response receipt does not match its raw document")
            decoded = read(response["decoded"])
            if not terminal_reference:
                row.update(result=decoded, state="responded")
            del decoded
        else:
            pending = pending or entry["mutation"]
        if "completion" in entry:
            completion = read(entry["completion"])
            row.update(read(completion["fields"]))
        rows.append(row)
    if terminal_reference:
        terminal = read(terminal_reference)
        report = read(terminal["application"])
        if len(terminal["stage_views"]) != len(rows):
            raise StorageError("Terminal stage count differs from dispatch receipts")
        rows = []
        for view in terminal["stage_views"]:
            row = read(view["fields"])
            for key in ("raw", "result"):
                if key in view:
                    row[key] = read(view[key])
            rows.append(row)
    else:
        report.update(head["summary"])
        report.update({key: read(reference) for key, reference in head.get("summary_blobs", {}).items()})
        if head.get("schemas"):
            report["schemas"] = read(head["schemas"])
        report["storage_materialization_scope"] = "initial_plus_summary_and_durable_receipts"
    report["stages"] = rows
    if pending:
        report["outcome_unknown"] = True
    return report


def export_legacy_report(path, output):
    """Create a fresh legacy-shaped JSON file offline; never overwrite evidence."""
    report = materialize_report(path)
    with Path(output).open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2))
        stream.flush()
        os.fsync(stream.fileno())
    return report
