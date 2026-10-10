"""External, read-only validation of a fresh Shipping game's SaveGame marker.

prepare records absence plus executable/fixture fingerprints before launch.
verify checks the caller's process evidence and the exact UE 5.8.3 save header.
This program never launches the game, creates its marker, or modifies assets.
self-test uses synthetic files only and is not Unreal runtime evidence.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile
import time


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_new(path, report):
    path = Path(path).resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def marker_path(fixture, runtime_user_dir):
    root = Path(runtime_user_dir).resolve()
    slot = fixture["slot_name"]
    require(slot == "Cubeless_" + fixture["run_id"], "Fixture slot/run identity mismatch")
    require(slot and all(char.isalnum() or char in "_-" for char in slot), "Unsafe marker slot name")
    result = (root / "Saved" / "SaveGames" / (slot + ".sav")).resolve()
    require(result.is_relative_to(root), "Marker escapes the runtime user directory")
    return result


class Reader:
    def __init__(self, data):
        self.data = data
        self.offset = 0

    def take(self, length):
        require(0 <= length <= len(self.data) - self.offset, "Truncated/invalid save-game header")
        start = self.offset
        self.offset += length
        return self.data[start:self.offset]

    def number(self, fmt):
        return struct.unpack("<" + fmt, self.take(struct.calcsize("<" + fmt)))[0]

    def string(self):
        length = self.number("i")
        require(abs(length) <= 16384, "Unreasonable FString length in save-game header")
        if length == 0:
            return ""
        if length > 0:
            raw = self.take(length)
            require(raw[-1:] == b"\0", "Unterminated ANSI FString")
            return raw[:-1].decode("utf-8", errors="strict")
        raw = self.take(-length * 2)
        require(raw[-2:] == b"\0\0", "Unterminated UTF-16 FString")
        return raw[:-2].decode("utf-16-le", errors="strict")


def parse_header(data):
    reader = Reader(data)
    require(reader.take(4) == b"GVAS", "Marker is not a versioned Unreal SaveGame")
    file_version = reader.number("i")
    require(file_version == 3, "Only the installed UE 5.8.3 SaveGame v3 layout is supported")
    ue4_version = reader.number("i")
    ue5_version = reader.number("i")
    engine = [reader.number("H"), reader.number("H"), reader.number("H")]
    changelist = reader.number("I")
    branch = reader.string()
    require(engine == [5, 8, 3], "SaveGame engine version is not UE 5.8.3")
    custom_format = reader.number("i")
    require(custom_format == 3, "Unsupported custom-version container encoding")
    custom_count = reader.number("i")
    require(0 <= custom_count <= 4096, "Invalid custom-version count")
    reader.take(custom_count * 20)  # Optimized FGuid + int32 entries.
    class_path = reader.string()
    require(len(data) - reader.offset >= 4, "SaveGame object payload is absent/truncated")
    return {"file_version": file_version, "package_file_ue4": ue4_version, "package_file_ue5": ue5_version,
            "engine_version": ".".join(map(str, engine)), "changelist": changelist,
            "engine_branch": branch, "custom_version_count": custom_count,
            "save_game_class": class_path, "header_bytes": reader.offset,
            "payload_bytes": len(data) - reader.offset, "payload_deserialization": "not_performed"}


def prepare(fixture_path, runtime_user_dir, executable):
    fixture_path = Path(fixture_path).resolve()
    fixture = read_json(fixture_path)
    require(fixture.get("schema") == "cubeless_runtime_fixture_v1" and fixture.get("success") is True, "Fixture authoring did not pass")
    require(fixture.get("validation", {}).get("runtime_marker_created_by_this_script") is False, "Fixture report does not exclude author-created runtime markers")
    marker = marker_path(fixture, runtime_user_dir)
    require(not marker.exists(), "Marker already exists; use a fresh runtime directory/run instead of deleting evidence")
    executable = Path(executable).resolve()
    require(executable.is_file(), "Packaged executable is missing")
    return {"schema": "cubeless_runtime_marker_baseline_v1", "success": True,
            "captured_ns": time.time_ns(), "fixture_report": str(fixture_path), "fixture_sha256": sha256(fixture_path),
            "run_id": fixture["run_id"], "runtime_user_dir": str(Path(runtime_user_dir).resolve()),
            "marker_path": str(marker), "marker_existed": False, "executable": str(executable),
            "executable_sha256": sha256(executable), "verifier_sha256": sha256(__file__)}


def verify(fixture_path, baseline, launch, tolerance_ns=2_000_000_000):
    report = {"schema": "cubeless_runtime_marker_verification_v1", "success": False,
              "checks": [], "scope": "fresh packaged-game Blueprint BeginPlay SaveGame marker",
              "complete_game_functionality": "not_verified", "plugin_exclusion": "requires_separate_package_receipt_and_staged_file_audit",
              "verifier_sha256": sha256(__file__), "timestamp_tolerance_ns": tolerance_ns}
    def check(name, condition, reason):
        report["checks"].append({"name": name, "passed": bool(condition), "reason": reason})
        require(condition, reason)
    try:
        fixture_path = Path(fixture_path).resolve()
        fixture = read_json(fixture_path)
        check("baseline_schema", baseline.get("schema") == "cubeless_runtime_marker_baseline_v1" and baseline.get("success") is True, "Require a successful pre-launch marker baseline")
        check("fixture_author_passed", fixture.get("schema") == "cubeless_runtime_fixture_v1" and fixture.get("success") is True, "Require successful fixture authoring")
        check("fixture_fingerprint", str(fixture_path) == baseline["fixture_report"] and sha256(fixture_path) == baseline["fixture_sha256"], "Fixture report must match the pre-launch fingerprint")
        check("verifier_fingerprint", baseline["verifier_sha256"] == sha256(__file__), "Verifier must match the pre-launch version")
        check("marker_absent_before_launch", baseline.get("marker_existed") is False, "Marker must be absent in the baseline")
        check("run_id", fixture["run_id"] == baseline["run_id"], "Runtime and fixture run IDs must agree")
        executable = Path(baseline["executable"]).resolve()
        check("executable_identity", Path(launch["executable"]).resolve() == executable and executable.is_file(), "Launch must identify the baseline executable")
        check("executable_fingerprint", sha256(executable) == baseline["executable_sha256"] == launch["executable_sha256"], "Packaged executable bytes must match the launch and baseline")
        start, end = int(launch["started_ns"]), int(launch["ended_ns"])
        check("process_evidence", int(launch["pid"]) > 0 and end >= start >= int(baseline["captured_ns"]), "Require a positive PID and an observed launch/end after the baseline")
        check("normal_exit", launch.get("exit_code") == 0, "Packaged runtime must exit successfully")
        user_dir_args = [arg.split("=", 1)[1].strip('"') for arg in launch["arguments"] if arg.lower().startswith("-userdir=")]
        check("isolated_runtime_directory", len(user_dir_args) == 1 and Path(user_dir_args[0]).resolve() == Path(baseline["runtime_user_dir"]).resolve(), "Launch must use exactly the baseline -UserDir")
        marker = marker_path(fixture, baseline["runtime_user_dir"])
        check("marker_identity", str(marker) == baseline["marker_path"] and marker.is_file(), "The exact expected marker must exist after launch")
        stat = marker.stat()
        check("marker_time", start - tolerance_ns <= stat.st_mtime_ns <= end + tolerance_ns, "Marker mtime must be inside the observed runtime window (stated tolerance)")
        check("marker_size", 32 <= stat.st_size <= 16 * 1024 * 1024, "Marker must be a bounded, nonempty SaveGame")
        header = parse_header(marker.read_bytes())
        report["header"] = header
        check("exact_save_game_class", header["save_game_class"] == fixture["marker_class_path"], "Parsed header class must equal the authored run-specific SaveGame class")
        report["marker"] = {"path": str(marker), "size": stat.st_size, "mtime_ns": stat.st_mtime_ns, "sha256": sha256(marker)}
        report["runtime_process"] = launch
        report["success"] = True
    except Exception as error:
        report["error"] = str(error)
    report["verified_ns"] = time.time_ns()
    return report


def self_test():
    """Positive and adversarial synthetic cases; never substitutes for UE execution."""
    def fstring(value):
        raw = value.encode("utf-8") + b"\0"
        return struct.pack("<i", len(raw)) + raw
    def synthetic_save(class_path):
        return (b"GVAS" + struct.pack("<iiiHHHI", 3, 522, 1018, 5, 8, 3, 58210709) +
                fstring("++UE5+Release-5.8") + struct.pack("<ii", 3, 0) + fstring(class_path) + fstring("None"))
    checks = []
    with tempfile.TemporaryDirectory(prefix="cubeless-marker-verifier-test-") as temp:
        root = Path(temp)
        fixture = {"schema": "cubeless_runtime_fixture_v1", "success": True, "run_id": "Synthetic",
                   "slot_name": "Cubeless_Synthetic", "marker_class_path": "/Game/CubelessMCPTests/RuntimeProof/Synthetic/BP_MarkerData.BP_MarkerData_C",
                   "validation": {"runtime_marker_created_by_this_script": False}}
        fixture_file = root / "fixture.json"
        fixture_file.write_text(json.dumps(fixture), encoding="utf-8")
        executable = root / "synthetic-not-a-real-executable.bin"
        executable.write_bytes(b"synthetic identity only; never executed")
        baseline = prepare(fixture_file, root / "runtime", executable)
        start = time.time_ns()
        marker = Path(baseline["marker_path"])
        marker.parent.mkdir(parents=True)
        marker.write_bytes(synthetic_save(fixture["marker_class_path"]))
        launch = {"executable": str(executable), "executable_sha256": sha256(executable), "pid": 123,
                  "started_ns": start, "ended_ns": time.time_ns(), "exit_code": 0,
                  "arguments": ["-UserDir=" + baseline["runtime_user_dir"]]}
        def expect(name, expected, changed_baseline=None, changed_launch=None):
            observed = verify(fixture_file, changed_baseline or baseline, changed_launch or launch)["success"]
            checks.append({"name": name, "passed": observed is expected})
            require(observed is expected, "Synthetic case failed: " + name)
        expect("valid_synthetic_header", True)
        marker.write_bytes(synthetic_save("/Game/Wrong.Wrong_C") + fixture["marker_class_path"].encode())
        expect("wrong_header_class_even_when_expected_string_is_in_payload", False)
        marker.write_bytes(b"GVAS")
        expect("truncated_header", False)
        marker.write_bytes(synthetic_save(fixture["marker_class_path"]))
        expect("nonzero_exit", False, changed_launch=dict(launch, exit_code=1))
        expect("wrong_executable_fingerprint", False, changed_launch=dict(launch, executable_sha256="0" * 64))
        expect("marker_existed_in_baseline", False, changed_baseline=dict(baseline, marker_existed=True))
        expect("wrong_fixture_fingerprint", False, changed_baseline=dict(baseline, fixture_sha256="0" * 64))
        expect("wrong_user_directory", False, changed_launch=dict(launch, arguments=["-UserDir=" + str(root / "wrong")]))
        os.utime(marker, ns=(start - 60_000_000_000, start - 60_000_000_000))
        expect("stale_marker", False)
        marker.unlink()
        expect("missing_marker", False)
    return {"schema": "cubeless_runtime_marker_synthetic_tests_v1", "success": True,
            "scope": "synthetic external verifier tests only; no Unreal runtime executed", "checks": checks}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    prepare_parser = sub.add_parser("prepare")
    prepare_parser.add_argument("--fixture-report", required=True)
    prepare_parser.add_argument("--runtime-user-dir", required=True)
    prepare_parser.add_argument("--executable", required=True)
    prepare_parser.add_argument("--output", required=True)
    verify_parser = sub.add_parser("verify")
    verify_parser.add_argument("--fixture-report", required=True)
    verify_parser.add_argument("--baseline", required=True)
    verify_parser.add_argument("--launch-report", required=True)
    verify_parser.add_argument("--output", required=True)
    test_parser = sub.add_parser("self-test")
    test_parser.add_argument("--output")
    args = parser.parse_args()
    if args.mode == "prepare":
        result = prepare(args.fixture_report, args.runtime_user_dir, args.executable)
    elif args.mode == "verify":
        result = verify(args.fixture_report, read_json(args.baseline), read_json(args.launch_report))
    else:
        result = self_test()
    if args.output:
        write_new(args.output, result)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
