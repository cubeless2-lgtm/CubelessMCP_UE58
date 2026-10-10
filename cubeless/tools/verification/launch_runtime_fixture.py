"""Launch the exact Shipping fixture recorded by validate_runtime_marker prepare.

The game owns marker creation. This launcher records process identity and never
retries or kills a timed-out process; an unknown result needs reconciliation.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()
    baseline = json.loads(args.baseline.read_text(encoding="utf-8-sig"))
    if baseline.get("schema") != "cubeless_runtime_marker_baseline_v1" or baseline.get("success") is not True:
        raise ValueError("Successful pre-launch baseline required")
    executable = Path(baseline["executable"]).resolve()
    if not executable.name.endswith("-Win64-Shipping.exe") or digest(executable) != baseline["executable_sha256"]:
        raise ValueError("The exact baseline Shipping executable is required")
    if Path(baseline["marker_path"]).exists() or baseline.get("marker_existed") is not False:
        raise ValueError("Marker already exists; refuse duplicate execution")
    if args.output.exists() or not 1 <= args.timeout <= 300:
        raise ValueError("Fresh report and bounded timeout required")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    log = args.output.with_suffix(".console.log")
    report = {"schema": "cubeless_runtime_launch_v1", "success": False,
              "baseline": str(args.baseline.resolve()), "baseline_sha256": digest(args.baseline),
              "executable": str(executable), "executable_sha256": digest(executable),
              "arguments": ["-nullrhi", "-unattended", "-NoSplash", "-NoSound",
                            "-UserDir=" + str(Path(baseline["runtime_user_dir"]).resolve())],
              "automatic_retry": False, "stage": "launch_not_attempted", "exit_code": None,
              "launcher_sha256": digest(__file__), "console_log": str(log.resolve())}

    def checkpoint():
        temporary = args.output.with_suffix(".tmp")
        temporary.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        os.replace(temporary, args.output)

    # Exclusive reservation prevents two launchers from sharing this report.
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(report, stream)
    try:
        with log.open("xb") as stream:
            report.update(started_ns=time.time_ns(), stage="launch_attempted")
            checkpoint()
            child = subprocess.Popen([str(executable)] + report["arguments"], cwd=executable.parent,
                                     stdout=stream, stderr=subprocess.STDOUT,
                                     creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            report.update(pid=child.pid, stage="running")
            checkpoint()
            try:
                report["exit_code"] = child.wait(timeout=args.timeout)
                report.update(ended_ns=time.time_ns(), stage="exited", success=report["exit_code"] == 0)
            except subprocess.TimeoutExpired:
                report.update(stage="timeout_process_may_still_be_running", observed_ns=time.time_ns())
    except Exception as error:
        report["error"] = str(error)
    finally:
        checkpoint()
    print(json.dumps(report, indent=2))
    return 0 if report["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
