"""Offline guard/cook-evidence counterexamples, never execute Unreal."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import validate_material_fixture_without_mcp as validator


class RuntimeFixtureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="material-proof-offline-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.fixture = {"schema": "cubeless_material_runtime_fixture_v1", "success": True, "assets": {
            "map": "/Game/CubelessMCPTests/RuntimeMaterial_test/Map_MaterialRuntimeProof",
            "runtime_material": "/Game/CubelessMCPTests/RuntimeMaterial_test/M_RuntimeMPC",
            "collection": "/Game/CubelessMCPTests/Material_test/MPC_Contract"}}
        self.lines = ["LogCook: Cooking " + value for value in self.fixture["assets"].values()] + ["********** COOK COMMAND COMPLETED **********"]

    def audit(self, lines):
        path = self.root / "cook.log"
        path.write_text("\n".join(lines), encoding="utf-8")
        return validator.audit_cook([path], self.fixture)

    def test_complete_log_observation_does_not_claim_shader_completion(self):
        result = self.audit(self.lines)
        self.assertTrue(result["passed_within_declared_scope"])
        self.assertFalse(result["shader_completion_verified"])
        self.assertEqual(result["missing_asset_mentions"], [])

    def test_missing_collection_and_missing_cook_end_remain_incomplete(self):
        self.assertFalse(self.audit(self.lines[0:2] + self.lines[3:])["passed_within_declared_scope"])
        self.assertFalse(self.audit(self.lines[:-1])["passed_within_declared_scope"])

    def test_shader_warnings_and_fatal_failures_are_not_ignored(self):
        for line in ("LogShaderCompilers: Warning: Failed to compile Material", "Fatal error: crash", "LogCook: Error: missing package", "BUILD FAILED"):
            with self.subTest(line=line):
                self.assertFalse(self.audit(self.lines + [line])["passed_within_declared_scope"])

    def test_short_asset_names_do_not_substitute_for_package_identity(self):
        self.assertFalse(self.audit(["Map_MaterialRuntimeProof M_RuntimeMPC MPC_Contract", self.lines[-1]])["passed_within_declared_scope"])

    def test_wrong_fixture_schema_and_empty_log_are_rejected(self):
        self.fixture["schema"] = "unrelated"
        with self.assertRaises(RuntimeError):
            self.audit(self.lines)
        self.fixture["schema"] = "cubeless_material_runtime_fixture_v1"
        with self.assertRaises(RuntimeError):
            self.audit([])

    def test_editor_authoring_rejects_other_project_before_unreal_access(self):
        with patch.dict(sys.modules, {"unreal": types.SimpleNamespace()}):
            spec = importlib.util.spec_from_file_location("material_fixture_offline_guard", Path(__file__).with_name("create_material_runtime_fixture.py"))
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            with self.assertRaisesRegex(RuntimeError, "Restricted"):
                module.preflight(str(self.root / "UserProject.uproject"), {})

    def test_offline_mode_preserves_existing_output_and_does_not_import_unreal(self):
        fixture = self.root / "fixture.json"
        fixture.write_text(json.dumps(self.fixture), encoding="utf-8")
        log = self.root / "cook.log"
        log.write_text("\n".join(self.lines), encoding="utf-8")
        report = self.root / "report.json"
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertTrue(validator.main(["--fixture-report", str(fixture), "--cook-log", str(log), "--report", str(report)]))
        previous = report.read_bytes()
        with self.assertRaises(RuntimeError):
            validator.main(["--fixture-report", str(fixture), "--cook-log", str(log), "--report", str(report)])
        self.assertEqual(report.read_bytes(), previous)

    def test_cube_readback_uses_reflected_static_mesh_property(self):
        def object_at(path):
            return types.SimpleNamespace(get_path_name=lambda: path)
        material = object_at("/Game/Test/M.M")
        mesh = object_at("/Engine/BasicShapes/Cube.Cube")
        component = types.SimpleNamespace(get_editor_property=lambda key: mesh if key == "static_mesh" else None, get_material=lambda index: material if index == 0 else None)
        cube = types.SimpleNamespace(get_path_name=lambda: "/Game/Test/Map.Map:Cube", get_actor_label=lambda *args: "CubelessMaterialRuntimeCube", get_class=lambda: object_at("/Script/Engine.StaticMeshActor"), get_component_by_class=lambda cls: component)
        fake = types.SimpleNamespace(Actor=types.SimpleNamespace(static_class=lambda: "Actor"), StaticMeshComponent=types.SimpleNamespace(static_class=lambda: "StaticMeshComponent"), GameplayStatics=types.SimpleNamespace(get_all_actors_of_class=lambda world, cls: [cube]))
        with patch.dict(sys.modules, {"unreal": fake}):
            spec = importlib.util.spec_from_file_location("material_fixture_property_guard", Path(__file__).with_name("create_material_runtime_fixture.py"))
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            result = module.inspect_map(object(), {"spawned_actors": []}, material, True)
            self.assertEqual(result["cube"]["mesh"], mesh.get_path_name())
            self.assertFalse(hasattr(component, "get_static_mesh"))

    def test_save_map_copy_only_rebases_the_exact_world_prefix(self):
        fixture = {"source_assets": {"map": "/Game/Source/Map_Old"}, "assets": {"map": "/Game/Copy/Map_New"},
                   "map_after": {"cube": {"path": "/Game/Source/Map_Old.Map_Old:PersistentLevel.StaticMeshActor_0", "label": "CubelessMaterialRuntimeCube", "material": "/Game/Copy/M.M"}}}
        mapped = validator.expected_saved_cube(fixture)
        self.assertEqual(mapped["path"], "/Game/Copy/Map_New.Map_New:PersistentLevel.StaticMeshActor_0")
        self.assertEqual(mapped["material"], "/Game/Copy/M.M")
        self.assertNotEqual(mapped["path"], "/Game/Copy/Map_New.Map_New:PersistentLevel.StaticMeshActor_1")
        for invalid in ("/Game/Other/Map_Old.Map_Old:PersistentLevel.StaticMeshActor_0", "/Game/Source/Map_Old.Other:PersistentLevel.StaticMeshActor_0", "/Game/Source/Map_Old.Map_Old:OtherLevel.StaticMeshActor_0", "/Game/Source/Map_Old.Map_Old"):
            fixture["map_after"]["cube"]["path"] = invalid
            with self.assertRaises(RuntimeError):
                validator.expected_saved_cube(fixture)


if __name__ == "__main__":
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(RuntimeFixtureTests))
    report = {"schema": "cubeless_material_runtime_offline_tests_v1", "passed": result.wasSuccessful(), "tests_run": result.testsRun, "scope": "Synthetic cook logs, external report preservation and wrong-project guard only; no Unreal assets or engine calls", "details": stream.getvalue()}
    if len(sys.argv) > 1:
        with Path(sys.argv[1]).open("x", encoding="utf-8") as output:
            json.dump(report, output, indent=2)
    print(json.dumps(report))
    raise SystemExit(0 if result.wasSuccessful() else 1)
