"""Synthetic MCP batch pipeline execution and adversarial evidence checks."""
import ast
import contextlib
import copy
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from pipelines import blueprint_batch_smoke as smoke
from verification.batch_pipeline_report import verify

ROOT = Path(__file__).resolve().parents[3]


def response(success=True):
    return {"success": success, "stage": "complete", "operation_id": "synthetic", "error_code": "", "error": "", "changes": [],
            "compile": {"requested": False, "entries": []}, "save": {"requested": False, "saved": False, "entries": []}, "validation": {},
            "retry": {"automatic_retry": False, "replayed": False}, "data": {}}


class FakeClient:
    def __init__(self, endpoint, timeout):
        self.state = CURRENT; self.observations = []
    def initialize(self): return {"synthetic": True}
    def list_tools(self): return {"tools": []}
    def close(self): pass
    def call(self, name, arguments=None):
        value = "\n".join("- " + key + ": synthetic" for key in self.state["schemas"]) if name == "list_toolsets" else self.state["schemas"][arguments["toolset_name"]]
        return {"structuredContent": {"returnValue": value}}
    def action(self, tool, args):
        state = self.state; value = response()
        package = args.get("blueprint_name")
        if tool == "create_blueprint":
            package = args["package_path"] + "/" + args["name"]
            if package in state["assets"]:
                value.update(success=False, stage="preflight", error_code="asset_already_exists", error="duplicate")
                return value, False
            state["assets"][package] = {"dirty": True, "nodes": []}
            value["changes"] = [{"kind": "create", "created": True}]
            value["data"] = {"path": package, "dirty": True, "parent_class": "/Script/Engine.Actor"}
            return value, True
        asset = state["assets"][package]
        if tool == "inspect_blueprint":
            value["data"] = {"path": package, "dirty": asset["dirty"], "parent_class": "/Script/Engine.Actor"}
            return value, False
        if tool == "compile_and_save_blueprint":
            asset["dirty"] = False; state["compile_calls"] += 1; state["save_calls"] += 1
            value["compile"].update(requested=True, attempted=True, compiled=True, error_count=0, status="up_to_date")
            value["save"].update(requested=True, attempted=True, saved=True, status="saved")
            value["validation"]["status"] = "compile_passed"
            value["data"].update(mutation_performed=True, validation_pass=True, compile_status="passed", compile_error_count=0, compile_errors=[], package_dirty_after=False, after={"dirty": False})
            return value, True
        nodes = asset["nodes"]
        if tool == "inspect_graph":
            value["data"] = {"graph_name": "EventGraph", "node_count": len(nodes), "nodes": copy.deepcopy(nodes)}
            return value, False
        value["data"].update(contract_version="mutation_result_v1", postcondition_verified=True, verified_success=True)
        if tool == "add_branch_node":
            node_id = package + "." + package.rsplit("/", 1)[1] + ":EventGraph.Branch_" + str(len(nodes))
            node = {"node_id": node_id, "x": args.get("x", 0), "y": args.get("y", 0), "pins": [{"name": "then", "links": []}, {"name": "execute", "links": []}]}
            nodes.append(node); value["data"]["node_id"] = node_id
        elif tool == "set_node_position":
            node = next(node for node in nodes if node["node_id"] == args["node_id"])
            node.update(x=args["x"], y=args["y"])
        elif tool == "connect_graph_pins":
            source = next(node for node in nodes if node["node_id"] == args["source_node_id"])
            source["pins"][0]["links"] = [{"node_id": args["target_node_id"], "pin_name": args["target_pin_name"]}]
            target = next(node for node in nodes if node["node_id"] == args["target_node_id"])
            target["pins"][1]["links"] = [{"node_id": args["source_node_id"], "pin_name": args["source_pin_name"]}]
        elif tool == "delete_node":
            asset["nodes"] = [node for node in nodes if node["node_id"] != args["node_id"]]
            for node in asset["nodes"]:
                for pin in node["pins"]: pin["links"] = [link for link in pin["links"] if link["node_id"] != args["node_id"]]
        else: raise AssertionError("Unexpected fake action: " + tool)
        asset["dirty"] = True; value["changes"] = [{"kind": tool, "attempted": True}]
        return value, True
    def batch(self, args):
        commands = json.loads(args["commands_json"]); op = args["operation_id"]; stop = args["stop_on_error"]
        if any(command["type"] == "CubelessUnsupported.never_execute" for command in commands):
            value = response(False); value.update(error_code="batch_unsupported_command", stage="preflight"); value["data"] = {"admission_rejected": True, "actions_executed": 0}; return value
        fingerprint = json.dumps([commands, stop], sort_keys=True)
        if op in self.state["journal"]:
            old_fingerprint, old = self.state["journal"][op]
            if fingerprint != old_fingerprint:
                value = response(False); value.update(error_code="operation_id_conflict", stage="preflight"); return value
            value = copy.deepcopy(old); value["retry"]["replayed"] = True; return value
        value = response(); results = []
        summary = {"manifest_version": "blueprint-graph-v1", "total": len(commands), "stop_on_error": stop, "results": results, "forced_stop": False, "mutation_state_unknown": False, "command_may_still_be_in_flight": False, "stopped_at_index": -1, "transaction": {"atomic_rollback": False, "disk_save_rollback": False, "group_undo_verified": False}}
        for index, cmd in enumerate(commands):
            child, mutation = self.action(cmd["type"].split(".")[-1], cmd["params"])
            results.append({"index": index, "type": cmd["type"], "params": cmd["params"], "phase": "returned", "status": "success" if child["success"] else "error", "response": child, "mutation_attempted": mutation, "mutation_performed": mutation, "mutation_state_unknown": False, "command_may_still_be_in_flight": False, "error_code": child["error_code"]})
            for field in ("compile", "save"):
                if child[field]["requested"]: value[field]["entries"].append(copy.deepcopy(child[field]))
            if not child["success"] and stop:
                summary["stopped_at_index"] = index; break
        failed = sum(not row["response"]["success"] for row in results)
        summary.update(executed=len(results), dispatched_count=len(results), succeeded=len(results)-failed, failed=failed, not_run=list(range(len(results), len(commands))), success=not failed, partial_mutation=bool(failed and any(row["mutation_attempted"] for row in results)))
        value["success"] = not failed; value["data"]["batch"] = summary
        value["compile"]["requested"] = bool(value["compile"]["entries"]); value["save"]["requested"] = bool(value["save"]["entries"])
        self.state["journal"][op] = (fingerprint, copy.deepcopy(value)); return value
    def call_toolset(self, toolset, tool, arguments, mutation=False):
        self.observations.append({"tool": tool, "arguments": arguments, "mutation": mutation, "synthetic": True})
        if tool == "GetEditorDiagnostics":
            value = {"success": True, "project_file": self.state["project"], "engine_version": "5.8.3-synthetic", "game_thread": True, "pie_or_sie_active": False, "instance_id": self.state["instance"], "dirty_packages": [path for path, asset in self.state["assets"].items() if asset["dirty"]]}
        elif tool == "execute_batch": value = self.batch(arguments)
        elif tool == "exists": value = arguments["path"] in self.state["assets"]
        else: value, _ = self.action(tool, arguments)
        return {"structuredContent": {"returnValue": value}}


