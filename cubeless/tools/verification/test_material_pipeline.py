"""Offline fake-MCP execution of the Material smoke and independent verifier."""
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
from pipelines import material_graph_smoke as smoke
from verification.material_pipeline_report import verify

ROOT = Path(__file__).resolve().parents[3]


class FakeClient:
    def __init__(self, endpoint, timeout):
        self.observations = []
        self.state = CURRENT
        self.schemas = CURRENT["schemas"]
    def initialize(self):
        return {"synthetic": True}
    def list_tools(self):
        return {"tools": []}
    def close(self):
        pass
    def call(self, name, arguments=None, **kwargs):
        if name == "list_toolsets":
            value = "\n".join("- " + key + ": synthetic" for key in self.schemas)
        else:
            value = self.schemas[arguments["toolset_name"]]
        return {"structuredContent": {"returnValue": value}}
    def call_toolset(self, toolset, tool, arguments, mutation=False):
        self.observations.append({"toolset": toolset, "tool": tool, "arguments": arguments, "mutation": mutation, "synthetic": True})
        state = self.state
        args = {smoke.canonical(key): value for key, value in arguments.items()}
        def validate_refs(value):
            if isinstance(value, dict):
                if "refPath" in value:
                    path = value["refPath"]
                    if path not in state["objects"] and not path.startswith("/Script/Engine.") and not any(node["object_path"] == path for node in state["nodes"]):
                        raise ValueError("not a valid object path: " + path)
                else:
                    for member in value.values():
                        validate_refs(member)
            elif isinstance(value, list):
                for member in value:
                    validate_refs(member)
        try:
            validate_refs(arguments)
            if tool == "set_properties":
                validate_refs(json.loads(args["values"]))
        except ValueError as error:
            return {"isError": True, "content": [{"type": "text", "text": str(error)}]}
        def arg(name):
            return args[smoke.canonical(name)]
        def ref(name):
            return smoke.ref_path(arg(name))
        native = {"success": True, "stage": "complete", "data": {}, "changes": [], "compile": {"requested": False}, "save": {"saved": False},
                  "retry": {"automatic_retry": False}, "validation": {"native_postcondition_passed": True}}
        native["data"]["native_outcome"] = {"postcondition_passed": True, "change_state_known": True, "changed": mutation}
        value = None
        if tool == "GetEditorDiagnostics":
            value = {"success": True, "project_file": state["project"], "instance_id": state["instance"], "engine_version": "5.8.3-synthetic", "game_thread": True, "pie_or_sie_active": False, "dirty_packages": []}
        elif tool == "exists":
            value = arg("path") in state["assets"]
        elif tool in {"create_material", "create_parameter_collection"}:
            path = arg("folder_path") + "/" + arg("asset_name")
            state["assets"].append(path)
            if tool == "create_material":
                state["material"] = path
            value = smoke.reference(path + "." + arg("asset_name"))
            state["objects"].append(value["refPath"])
        elif tool == "add_expression":
            path = state["material"] + ".M_Contract:Expression_" + str(len(state["nodes"]))
            state["nodes"].append({"object_path": path, "node_key": "path:" + path, "x": arg("x"), "y": arg("y")})
            value = smoke.reference(path)
        elif tool == "set_properties":
            value = True
        elif tool == "get_properties":
            value = json.dumps(state["added_properties"][ref("instance")])
        elif tool == "add_material_node":
            op_id = arg("operation_id")
            if op_id in state["journal"]:
                value = copy.deepcopy(state["journal"][op_id])
                value["retry"]["replayed"] = True
            else:
                path = state["material"] + ".M_Contract:Expression_" + str(len(state["nodes"]))
                position = arg("node_position")
                state["nodes"].append({"object_path": path, "node_key": "path:" + path, "x": position[0], "y": position[1]})
                state["added_properties"][path] = json.loads(arg("properties_json"))
                native["data"].update(object_path=path, node_key="path:" + path)
                value = native
                state["journal"][op_id] = copy.deepcopy(value)
        elif tool == "set_material_node_property":
            path = arg("node_id").removeprefix("path:")
            parsed = json.loads(arg("value_json"))
            native["data"].update(object_path=path, node_key="path:" + path)
            if type(parsed) not in (int, float):
                native.update(success=False, error_code="synthetic_invalid_property")
                native["data"]["native_outcome"].update(mutation_attempted=False, changed=False)
                if state.get("invalid_property_changes"):
                    state["added_properties"][path]["R"] = 99.0
            else:
                state["added_properties"][path][arg("property_name")] = parsed
            value = native
        elif tool in {"resolve_material_graph", "analyze_material_graph"}:
            value = native
        elif tool == "list_material_nodes":
            native["data"].update(material=state["material"] + ".M_Contract", graph_type="material", node_count=len(state["nodes"]), nodes=copy.deepcopy(state["nodes"]), material_properties=copy.deepcopy(state["properties"]))
            value = native
        elif tool == "list_material_collection_parameter_nodes":
            native["data"].update(collection_parameter_node_count=1, nodes=[{"parameter_name": "CubelessScalar", "collection_parameter_found": True, "parameter_id_matches_collection": True}])
            value = native
        elif tool == "get_material_parameter_collection_values":
            native["data"].update(scalars=[{"name": "CubelessScalar", "asset_default": 0.25, "runtime_resolved": True}], vectors=[{"name": "CubelessVector", "asset_default": {"r": 0.1, "g": 0.2, "b": 0.3, "a": 1.0}, "runtime_resolved": True}], has_world=True, has_runtime_instance=True)
            value = native
        elif tool == "connect_material_nodes":
            op_id = arg("operation_id")
            if op_id in state["journal"]:
                value = copy.deepcopy(state["journal"][op_id])
                value["retry"]["replayed"] = True
            elif arg("to_input") == "CubelessMissingPin":
                native.update(success=False, error_code="synthetic_bad_pin")
                native["data"]["native_outcome"]["changed"] = state.get("invalid_pin_changes", False)
                value = native
            else:
                state["inputs"][arg("to_input")] = arg("from_node_id").removeprefix("path:")
                value = native
                if state.get("missing_postcondition"):
                    value["validation"]["native_postcondition_passed"] = False
                state["journal"][op_id] = copy.deepcopy(value)
        elif tool == "get_expression_inputs":
            value = [{"input_name": name, "expression": smoke.reference(state["inputs"][name]) if state["inputs"].get(name) else "None", "output_name": ""} for name in ("A", "B")]
        elif tool == "connect_material_property":
            state["properties"] = [{"property": "BaseColor", "source": arg("from_node_id").removeprefix("path:")}]
            value = native
        elif tool == "get_property_input":
            value = {"expression": smoke.reference(state["properties"][0]["source"]), "input_name": "", "output_name": ""}
        elif tool == "layout_material_nodes":
            for index, node in enumerate(state["nodes"]):
                node["x"], node["y"] = index * 200, 0
            value = native
        elif tool == "delete_material_node":
            path = arg("node_id").removeprefix("path:")
            state["nodes"] = [node for node in state["nodes"] if node["object_path"] != path]
            state["inputs"] = {pin: source for pin, source in state["inputs"].items() if source != path}
            value = native
        elif tool == "recompile":
            state["compile_calls"] += 1
            if state.get("compile_error"):
                return {"isError": True, "content": [{"type": "text", "text": "synthetic compiler error"}]}
        elif tool == "save_assets":
            state["save_calls"] += 1
            value = True
        elif tool == "is_dirty":
            value = False
        else:
            raise AssertionError("Unimplemented fake tool: " + tool)
        return {"structuredContent": {"returnValue": value}}


