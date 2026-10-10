"""Synthetic and adversarial tests; no Unreal process or asset changes."""
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
from pipelines import material_add_set_edges as pipeline
from verification import material_add_set_edges_report as validator


def outcome(success=True):
    return {"success": success, "error_code": "", "error": "", "stage": "complete", "changes": [],
            "compile": {"requested": False}, "save": {"saved": False}, "retry": {"automatic_retry": False},
            "validation": {"native_postcondition_passed": success},
            "data": {"native_outcome": {"change_state_known": True, "changed": success, "mutation_attempted": success, "postcondition_passed": success}}}


class FakeClient:
    def __init__(self, endpoint, timeout): self.state = CURRENT; self.observations = []
    def initialize(self): return {"synthetic": True}
    def list_tools(self): return {"tools": []}
    def close(self): pass
    def call(self, name, arguments=None):
        value = "\n".join("- " + key + ": synthetic" for key in self.state["schemas"]) if name == "list_toolsets" else self.state["schemas"][arguments["toolset_name"]]
        return {"structuredContent": {"returnValue": value}}
    def call_toolset(self, toolset, tool, arguments=None, mutation=False):
        args = arguments or {}; state = self.state
        state["calls"].append((tool, copy.deepcopy(args))); self.observations.append({"tool": tool, "mutation": mutation})
        if tool == "GetEditorDiagnostics":
            value = {"success": True, "engine_version": "5.8.3-synthetic", "project_file": state["project"], "instance_id": state["instance"], "game_thread": True, "pie_or_sie_active": False, "dirty_packages": [state["package"]] if state["dirty"] else []}
        elif tool == "exists": value = state["package"] is not None
        elif tool == "create_material":
            state["package"] = args["folder_path"] + "/" + args["asset_name"]; state["material"] = state["package"] + "." + args["asset_name"]; state["dirty"] = True; value = {"refPath": state["material"]}
        elif tool == "add_material_node":
            position = args.get("node_position", [0, 0]); valid = type(position) is list and len(position) == 2 and all(type(x) is int and -(2**31) <= x < 2**31 for x in position)
            if position is None and state.get("null_skipped"):
                value = outcome(False); value.update(stage="preflight", error_code="material_preflight_failed", error="Material not found: ")
                return {"structuredContent": {"returnValue": value}}
            if not valid:
                if state.get("converter_reject"):
                    if state.get("converter_direct"):
                        return {"isError": True, "content": [{"type": "text", "text": validator.POSITION_CONVERTER_ERROR}]}
                    return {"isError": True, "content": [{"type": "text", "text": 'Function "add_material_node", could not convert incoming function input params Json to a UStruct.\n' + json.dumps(args)}]}
                value = outcome(False); value.update(stage="preflight", error_code="invalid_material_node_position", error="position invalid")
            else:
                assert args["material_path"] == state["package"], "Invalid array unexpectedly reached asset resolver"
                path = state["material"] + ":MaterialExpressionConstant_" + str(len(state["nodes"]))
                props = json.loads(args["properties_json"]); props.update(MaterialExpressionEditorX=position[0], MaterialExpressionEditorY=position[1]); state["nodes"][path] = props; state["dirty"] = True
                value = outcome(); value["data"].update(object_path=path, node_key=path); value["changes"] = [{"kind": "create", "attempted": True}]
        elif tool == "set_material_node_property":
            value = outcome()
            try: incoming = json.loads(args["value_json"])
            except ValueError:
                value = outcome(False); value.update(stage="preflight", error_code="invalid_material_json", error="Extra data")
            else:
                if args["property_name"] == "R" and type(incoming) not in (int, float) and not state.get("accept_bad_numeric"):
                    value = outcome(False); value.update(stage="preflight", error_code="material_mutation_target_rejected", error="Numeric property requires a finite number: R")
                else:
                    state["nodes"][args["node_id"]][args["property_name"]] = incoming; state["dirty"] = True; value["changes"] = [{"kind": "set", "attempted": True}]
        elif tool == "get_properties":
            assert type(args["properties"]) is list and args["properties"] == validator.PROPERTIES
            value = copy.deepcopy(state["nodes"][args["instance"]["refPath"]])
        elif tool == "get_expressions": value = [{"refPath": name} for name in state["nodes"]]
        elif tool == "save_assets":
            assert args["asset_paths"] == [state["package"]]; state["save_count"] += 1; state["dirty"] = False; value = True
        elif tool == "is_dirty": value = state["dirty"]
        else: raise AssertionError("Unexpected fake tool: " + tool)
        return {"structuredContent": {"returnValue": copy.deepcopy(value)}}


