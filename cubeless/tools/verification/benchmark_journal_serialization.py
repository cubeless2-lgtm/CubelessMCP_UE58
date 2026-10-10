"""Compare journal encoders on read-only input records and disposable files; no UE."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import statistics
import tempfile
import time
import uuid


class CountWrites(io.StringIO):
    def __init__(self):
        super().__init__()
        self.calls = 0

    def write(self, value):
        self.calls += 1
        return super().write(value)


def encode(record, mode):
    stream = CountWrites()
    if mode == "dump":
        json.dump(record, stream, ensure_ascii=False, sort_keys=True)
    else:
        stream.write(json.dumps(record, ensure_ascii=False, sort_keys=True))
    return stream.getvalue(), stream.calls


def durable(path, record, mode):
    start = time.perf_counter_ns()
    # Candidate deliberately serializes before exclusive file creation.
    encoded = json.dumps(record, ensure_ascii=False, sort_keys=True) if mode == "dumps" else None
    temporary = path.with_name(path.name + "." + str(uuid.uuid4()) + ".tmp")
    with temporary.open("x", encoding="utf-8") as stream:
        if mode == "dump":
            json.dump(record, stream, ensure_ascii=False, sort_keys=True)
        else:
            stream.write(encoded)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)
    return (time.perf_counter_ns() - start) / 1e6


def stats(samples):
    return {"samples": samples, "median_ms": statistics.median(samples),
            "minimum_ms": min(samples), "maximum_ms": max(samples)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--journal", action="append", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--iterations", type=int, default=30)
    parser.add_argument("--encode-iterations", type=int, default=100)
    args = parser.parse_args()
    if args.report.exists() or not 2 <= args.iterations <= 200 or not 1 <= args.encode_iterations <= 1000:
        raise ValueError("Use a fresh report, 2-200 pairs, and 1-1000 encoder repeats")
    result = {"schema": "cubeless_journal_encoder_benchmark_v1", "python": platform.python_version(),
              "platform": platform.platform(), "records": [], "passed": False,
              "conditions": {"durable_iterations_each": args.iterations, "encoder_repeats_per_sample": args.encode_iterations,
                             "pair_order": "alternating dump/dumps then dumps/dump", "warmups_each": 1,
                             "filesystem": "OS-selected tempfile directory; same volume and directory for both variants",
                             "flush_fsync_replace": True, "original_journals_read_only": True,
                             "limits": "Local serialization/write microbenchmark; no UE, MCP latency, crash durability or universal performance proof. Candidate holds one complete encoded string in memory."}}
    with tempfile.TemporaryDirectory(prefix="cubeless-journal-ab-") as directory:
        result["temporary_volume"] = Path(directory).anchor
        for source in args.journal:
            original = source.read_bytes()
            record = json.loads(original)
            old, calls_old = encode(record, "dump")
            new, calls_new = encode(record, "dumps")
            if old != new or old.encode("utf-8") != original:
                raise AssertionError("Journal bytes differ before timing: " + str(source))
            target = Path(directory) / "journal.json"
            for mode in ("dump", "dumps"):
                durable(target, record, mode)
            timings = {mode: {"encode": [], "durable": []} for mode in ("dump", "dumps")}
            for index in range(args.iterations):
                order = ("dump", "dumps") if index % 2 == 0 else ("dumps", "dump")
                for mode in order:
                    start = time.perf_counter_ns()
                    for _ in range(args.encode_iterations):
                        # Match production encoder choices without the counting
                        # subclass overhead used only for the calls observation.
                        if mode == "dump":
                            json.dump(record, io.StringIO(), ensure_ascii=False, sort_keys=True)
                        else:
                            json.dumps(record, ensure_ascii=False, sort_keys=True)
                    timings[mode]["encode"].append((time.perf_counter_ns() - start) / 1e6 / args.encode_iterations)
                    timings[mode]["durable"].append(durable(target, record, mode))
                    if target.read_bytes() != original:
                        raise AssertionError("Durable byte mismatch")
            unchanged = source.read_bytes() == original
            row = {"source": str(source.resolve()), "source_sha256": hashlib.sha256(original).hexdigest(), "bytes": len(original),
                   "original_unchanged": unchanged, "encoded_bytes_identical": True,
                   "write_calls": {"dump": calls_old, "dumps": calls_new},
                   "timings": {mode: {kind: stats(values) for kind, values in fields.items()} for mode, fields in timings.items()}}
            row["median_reduction_percent"] = {kind: 100 * (1 - row["timings"]["dumps"][kind]["median_ms"] / row["timings"]["dump"][kind]["median_ms"]) for kind in ("encode", "durable")}
            result["records"].append(row)
        result["passed"] = all(row["original_unchanged"] for row in result["records"])
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with args.report.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
    print(json.dumps({"passed": result["passed"], "records": [{key: row[key] for key in ("bytes", "write_calls", "median_reduction_percent")} for row in result["records"]]}))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
