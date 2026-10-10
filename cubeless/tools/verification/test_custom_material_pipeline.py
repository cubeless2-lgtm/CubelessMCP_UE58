"""Synthetic orchestration/tampering tests; does not execute Unreal or HLSL."""
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
from pipelines import custom_material_smoke as pipeline
from verification import custom_material_report as validator


def result(success=True, code=""):
    return {"success": success, "error_code": code, "error": "synthetic rejection" if code else "", "stage": "complete" if success else "preflight", "changes": [{"changed": True}] if success else [], "compile": {"requested": False}, "save": {"saved": False}, "retry": {"automatic_retry": False, "replayed": False}, "validation": {"native_postcondition_passed": success}, "data": {"native_outcome": {"mutation_attempted": success, "changed": success, "change_state_known": True, "postcondition_passed": success}}}


def schemas():
    module = validator.ROOT / "Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58/registration/material_toolset.py"
    cls = next(n for n in ast.parse(module.read_text(encoding="utf-8")).body if isinstance(n, ast.ClassDef) and n.name == "CubelessMaterialTools")
    functions = {}
    for f in cls.body:
        if isinstance(f, ast.FunctionDef): functions[f.name] = [a.arg for a in f.args.args]
    definitions = {"CubelessMaterialTools": functions, "CubelessNativeToolset": {"GetEditorDiagnostics": []}, "ObjectTools": {"get_properties": ["instance", "properties"]}, "AssetTools": {"exists": ["path"], "save_assets": ["asset_paths"], "is_dirty": ["asset_path"]}, "MaterialTools": {"create_material": ["folder_path", "asset_name"], "get_expressions": ["material_or_function"], "get_expression_inputs": ["material_or_function", "expression"], "get_property_input": ["material", "material_property"], "recompile": ["material_or_function"]}}
    return {"Synthetic." + ts: {"tools": [{"name": name, "inputSchema": {"properties": {key: {} for key in keys}}} for name, keys in tools.items()]} for ts, tools in definitions.items()}


class Client:
    def __init__(self, endpoint, timeout): self.state = CURRENT; self.observations = []
    def initialize(self): return {"synthetic": True}
    def list_tools(self): return {"tools": []}
    def close(self): pass
    def call(self, name, arguments=None):
        value = "\n".join("- " + key + ": synthetic" for key in self.state["schemas"]) if name == "list_toolsets" else self.state["schemas"][arguments["toolset_name"]]
        return {"structuredContent": {"returnValue": value}}
    def call_toolset(self, toolset, tool, arguments=None, mutation=False):
        a = arguments or {}; s = self.state
        s["calls"].append((tool, copy.deepcopy(a))); self.observations.append({"tool": tool, "mutation": mutation})
        if tool == "GetEditorDiagnostics":
            value = {"success": True, "project_file": s["project"], "instance_id": s["instance"], "game_thread": True, "pie_or_sie_active": False, "dirty_packages": [s["package"]] if s["dirty"] else [], "engine_version": "5.8.3-synthetic"}
        elif tool == "exists": value = s["package"] is not None
        elif tool == "create_material":
            s["package"] = a["folder_path"] + "/" + a["asset_name"]; s["object"] = s["package"] + ".M_Custom"; s["dirty"] = True
            value = {"refPath": s["object"]}
        elif tool == "add_custom_material_node":
            identifier = a["operation_id"]
            if identifier in s["journal"]:
                previous, saved = s["journal"][identifier]
                value = copy.deepcopy(saved) if previous == a else result(False, "operation_id_conflict")
                if previous == a: value["retry"]["replayed"] = True
            else:
                negative = next((key for key in validator.NEGATIVES if identifier.endswith("_negative_" + key)), None)
                if negative and not s.get("accept_bad"):
                    value = result(False, validator.NEGATIVES[negative][1])
                else:
                    path = s["object"] + ":MaterialExpressionCustom_" + str(len(s["nodes"]))
                    alias = a.get("output_type") == "FLOAT3"; position = [-400, 100] if alias else [0, 0]
                    props = {"Code": a["code"], "OutputType": "CMOT_Float3", "Description": validator.DESCRIPTION if alias else "", "Desc": validator.DESCRIPTION if alias else "", "Inputs": [{"inputName": name, "input": {"expression": "None"}} for name in (["Signal", "Bias"] if alias else [])], "AdditionalDefines": [{"defineName": name, "defineValue": v} for name, v in ([("CUBE_SCALE", "0.5"), ("CUBE_FLAG", "1"), ("CUBE_DEFAULT", "1")] if alias else [])], "IncludeFilePaths": [], "MaterialExpressionEditorX": position[0], "MaterialExpressionEditorY": position[1]}
                    s["nodes"][path] = props; s["dirty"] = True
                    value = result(); value["data"].update(object_path=path, node_key="path:" + path, created_custom_node=True, input_count=2 if alias else 0, custom_shader_code_compiled=False)
                s["journal"][identifier] = (copy.deepcopy(a), copy.deepcopy(value))
        elif tool == "get_properties":
            assert a["properties"] == validator.PROPERTIES
            value = s["nodes"][a["instance"]["refPath"]]
        elif tool == "get_expressions": value = [{"refPath": path} for path in s["nodes"]]
        elif tool == "connect_material_nodes":
            source = a["from_node_id"].removeprefix("path:"); target = a["to_node_id"].removeprefix("path:")
            row = next(row for row in s["nodes"][target]["Inputs"] if row["inputName"] == a["to_input"])
            row["input"]["expression"] = {"refPath": source}; s["dirty"] = True; value = result()
        elif tool == "connect_material_property": s["root"] = a["from_node_id"].removeprefix("path:"); s["dirty"] = True; value = result()
        elif tool == "get_expression_inputs":
            value = [{"input_name": row["inputName"], "expression": row["input"]["expression"], "output_name": ""} for row in s["nodes"][a["expression"]["refPath"]]["Inputs"]]
        elif tool == "get_property_input": value = {"input_name": "", "expression": {"refPath": s["root"]}, "output_name": ""}
        elif tool == "recompile": s["compile_count"] += 1; value = {}
        elif tool == "save_assets":
            assert a["asset_paths"] == [s["package"]]; s["save_count"] += 1; s["dirty"] = False
            path = Path(s["project"]).parent / "Content" / (s["package"][len("/Game/"):] + ".uasset")
            path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(json.dumps(s["nodes"]).encode()); value = True
        elif tool == "is_dirty": value = s["dirty"]
        else: raise AssertionError("Unexpected fake tool: " + tool)
        return {"structuredContent": {"returnValue": copy.deepcopy(value)}}


