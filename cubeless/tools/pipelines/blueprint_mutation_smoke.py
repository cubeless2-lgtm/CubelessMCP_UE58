"""External official-MCP Blueprint mutation smoke with durable phase reports.

Port target OPS56-VERIFY-002. Author, new-process reload and cleanup are separate
phases so evidence survives failures and the saved fixture remains for packaging.
This script never starts/stops editors and never retries mutations automatically.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import MCPError, OfficialMCPClient, tool_payload
from verification.pipeline_report import verify


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def canonical(name):
    return name.replace("_", "").lower()


def operation_result(raw):
    value = tool_payload(raw)
    if isinstance(value, str):
        value = json.loads(value)
    if not isinstance(value, dict):
        return value
    mapped = {canonical(k): v for k, v in value.items()}
    if "changesjson" not in mapped:
        return value
    record = {}
    for key in ("success", "operation_id", "stage", "error_code", "error"):
        record[key] = mapped[canonical(key)]
    for key in ("changes", "compile", "save", "validation", "retry", "data"):
        record[key] = json.loads(mapped[key + "json"])
    return record


def ensure(condition, message):
    if not condition:
        raise AssertionError(message)


def require_success(result):
    ensure(isinstance(result, dict) and result.get("success") is True, f"Operation failed: {result}")


def strict_mutation(result):
    require_success(result)
    data = result["data"]
    ensure(data.get("contract_version") == "mutation_result_v1", "Missing legacy mutation contract")
    ensure(data.get("verified_success") is True and data.get("postcondition_verified") is True, "Unverified mutation postcondition")
    ensure(isinstance(data.get("before"), dict) and isinstance(data.get("after"), dict), "Missing before/after")
    ensure(data.get("verification", {}).get("status") == "passed" and data.get("verification", {}).get("method"), "Missing verification method")
    fields = {"affected_count", "created_count", "updated_count", "removed_count", "skipped_count", "failed_count"}
    ensure(fields <= set(data.get("delta", {})), "Missing delta counts")
    ensure(all(type(data["delta"][key]) is int and data["delta"][key] >= 0 for key in fields), "Invalid delta count")


def nodes(result):
    require_success(result)
    return result["data"]["nodes"]


def node_id(result):
    require_success(result)
    return result["data"]["node_id"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True, help="Official UE MCP HTTP endpoint; no hardcoded old port")
    parser.add_argument("--expected-project", required=True, type=Path)
    parser.add_argument("--phase", required=True, choices=["author", "reload", "cleanup"])
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--parent-report", type=Path, help="Passing author report for reload/cleanup")
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Refusing to overwrite prior evidence; choose a fresh output")
    parent = None
    if args.phase != "author":
        if not args.parent_report:
            parser.error("reload/cleanup require --parent-report from author")
        parent = json.loads(args.parent_report.read_text(encoding="utf-8"))
        if not verify(parent, require_phase="author")["passed"]:
            parser.error("Parent author report failed independent contract checks")
    run_id = parent["run_id"] if parent else uuid.uuid4().hex
    package_dir = "/Game/CubelessMCPTests/Run_" + run_id
    asset_name = "BP_MutationContract"
    asset_path = package_dir + "/" + asset_name
    ensure(re.fullmatch(r"[a-f0-9]{32}", run_id), "Invalid run ownership ID")
    if parent:
        ensure(parent["asset_path"] == asset_path, "Parent fixture does not match generated ownership path")
    project = str(args.expected_project.resolve())
    report = {"schema": "cubeless_blueprint_pipeline_v1", "legacy_id": "OPS56-VERIFY-002", "phase": args.phase,
              "run_id": run_id, "endpoint": args.endpoint, "project_file": project, "asset_path": asset_path,
              "started_utc": datetime.now(timezone.utc).isoformat(), "status": "running", "stages": [],
              "compile": {"status": "not_run"}, "save": {"status": "not_run"}, "reload": {"status": "not_run"},
              "cleanup": {"status": "deferred_to_explicit_cleanup_phase"}, "packaging": {"status": "not_run"},
              "outcome_unknown": False, "automatic_retries": 0, "atomic_rollback": False,
              "legacy_scope": "Actor Blueprint / two Branch nodes / exec connect / position / delete / readback, augmented compile+save+cold reload. General graph tool parity is separate."}
    report["input_fingerprint"] = digest({key: report[key] for key in ("phase", "run_id", "endpoint", "project_file", "asset_path")})
    if parent:
        ensure(Path(parent["project_file"]).resolve() == args.expected_project.resolve(), "Parent belongs to a different project")
        report["author_report_sha256"] = hashlib.sha256(args.parent_report.read_bytes()).hexdigest()
        report["author_editor"] = parent["editor"]
        report["fixture"] = parent["fixture"]
    client = OfficialMCPClient(args.endpoint, args.timeout)
    schemas = {}
    started = time.perf_counter()

    def persist():
        args.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.output.with_suffix(args.output.suffix + ".partial")
        temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        temporary.replace(args.output)

    def invoke(toolset, tool, parameters=None, mutation=False):
        schema = schemas[toolset]
        selected = next((x for x in schema["tools"] if x["name"].rsplit(".", 1)[-1] == tool), None)
        ensure(selected is not None, f"Tool not present in actual schema: {tool}")
        props = selected.get("inputSchema", {}).get("properties", {})
        params = {}
        for key, value in (parameters or {}).items():
            actual = next((name for name in props if canonical(name) == canonical(key)), None)
            ensure(actual is not None, f"Input absent from actual schema: {tool}.{key}")
            params[actual] = value
        try:
            return operation_result(client.call_toolset(toolset, tool, params, mutation=mutation))
        except MCPError as exc:
            exc.outcome_unknown = exc.outcome_unknown or mutation
            raise

    def step(name, action, check=require_success):
        entry = {"name": name, "passed": False}
        report["stages"].append(entry)
        began = time.perf_counter()
        try:
            entry["result"] = action()
            check(entry["result"])
            entry["passed"] = True
            return entry["result"]
        except Exception as exc:
            entry["error"] = str(exc)
            if isinstance(exc, MCPError):
                entry["mcp_error"] = exc.detail
                report["outcome_unknown"] = report["outcome_unknown"] or exc.outcome_unknown
            raise
        finally:
            entry["elapsed_seconds"] = round(time.perf_counter() - began, 6)
            persist()

    try:
        report["server"] = client.initialize()
        report["top_level_schema"] = client.list_tools()
        catalog = tool_payload(client.call("list_toolsets"))
        ensure(isinstance(catalog, str), "Expected official list_toolsets text catalog")
        names = re.findall(r"^- ([\w.]+):", catalog, flags=re.MULTILINE)
        def discover(suffix):
            matches = [name for name in names if name.endswith("." + suffix)]
            ensure(len(matches) == 1, f"Expected one toolset ending {suffix}, found {matches}")
            name = matches[0]
            schemas[name] = tool_payload(client.call("describe_toolset", {"toolset_name": name}))
            return name
        native = discover("CubelessNativeToolset")
        blueprint = discover("CubelessBlueprintTools")
        graph = discover("CubelessBlueprintGraphTools")
        report["discovered_schemas"] = schemas

        def preflight(result):
            require_success(result)
            ensure(Path(result["project_file"]).resolve() == args.expected_project.resolve(), "Endpoint serves a different project")
            ensure(str(result["engine_version"]).startswith("5.8.3"), "This migration is only validated for UE5.8.3")
            ensure(result["pie_or_sie_active"] is False, "PIE/SIE must be stopped")
            ensure(result["game_thread"] is True, "Official call did not execute on game thread")
            ensure(not result.get("dirty_packages"), "Use a clean isolated editor; do not save/discard someone else's work")
        report["editor"] = step("preflight", lambda: invoke(native, "GetEditorDiagnostics"), preflight)

        def bp(tool, parameters, mutation=False):
            if mutation:
                parameters = {**parameters, "operation_id": run_id + "_" + tool + "_" + str(len(report["stages"]))}
            return invoke(blueprint, tool, parameters, mutation)

        def gr(tool, parameters=None, mutation=False):
            parameters = {"blueprint_name": asset_path, "graph_name": "EventGraph", **(parameters or {})}
            if mutation:
                parameters["operation_id"] = run_id + "_" + tool + "_" + str(len(report["stages"]))
            return invoke(graph, tool, parameters, mutation)

        def expect_rejected(code):
            def check(result):
                ensure(result.get("success") is False and result.get("error_code") == code, f"Expected rejection {code}: {result}")
                ensure(result.get("changes") == [], "Rejected input mutated state")
                ensure(not result.get("save", {}).get("saved"), "Rejected input unexpectedly saved")
            return check

        def audit(result):
            require_success(result)
            ensure(result.get("passed") is True, "Forbidden or incomplete saved dependency audit")

        if args.phase == "author":
            step("invalid_create_rejected", lambda: bp("create_blueprint", {"name": "bad/name", "parent_class": "/Script/Engine.Actor", "package_path": package_dir}, True), expect_rejected("invalid_asset_name"))
            step("create", lambda: bp("create_blueprint", {"name": asset_name, "parent_class": "/Script/Engine.Actor", "package_path": package_dir}, True))
            step("duplicate_create_rejected", lambda: bp("create_blueprint", {"name": asset_name, "parent_class": "/Script/Engine.Actor", "package_path": package_dir}, True), expect_rejected("asset_already_exists"))
            first = step("add_first_branch", lambda: gr("add_branch_node", {"x": 0, "y": 0}, True))
            second = step("add_second_branch", lambda: gr("add_branch_node", {"x": 300, "y": 0}, True))
            source, target = node_id(first), node_id(second)
            report["fixture"] = {"remaining_node_id": source, "deleted_node_id": target, "graph_name": "EventGraph", "parent_class": "/Script/Engine.Actor"}
            step("connect", lambda: gr("connect_graph_pins", {"source_node_id": source, "source_pin_name": "then", "target_node_id": target, "target_pin_name": "execute"}, True), strict_mutation)
            def check_connection(result):
                current = nodes(result)
                node = next(x for x in current if x["node_id"] == source)
                pin = next(x for x in node["pins"] if x["name"] == "then")
                ensure(any(link["node_id"] == target and link["pin_name"] == "execute" for link in pin["links"]), "Independent readback found no expected exec connection")
            step("readback_connection", lambda: gr("inspect_graph"), check_connection)
            step("stale_position_rejected", lambda: gr("set_node_position", {"node_id": target, "x": 999, "y": 999, "expected_x": 301, "expected_y": 0}, True), expect_rejected("precondition_mismatch"))
            def check_stale_unchanged(result):
                node = next(x for x in nodes(result) if x["node_id"] == target)
                ensure(node["x"] == 300 and node["y"] == 0, "Rejected stale-position mutation changed node coordinates")
            step("readback_stale_unchanged", lambda: gr("inspect_graph"), check_stale_unchanged)
            step("move", lambda: gr("set_node_position", {"node_id": target, "x": 600, "y": 200}, True), strict_mutation)
            def check_position(result):
                node = next(x for x in nodes(result) if x["node_id"] == target)
                ensure(node["x"] == 600 and node["y"] == 200, "Independent position readback mismatch")
            step("readback_position", lambda: gr("inspect_graph"), check_position)
            step("linked_delete_rejected", lambda: gr("delete_node", {"node_id": target, "allow_exec_linked_delete": False}, True), expect_rejected("linked_exec_delete_rejected"))
            step("readback_rejected_delete", lambda: gr("inspect_graph"), check_connection)
            step("delete", lambda: gr("delete_node", {"node_id": target, "allow_exec_linked_delete": True}, True), strict_mutation)
            step("readback_deleted", lambda: gr("inspect_graph"), lambda result: ensure(target not in {x["node_id"] for x in nodes(result)}, "Deleted node still exists"))
            def check_saved(result):
                require_success(result)
                ensure(result["compile"].get("compiled") is True, "Compiler failed")
                ensure(result["save"].get("saved") is True, "Asset save failed")
            saved = step("compile_save", lambda: bp("compile_and_save_blueprint", {"blueprint_name": asset_path, "save": True}, True), check_saved)
            report["compile"], report["save"] = saved["compile"], saved["save"]
            def check_identity(result):
                require_success(result)
                ensure(result["data"]["path"] == asset_path and result["data"]["parent_class"] == "/Script/Engine.Actor", "Wrong Blueprint identity/parent")
                ensure(result["data"]["dirty"] is False, "Saved asset remains dirty")
            step("saved_readback", lambda: bp("inspect_blueprint", {"blueprint_name": asset_path}), check_identity)
            step("dependency_audit", lambda: invoke(native, "AuditPackageDependencies", {"packageNames": [asset_path], "bRecursive": True, "maxPackages": 256}), audit)
        elif args.phase == "reload":
            step("new_process", lambda: {"before": parent["editor"]["instance_id"], "after": report["editor"]["instance_id"]}, lambda value: ensure(value["before"] != value["after"], "A new client/session in the same editor is not cold reload"))
            def check_reloaded(result):
                require_success(result)
                ensure(result["data"]["path"] == asset_path and result["data"]["parent_class"] == parent["fixture"]["parent_class"], "Reloaded identity mismatch")
                ensure(result["data"]["dirty"] is False, "Reloaded package is dirty")
            step("reload_readback", lambda: bp("inspect_blueprint", {"blueprint_name": asset_path}), check_reloaded)
            def check_graph(result):
                found = {x["node_id"] for x in nodes(result)}
                ensure(parent["fixture"]["remaining_node_id"] in found, "Saved remaining node was not reloaded")
                ensure(parent["fixture"]["deleted_node_id"] not in found, "Deleted node reappeared after reload")
            step("reload_graph", lambda: gr("inspect_graph"), check_graph)
            step("dependency_audit", lambda: invoke(native, "AuditPackageDependencies", {"packageNames": [asset_path], "bRecursive": True, "maxPackages": 256}), audit)
            report["reload"] = {"status": "passed", "method": "different_editor_instance_identity_and_saved_graph_readback"}
        else:
            assets = discover("AssetTools")
            step("cleanup_delete", lambda: invoke(assets, "delete", {"path": asset_path}, True), lambda result: ensure(result is True, "Exact fixture deletion failed"))
            step("cleanup_absent", lambda: bp("inspect_blueprint", {"blueprint_name": asset_path}), expect_rejected("blueprint_not_found"))
            report["cleanup"] = {"status": "passed", "deleted_asset": asset_path, "directory": "empty virtual folder may remain; no recursive folder deletion"}
        report["status"] = "pass"
    except Exception as exc:
        report["status"] = "fail"
        report["error"] = str(exc)
        if isinstance(exc, MCPError):
            report["outcome_unknown"] = report["outcome_unknown"] or exc.outcome_unknown
        report["retry_guidance"] = "No automatic retry. Inspect operation journal and fixture first; retain failed-stage evidence."
    finally:
        report["calls"] = client.observations
        report["call_count"] = len(client.observations)
        stage_results = [entry.get("result") for entry in report["stages"] if isinstance(entry.get("result"), dict)]
        report["compile_attempt_count"] = sum(value.get("compile", {}).get("attempted") is True for value in stage_results)
        report["save_attempt_count"] = sum(value.get("save", {}).get("attempted") is True for value in stage_results)
        report["mutation_request_count"] = sum(call.get("mutation") is True for call in client.observations)
        report["observed_changes"] = [change for value in stage_results for change in value.get("changes", [])]
        report["elapsed_seconds"] = round(time.perf_counter() - started, 6)
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        client.close()
        report["report_verification"] = verify(report)
        if report["status"] == "pass" and not report["report_verification"]["passed"]:
            report["status"] = "fail"
        persist()
    print(json.dumps({"status": report["status"], "phase": args.phase, "report": str(args.output), "asset_path": asset_path, "error": report.get("error", "")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
