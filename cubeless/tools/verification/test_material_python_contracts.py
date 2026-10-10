"""Material wrapper/journal counterexamples with a fake native library, no UE."""
import ast
import importlib
import io
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

PROJECT = Path(__file__).resolve().parents[3]
PACKAGE = PROJECT / "Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58"
NAMES = ("resolve_material_graph", "list_material_nodes", "analyze_material_graph", "list_material_collection_parameter_nodes",
         "get_material_parameter_collection_values", "connect_material_nodes", "connect_material_property", "delete_material_node", "layout_material_nodes",
         "add_material_node", "set_material_node_property", "add_custom_material_node")
MUTATIONS = NAMES[5:]


class MaterialTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cubeless-material-unit-")
        self.addCleanup(self.temp.cleanup)
        self.calls = []
        self.overrides = {}
        self.path = "/Game/CubelessMCPTests/M_Fake.M_Fake"
        library = types.SimpleNamespace(**{name: self.native(name) for name in NAMES})
        unreal = types.SimpleNamespace(CubelessMaterialLibrary=library,
            Paths=types.SimpleNamespace(project_saved_dir=lambda: self.temp.name),
            SystemLibrary=types.SimpleNamespace(get_engine_version=lambda: "5.8.3-synthetic"))
        packages = {"unreal": unreal}
        for suffix in ("", ".compatibility", ".compatibility.ue58", ".operations"):
            name = "cubeless_mcp_ue58" + suffix
            module = types.ModuleType(name)
            module.__path__ = [str(PACKAGE.joinpath(*suffix.strip(".").split("."))) if suffix else str(PACKAGE)]
            packages[name] = module
        self.modules = patch.dict(sys.modules, packages)
        self.modules.start()
        self.addCleanup(self.modules.stop)
        for name in list(sys.modules):
            if name.startswith("cubeless_mcp_ue58.") and name not in packages:
                del sys.modules[name]
        self.api = importlib.import_module("cubeless_mcp_ue58.compatibility.ue58.material")
        self.operations = importlib.import_module("cubeless_mcp_ue58.operations.material")
        self.state = importlib.import_module("cubeless_mcp_ue58.operations.state")

    def native(self, name):
        def call(*args):
            self.calls.append((name, args))
            if name in self.overrides:
                value = self.overrides[name]
                value = value(*args) if callable(value) else value
                return value if isinstance(value, str) else json.dumps(value)
            mutated = name in MUTATIONS
            return json.dumps({"success": True, "stage": "complete", "data": {"material": self.path, "nodes": [{"node_key": "name:Fake"}], "node_count": 1},
                               "mutation_attempted": mutated, "changed": mutated, "before": {"node_count": 1}, "after": {"node_count": 1},
                               "dirty_before": False, "dirty_after": mutated, "postcondition_passed": mutated,
                               "verification": {"method": "synthetic_readback"}, "compile_status": "not_requested", "save_status": "not_requested",
                               "implicit_compile_possible": mutated})
        return call

    def test_named_signature_defaults_match_captured_legacy_catalog(self):
        catalog = json.loads((PROJECT / "cubeless/records/migration/implementation-2026-10-10/legacy-runtime-registration.eager.json").read_text(encoding="utf-8"))
        schemas = {row["name"]: row["inputSchema"] for row in catalog["catalog_tools"] if row["name"] in NAMES}
        tree = ast.parse((PACKAGE / "registration/material_toolset.py").read_text(encoding="utf-8"))
        methods = {node.name: node for cls in tree.body if isinstance(cls, ast.ClassDef) for node in cls.body if isinstance(node, ast.FunctionDef)}
        self.assertEqual(set(methods), set(NAMES))
        self.assertEqual(len(schemas), 12)
        for name, node in methods.items():
            args = [item.arg for item in node.args.args]
            renames = {"properties": "properties_json"} if name == "add_material_node" else {"value": "value_json"} if name == "set_material_node_property" else {}
            if name == "add_custom_material_node":
                renames = {key: key + "_json" for key in ("inputs", "node_position", "additional_defines", "include_file_paths")}
            expected = [renames.get(key, key) for key in schemas[name]["properties"]]
            self.assertEqual(args, expected + (["operation_id"] if name in MUTATIONS else []))
            defaults = dict(zip(args[len(args) - len(node.args.defaults):], map(ast.literal_eval, node.args.defaults)))
            for key, value in schemas[name]["properties"].items():
                if "default" in value:
                    # UE58's schema generator marks empty-string defaults as
                    # required. Nullable registration normalizes null to the
                    # legacy empty string; operation defaults remain unchanged.
                    expected_default = None if value["default"] == "" else value["default"]
                    # Non-null reflected arrays retain legacy omission
                    # semantics without UE58's broken optional-array importer.
                    if name == "add_material_node" and key == "node_position":
                        expected_default = [0, 0]
                    if name == "get_material_parameter_collection_values" and key == "parameter_names":
                        expected_default = []
                    if name == "add_custom_material_node" and key in renames:
                        expected_default = "[0,0]" if key == "node_position" else "[]"
                    self.assertEqual(defaults[renames.get(key, key)], expected_default)
            self.assertEqual([key for key in args if key not in defaults], [renames.get(key, key) for key in schemas[name]["required"]])

    def test_all_twelve_bindings_use_native_methods_and_exact_positional_contract(self):
        self.api.resolve_material_graph("M", "auto")
        self.api.list_material_nodes("M", "function", "Texture", "sample", False)
        self.api.analyze_material_graph("M", "material", True, False, 7)
        self.api.list_material_collection_parameter_nodes("M", "auto")
        self.api.get_material_parameter_collection_values("MPC", None, False, True)
        self.api.connect_material_nodes("M", "F", "T", "A", "RGB", "auto")
        self.api.connect_material_property("M", "F", "Roughness", "")
        self.api.delete_material_node("M", "N", "function")
        self.api.layout_material_nodes("M", "auto")
        self.api.add_material_node("M", "Constant", "material", [-100, 200], "", '{"R":0.25}')
        self.api.set_material_node_property("M", "name:N", "R", "0.75", "auto")
        self.api.add_custom_material_node("M", "return Tint;", "float3", '["Tint"]', "custom", "material",
                                          [-200, 300], '[{"name":"GAIN","value":2}]', '[]')
        self.assertEqual(self.calls, [
            (NAMES[0], ("M", "auto", 2048)), (NAMES[1], ("M", "function", "Texture", "sample", False, 2048)),
            (NAMES[2], ("M", "material", True, False, 7, 2048)), (NAMES[3], ("M", "auto", 2048)),
            (NAMES[4], ("MPC", [], False, True)), (NAMES[5], ("M", "F", "T", "A", "RGB", "auto", 2048)),
            (NAMES[6], ("M", "F", "Roughness", "", 2048)), (NAMES[7], ("M", "N", "function", 2048)), (NAMES[8], ("M", "auto", 2048)),
            (NAMES[9], ("M", "Constant", "material", -100, 200, "", '{"R":0.25}', 2048)),
            (NAMES[10], ("M", "name:N", "R", "0.75", "auto", 2048)),
            (NAMES[11], ("M", "return Tint;", "float3", '["Tint"]', "custom", "material", -200, 300,
                         '[{"name":"GAIN","value":2}]', '[]', 2048))])

    def test_custom_defaults_and_normalized_inputs_use_one_shared_journal(self):
        result = self.operations.add_custom_material_node("M", "return float3(1,0,0);", operation_id="custom-default")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls[-1], ("add_custom_material_node", (self.path, "return float3(1,0,0);", "CMOT_Float3",
                         "[]", "", "auto", 0, 0, "[]", "[]", 2048)))
        self.assertEqual(len(list(Path(self.temp.name).rglob("*.json"))), 1)
        result = self.operations.add_custom_material_node("M", "return Tint;", inputs_json='[ { "name" : "Tint" } ]',
            additional_defines_json='[ { "value" : 2, "name" : "GAIN" } ]', node_position_json='[ -10, 20 ]', operation_id="custom-normalize")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls[-1][1][3], '[{"name":"Tint"}]')
        self.assertEqual(self.calls[-1][1][8], '[{"name":"GAIN","value":2}]')
        self.assertEqual(self.calls[-1][1][6:8], (-10, 20))

    def test_custom_malformed_shape_or_limits_never_load_or_mutate_asset(self):
        cases = [
            {"code": "  "}, {"code": "x" * 65537}, {"description": "x" * 4097},
            {"inputs_json": "{}"}, {"inputs_json": "null"}, {"inputs_json": '[{"name":"A","name":"B"}]'},
            {"inputs_json": '[NaN]'}, {"inputs_json": json.dumps(["A"] * 65)},
            {"additional_defines_json": json.dumps([{}] * 65)}, {"additional_defines_json": "false"},
            {"include_file_paths_json": json.dumps(["path"] * 33)},
            {"node_position_json": "[true,0]"}, {"node_position_json": "[0.0,0]"},
            {"node_position_json": "[2147483648,0]"}, {"node_position_json": "[0]"},
        ]
        for overrides in cases:
            with self.subTest(overrides={key: str(value)[:90] for key, value in overrides.items()}):
                result = self.operations.add_custom_material_node(**{"material_path": "M", "code": "return 1;", **overrides})
                self.assertFalse(result["success"], result)
                self.assertEqual(result["changes"], [])
        self.assertEqual(self.calls, [])
        self.assertFalse((Path(self.temp.name) / "CubelessMCP_UE58").exists())

    def test_custom_native_include_guard_failure_and_cleanup_keep_actual_scope(self):
        self.overrides["add_custom_material_node"] = {"success": False, "stage": "preflight", "data": {},
            "error_code": "custom_shader_includes_unsupported", "error": "Include closure is not audited", "mutation_attempted": False, "changed": False}
        result = self.operations.add_custom_material_node("M", "return 1;", include_file_paths_json='["/Engine/X.ush"]', operation_id="custom-includes")
        self.assertEqual(result["error_code"], "custom_shader_includes_unsupported")
        self.assertEqual(result["changes"], [])
        self.overrides["add_custom_material_node"] = {"success": False, "stage": "readback", "data": {
            "created_node_key": "guid:new", "cleanup_attempted": True, "cleanup_node_absent": True},
            "error_code": "native_material_operation_failed", "error": "Unexpected custom pins", "mutation_attempted": True, "changed": True}
        result = self.operations.add_custom_material_node("M", "return 1;", operation_id="custom-cleanup")
        self.assertFalse(result["success"])
        self.assertTrue(result["changes"][0]["changed"])
        self.assertTrue(result["data"]["cleanup_node_absent"])
        self.assertFalse(result["retry"]["atomic_rollback"])
        self.assertFalse(result["compile"]["requested"])

    def test_custom_replay_skips_native_and_checkpoint_failure_blocks_creation(self):
        first = self.operations.add_custom_material_node("M", "return 1;", operation_id="custom-replay")
        count = len(self.calls)
        replay = self.operations.add_custom_material_node("M", "return 1;", operation_id="custom-replay")
        self.assertTrue(first["success"])
        self.assertTrue(replay["success"])
        self.assertEqual(len(self.calls), count)
        with patch.object(self.state, "checkpoint", side_effect=self.state.OperationError("journal_checkpoint_failed", "disk full")):
            result = self.operations.add_custom_material_node("M", "return 1;", operation_id="custom-no-dispatch")
        self.assertEqual(result["error_code"], "journal_checkpoint_failed")
        self.assertEqual(len(self.calls), count + 1)  # only the before snapshot
        self.assertEqual(self.calls[-1][0], "list_material_nodes")

    def test_custom_undecodable_native_result_is_unknown_not_safe_to_retry(self):
        self.overrides["add_custom_material_node"] = "not JSON after potential creation"
        result = self.operations.add_custom_material_node("M", "return 1;", operation_id="custom-unknown")
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "invalid_native_material_result")
        self.assertEqual(result["changes"][0]["outcome"], "unknown_until_native_result")
        self.assertFalse(result["retry"]["automatic_retry"])

    def test_read_preserves_native_data_and_never_creates_mutation_journal(self):
        result = self.operations.list_material_nodes("M")
        self.assertTrue(result["success"])
        self.assertEqual(result["data"]["node_count"], 1)
        self.assertFalse(result["compile"]["requested"])
        self.assertFalse(result["save"]["saved"])
        self.assertFalse((Path(self.temp.name) / "CubelessMCP_UE58").exists())

    def test_unresolved_read_keeps_candidates_and_error_stage(self):
        self.overrides[NAMES[0]] = {"success": False, "stage": "resolve", "error_code": "ambiguous", "error": "Use full path", "data": {"resolved": False, "candidates": ["A", "B"]}}
        result = self.operations.resolve_material_graph("ambiguous")
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "ambiguous")
        self.assertEqual(result["stage"], "resolve")
        self.assertEqual(result["data"]["candidates"], ["A", "B"])

    def test_mutation_checkpoint_is_durable_before_native_dispatch(self):
        def mutate(*args):
            record = self.state.get_operation("durable-first")
            self.assertEqual(record["state"], "started")
            self.assertEqual(record["result"]["stage"], "before_mutation_checkpoint")
            self.assertEqual(record["result"]["data"]["before"]["material"], self.path)
            self.assertEqual(record["result"]["changes"], [])
            self.assertEqual(args[0], self.path)
            return {"success": True, "stage": "complete", "data": {}, "postcondition_passed": True,
                    "mutation_attempted": True, "changed": True, "dirty_before": False, "dirty_after": True,
                    "before": {"node_count": 1}, "after": {"node_count": 0}, "implicit_compile_possible": True}
        self.overrides["delete_material_node"] = mutate
        result = self.operations.delete_material_node("M_Fake", "name:Fake", operation_id="durable-first")
        self.assertTrue(result["success"])
        self.assertTrue(result["changes"][0]["dirty_after"])
        self.assertTrue(result["compile"]["implicit_compile_possible"])
        self.assertFalse(result["compile"]["requested"])
        self.assertFalse(result["save"]["saved"])
        self.assertEqual(result["data"]["after"]["node_count"], 0)

    def test_checkpoint_failure_prevents_native_mutation(self):
        with patch.object(self.state, "checkpoint", side_effect=self.state.OperationError("journal_checkpoint_failed", "synthetic disk failure")):
            result = self.operations.layout_material_nodes("M", operation_id="checkpoint-failure")
        self.assertFalse(result["success"])
        self.assertEqual([name for name, _ in self.calls], ["list_material_nodes"])
        self.assertEqual(result["changes"], [])

    def test_native_failure_preserves_actual_partial_change_and_stage(self):
        self.overrides["connect_material_nodes"] = {"success": False, "stage": "readback", "data": {"from_node_id": "F"},
            "error_code": "postcondition_failed", "error": "Observed wrong link", "mutation_attempted": True, "changed": True,
            "before": {"links": []}, "after": {"links": ["unexpected"]}, "dirty_before": False, "dirty_after": True}
        result = self.operations.connect_material_nodes("M", "F", "T", "A", operation_id="partial")
        self.assertFalse(result["success"])
        self.assertEqual(result["stage"], "readback")
        self.assertTrue(result["changes"][0]["changed"])
        self.assertEqual(result["data"]["after"]["links"], ["unexpected"])
        self.assertEqual(result["retry"]["duplicate_risk"], "partial_or_attempted_changes")

    def test_native_preflight_failure_reports_no_mutation(self):
        self.overrides["delete_material_node"] = {"success": False, "stage": "preflight", "data": {}, "error_code": "node_not_found", "error": "Missing", "mutation_attempted": False, "changed": False}
        result = self.operations.delete_material_node("M", "missing", operation_id="missing-node")
        self.assertEqual(result["error_code"], "node_not_found")
        self.assertEqual(result["changes"], [])
        self.assertEqual(result["retry"]["duplicate_risk"], "none_observed")

    def test_native_success_without_postcondition_is_not_verified_success(self):
        self.overrides["layout_material_nodes"] = {"success": True, "stage": "complete", "data": {}, "mutation_attempted": True, "changed": True}
        result = self.operations.layout_material_nodes("M", operation_id="unverified")
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "material_postcondition_not_verified")

    def test_invalid_native_response_keeps_uncertain_dispatch_no_retry(self):
        self.overrides["delete_material_node"] = "{invalid json"
        result = self.operations.delete_material_node("M", "name:Fake", operation_id="bad-native-json")
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "invalid_native_material_result")
        self.assertEqual(result["stage"], "native_mutation")
        self.assertEqual(result["changes"][0]["outcome"], "unknown_until_native_result")
        self.assertFalse(result["retry"]["automatic_retry"])

    def test_replay_does_not_repeat_snapshot_or_native_mutation(self):
        first = self.operations.layout_material_nodes("M", operation_id="one-only")
        replay = self.operations.layout_material_nodes("M", operation_id="one-only")
        self.assertTrue(first["success"])
        self.assertTrue(replay["retry"]["replayed"])
        self.assertEqual([name for name, _ in self.calls], ["list_material_nodes", "layout_material_nodes"])

    def test_engine_and_plugin_mutation_paths_rejected_but_game_fixture_allowed(self):
        for index, path in enumerate(("/Engine/Materials/M_Test.M_Test", "/CubelessMCP_UE58/M_Test.M_Test")):
            self.path = path
            result = self.operations.layout_material_nodes(path, operation_id="restricted-" + str(index))
            self.assertEqual(result["error_code"], "material_mutation_path_restricted")
        self.assertTrue(all(name == "list_material_nodes" for name, _ in self.calls))

    def test_before_snapshot_failure_prevents_mutation(self):
        self.overrides["list_material_nodes"] = {"success": False, "stage": "preflight", "data": {}, "error_code": "graph_too_large", "error": "Limit", "mutation_attempted": False}
        result = self.operations.layout_material_nodes("M", operation_id="large")
        self.assertEqual(result["error_code"], "graph_too_large")
        self.assertEqual([name for name, _ in self.calls], ["list_material_nodes"])

    def test_missing_native_binding_fails_explicitly(self):
        with patch.object(self.api.unreal, "CubelessMaterialLibrary", None):
            result = self.operations.resolve_material_graph("M")
        self.assertEqual(result["error_code"], "native_material_api_unavailable")

    def test_add_defaults_normalize_and_replay_without_second_native_call(self):
        first = self.operations.add_material_node("M", "Constant", operation_id="add-once")
        replay = self.operations.add_material_node("M", "Constant", node_position=[0, 0], selected_asset="", properties_json="{ }", operation_id="add-once")
        self.assertTrue(first["success"])
        self.assertTrue(replay["retry"]["replayed"])
        self.assertEqual([name for name, _ in self.calls], ["list_material_nodes", "add_material_node"])
        self.assertEqual(self.calls[-1][1], (self.path, "Constant", "auto", 0, 0, "", "{}", 2048))

    def test_property_json_is_canonical_and_quoted_strings_reach_native(self):
        result = self.operations.set_material_node_property("M", "name:Fake", "Desc", '"caption"', operation_id="string-property")
        self.assertTrue(result["success"])
        self.assertEqual(self.calls[-1][1], (self.path, "name:Fake", "Desc", '"caption"', "auto", 2048))
        self.assertEqual(self.operations._json_input('{"B":2,"A":1}'), '{"A":1,"B":2}')

    def test_invalid_json_and_positions_fail_before_snapshot_or_mutation(self):
        for value in ('not-json', '{"x":1,"x":2}', 'NaN', '1e400', '[' * 18 + '0' + ']' * 18):
            with self.subTest(value=value):
                result = self.operations.set_material_node_property("M", "N", "R", value)
                self.assertEqual(result["error_code"], "invalid_material_json")
        for position in ([True, 0], [1.5, 0], [0], [2**31, 0]):
            self.assertEqual(self.operations.add_material_node("M", "Constant", node_position=position)["error_code"], "invalid_material_node_position")
        self.assertEqual(self.operations.add_material_node("M", "Constant", properties_json="[]")["error_code"], "invalid_material_properties")
        self.assertEqual(self.operations.add_material_node("M", "Constant", properties_json=json.dumps({str(i): i for i in range(65)}))["error_code"], "invalid_material_properties")
        self.assertEqual(self.calls, [])

    def test_add_native_guard_failure_preserves_no_change_and_no_compile_save(self):
        self.overrides["add_material_node"] = {"success": False, "stage": "preflight", "data": {},
            "error_code": "invalid_mutation_input", "error": "Unaudited class", "mutation_attempted": False, "changed": False}
        result = self.operations.add_material_node("M", "/Script/UnrealEd.EditorClass", operation_id="rejected-class")
        self.assertEqual(result["error_code"], "invalid_mutation_input")
        self.assertEqual(result["changes"], [])
        self.assertFalse(result["compile"]["requested"])
        self.assertFalse(result["save"]["saved"])

    def test_add_partial_cleanup_is_preserved_without_rollback_claim(self):
        self.overrides["add_material_node"] = {"success": False, "stage": "mutation", "data": {
            "created_node_key": "guid:temporary", "transient_graph_changes_occurred": True,
            "cleanup_attempted": True, "cleanup_node_absent": True, "properties_applied_before_failure": ["R"]},
            "error_code": "native_material_operation_failed", "error": "assignment failed after create", "mutation_attempted": True, "changed": True}
        result = self.operations.add_material_node("M", "Constant", operation_id="partial-add")
        self.assertFalse(result["success"])
        self.assertTrue(result["data"]["cleanup_node_absent"])
        self.assertTrue(result["changes"][0]["changed"])
        self.assertFalse(result["retry"]["atomic_rollback"])
        self.assertFalse(result["retry"]["automatic_retry"])

    def test_set_before_checkpoint_failure_never_dispatches_property_write(self):
        with patch.object(self.state, "checkpoint", side_effect=self.state.OperationError("journal_checkpoint_failed", "disk full")):
            result = self.operations.set_material_node_property("M", "N", "R", "0.75", operation_id="set-no-dispatch")
        self.assertEqual(result["error_code"], "journal_checkpoint_failed")
        self.assertEqual([name for name, _ in self.calls], ["list_material_nodes"])

    def test_registration_normalizes_reflected_array_before_strict_operation(self):
        # Execute the actual wrapper body with decorators removed. This models
        # iterable unreal.Array input, not real Registry schema/dispatch proof.
        tree = ast.parse((PACKAGE / "registration/material_toolset.py").read_text(encoding="utf-8"))
        function = next(item for cls in tree.body if isinstance(cls, ast.ClassDef) for item in cls.body
                        if isinstance(item, ast.FunctionDef) and item.name == "add_material_node")
        function.decorator_list = []
        namespace = {"CubelessPythonOperationResult": dict, "material": self.operations, "reflected_result": lambda value: value}
        exec(compile(ast.fix_missing_locations(ast.Module(body=[function], type_ignores=[])), "material_wrapper_test", "exec"), namespace)
        class ReflectedArray:
            def __iter__(self):
                return iter([-300, 150])
        result = namespace["add_material_node"]("M", "Constant", node_position=ReflectedArray(), operation_id="array-boundary")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls[-1][1][3:5], (-300, 150))

    def test_reflected_array_defaults_are_not_optional_or_shared_with_operation(self):
        # The prior optional-array signature passed Python-only tests but failed
        # before dispatch in actual UE58 JsonObjectConverter. Check that actual
        # decorator inputs are bare list[T], then exercise both wrapper defaults.
        tree = ast.parse((PACKAGE / "registration/material_toolset.py").read_text(encoding="utf-8"))
        for name, argument, expected in (("add_material_node", "node_position", [0, 0]),
                                         ("get_material_parameter_collection_values", "parameter_names", [])):
            function = next(item for cls in tree.body if isinstance(cls, ast.ClassDef) for item in cls.body
                            if isinstance(item, ast.FunctionDef) and item.name == name)
            annotation = next(item.annotation for item in function.args.args if item.arg == argument)
            self.assertIsInstance(annotation, ast.Subscript)
            self.assertEqual(annotation.value.id, "list")
            function.decorator_list = []
            seen = []
            def operation(*args):
                position = args[3] if name == "add_material_node" else args[1]
                seen.append(list(position))
                position.append("callee mutation")
                return {"success": True}
            namespace = {"CubelessPythonOperationResult": dict, "material": types.SimpleNamespace(**{name: operation}), "reflected_result": lambda value: value}
            exec(compile(ast.fix_missing_locations(ast.Module(body=[function], type_ignores=[])), "material_array_defaults_test", "exec"), namespace)
            args = ("M", "Constant") if name == "add_material_node" else ("MPC",)
            namespace[name](*args)
            namespace[name](*args)
            self.assertEqual(seen, [expected, expected])


if __name__ == "__main__":
    stream = io.StringIO()
    outcome = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(MaterialTests))
    report = {"schema": "cubeless_material_python_contract_tests_v1", "tests_run": outcome.testsRun, "passed": outcome.wasSuccessful(),
              "failures": len(outcome.failures), "errors": len(outcome.errors), "scope": "Synthetic native library plus real durable journal, no engine API execution", "details": stream.getvalue()}
    if len(sys.argv) > 1:
        path = Path(sys.argv[1])
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("x", encoding="utf-8") as output:
            json.dump(report, output, indent=2)
    print(json.dumps(report))
    raise SystemExit(0 if outcome.wasSuccessful() else 1)
