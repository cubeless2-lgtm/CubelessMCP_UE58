"""Bounded Blueprint/graph batch smoke through the existing official MCP endpoint.

Author and new-process reload are separate. No editor launch/stop, cleanup,
automatic mutation retry, hidden executor, or full legacy-batch parity claim.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, MCPError, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result, require_success
from verification.batch_pipeline_report import batch_result, graph, identity, require, saved, verify


def command(tool, **params):
    return {"type": tool, "params": params}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True); parser.add_argument("--expected-project", type=Path, required=True)
    parser.add_argument("--phase", choices=["author", "reload"], required=True); parser.add_argument("--parent-report", type=Path)
    parser.add_argument("--output", type=Path, required=True); parser.add_argument("--timeout", type=float, default=180)
    args = parser.parse_args(argv)
    require(not args.output.exists(), "Refusing to overwrite evidence")
    parent = None
    if args.phase == "reload":
        require(args.parent_report is not None, "Reload requires a parent report")
        parent = json.loads(args.parent_report.read_text(encoding="utf-8-sig"))
        require(verify(parent, require_phase="author")["passed"], "Parent failed independent verification")
        require(Path(parent["project_file"]).resolve() == args.expected_project.resolve(), "Parent belongs to a different project")
    run_id = parent["run_id"] if parent else uuid.uuid4().hex
    require(re.fullmatch("[a-f0-9]{32}", run_id), "Invalid run ID")
    root = "/Game/CubelessMCPTests/Batch_" + run_id
    assets = {key: root + "/" + name for key, name in {"batch": "BP_Batch", "standalone": "BP_Standalone", "stop_true": "BP_StopTrue", "stop_false": "BP_StopFalse", "unsupported": "BP_UnsupportedNever"}.items()}
    report = {"schema": "cubeless_blueprint_batch_pipeline_v1", "phase": args.phase, "status": "running", "run_id": run_id,
              "endpoint": args.endpoint, "project_file": str(args.expected_project.resolve()), "asset_root": root, "assets": assets,
              "started_ns": time.time_ns(), "stages": [], "outcome_unknown": False, "automatic_retries": 0, "atomic_rollback": False, "full_legacy_batch_parity": False,
              "undo": {"status": "not_run", "reason": "Public ToolsetLibrary UndoTransaction/GetActiveUndoCount confirmed in source; direct official invocation plus newest-transaction ownership is not implemented in this HTTP smoke"},
              "packaging": "not_run", "cleanup": "not_run", "scope": "Ten allowed Blueprint/graph actions; no arbitrary legacy command coverage or asynchronous batch completion"}
    report["input_fingerprint"] = hashlib.sha256(json.dumps({key: report[key] for key in ("phase", "run_id", "project_file", "endpoint", "assets")}, sort_keys=True).encode()).hexdigest()
    if parent:
        report.update(author_report_sha256=hashlib.sha256(args.parent_report.read_bytes()).hexdigest(), author_editor=parent["editor"], fixture=parent["fixture"])
    client = OfficialMCPClient(args.endpoint, args.timeout)
    schemas = {}

    def persist():
        args.output.parent.mkdir(parents=True, exist_ok=True)
        temp = args.output.with_suffix(args.output.suffix + ".partial")
        temp.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"); temp.replace(args.output)

    def invoke(toolset, tool, params=None, mutation=False):
        choices = [row for row in schemas[toolset]["tools"] if row["name"].rsplit(".", 1)[-1] == tool]
        require(len(choices) == 1, "Expected one live tool: " + tool)
        schema = choices[0].get("inputSchema", {})
        mapped = {}
        for key, value in (params or {}).items():
            names = [name for name in schema.get("properties", {}) if canonical(name) == canonical(key)]
            require(len(names) == 1, "Input missing/ambiguous in live schema: " + key)
            mapped[names[0]] = value
        require(not set(schema.get("required", [])) - set(mapped), "Required live inputs were not provided")
        raw = client.call_toolset(toolset, tool, mapped, mutation=mutation)
        try:
            return operation_result(raw)
        except MCPError as error:
            error.outcome_unknown = error.outcome_unknown or mutation; raise
        except (ValueError, TypeError, KeyError) as error:
            raise MCPError(str(error), outcome_unknown=mutation, detail=raw) from error

    def step(name, action, check=require_success, mutation=False):
        row = {"name": name, "passed": False, "mutation": mutation}
        report["stages"].append(row); persist(); started = time.perf_counter()
        before_calls = len(client.observations)
        try:
            row["result"] = action(); check(row["result"]); row["passed"] = True; return row["result"]
        except Exception as error:
            row["error"] = str(error)
            if isinstance(error, MCPError):
                row["mcp_error"] = error.detail; report["outcome_unknown"] = report["outcome_unknown"] or error.outcome_unknown
            raise
        finally:
            row["elapsed_seconds"] = time.perf_counter() - started
            row["http_observations"] = len(client.observations) - before_calls; persist()

    try:
        persist(); report["server"] = client.initialize(); report["top_level_schema"] = client.list_tools()
        catalog = tool_payload(client.call("list_toolsets"))
        require(isinstance(catalog, str), "Unexpected toolset catalog")
        names = re.findall(r"^- ([\w.]+):", catalog, re.MULTILINE)
        def discover(suffix):
            matching = [name for name in names if name.endswith("." + suffix)]
            require(len(matching) == 1, "Expected exactly one toolset: " + suffix)
            name = matching[0]; schemas[name] = tool_payload(client.call("describe_toolset", {"toolset_name": name})); return name
        native, batches, blueprints, graphs, official_assets = [discover(name) for name in ("CubelessNativeToolset", "CubelessBatchTools", "CubelessBlueprintTools", "CubelessBlueprintGraphTools", "AssetTools")]
        report["discovered_schemas"] = schemas
        def preflight(value):
            require_success(value)
            require(Path(value["project_file"]).resolve() == args.expected_project.resolve() and str(value["engine_version"]).startswith("5.8.3"), "Wrong project/engine")
            require(value.get("game_thread") is True and value.get("pie_or_sie_active") is False and not value.get("dirty_packages"), "Require a clean idle isolated editor")
        report["editor"] = step("preflight", lambda: invoke(native, "GetEditorDiagnostics"), preflight)
        def execute(commands, *, label, stop=True, operation_id=None, mutation=True):
            return invoke(batches, "execute_batch", {"commands_json": json.dumps(commands), "stop_on_error": stop, "operation_id": operation_id or run_id + "_" + label}, mutation)
        def bp(tool, package, mutation=False, **extra):
            params = {"blueprint_name": package, **extra}
            if mutation: params["operation_id"] = run_id + "_" + str(len(report["stages"])) + "_" + tool
            return invoke(blueprints, tool, params, mutation)
        def create(package):
            return command("CubelessBlueprintTools.create_blueprint", name=package.rsplit("/", 1)[1], parent_class="/Script/Engine.Actor", package_path=root)
        def inspect(package): return command("CubelessBlueprintTools.inspect_blueprint", blueprint_name=package)
        def compile_save(package): return command("CubelessBlueprintTools.compile_and_save_blueprint", blueprint_name=package, save=True)
        def graph_command(tool, **params): return command("CubelessBlueprintGraphTools." + tool, blueprint_name=assets["batch"], **params)

        if args.phase == "author":
            lifecycle = [create(assets["batch"]), compile_save(assets["batch"]), inspect(assets["batch"])]
            op = run_id + "_lifecycle"
            def lifecycle_check(value):
                children = batch_result(value, total=3); saved(children[1]); identity(children[2], assets["batch"])
            first = step("batch_lifecycle", lambda: execute(lifecycle, label="lifecycle", operation_id=op), lifecycle_check, True)
            standalone_params = {**create(assets["standalone"])["params"], "operation_id": run_id + "_standalone_create"}
            step("standalone_create", lambda: invoke(blueprints, "create_blueprint", standalone_params, True), mutation=True)
            step("standalone_compile_save", lambda: bp("compile_and_save_blueprint", assets["standalone"], True, save=True), saved, True)
            step("standalone_inspect", lambda: bp("inspect_blueprint", assets["standalone"]), lambda value: identity(value, assets["standalone"]))
            def replay_check(value):
                lifecycle_check(value); require(value["retry"].get("replayed") is True and value["data"]["batch"] == first["data"]["batch"], "Replay changed batch observations")
            step("replay_lifecycle", lambda: execute(lifecycle, label="replay", operation_id=op), replay_check, True)
            def reject(value, code): require(value.get("success") is False and value.get("error_code") == code and value.get("changes") == [], "Expected no-mutation rejection: " + code)
            step("conflicting_id", lambda: execute([inspect(assets["batch"])], label="conflict", operation_id=op), lambda value: reject(value, "operation_id_conflict"), True)
            step("independent_batch_identity", lambda: bp("inspect_blueprint", assets["batch"]), lambda value: identity(value, assets["batch"]))
            def unsupported_check(value):
                reject(value, "batch_unsupported_command"); require(value["data"].get("admission_rejected") is True and value["data"].get("actions_executed") == 0, "Admission did not prevent all execution")
            step("unsupported_batch", lambda: execute([create(assets["unsupported"]), command("CubelessUnsupported.never_execute")], label="unsupported"), unsupported_check, True)
            step("unsupported_prefix_absent", lambda: invoke(official_assets, "exists", {"path": assets["unsupported"]}), lambda value: require(value is False, "Unsupported prefix asset exists"))
            for key, stop in (("stop_true", True), ("stop_false", False)):
                package = assets[key]
                steps = [create(package), create(package), compile_save(package), inspect(package)]
                step(key, lambda steps=steps, key=key, stop=stop: execute(steps, label=key, stop=stop), lambda value, stop=stop: batch_result(value, total=4, failure_index=1, stop=stop), True)
                if stop:
                    step("stop_true_dirty_readback", lambda: bp("inspect_blueprint", package), lambda value: identity(value, package, True))
                    step("preserve_stop_true", lambda: bp("compile_and_save_blueprint", package, True, save=True), saved, True)
                else:
                    step("stop_false_clean_readback", lambda: bp("inspect_blueprint", package), lambda value: identity(value, package))
            added = step("graph_add_batch", lambda: execute([graph_command("add_branch_node", x=0, y=0), graph_command("add_branch_node", x=300, y=0), graph_command("inspect_graph")], label="graph_add"), lambda value: batch_result(value, total=3), True)
            children = added["data"]["batch"]["results"]
            source, target = [children[index]["response"]["data"]["node_id"] for index in (0, 1)]
            report["fixture"] = {"remaining_node_id": source, "deleted_node_id": target}
            edits = [graph_command("set_node_position", node_id=target, x=600, y=200, expected_x=300, expected_y=0), graph_command("connect_graph_pins", source_node_id=source, source_pin_name="then", target_node_id=target, target_pin_name="execute"), graph_command("inspect_graph"), graph_command("delete_node", node_id=target, allow_exec_linked_delete=True), graph_command("inspect_graph")]
            edited = step("graph_edit_batch", lambda: execute(edits, label="graph_edit"), lambda value: batch_result(value, total=5), True)
            expected_graph = graph(edited["data"]["batch"]["results"][4]["response"])
            step("graph_independent_readback", lambda: invoke(graphs, "inspect_graph", {"blueprint_name": assets["batch"]}), lambda value: require(graph(value) == expected_graph, "Independent graph differs"))
            final = step("graph_compile_save_batch", lambda: execute([compile_save(assets["batch"]), graph_command("inspect_graph")], label="graph_save"), lambda value: (saved(batch_result(value, total=2)[0])), True)
            report["saved_graph"] = graph(final["data"]["batch"]["results"][1]["response"])
            for key, name in (("batch", "batch"), ("stop_true", "true"), ("stop_false", "false"), ("standalone", "standalone")):
                step("final_" + name + "_identity", lambda key=key: bp("inspect_blueprint", assets[key]), lambda value, key=key: identity(value, assets[key]))
            step("final_editor", lambda: invoke(native, "GetEditorDiagnostics"), preflight)
            stages = {row["name"]: row for row in report["stages"]}
            report["performance"] = {"comparison_action_count": 3, "explicit_compile_calls_each": 1, "explicit_save_calls_each": 1, "batch_tool_calls": 1, "standalone_tool_calls": 3, "samples_each": 1,
                "batch_elapsed_seconds": stages["batch_lifecycle"]["elapsed_seconds"], "standalone_elapsed_seconds": sum(stages[key]["elapsed_seconds"] for key in ("standalone_create", "standalone_compile_save", "standalone_inspect")),
                "scope": "Same Actor create+compile/save+inspect action workload; calls include response/check time and durable plugin journal, exclude discovery and report persistence",
                "order": "batch_then_standalone", "limitations": "One sample per path; warmup/order/disk/cache effects uncorrected; no statistically supported speedup claim"}
        else:
            require(report["editor"]["instance_id"] != parent["editor"]["instance_id"], "Cold reload requires a new editor process")
            commands = [inspect(assets[key]) for key in ("batch", "standalone", "stop_true", "stop_false")] + [graph_command("inspect_graph")]
            def reload_check(value):
                children = batch_result(value, total=5)
                for key, child in zip(("batch", "standalone", "stop_true", "stop_false"), children[:4]): identity(child, assets[key])
                require(graph(children[4]) == parent["saved_graph"], "Cold graph differs")
            step("reload_read_batch", lambda: execute(commands, label="cold_reload", mutation=False), reload_check)
            step("reload_editor", lambda: invoke(native, "GetEditorDiagnostics"), preflight)
        report["status"] = "pass"
        checked = verify(report, require_phase=args.phase, parent=parent, parent_bytes=args.parent_report.read_bytes() if parent else None)
        report["self_contract_check"] = checked
        require(checked["passed"], "Completed run failed independent contract: " + "; ".join(checked["errors"]))
    except Exception as error:
        report.update(status="fail", error=str(error), failed_stage=report["stages"][-1]["name"] if report["stages"] else "discovery")
        if isinstance(error, MCPError): report["outcome_unknown"] = report["outcome_unknown"] or error.outcome_unknown
    finally:
        report["ended_ns"] = time.time_ns(); report["elapsed_seconds"] = (report["ended_ns"] - report["started_ns"]) / 1e9
        report["calls"] = client.observations; client.close(); persist()
    print(json.dumps({"status": report["status"], "phase": args.phase, "output": str(args.output), "failed_stage": report.get("failed_stage")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