def schemas_from_source():
    root = ROOT / "Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58/registration"
    result = {}
    for filename, target in (("toolsets.py", "CubelessBlueprintTools"), ("graph_toolset.py", "CubelessBlueprintGraphTools"), ("batch_toolset.py", "CubelessBatchTools")):
        tree = ast.parse((root / filename).read_text(encoding="utf-8")); cls = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == target)
        functions = [node for node in cls.body if isinstance(node, ast.FunctionDef) and not node.name.startswith("_")]
        tools = []
        for fn in functions:
            names = [arg.arg for arg in fn.args.args]
            tools.append({"name": fn.name, "inputSchema": {"properties": {name: {} for name in names}, "required": names[:len(names)-len(fn.args.defaults)]}})
        result["Synthetic." + target] = {"tools": tools}
    result["Synthetic.CubelessNativeToolset"] = {"tools": [{"name": "GetEditorDiagnostics", "inputSchema": {"properties": {}}}]}
    result["Synthetic.AssetTools"] = {"tools": [{"name": "exists", "inputSchema": {"properties": {"path": {}}, "required": ["path"]}}]}
    return result


class BatchPipelineTests(unittest.TestCase):
    def setUp(self):
        global CURRENT
        self.temp = tempfile.TemporaryDirectory(prefix="cubeless-batch-pipeline-"); self.addCleanup(self.temp.cleanup); self.root = Path(self.temp.name)
        project = self.root / "Synthetic.uproject"; project.write_text("{}", encoding="utf-8")
        CURRENT = {"schemas": schemas_from_source(), "project": str(project.resolve()), "instance": "author", "assets": {}, "journal": {}, "compile_calls": 0, "save_calls": 0}
        self.state = CURRENT
    def run_phase(self, phase, parent=None):
        output = self.root / (phase + ".json")
        args = ["--endpoint", "http://synthetic.invalid/mcp", "--expected-project", self.state["project"], "--phase", phase, "--output", str(output)]
        if parent: args += ["--parent-report", str(parent)]
        with patch.object(smoke, "OfficialMCPClient", FakeClient), contextlib.redirect_stdout(io.StringIO()): code = smoke.main(args)
        return code, output, json.loads(output.read_text(encoding="utf-8"))
    def test_author_and_distinct_process_reload_pass(self):
        code, path, author = self.run_phase("author"); self.assertEqual(code, 0, author.get("error")); self.assertEqual((self.state["compile_calls"], self.state["save_calls"]), (5, 5))
        self.assertTrue(verify(author)["passed"])
        self.state["instance"] = "reload"
        code, _, reload = self.run_phase("reload", path); self.assertEqual(code, 0, reload.get("error"))
        self.assertTrue(verify(reload, parent=author, parent_bytes=path.read_bytes())["passed"])
        self.assertEqual((self.state["compile_calls"], self.state["save_calls"]), (5, 5))
    def test_same_process_reload_is_rejected(self):
        _, path, _ = self.run_phase("author"); code, _, report = self.run_phase("reload", path)
        self.assertEqual(code, 1); self.assertIn("new editor process", report["error"])
    def test_verifier_rejects_fake_counts_and_wrong_child_order(self):
        _, _, report = self.run_phase("author")
        for change in (lambda summary: summary.update(executed=99), lambda summary: summary["results"][0].update(type="CubelessBlueprintTools.inspect_blueprint"), lambda summary: summary.update(forced_stop=True), lambda summary: summary["transaction"].update(atomic_rollback=True)):
            modified = copy.deepcopy(report); value = next(row for row in modified["stages"] if row["name"] == "batch_lifecycle")["result"]["data"]["batch"]; change(value); self.assertFalse(verify(modified)["passed"])
    def test_verifier_rechecks_failure_readback_and_saved_graph(self):
        _, _, report = self.run_phase("author")
        for stage, edit in (("stop_true_dirty_readback", lambda v: v["data"].update(dirty=False)), ("unsupported_prefix_absent", None), ("graph_independent_readback", lambda v: v["data"].update(nodes=[])), ("stop_false", lambda v: v["data"]["batch"]["results"][1].update(mutation_performed=True))):
            modified = copy.deepcopy(report); row = next(row for row in modified["stages"] if row["name"] == stage)
            if edit: edit(row["result"])
            else: row["result"] = True
            self.assertFalse(verify(modified)["passed"], stage)
    def test_replay_and_compile_workload_cannot_be_counterfeited(self):
        _, _, report = self.run_phase("author")
        modified = copy.deepcopy(report); next(row for row in modified["stages"] if row["name"] == "replay_lifecycle")["result"]["retry"]["replayed"] = False; self.assertFalse(verify(modified)["passed"])
        modified = copy.deepcopy(report); modified["performance"]["explicit_compile_calls_each"] = 0; self.assertFalse(verify(modified)["passed"])
    def test_failed_or_missing_stage_cannot_pass(self):
        _, _, report = self.run_phase("author")
        for modified in (dict(report, status="fail"), dict(report, stages=report["stages"][:-1]), dict(report, outcome_unknown=True)):
            self.assertFalse(verify(modified)["passed"])
    def test_one_way_or_dangling_graph_links_are_rejected(self):
        _, _, report = self.run_phase("author")
        modified = copy.deepcopy(report)
        connected = next(row for row in modified["stages"] if row["name"] == "graph_edit_batch")["result"]["data"]["batch"]["results"][2]["response"]["data"]
        target = next(node for node in connected["nodes"] if node["node_id"] == modified["fixture"]["deleted_node_id"])
        next(pin for pin in target["pins"] if pin["name"] == "execute")["links"] = []
        self.assertFalse(verify(modified)["passed"])
        modified = copy.deepcopy(report)
        deleted = next(row for row in modified["stages"] if row["name"] == "graph_edit_batch")["result"]["data"]["batch"]["results"][4]["response"]["data"]
        deleted["nodes"][0]["pins"][0]["links"] = [{"node_id": modified["fixture"]["deleted_node_id"], "pin_name": "execute"}]
        self.assertFalse(verify(modified)["passed"])
    def test_saved_graph_cannot_change_after_independent_readback(self):
        _, _, report = self.run_phase("author")
        for mutation in ("remove", "move"):
            modified = copy.deepcopy(report)
            saved = next(row for row in modified["stages"] if row["name"] == "graph_compile_save_batch")["result"]["data"]["batch"]["results"][1]["response"]["data"]
            if mutation == "remove": saved.update(nodes=[], node_count=0)
            else: saved["nodes"][0]["x"] = 999
            modified["saved_graph"] = copy.deepcopy(saved)
            self.assertFalse(verify(modified)["passed"], mutation)
    def test_compile_or_save_contradictions_cannot_pass(self):
        _, _, report = self.run_phase("author")
        for edit in (lambda v: v["data"].update(validation_pass=False), lambda v: v["compile"].update(error_count=1), lambda v: v["compile"].update(status="error"), lambda v: v["data"].update(compile_errors=["bad"]), lambda v: v["validation"].update(status="failed"), lambda v: v["data"].update(package_dirty_after=True), lambda v: v["data"]["after"].update(dirty=True), lambda v: v["save"].update(status="failed")):
            modified = copy.deepcopy(report)
            value = next(row for row in modified["stages"] if row["name"] == "standalone_compile_save")["result"]
            edit(value); self.assertFalse(verify(modified)["passed"])


if __name__ == "__main__":
    stream = io.StringIO(); result = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(BatchPipelineTests))
    report = {"schema": "cubeless_batch_pipeline_offline_tests_v1", "passed": result.wasSuccessful(), "tests_run": result.testsRun, "scope": "Synthetic official client with source-derived signatures; no Unreal/native execution", "details": stream.getvalue()}
    if len(sys.argv) > 1:
        with Path(sys.argv[1]).open("x", encoding="utf-8") as output: json.dump(report, output, indent=2)
    print(json.dumps(report)); raise SystemExit(0 if result.wasSuccessful() else 1)