class PipelineTests(unittest.TestCase):
    def setUp(self):
        global CURRENT
        self.temp = tempfile.TemporaryDirectory(prefix="cubeless-material-pipeline-test-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        project = self.directory / "Synthetic.uproject"
        project.write_text("{}")
        official = json.loads((ROOT / "cubeless/records/migration/implementation-2026-10-10/operations-material-official-schemas-C.json").read_text(encoding="utf-8"))["schemas"]
        raw = json.loads((ROOT / "cubeless/records/verification/implementation-2026-10-10/material-discovery-E2.json").read_text(encoding="utf-8"))
        cubeless = smoke.tool_payload(raw["description"])
        schemas = {**official, "cubeless_mcp_ue58.registration.material_toolset.CubelessMaterialTools": cubeless,
                   "Synthetic.CubelessNativeToolset": {"tools": [{"name": "GetEditorDiagnostics", "inputSchema": {"properties": {}}}]}}
        # New tools have not yet been registered in a real editor. Only this fake
        # execution uses explicit synthetic schemas; runtime must rediscover them.
        for name, fields in {
            "add_material_node": ["material_path", "expression_class", "graph_type", "node_position", "selected_asset", "properties_json", "operation_id"],
            "set_material_node_property": ["material_path", "node_id", "property_name", "value_json", "graph_type", "operation_id"],
        }.items():
            cubeless["tools"].append({"name": name, "inputSchema": {"properties": {key: {} for key in fields}, "required": fields[:2] if name == "add_material_node" else fields[:4]}, "synthetic_schema": True})
        CURRENT = {"schemas": schemas, "project": str(project.resolve()), "instance": "synthetic-author", "assets": [], "objects": [], "nodes": [], "inputs": {}, "properties": [], "journal": {}, "compile_calls": 0, "save_calls": 0, "added_properties": {}}
        self.state = CURRENT

    def run_phase(self, phase, parent=None, extended=False):
        path = self.directory / (phase + ".json")
        argv = ["--endpoint", "http://synthetic.invalid/mcp", "--expected-project", self.state["project"], "--phase", phase, "--output", str(path)]
        if parent:
            argv += ["--parent-report", str(parent)]
        if extended:
            argv += ["--exercise-add-set"]
        with patch.object(smoke, "OfficialMCPClient", FakeClient), contextlib.redirect_stdout(io.StringIO()):
            code = smoke.main(argv)
        return code, path, json.loads(path.read_text(encoding="utf-8"))

    def test_entire_author_and_cold_reload_contracts(self):
        code, author_path, author = self.run_phase("author")
        self.assertEqual(code, 0, author.get("error"))
        self.assertEqual((self.state["compile_calls"], self.state["save_calls"]), (1, 1))
        self.assertTrue(verify(author, require_phase="author")["passed"], verify(author)["errors"])
        self.state["instance"] = "synthetic-reload"
        code, _, reload = self.run_phase("reload", author_path)
        self.assertEqual(code, 0, reload.get("error"))
        checked = verify(reload, require_phase="reload", parent=author, parent_bytes=author_path.read_bytes())
        self.assertTrue(checked["passed"], checked["errors"])
        self.assertEqual((self.state["compile_calls"], self.state["save_calls"]), (1, 1))

    def test_extended_add_set_author_and_cold_reload(self):
        code, author_path, author = self.run_phase("author", extended=True)
        self.assertEqual(code, 0, author.get("error"))
        self.assertEqual(author["schema"], "cubeless_material_pipeline_v2")
        checked = verify(author, require_phase="author")
        self.assertTrue(checked["passed"], checked["errors"])
        self.assertEqual(author["saved_graph"]["node_count"], 4)
        self.assertFalse(author["compile"]["shader_completion_verified"])
        self.state["instance"] = "synthetic-cold-reload"
        code, _, reload = self.run_phase("reload", author_path)
        self.assertEqual(code, 0, reload.get("error"))
        checked = verify(reload, require_phase="reload", parent=author, parent_bytes=author_path.read_bytes())
        self.assertTrue(checked["passed"], checked["errors"])
        self.assertEqual((self.state["compile_calls"], self.state["save_calls"]), (1, 1))

    def test_extended_invalid_property_with_hidden_change_is_caught(self):
        self.state["invalid_property_changes"] = True
        code, _, report = self.run_phase("author", extended=True)
        self.assertEqual(code, 1)
        self.assertEqual(report["failed_stage"], "independent_rejected_property")
        self.assertEqual(self.state["save_calls"], 0)

    def test_extended_verifier_rejects_counterfeit_property_or_shader_claim(self):
        code, _, report = self.run_phase("author", extended=True)
        self.assertEqual(code, 0, report.get("error"))
        for name in ("independent_added_property", "independent_rejected_property", "saved_added_property"):
            modified = copy.deepcopy(report)
            next(row for row in modified["stages"] if row["name"] == name)["result"]["R"] = 0.25
            self.assertFalse(verify(modified)["passed"])
        modified = copy.deepcopy(report)
        modified["compile"]["shader_completion_verified"] = True
        self.assertFalse(verify(modified)["passed"])

    def test_missing_native_postcondition_stops_before_compile_save(self):
        self.state["missing_postcondition"] = True
        code, _, report = self.run_phase("author")
        self.assertEqual(code, 1)
        self.assertEqual(report["failed_stage"], "connect_A")
        self.assertEqual((self.state["compile_calls"], self.state["save_calls"]), (0, 0))
        self.assertFalse(verify(report)["passed"])

    def test_invalid_pin_with_changed_state_is_not_accepted(self):
        self.state["invalid_pin_changes"] = True
        code, _, report = self.run_phase("author")
        self.assertEqual(code, 1)
        self.assertEqual(report["failed_stage"], "invalid_pin_rejected")
        self.assertEqual(self.state["save_calls"], 0)

    def test_compile_error_is_uncertain_and_never_saved_or_retried(self):
        self.state["compile_error"] = True
        code, _, report = self.run_phase("author")
        self.assertEqual(code, 1)
        self.assertEqual(report["failed_stage"], "compile_once")
        self.assertTrue(report["outcome_unknown"])
        self.assertEqual((self.state["compile_calls"], self.state["save_calls"]), (1, 0))

    def test_same_editor_cannot_pass_cold_reload(self):
        code, author_path, _ = self.run_phase("author")
        self.assertEqual(code, 0)
        code, _, report = self.run_phase("reload", author_path)
        self.assertEqual(code, 1)
        self.assertIn("different editor process", report["error"])

    def test_verifier_rejects_missing_stage_and_counterfeit_summaries(self):
        _, _, report = self.run_phase("author")
        modified = copy.deepcopy(report)
        modified["stages"] = [row for row in modified["stages"] if row["name"] != "compile_once"]
        self.assertFalse(verify(modified)["passed"])
        modified = copy.deepcopy(report)
        modified["save"]["explicit_calls"] = 2
        self.assertFalse(verify(modified)["passed"])
        modified = copy.deepcopy(report)
        modified["assets"]["material"] = "/Game/UserAsset"
        self.assertFalse(verify(modified)["passed"])

    def test_package_path_is_not_accepted_as_object_reference(self):
        self.state["objects"] = ["/Game/Test/MPC.MPC"]
        client = FakeClient("synthetic", 1)
        result = client.call_toolset("unused", "set_properties", {"instance": smoke.reference("/Game/Test/MPC"), "values": "{}"}, mutation=True)
        self.assertTrue(result["isError"])

    def test_verifier_rechecks_official_readback_and_mpc_values(self):
        _, _, report = self.run_phase("author")
        def counterfeit(stage_name, edit):
            modified = copy.deepcopy(report)
            row = next(row for row in modified["stages"] if row["name"] == stage_name)
            edit(row["result"])
            self.assertTrue(row["passed"])
            self.assertFalse(verify(modified)["passed"], stage_name)
        counterfeit("collection_values", lambda result: result["data"]["scalars"][0].update(asset_default=0.75))
        counterfeit("collection_values", lambda result: result["data"]["vectors"][0]["asset_default"].update(b=9.0))
        counterfeit("collection_nodes", lambda result: result["data"]["nodes"][0].update(parameter_id_matches_collection=False))
        counterfeit("independent_connection_readback", lambda result: result[0].update(expression=None))
        counterfeit("independent_deleted_link_readback", lambda result: result[1].update(expression=smoke.reference(report["fixture"]["nodes"]["delete"])))
        counterfeit("independent_property_readback", lambda result: result.update(expression=None))

    def test_verifier_rejects_unverified_null_spellings_and_missing_field(self):
        _, _, report = self.run_phase("author")
        for invalid in (None, "", "none", "null", "NULL", "unresolved", {}, {"refPath": ""}, {"refPath": "None"}):
            with self.subTest(invalid=invalid):
                modified = copy.deepcopy(report)
                row = next(row for row in modified["stages"] if row["name"] == "independent_deleted_link_readback")
                row["result"][1]["expression"] = invalid
                self.assertFalse(verify(modified)["passed"])
        modified = copy.deepcopy(report)
        row = next(row for row in modified["stages"] if row["name"] == "independent_deleted_link_readback")
        del row["result"][1]["expression"]
        self.assertFalse(verify(modified)["passed"])

    @unittest.skipUnless(sys.platform == "win32", "Windows path identity behavior")
    def test_project_identity_uses_windows_resolved_path_semantics(self):
        _, _, report = self.run_phase("author")
        for alternate in (report["project_file"].replace("\\", "/"), report["project_file"].swapcase()):
            modified = copy.deepcopy(report)
            modified["editor"]["project_file"] = alternate
            next(row for row in modified["stages"] if row["name"] == "preflight")["result"]["project_file"] = alternate
            checked = verify(modified)
            self.assertTrue(checked["passed"], checked["errors"])
        modified = copy.deepcopy(report)
        alternate = str(Path(report["project_file"]).with_name("Different.uproject"))
        modified["editor"]["project_file"] = alternate
        next(row for row in modified["stages"] if row["name"] == "preflight")["result"]["project_file"] = alternate
        self.assertFalse(verify(modified)["passed"])


if __name__ == "__main__":
    stream = io.StringIO()
    outcome = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(PipelineTests))
    report = {"schema": "cubeless_material_pipeline_offline_tests_v1", "passed": outcome.wasSuccessful(), "tests_run": outcome.testsRun,
              "scope": "Fake official MCP with captured live schemas; no network/Unreal/assets", "details": stream.getvalue()}
    if len(sys.argv) > 1:
        path = Path(sys.argv[1])
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("x", encoding="utf-8") as output:
            json.dump(report, output, indent=2)
    print(json.dumps(report))
    raise SystemExit(0 if outcome.wasSuccessful() else 1)