class CustomTests(unittest.TestCase):
    def setUp(self):
        global CURRENT
        temporary = tempfile.TemporaryDirectory(prefix="cubeless-custom-offline-"); self.addCleanup(temporary.cleanup); self.temp = Path(temporary.name)
        project = self.temp / "Allowed.uproject"; project.write_text("{}")
        for module in (pipeline, validator):
            p = patch.object(module, "ALLOWED_PROJECT", project); p.start(); self.addCleanup(p.stop)
        CURRENT = {"project": str(project.resolve()), "schemas": schemas(), "instance": "author", "calls": [], "nodes": {}, "journal": {}, "dirty": False, "package": None, "object": None, "root": None, "save_count": 0, "compile_count": 0}
        self.state = CURRENT
    def execute(self, phase="author", parent=None):
        output = self.temp / (phase + ".json")
        args = ["--endpoint", "http://synthetic.invalid/mcp", "--expected-project", self.state["project"], "--phase", phase, "--output", str(output)]
        if parent: args += ["--parent-report", str(parent)]
        with patch.object(pipeline, "OfficialMCPClient", Client), contextlib.redirect_stdout(io.StringIO()): code = pipeline.main(args)
        return code, output, json.loads(output.read_bytes())
    def test_author_then_fresh_process_cold_readback(self):
        code, path, author = self.execute(); self.assertEqual(code, 0, author.get("error")); self.assertTrue(validator.verify(author)["passed"])
        self.assertEqual((self.state["compile_count"], self.state["save_count"], len(self.state["nodes"])), (1, 1, 2))
        self.state["instance"] = "new-editor"
        code, _, reload = self.execute("reload", path); self.assertEqual(code, 0, reload.get("error"))
        self.assertTrue(validator.verify(reload, parent=author, parent_bytes=path.read_bytes())["passed"])
        self.assertEqual((self.state["compile_count"], self.state["save_count"]), (1, 1))
    def test_unexpected_negative_acceptance_reads_then_stops_without_retry(self):
        self.state["accept_bad"] = True
        code, _, report = self.execute(); self.assertEqual(code, 1); self.assertEqual(report["failed_stage"], "nodes_after_identifier")
        self.assertEqual(len(report["stages"][-1]["result"]), 3)
        self.assertEqual((self.state["compile_count"], self.state["save_count"]), (0, 0))
        self.assertEqual(sum(tool == "add_custom_material_node" and args["operation_id"].endswith("_negative_identifier") for tool, args in self.state["calls"]), 1)
    def test_same_process_reload_blocked(self):
        _, path, _ = self.execute(); code, _, report = self.execute("reload", path)
        self.assertEqual(code, 1); self.assertIn("new editor process", report["error"])
    def test_wrong_project_rejected_before_client(self):
        with patch.object(pipeline, "OfficialMCPClient") as client, self.assertRaises(AssertionError):
            pipeline.main(["--endpoint", "http://synthetic.invalid/mcp", "--expected-project", str(self.temp / "Other.uproject"), "--phase", "author", "--output", str(self.temp / "other.json")])
        client.assert_not_called()
    def test_changed_disk_blocks_reload_before_client(self):
        _, path, report = self.execute(); Path(report["asset_file"]["path"]).write_bytes(b"tampered temporary bytes")
        self.state["instance"] = "other"
        with patch.object(pipeline, "OfficialMCPClient") as client, self.assertRaises(AssertionError): self.execute("reload", path)
        client.assert_not_called()
    def test_raw_readback_mutation_and_scope_tampering_fails(self):
        _, _, report = self.execute()
        for change in (lambda r: r.update(shader_completion_verified=True), lambda r: r.update(external_includes_supported=True), lambda r: r["compile"].update(explicit_calls=2), lambda r: r["save"].update(saved=False), lambda r: r["stages"].pop(10), lambda r: r["nodes"].update(alias=r["nodes"]["default"]), lambda r: r["selectors"].update(alias="path:/Game/Other.Other:Node")):
            with self.subTest(change=change):
                edited = copy.deepcopy(report); change(edited); self.assertFalse(validator.verify(edited)["passed"])
        edited = copy.deepcopy(report); call = next(c for c in edited["tool_calls"] if c["tool"] == "get_properties"); call["decoded"]["Code"] = "wrong"
        self.assertFalse(validator.verify(edited)["passed"])
    def test_coherently_tampered_property_and_wiring_rejected(self):
        _, _, report = self.execute()
        for name, field, value in (("read_alias", "OutputType", "CMOT_Float4"), ("read_alias", "Code", "return 1;"), ("read_alias", "Inputs", []), ("read_alias", "AdditionalDefines", []), ("read_alias", "IncludeFilePaths", ["/External.ush"]), ("independent_root", "expression", {"refPath": report["nodes"]["default"]})):
            with self.subTest(name=name, field=field):
                edited = copy.deepcopy(report); row = next(s for s in edited["stages"] if s["name"] == name); call = edited["tool_calls"][row["call_indices"][0]]
                for target in (row["result"], call["decoded"], call["raw"]["structuredContent"]["returnValue"]): target[field] = value
                self.assertFalse(validator.verify(edited)["passed"])
    def test_coherently_wrong_negative_category_rejected(self):
        _, _, report = self.execute(); row = next(s for s in report["stages"] if s["name"] == "negative_identifier"); call = report["tool_calls"][row["call_indices"][0]]
        for target in (row["result"], call["decoded"], call["raw"]["structuredContent"]["returnValue"]): target["error_code"] = "wrong_asset_not_found"
        self.assertFalse(validator.verify(report)["passed"])
    def test_missing_native_proof_and_wrong_toolset_rejected(self):
        _, _, report = self.execute()
        edited = copy.deepcopy(report); row = next(s for s in edited["stages"] if s["name"] == "negative_identifier"); call = edited["tool_calls"][row["call_indices"][0]]
        for target in (row["result"], call["decoded"], call["raw"]["structuredContent"]["returnValue"]): target["data"].pop("native_outcome", None)
        self.assertFalse(validator.verify(edited)["passed"])
        edited = copy.deepcopy(report); next(c for c in edited["tool_calls"] if c["tool"] == "get_properties")["toolset"] = "Synthetic.CubelessMaterialTools"
        self.assertFalse(validator.verify(edited)["passed"])


if __name__ == "__main__":
    stream = io.StringIO(); result = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(CustomTests))
    evidence = {"schema": "cubeless_custom_material_offline_tests_v1", "passed": result.wasSuccessful(), "tests_run": result.testsRun, "scope": "Synthetic source-derived schemas and return shapes; no native Custom creation, shader compile or Unreal execution", "details": stream.getvalue()}
    if len(sys.argv) > 1:
        with Path(sys.argv[1]).open("x", encoding="utf-8") as stream: json.dump(evidence, stream, indent=2)
    print(json.dumps(evidence)); raise SystemExit(0 if result.wasSuccessful() else 1)
