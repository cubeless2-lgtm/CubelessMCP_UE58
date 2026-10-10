"""Reject incomplete or internally inconsistent Blueprint pipeline reports."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


def verify(report, *, require_phase=None):
    errors = []
    def require(condition, message):
        if not condition:
            errors.append(message)
    require(report.get("schema") == "cubeless_blueprint_pipeline_v1", "Unexpected report schema")
    phase = report.get("phase")
    require(phase in {"author", "reload", "cleanup"}, "Unknown phase")
    if require_phase:
        require(phase == require_phase, "Requested phase differs from report")
    require(report.get("status") == "pass", "Pipeline phase did not pass")
    require(report.get("outcome_unknown") is False, "Mutation outcome uncertain or absent")
    require(report.get("automatic_retries") == 0, "Unexpected mutation retry behavior")
    run_id = report.get("run_id", "")
    require(isinstance(run_id, str) and re.fullmatch(r"[a-f0-9]{32}", run_id) is not None, "Missing/invalid run ownership ID")
    require(report.get("asset_path") == "/Game/CubelessMCPTests/Run_" + str(run_id) + "/BP_MutationContract", "Fixture does not match exact run ownership path")
    fingerprint_fields = ("phase", "run_id", "endpoint", "project_file", "asset_path")
    fingerprint = hashlib.sha256(json.dumps({key: report.get(key) for key in fingerprint_fields}, sort_keys=True).encode()).hexdigest()
    require(report.get("input_fingerprint") == fingerprint, "Input/environment fingerprint mismatch")
    require(bool(report.get("project_file")), "Missing project identity")
    require(bool(report.get("editor", {}).get("instance_id")), "Missing editor instance identity")
    stages = report.get("stages", [])
    names = [x.get("name") for x in stages]
    require(len(names) == len(set(names)), "Duplicate stage names")
    lookup = {x.get("name"): x for x in stages}
    required = {
        "author": ["preflight", "invalid_create_rejected", "create", "duplicate_create_rejected", "add_first_branch", "add_second_branch", "connect", "readback_connection", "stale_position_rejected", "readback_stale_unchanged", "move", "readback_position", "linked_delete_rejected", "readback_rejected_delete", "delete", "readback_deleted", "compile_save", "saved_readback", "dependency_audit"],
        "reload": ["preflight", "new_process", "reload_readback", "reload_graph", "dependency_audit"],
        "cleanup": ["preflight", "cleanup_delete", "cleanup_absent"],
    }.get(phase, [])
    for name in required:
        require(name in lookup and lookup[name].get("passed") is True, f"Missing/failed stage: {name}")
    require(all(x.get("passed") is True for x in stages), "A recorded stage failed")
    if phase == "author":
        require(report.get("compile", {}).get("compiled") is True, "Compilation did not pass")
        require(report.get("save", {}).get("saved") is True, "Asset was not saved")
        require(report.get("reload", {}).get("status") == "not_run", "Author phase must not claim new-process reload")
        result = lookup.get("compile_save", {}).get("result", {})
        require(result.get("compile", {}).get("compiled") is True, "No compile evidence in actual stage result")
        require(result.get("save", {}).get("saved") is True, "No save evidence in actual stage result")
        require(report.get("compile") == result.get("compile"), "Compile summary differs from stage result")
        require(report.get("save") == result.get("save"), "Save summary differs from stage result")
    if phase == "reload":
        require(report.get("reload", {}).get("status") == "passed", "Reload not verified")
        before = report.get("author_editor", {}).get("instance_id")
        after = report.get("editor", {}).get("instance_id")
        require(bool(before) and before != after, "Reload reused author editor instance")
        require(bool(report.get("author_report_sha256")), "Missing author report fingerprint")
    if phase in {"author", "reload"}:
        require(lookup.get("dependency_audit", {}).get("result", {}).get("passed") is True, "No independent dependency pass")
    require(report.get("packaging", {}).get("status") == "not_run", "This harness cannot prove packaging")
    return {"schema": "cubeless_pipeline_report_verification_v1", "passed": not errors, "errors": errors, "phase": phase, "verified_scope": "Report contract and included assertions; authenticity and engine behavior depend on attached live evidence"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--phase", choices=["author", "reload", "cleanup"])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = verify(json.loads(args.report.read_text(encoding="utf-8")), require_phase=args.phase)
    if args.output:
        if args.output.exists():
            parser.error("Refusing to overwrite verification evidence")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