def schemas():
    result = {}; path = validator.ROOT / "Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58/registration/material_toolset.py"
    cls = next(n for n in ast.parse(path.read_text(encoding="utf-8")).body if isinstance(n, ast.ClassDef) and n.name == "CubelessMaterialTools")
    items = []
    for fn in (n for n in cls.body if isinstance(n, ast.FunctionDef)):
        items.append({"name": fn.name, "inputSchema": {"properties": {a.arg: {} for a in fn.args.args}}})
    result["Synthetic.CubelessMaterialTools"] = {"tools": items}
    for ts, tools in {"CubelessNativeToolset": {"GetEditorDiagnostics": []}, "AssetTools": {"exists": ["path"], "save_assets": ["asset_paths"], "is_dirty": ["asset_path"]}, "ObjectTools": {"get_properties": ["instance", "properties"]}, "MaterialTools": {"create_material": ["folder_path", "asset_name"], "get_expressions": ["material_or_function"]}}.items():
        result["Synthetic." + ts] = {"tools": [{"name": n, "inputSchema": {"properties": {p: {} for p in params}}} for n, params in tools.items()]}
    return result


class EdgeTests(unittest.TestCase):
    def setUp(self):
        global CURRENT
        temporary = tempfile.TemporaryDirectory(prefix="cubeless-material-edges-"); self.addCleanup(temporary.cleanup); self.temp = Path(temporary.name)
        project = self.temp / "Allowed.uproject"; project.write_text("{}", encoding="utf-8")
        for module in (pipeline, validator):
            patcher = patch.object(module, "ALLOWED_PROJECT", project); patcher.start(); self.addCleanup(patcher.stop)
        CURRENT = {"schemas": schemas(), "project": str(project.resolve()), "instance": "author", "package": None, "material": None, "nodes": {}, "dirty": False, "calls": [], "save_count": 0}
        self.state = CURRENT
    def execute(self, phase, parent=None):
        path = self.temp / (phase + ".json"); args = ["--endpoint", "http://synthetic.invalid/mcp", "--expected-project", self.state["project"], "--phase", phase, "--output", str(path)]
        if parent: args += ["--parent-report", str(parent)]
        with patch.object(pipeline, "OfficialMCPClient", FakeClient), contextlib.redirect_stdout(io.StringIO()): code = pipeline.main(args)
        return code, path, json.loads(path.read_text(encoding="utf-8"))
    def test_author_and_new_process_reload(self):
        code, path, report = self.execute("author"); self.assertEqual(code, 0, report.get("error")); self.assertTrue(validator.verify(report)["passed"]); self.assertEqual(self.state["save_count"], 1)
        self.state["instance"] = "reload"; code, _, reload = self.execute("reload", path); self.assertEqual(code, 0, reload.get("error")); self.assertTrue(validator.verify(reload, parent=report, parent_bytes=path.read_bytes())["passed"]); self.assertEqual(self.state["save_count"], 1)
    def test_official_converter_decline_is_preserved(self):
        self.state["converter_reject"] = True; code, _, report = self.execute("author"); self.assertEqual(code, 0, report.get("error")); self.assertTrue(report["stages"][1]["result"]["raw"]["isError"])
    def test_null_skipped_by_engine_is_only_an_observation(self):
        self.state["null_skipped"] = True; code, _, report = self.execute("author"); self.assertEqual(code, 0, report.get("error")); self.assertFalse(report["null_position_must_reject"]); self.assertIn("material_preflight_failed", report["null_position_observation"])
    def test_direct_converter_error_is_exact_and_preserved(self):
        self.state.update(converter_reject=True, converter_direct=True)
        code, _, report = self.execute("author")
        self.assertEqual(code, 0, report.get("error")); self.assertTrue(validator.verify(report)["passed"])
        probe = copy.deepcopy(report["stages"][1]["result"])
        self.assertEqual(probe["raw"]["content"][0]["text"], validator.POSITION_CONVERTER_ERROR)
        for text in ("Parameter error: Other failure", validator.POSITION_CONVERTER_ERROR + " extra", validator.POSITION_CONVERTER_ERROR.replace("node_position", "other_field")):
            with self.subTest(text=text), self.assertRaises(AssertionError):
                probe["raw"]["content"][0]["text"] = text
                validator.rejected(probe, "position")
    def test_unexpected_type_acceptance_reads_actual_then_stops(self):
        self.state["accept_bad_numeric"] = True; code, _, report = self.execute("author"); self.assertEqual(code, 1); self.assertEqual(report["failed_stage"], "read_after_boolean"); self.assertIs(report["stages"][-1]["result"]["R"], True); self.assertEqual(self.state["save_count"], 0)
        self.assertEqual(sum(t == "set_material_node_property" and a["value_json"] == "true" for t, a in self.state["calls"]), 1)
    def test_same_process_reload_rejected(self):
        _, path, _ = self.execute("author"); code, _, report = self.execute("reload", path); self.assertEqual(code, 1); self.assertIn("new editor process", report["error"])
    def test_changed_defaults_positions_and_raw_evidence_rejected(self):
        _, _, report = self.execute("author")
        for mutate in (lambda r: r["nodes"].update(default="/Game/Other.Other:Node"), lambda r: r["save"].update(explicit_calls=2), lambda r: r["compile"].update(shader_completion_verified=True), lambda r: r["stages"][1]["result"]["decoded"].update(error_code="material_preflight_failed")):
            changed = copy.deepcopy(report); mutate(changed); self.assertFalse(validator.verify(changed)["passed"])
        changed = copy.deepcopy(report); call = next(c for c in changed["tool_calls"] if c["tool"] == "add_material_node" and c["arguments"]["material_path"]); call["arguments"]["node_position"] = [0, 0]; self.assertFalse(validator.verify(changed)["passed"])
    def test_wrong_failure_kind_and_string_array_cannot_pass(self):
        _, _, report = self.execute("author")
        changed = copy.deepcopy(report); next(r for r in changed["stages"] if r["name"] == "reject_null")["result"] = "python_json_parse_rejected"; self.assertFalse(validator.verify(changed)["passed"])
        changed = copy.deepcopy(report); next(c for c in changed["tool_calls"] if c["tool"] == "get_properties")["arguments"]["properties"] = "R,Desc"; self.assertFalse(validator.verify(changed)["passed"])
    def test_wrong_project_blocked_before_client(self):
        with patch.object(pipeline, "OfficialMCPClient") as client, self.assertRaises(AssertionError):
            pipeline.main(["--endpoint", "http://synthetic.invalid/mcp", "--expected-project", str(self.temp / "Other.uproject"), "--phase", "author", "--output", str(self.temp / "wrong.json")])
        client.assert_not_called()


if __name__ == "__main__":
    stream = io.StringIO(); result = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(EdgeTests))
    report = {"schema": "cubeless_material_add_set_edges_offline_tests_v1", "passed": result.wasSuccessful(), "tests_run": result.testsRun, "scope": "Synthetic source-derived tool signatures; no Unreal/native converter execution", "details": stream.getvalue()}
    if len(sys.argv) > 1:
        with Path(sys.argv[1]).open("x", encoding="utf-8") as f: json.dump(report, f, indent=2)
    print(json.dumps(report)); raise SystemExit(0 if result.wasSuccessful() else 1)
