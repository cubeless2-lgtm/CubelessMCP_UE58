"""Registration cache/default failure isolation using fake UE boundaries, no engine."""
import importlib.util
import json
from pathlib import Path
import sys
import types
import unittest
from unittest.mock import patch


class RegistrationTests(unittest.TestCase):
    def setUp(self):
        self.events = []
        self.registered = set()
        self.bad_schema = False
        self.preparation = {"success": True, "both_exact_converters_active": True,
                            "node_position_default": [0, 0], "parameter_names_default": []}
        self.classes = {}
        modules = {}
        for module_name, class_names in {
            "toolsets": ["CubelessBlueprintTools", "CubelessPythonDiagnostics"],
            "graph_toolset": ["CubelessBlueprintGraphTools"],
            "material_toolset": ["CubelessMaterialTools"],
            "batch_toolset": ["CubelessBatchTools"],
            "actor_toolset": ["CubelessActorTools"],
            "component_toolset": ["CubelessComponentTools"],
        }.items():
            full_name = "cubeless_mcp_ue58.registration." + module_name
            module = types.ModuleType(full_name)
            for name in class_names:
                cls = type(name, (), {"__module__": full_name})
                cls.static_class = classmethod(lambda value: value)
                self.classes[name] = cls
                setattr(module, name, cls)
            modules[full_name] = module
        def register(cls):
            self.events.append(("register", cls.__name__))
            self.registered.add(cls)
        def unregister(cls):
            self.events.append(("unregister", cls.__name__))
            self.registered.discard(cls)
        def prepare(cls):
            self.assertNotIn(cls, self.registered, "Native helper must precede cached function schema creation")
            self.events.append(("prepare", cls.__name__))
            return json.dumps(self.preparation)
        def schema(cls):
            values = []
            if cls is self.classes["CubelessMaterialTools"]:
                for name, parameter, default, kind in (
                    ("add_material_node", "node_position", [0, 0], "integer"),
                    ("get_material_parameter_collection_values", "parameter_names", [], "string"),
                ):
                    values.append({"name": cls.__name__ + "." + name,
                        "inputSchema": {"required": [parameter] if self.bad_schema else [],
                            "properties": {parameter: {"type": "array", "items": {"type": kind}, "default": default}}}})
            return json.dumps({"name": cls.__name__, "tools": values})
        self.registry = types.SimpleNamespace(is_available=lambda: True,
            is_toolset_class_registered=lambda cls: cls in self.registered,
            register_toolset_class=register, unregister_toolset_class=unregister,
            get_toolset_json_schema=schema)
        self.unreal = types.SimpleNamespace(ToolsetRegistry=self.registry,
            CubelessRegistrationLibrary=types.SimpleNamespace(prepare_material_toolset=prepare),
            log=lambda message: self.events.append(("log", message)),
            log_warning=lambda message: self.events.append(("warning", message)))
        modules["unreal"] = self.unreal
        modules["atexit"] = types.SimpleNamespace(register=lambda fn: None)
        self.modules = patch.dict(sys.modules, modules)
        self.modules.start()
        self.addCleanup(self.modules.stop)
        path = Path(__file__).parents[1] / "cubeless_mcp_ue58/registration/__init__.py"
        spec = importlib.util.spec_from_file_location("cubeless_mcp_ue58.registration", path)
        self.subject = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.subject)
        self.material = self.classes["CubelessMaterialTools"]

    def test_prepare_precedes_cache_creation_and_success_checks_defaults(self):
        result = self.subject.register()
        self.assertTrue(result["ready"], result)
        self.assertEqual(result["material_preparation"]["stage"], "registered_schema_verified")
        self.assertLess(self.events.index(("prepare", self.material.__name__)), self.events.index(("register", self.material.__name__)))

    def test_existing_material_cache_is_invalidated_without_reregistering_other_tools(self):
        self.registered.update(self.classes.values())
        result = self.subject.register()
        self.assertTrue(result["ready"])
        writes = [event for event in self.events if event[0] in {"register", "unregister", "prepare"}]
        self.assertEqual(writes, [("unregister", self.material.__name__), ("prepare", self.material.__name__), ("register", self.material.__name__)])

    def test_missing_helper_blocks_only_material_and_health_reports_partial(self):
        del self.unreal.CubelessRegistrationLibrary
        result = self.subject.register()
        self.assertFalse(result["ready"])
        self.assertNotIn(self.material, self.registered)
        self.assertEqual(len(self.registered), len(self.classes) - 1)
        self.assertIn("helper", result["material_preparation"]["error"])
        self.assertEqual(self.subject.registration_status(), result)

    def test_failed_prepare_removes_cached_material_and_reports_native_reason(self):
        self.registered.add(self.material)
        self.preparation = {"success": False, "error": "wrong exact owner path"}
        result = self.subject.register()
        self.assertFalse(result["ready"])
        self.assertNotIn(self.material, self.registered)
        self.assertEqual(len(self.registered), len(self.classes) - 1)
        self.assertEqual(result["material_preparation"]["native"]["error"], "wrong exact owner path")

    def test_converter_false_or_wrong_default_cannot_claim_ready(self):
        for field, bad in (("both_exact_converters_active", False), ("node_position_default", [1, 2]), ("parameter_names_default", ["Root"])):
            with self.subTest(field=field):
                previous = self.preparation[field]
                self.preparation[field] = bad
                result = self.subject.register()
                self.assertFalse(result["ready"])
                self.assertNotIn(self.material, self.registered)
                self.assertEqual(len(self.registered), len(self.classes) - 1)
                self.preparation[field] = previous

    def test_actual_schema_required_regression_unregisters_material(self):
        self.bad_schema = True
        result = self.subject.register()
        self.assertFalse(result["ready"])
        self.assertNotIn(self.material, self.registered)
        self.assertEqual(len(self.registered), len(self.classes) - 1)
        self.assertEqual(result["material_preparation"]["stage"], "material_registration_failed")
        self.assertIn("array/default contract", result["material_preparation"]["error"])

    def test_unavailable_registry_diagnostic_never_claims_ready(self):
        self.subject.register()
        self.registry.is_available = lambda: False
        result = self.subject.registration_status()
        self.assertFalse(result["available"])
        self.assertFalse(result["ready"])
        self.assertIn("material_preparation", result)


if __name__ == "__main__":
    unittest.main()
