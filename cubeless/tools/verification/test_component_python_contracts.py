"""N local-SCS Python boundary counterexamples; synthetic native, no UE calls."""

import ast
import copy
import importlib
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
PACKAGE = ROOT / "Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58"


class ComponentTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.calls = []
        self.override = None
        self.library = types.SimpleNamespace(list_blueprint_components=self.native("list_blueprint_components"),
            get_component_property=self.native("get_component_property"),
            add_component_to_blueprint=self.native("add_component_to_blueprint"), set_component_property=self.native("set_component_property"),
            set_physics_properties=self.native("set_physics_properties"), set_static_mesh_properties=self.native("set_static_mesh_properties"))
        modules = {"unreal": types.SimpleNamespace(CubelessComponentLibrary=self.library,
            Paths=types.SimpleNamespace(project_saved_dir=lambda: self.temporary.name),
            SystemLibrary=types.SimpleNamespace(get_engine_version=lambda: "5.8.3-offline-test"))}
        for suffix in ("", ".compatibility", ".compatibility.ue58", ".operations"):
            name = "cubeless_mcp_ue58" + suffix
            module = types.ModuleType(name)
            module.__path__ = [str(PACKAGE.joinpath(*suffix.strip(".").split("."))) if suffix else str(PACKAGE)]
            modules[name] = module
        self.patch = patch.dict(sys.modules, modules)
        self.patch.start(); self.addCleanup(self.patch.stop)
        for name in list(sys.modules):
            if name.startswith("cubeless_mcp_ue58.") and name not in modules: del sys.modules[name]
        self.api = importlib.import_module("cubeless_mcp_ue58.compatibility.ue58.component")
        self.ops = importlib.import_module("cubeless_mcp_ue58.operations.component")

    def native(self, method):
        def call(*args):
            self.calls.append((method, args))
            value = self.override
            if isinstance(value, BaseException): raise value
            if callable(value): value = value(*args)
            if value is None:
                value = (self.mutation_envelope(compile_requested=args[-1], compile_attempted=args[-1], compiled=True if args[-1] else None)
                    if method == "add_component_to_blueprint" else self.mutation_envelope() if method in ("set_component_property", "set_physics_properties", "set_static_mesh_properties") else self.envelope())
            return value if isinstance(value, str) else json.dumps(value)
        return call

    def envelope(self, **overrides):
        data = {"blueprint_name": "/Game/Blueprints/BP_Test", "blueprint_path": "/Game/Blueprints/BP_Test.BP_Test",
            "has_simple_construction_script": True, "component_count": 1,
            "components": [{"component_name": "Root", "variable_name": "Root", "parent_component_name": "", "node_guid": "GUID"}],
            "component_name": "Root", "component_class": "SceneComponent", "property_name": "ComponentTags",
            "property_type": "TArray<FName>", "property_value": '("One","Two")',
            "property_encoding": "legacy_ue_export_text_bounded_scalar_array", "success": True}
        result = {"success": True, "data": data, "stage": "complete", "mutation_attempted": False,
            "compile_requested": False, "save_requested": False, "saved": False, "loaded_during_read": False,
            "change_state_known": True, "changed": False, "dirty_before": False, "dirty_after": False,
            "blueprint_status_before": 0, "blueprint_status_after": 0,
            "postcondition_passed": True, "auto_retry_safe": True, "implicit_postload_work_possible": True,
            "load_scope": "baseline after Blueprint load", "verification_scope": "local reachable SCS, dirty/status only",
            "limits": {"max_components": 10000, "max_value_chars": 65536}, "performance": {"local_scs_nodes_visited": 1}}
        result.update(overrides)
        return result

    def mutation_envelope(self, **overrides):
        result = {"success": True, "data": {"component_name": "View"}, "stage": "complete",
            "mutation_attempted": True, "change_state_known": True, "changed": True, "postcondition_passed": True,
            "compile_requested": False, "compile_attempted": False, "compiled": None, "save_requested": False, "saved": False}
        result.update(overrides)
        return result

    def test_exact_native_arguments_and_pure_text_short_name_adapter(self):
        result = self.ops.list_blueprint_components("BP_Test")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls, [("list_blueprint_components", ("/Game/Blueprints/BP_Test", "", 10000))])
        self.assertEqual(result["data"]["blueprint_name"], "BP_Test")
        self.assertFalse(result["data"]["adapter_preloaded_asset"])
        self.assertFalse(result["data"]["journaled"])

    def test_exact_engine_or_plugin_path_passes_without_game_restriction(self):
        for path in ("/Engine/Example/BP", "/SomePlugin/Example/BP.BP"):
            self.assertTrue(self.ops.get_component_property(path, "Root", "ComponentTags", 9, 128)["success"])
            self.assertEqual(self.calls[-1][1], (path, "Root", "ComponentTags", 9, 128))

    def test_missing_filter_none_and_empty_share_behavior(self):
        for value in (None, ""):
            self.assertTrue(self.ops.list_blueprint_components("BP_Test", value)["success"])
            self.assertEqual(self.calls[-1][1][1], "")

    def test_no_journal_or_engine_object_api_is_required_by_a_successful_read(self):
        # No journal or loading API is required by reads, even though mutation
        # cases in this suite also provide fake journal paths.
        with patch.object(sys.modules["unreal"], "Paths", None):
            result = self.ops.get_component_property("BP_Test", "Root", "componenttags")
        self.assertTrue(result["success"])
        self.assertEqual(result["changes"], [])
        self.assertFalse(result["compile"]["attempted"])
        self.assertFalse(result["save"]["saved"])
        self.assertFalse(result["retry"]["automatic_retry"])
        self.assertEqual(result["validation"]["independent_value_check"], "not_run")

    def test_old_array_text_encoding_is_not_silently_converted(self):
        result = self.ops.get_component_property("BP_Test", "Root", "ComponentTags")
        self.assertEqual(result["data"]["property_value"], '("One","Two")')
        self.assertEqual(result["data"]["property_encoding"], "legacy_ue_export_text_bounded_scalar_array")

    def test_native_missing_and_unsupported_errors_retain_partial_diagnostics(self):
        for code in ("component_not_found", "component_property_not_found", "component_property_type_unsupported", "component_budget_exceeded"):
            self.override = self.envelope(success=False, error_code=code, error="native diagnostic", postcondition_passed=False, auto_retry_safe=False)
            result = self.ops.get_component_property("BP_Test", "Root", "Unsupported")
            self.assertFalse(result["success"])
            self.assertEqual(result["error_code"], code)
            self.assertEqual(result["data"]["components"], self.override["data"]["components"])
            self.assertEqual(result["changes"], [])

    def test_dirty_and_status_failure_records_observed_changes(self):
        self.override = self.envelope(success=False, error_code="component_read_state_changed", error="dirty/status changed", changed=True,
            dirty_after=True, blueprint_status_after=4, postcondition_passed=False, auto_retry_safe=False)
        result = self.ops.list_blueprint_components("BP_Test")
        self.assertFalse(result["success"])
        self.assertTrue(result["changes"][0]["changed"])
        self.assertEqual(result["changes"][0]["blueprint_status_after"], 4)
        self.assertFalse(result["changes"][0]["explicit_mutation_requested"])

    def test_failed_load_is_unknown_not_an_empty_no_change_assertion(self):
        self.override = self.envelope(success=False, loaded_during_read=True, change_state_known=False,
            error_code="blueprint_not_found", error="failed load", postcondition_passed=False, auto_retry_safe=False)
        self.override.pop("changed")
        result = self.ops.list_blueprint_components("Missing")
        self.assertIsNone(result["changes"][0]["changed"])
        self.assertFalse(result["changes"][0]["change_state_known"])
        self.assertFalse(result["retry"]["automatic_retry"])

    def test_successful_load_does_not_claim_loading_side_effects_verified(self):
        self.override = self.envelope(loaded_during_read=True, auto_retry_safe=False)
        result = self.ops.list_blueprint_components("BP_Test")
        self.assertTrue(result["success"])
        self.assertFalse(result["data"]["loading_side_effects_verified"])
        self.assertFalse(result["retry"]["native_auto_retry_safe_within_scope"])
        self.assertEqual(result["retry"]["duplicate_risk"], "implicit_loading_effects_not_audited")

    def test_false_success_with_unknown_or_changed_state_is_rejected(self):
        for overrides in ({"change_state_known": False}, {"changed": True}, {"postcondition_passed": False}):
            self.override = self.envelope(**overrides)
            result = self.ops.list_blueprint_components("BP_Test")
            self.assertEqual(result["error_code"], "component_read_unverified")

    def test_unexpected_mutation_or_save_is_never_reported_as_clean_read(self):
        for key in ("mutation_attempted", "compile_requested", "save_requested", "saved"):
            self.override = self.envelope(**{key: True})
            result = self.ops.list_blueprint_components("BP_Test")
            self.assertEqual(result["error_code"], "unexpected_component_read_mutation")
            self.assertTrue(result["data"]["native_outcome"][key])
            self.assertTrue(result["changes"])
            self.assertIsNone(result["changes"][-1]["changed"])
            self.assertEqual(result["stage"], "validate_native_read")

    def test_malformed_json_and_dispatch_exception_preserve_uncertainty(self):
        for raw in ("not json", "[]", self.envelope(mutation_attempted="false"), RuntimeError("native exception")):
            self.override = raw
            result = self.ops.list_blueprint_components("BP_Test")
            self.assertFalse(result["success"])
            self.assertIsNone(result["changes"][0]["changed"])
            self.assertEqual(result["changes"][0]["outcome"], "native_read_outcome_unknown")

    def test_unavailable_native_library_is_known_not_dispatched(self):
        del self.library.list_blueprint_components
        result = self.ops.list_blueprint_components("BP_Test")
        self.assertEqual(result["error_code"], "native_component_api_unavailable")
        self.assertFalse(result["data"]["native_dispatched"])
        self.assertEqual(result["changes"], [])

    def test_budgets_reject_bool_float_overflow_and_do_not_dispatch(self):
        for value in (True, 0, 10001, 1.0, "10", None):
            self.assertEqual(self.ops.list_blueprint_components("BP_Test", max_components=value)["error_code"], "component_budget_invalid")
        for value in (True, 63, 65537, 128.0):
            self.assertEqual(self.ops.get_component_property("BP_Test", "Root", "P", max_value_chars=value)["error_code"], "component_budget_invalid")
        self.assertEqual(self.calls, [])

    def test_selector_control_surrogate_and_utf16_boundaries_precede_native(self):
        for value in ("", None, "Root\x00Other", "X" * 1024, "\U0001f600" * 512, "\ud800"):
            self.assertEqual(self.ops.get_component_property("BP_Test", value, "P")["error_code"], "component_selector_invalid")
        self.assertEqual(self.calls, [])
        self.assertTrue(self.ops.get_component_property("BP_Test", "X" * 1023, "P")["success"])

    def test_relative_export_aliases_rejected_without_loading_fallback(self):
        for name in ("Folder/BP_Test", "Blueprint'/Game/BP_Test.BP_Test'", "BP_Test.BP_Test", " "):
            self.assertEqual(self.ops.list_blueprint_components(name)["error_code"], "blueprint_path_invalid")
        self.assertEqual(self.calls, [])

    def test_result_copies_native_data_and_does_not_mutate_it(self):
        self.override = self.envelope()
        before = copy.deepcopy(self.override)
        result = self.ops.list_blueprint_components("BP_Test")
        result["data"]["components"][0]["component_name"] = "ModifiedResult"
        self.assertEqual(self.override, before)

    def test_schema_preserves_two_legacy_names_and_nonempty_or_nullable_defaults(self):
        tree = ast.parse((PACKAGE / "registration/component_toolset.py").read_text(encoding="utf-8"))
        methods = {node.name: node for cls in tree.body if isinstance(cls, ast.ClassDef) for node in cls.body if isinstance(node, ast.FunctionDef)}
        self.assertEqual(set(methods), {"list_blueprint_components", "get_component_property", "add_component_to_blueprint", "set_component_property", "set_static_mesh_properties", "set_physics_properties"})
        mesh = methods["set_static_mesh_properties"]
        self.assertEqual([arg.arg for arg in mesh.args.args], ["blueprint_name", "component_name", "static_mesh", "operation_id", "material"])
        self.assertEqual(list(map(ast.literal_eval, mesh.args.defaults)), ["/Engine/BasicShapes/Cube.Cube", None, None])
        for name in ("list_blueprint_components", "get_component_property"):
            method = methods[name]
            args = [v.arg for v in method.args.args]
            defaults = dict(zip(args[len(args) - len(method.args.defaults):], map(ast.literal_eval, method.args.defaults)))
            self.assertEqual(defaults["max_components"], 10000)
            if name == "list_blueprint_components":
                self.assertIsNone(defaults["component_name"])
                self.assertEqual([v for v in args if v not in defaults], ["blueprint_name"])
            else:
                self.assertEqual(defaults["max_value_chars"], 65536)
                self.assertEqual([v for v in args if v not in defaults], ["blueprint_name", "component_name", "property_name"])

    def test_add_maps_json_and_compile_without_save_then_replays(self):
        result = self.ops.add_component_to_blueprint("BP_Test", "CameraComponent", "View",
            component_properties_json='{"bVisible":false}', operation_id="component-add")
        self.assertTrue(result["success"], result)
        method, args = self.calls[0]
        self.assertEqual(method, "add_component_to_blueprint")
        self.assertEqual(args[:3], ("/Game/Blueprints/BP_Test", "CameraComponent", "View"))
        self.assertEqual(json.loads(args[3]), {"location": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1]})
        self.assertEqual(args[4:], ("", '{"bVisible":false}', True))
        self.assertTrue(result["compile"]["compiled"])
        self.assertFalse(result["save"]["saved"])
        replay = self.ops.add_component_to_blueprint("BP_Test", "CameraComponent", "View",
            component_properties_json='{"bVisible":false}', operation_id="component-add")
        self.assertTrue(replay["retry"]["replayed"])
        self.assertEqual(len(self.calls), 1)

    def test_set_verified_noop_retains_native_result_without_changes(self):
        self.override = self.mutation_envelope(mutation_attempted=False, changed=False)
        result = self.ops.set_component_property("BP_Test", "View", "bVisible", "false", "component-noop")
        self.assertTrue(result["success"], result)
        self.assertEqual(result["changes"], [])
        self.assertFalse(result["data"]["native_mutation_attempted"])
        self.assertFalse(result["compile"]["attempted"])
        self.assertEqual(self.calls[0][1][-1], "false")

    def test_lost_mutation_response_keeps_unknown_compile_and_never_reexecutes(self):
        missing_compile = self.mutation_envelope(compile_requested=True, compile_attempted=True, compiled=True)
        missing_compile.pop("compiled")
        for index, response in enumerate(("not JSON", RuntimeError("response lost"), missing_compile)):
            self.override = response
            operation_id = "component-lost-" + str(index)
            result = self.ops.add_component_to_blueprint("BP_Test", "SceneComponent", "View", operation_id=operation_id)
            self.assertFalse(result["success"])
            self.assertIsNone(result["compile"]["attempted"])
            self.assertIsNone(result["changes"][0]["changed"])
            self.assertTrue(result["data"]["mutation_state_unknown"])
            self.assertFalse(result["retry"]["automatic_retry"])
            calls = len(self.calls)
            replay = self.ops.add_component_to_blueprint("BP_Test", "SceneComponent", "View", operation_id=operation_id)
            self.assertTrue(replay["retry"]["replayed"])
            self.assertEqual(len(self.calls), calls)

    def test_mutation_json_boundaries_reject_before_engine_or_journal(self):
        for value in ('{"X":1,"X":2}', 'NaN', '1e309', '9007199254740992', '"\\u0000"', '"\\ud800"'):
            result = self.ops.set_component_property("BP_Test", "View", "P", value)
            self.assertFalse(result["success"], value)
        result = self.ops.add_component_to_blueprint("BP_Test", "SceneComponent", "View", location_json="[true,0,0]")
        self.assertEqual(result["error_code"], "component_transform_invalid")
        self.assertEqual(self.calls, [])
        self.assertEqual(list(Path(self.temporary.name).rglob("*.json")), [])

    def test_failed_compile_retains_known_mutation_and_diagnostics(self):
        self.override = self.mutation_envelope(success=False, stage="compile", compile_requested=True,
            compile_attempted=True, compiled=False, error_code="component_compile_failed", error="compile failed",
            data={"compile": {"status": "error", "compile_error_count": 1,
                "diagnostics": [{"severity": "error", "message": "bad component"}]}})
        result = self.ops.add_component_to_blueprint("BP_Test", "SceneComponent", "View", operation_id="component-compile-fail")
        self.assertEqual(result["error_code"], "component_compile_failed")
        self.assertTrue(result["changes"][0]["changed"])
        self.assertFalse(result["compile"]["compiled"])
        self.assertEqual(result["compile"]["diagnostics"], self.override["data"]["compile"]["diagnostics"])
        self.assertEqual(result["compile"]["error_count"], 1)
        self.assertFalse(result["save"]["saved"])

    def test_named_static_mesh_default_shares_native_core_and_keeps_own_identity(self):
        result = self.ops.set_static_mesh_properties("BP_Test", "View", operation_id="named-mesh")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls, [("set_component_property", ("/Game/Blueprints/BP_Test", "View", "StaticMesh", '"/Engine/BasicShapes/Cube.Cube"'))])
        self.assertEqual(result["data"]["operation"], "set_static_mesh_properties")
        self.assertEqual(result["data"]["component"], "View")
        self.assertEqual(result["data"]["requested_static_mesh"], "/Engine/BasicShapes/Cube.Cube")
        self.assertTrue(result["data"]["legacy_internal_material_supported"])
        self.assertFalse(result["data"]["material_requested"])
        self.assertFalse(result["data"]["full_legacy_command_parity"])
        self.assertFalse(result["compile"]["attempted"])
        self.assertFalse(result["save"]["saved"])
        replay = self.ops.set_static_mesh_properties("BP_Test", "View", "/Engine/BasicShapes/Cube", "named-mesh")
        self.assertTrue(replay["retry"]["replayed"])
        conflict = self.ops.set_component_property("BP_Test", "View", "StaticMesh", '"/Engine/BasicShapes/Cube.Cube"', "named-mesh")
        self.assertEqual(conflict["error_code"], "operation_id_conflict")
        self.assertEqual(len(self.calls), 1)

    def test_pre_material_extension_mesh_journal_replays_with_unchanged_arguments(self):
        arguments = {"blueprint_path": "/Game/Blueprints/BP_Test", "component_name": "View", "static_mesh": "/Engine/BasicShapes/Cube.Cube"}
        self.ops.state.mutate("set_static_mesh_properties", arguments, "pre-r-mesh",
            lambda result: result.update(success=True, stage="complete", data={"legacy_internal_material_supported": False, "old_record": True}))
        replay = self.ops.set_static_mesh_properties("BP_Test", "View", "/Engine/BasicShapes/Cube", "pre-r-mesh", material=None)
        self.assertTrue(replay["retry"]["replayed"])
        self.assertTrue(replay["data"]["old_record"])
        self.assertFalse(replay["data"]["legacy_internal_material_supported"])
        self.assertEqual(self.ops.state.get_operation("pre-r-mesh")["arguments"], arguments)
        self.assertEqual(self.calls, [])

    def test_named_static_mesh_does_not_clear_or_hide_native_load_failure(self):
        for mesh in (None, "", "Cube", "/UnauditedPlugin/Mesh.Mesh"):
            result = self.ops.set_static_mesh_properties("BP_Test", "View", mesh)
            self.assertFalse(result["success"])
            self.assertEqual(result["changes"], [])
        self.assertEqual(self.calls, [])
        self.override = self.mutation_envelope(success=False, stage="preflight", mutation_attempted=False,
            changed=False, postcondition_passed=False, error_code="component_mesh_reference_invalid", error="Mesh did not resolve")
        result = self.ops.set_static_mesh_properties("BP_Test", "View", "/Game/Missing.Missing", "bad-mesh")
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "component_mesh_reference_invalid")
        self.assertEqual(result["data"]["component"], "View")
        self.assertEqual(result["changes"], [])
        self.assertFalse(result["save"]["saved"])

    def test_named_material_only_and_both_use_one_native_dispatch_with_explicit_intent(self):
        for mesh in ("", "/Engine/BasicShapes/Sphere"):
            with self.subTest(mesh=mesh):
                result = self.ops.set_static_mesh_properties("BP_Test", "View", mesh, "material-" + str(len(self.calls)), "/Game/M_Test")
                self.assertTrue(result["success"], result)
                normalized_mesh = mesh + ".Sphere" if mesh else ""
                self.assertEqual(self.calls[-1], ("set_static_mesh_properties", ("/Game/Blueprints/BP_Test", "View", normalized_mesh, "/Game/M_Test.M_Test")))
                self.assertTrue(result["data"]["material_requested"])
                self.assertEqual(result["data"]["requested_material"], "/Game/M_Test.M_Test")
                self.assertFalse(result["compile"]["attempted"])
                self.assertFalse(result["save"]["saved"])
        default = self.ops.set_static_mesh_properties("BP_Test", "View", material="/Game/M_Test.M_Test", operation_id="material-default-mesh")
        self.assertTrue(default["success"])
        self.assertEqual(self.calls[-1][1][2], "/Engine/BasicShapes/Cube.Cube")
        self.assertEqual(len(self.calls), 3)

    def test_material_invalid_inputs_and_lost_combined_response_do_not_dispatch_twice(self):
        for value in ("", "M_Test", "/UnauditedPlugin/M_Test", False, 7):
            result = self.ops.set_static_mesh_properties("BP_Test", "View", "", material=value)
            self.assertFalse(result["success"])
            self.assertEqual(result["changes"], [])
        self.assertEqual(self.calls, [])
        self.assertEqual(list(Path(self.temporary.name).rglob("*.json")), [])
        self.override = self.mutation_envelope(success=False, stage="preflight", mutation_attempted=False, changed=False,
            postcondition_passed=False, error_code="component_material_reference_invalid", error="Material reference failed before editing")
        rejected = self.ops.set_static_mesh_properties("BP_Test", "View", material="/Game/Missing", operation_id="bad-material")
        self.assertEqual(rejected["error_code"], "component_material_reference_invalid")
        self.assertEqual(rejected["changes"], [])
        self.override = RuntimeError("combined native response lost")
        lost = self.ops.set_static_mesh_properties("BP_Test", "View", "", "material-lost", "/Game/M_Test")
        self.assertTrue(lost["data"]["mutation_state_unknown"])
        self.assertIsNone(lost["changes"][0]["changed"])
        self.assertFalse(lost["retry"]["automatic_retry"])
        replay = self.ops.set_static_mesh_properties("BP_Test", "View", "", "material-lost", "/Game/M_Test.M_Test")
        self.assertTrue(replay["retry"]["replayed"])
        self.assertEqual([method for method, _ in self.calls], ["set_static_mesh_properties", "set_static_mesh_properties"])

    def test_shape_class_aliases_use_same_add_adapter_without_extra_compile(self):
        for alias, kind in (("Sphere", "SphereComponent"), ("UBoxComponent", "BoxComponent"),
                ("/Script/Engine.CapsuleComponent", "CapsuleComponent")):
            result = self.ops.add_component_to_blueprint("BP_Test", alias, kind + "Node", compile_blueprint=False, operation_id=kind)
            self.assertTrue(result["success"], result)
            self.assertEqual(self.calls[-1][0], "add_component_to_blueprint")
            self.assertEqual(self.calls[-1][1][1], kind)
            self.assertFalse(result["compile"]["attempted"])
            self.assertFalse(result["save"]["saved"])

    def test_named_physics_defaults_keep_native_observations_and_replay_identity(self):
        observed = {"simulate_physics": True, "gravity_enabled": True, "mass_override_enabled": True,
            "mass": 1.0, "linear_damping": 0.009999999776482582, "angular_damping": 0.0, "physics_blend_weight": 1.0}
        self.override = self.mutation_envelope(data={"component": "Body", "physics_before": {}, "physics_after": observed,
            "legacy_gravity_ignored_fixed": True, "runtime_simulation_verified": False})
        result = self.ops.set_physics_properties("BP_Test", "Body", operation_id="physics-defaults")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls, [("set_physics_properties", ("/Game/Blueprints/BP_Test", "Body", True, True, 1.0, 0.01, 0.0))])
        self.assertEqual(result["data"]["operation"], "set_physics_properties")
        self.assertEqual(result["data"]["physics_after"], observed)
        self.assertTrue(result["data"]["legacy_gravity_ignored_fixed"])
        self.assertFalse(result["data"]["runtime_simulation_verified"])
        self.assertFalse(result["compile"]["attempted"])
        self.assertFalse(result["save"]["saved"])
        replay = self.ops.set_physics_properties("BP_Test", "Body", mass=1, angular_damping=0, operation_id="physics-defaults")
        self.assertTrue(replay["retry"]["replayed"])
        self.assertEqual(len(self.calls), 1)
        self.override = self.mutation_envelope(mutation_attempted=False, changed=False, data={"no_op": True})
        noop = self.ops.set_physics_properties("BP_Test", "Body", operation_id="physics-noop")
        self.assertTrue(noop["success"])
        self.assertEqual(noop["changes"], [])

    def test_physics_input_types_and_numeric_bounds_precede_dispatch_and_journal(self):
        cases = [{"simulate_physics": 1}, {"gravity_enabled": "true"}, {"mass": 0}, {"mass": 0.0009}, {"mass": -1},
            {"mass": True}, {"mass": "1"}, {"mass": None}, {"mass": 10**400}, {"linear_damping": float("nan")},
            {"linear_damping": float("inf")}, {"linear_damping": -0.001}, {"angular_damping": 1.0e12 + 1}]
        for values in cases:
            with self.subTest(values=values):
                result = self.ops.set_physics_properties("BP_Test", "Body", **values)
                self.assertEqual(result["error_code"], "component_physics_value_invalid")
                self.assertEqual(result["changes"], [])
        self.assertEqual(self.calls, [])
        self.assertEqual(list(Path(self.temporary.name).rglob("*.json")), [])

    def test_physics_lost_reply_remains_unknown_and_same_id_never_reexecutes(self):
        self.override = RuntimeError("response lost after public setters")
        result = self.ops.set_physics_properties("BP_Test", "Body", simulate_physics=False, gravity_enabled=False,
            mass=7.5, linear_damping=0.2, angular_damping=0.3, operation_id="physics-lost")
        self.assertFalse(result["success"])
        self.assertTrue(result["data"]["mutation_state_unknown"])
        self.assertIsNone(result["changes"][0]["changed"])
        self.assertFalse(result["compile"]["requested"])
        self.assertFalse(result["save"]["saved"])
        replay = self.ops.set_physics_properties("BP_Test", "Body", False, False, 7.5, 0.2, 0.3, "physics-lost")
        self.assertTrue(replay["retry"]["replayed"])
        self.assertFalse(replay["retry"]["automatic_retry"])
        self.assertEqual(len(self.calls), 1)


if __name__ == "__main__": unittest.main()
