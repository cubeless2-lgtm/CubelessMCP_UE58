"""Z parent preservation and event journal adapters; no live-engine claims."""

import copy
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import Mock

import test_batch_contract as batch_fixture
import test_generated_component_contract as getter_fixture


PARENT = "/Game/CubelessMCPTests/Z/BP_Parent.BP_Parent_C"


class ParentAndEventTests(unittest.TestCase):
    def setUp(self):
        self.f = batch_fixture.BatchContractTests()
        self.f.setUp()
        self.addCleanup(self.f.doCleanups)
        state = {"class_path": PARENT, "package_name": PARENT.split(".")[0],
            "file": {"exists": True, "sha256": "parent-bytes"}, "loaded": True, "dirty": False, "status": "compiled"}
        self.baseline = {"inspection": {"success": True}, "parents": [state]}
        self.f.api.parent_reference_state = lambda path: copy.deepcopy(state)
        self.f.api.generated_parent_observation = lambda parent: copy.deepcopy(self.baseline)

    def test_parent_named_test_path_allowed_but_real_tool_module_and_mount_rejected(self):
        path = Path(__file__).parents[1] / "cubeless_mcp_ue58/validators/dependencies.py"
        spec = importlib.util.spec_from_file_location("tested_dependencies", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for value in (PARENT, "/Game/MyModelContextProtocolNotes/BP_A.BP_A_C", "/Script/Engine.Actor"):
            module.reject_tool_reference(value)
        for value in ("/CubelessMCP_UE58/Thing.Thing_C", "/Script/CubelessMCPUE58Editor.Type",
                      "/Script/PythonScriptPlugin.Class", "/Engine/PythonTypes/Class"):
            with self.subTest(value=value), self.assertRaises(self.f.state.OperationError):
                module.reject_tool_reference(value)

    def test_factory_preserves_generated_parent_and_old_argument_fingerprint(self):
        observations = []

        def timed(parent):
            value = copy.deepcopy(self.baseline)
            value["inspection"]["dependency_audit"] = {"elapsed_ms": len(observations) + .125,
                "edges": [{"from": "/Game/Parent", "to": "/Script/Engine"}]}
            observations.append(value)
            return value

        self.f.api.generated_parent_observation = timed
        result = self.f.blueprint.create_blueprint("BP_Child", PARENT, "/Game/Blueprints", "child")
        self.assertTrue(result["success"], result)
        self.assertTrue(result["validation"]["generated_parent_preserved"])
        self.assertNotEqual(result["data"]["parent_before_factory"], result["data"]["parent_after_factory"])
        self.assertEqual(self.f.blueprint.parent_comparison_state(result["data"]["parent_before_factory"]),
                         self.f.blueprint.parent_comparison_state(result["data"]["parent_after_factory"]))
        replay = self.f.blueprint.create_blueprint("BP_Child", PARENT, "/Game/Blueprints", "child")
        self.assertTrue(replay["success"])
        self.assertEqual(len(self.f.calls), 1)

    def test_parent_dependency_edge_change_is_not_filtered_as_timing(self):
        reads = []

        def changed(parent):
            value = copy.deepcopy(self.baseline)
            value["inspection"]["dependency_audit"] = {"elapsed_ms": len(reads),
                "edges": ["/Script/Engine" if not reads else "/Script/UnrealEd"]}
            reads.append(1)
            return value

        self.f.api.generated_parent_observation = changed
        result = self.f.blueprint.create_blueprint("BP_Child", PARENT, "/Game/Blueprints", "child")
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "generated_parent_changed")
        self.assertEqual(self.f.calls, [])

    def test_parent_changes_after_known_child_creation_fail_without_losing_creation(self):
        original = self.f.factory_create.side_effect

        def changed(*args):
            asset = original(*args)
            self.baseline["parents"][0]["file"]["sha256"] = "changed-parent"
            return asset

        self.f.factory_create.side_effect = changed
        result = self.f.blueprint.create_blueprint("BP_Child", PARENT, "/Game/Blueprints", "child")
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "generated_parent_changed")
        self.assertTrue(result["changes"][0]["created"])
        self.assertTrue(result["compile"]["implicit_factory_compile"]["factory_returned"])
        self.assertFalse(result["save"]["attempted"])

    def test_event_batch_replay_and_lost_reply_force_stop(self):
        envelope = {"success": True, "stage": "complete", "data": {"node_id": "a" * 32},
            "mutation_attempted": False, "changed": False, "change_state_known": True, "postcondition_passed": True,
            "compile_requested": False, "compile_attempted": False, "compiled": False,
            "save_requested": False, "save_attempted": False, "saved": False}
        call = self.f.events_api.add_blueprint_event_node = Mock(return_value=envelope)
        entry = {"type": "CubelessBlueprintGraphTools.add_blueprint_event_node", "params": {
            "blueprint_name": "BP_Test", "event_name": "ReceiveTick"}}
        result = self.f.execute([entry])
        self.assertTrue(result["success"], result)
        self.assertEqual(call.call_args.kwargs["x"], 0)
        self.assertTrue(self.f.execute([entry])["success"])
        self.assertEqual(call.call_count, 1)
        call.side_effect = RuntimeError("native reply lost")
        failed = self.f.execute([entry, self.f.create_entry()], stop=False, operation_id="lost")
        self.assertFalse(failed["success"])
        self.assertEqual(call.call_count, 2)
        self.assertEqual(self.f.calls, [])


class InheritedGetterTests(unittest.TestCase):
    def setUp(self):
        self.f = getter_fixture.GeneratedComponentGetterTests()
        self.f.setUp()
        self.addCleanup(self.f.doCleanups)

    def test_declaring_parent_preservation_is_checked_after_getter_spawn(self):
        reads = []

        def plan(*args):
            reads.append(1)
            return self.f.f.native_result(variable_name="Mass", property_path="Test_C:Mass", setter=False,
                member_scope="inherited_actor_boolean", variable_guid="b" * 32, declaring_class_path=PARENT,
                declaring_blueprint_path=PARENT[:-2], expected_pin_type_text=self.f.expected_type,
                declaring_parent_validation={"success": True, "status": "compiled" if len(reads) == 1 else "changed"})

        self.f.f.api.unreal.CubelessGraphLibrary.read_member_variable_plan = plan
        result = self.f.get()
        self.assertFalse(result["success"])
        self.assertFalse(result["data"]["declaring_parent_preserved"])
        self.assertEqual(result["data"]["after"]["node_count"], 4)
        self.assertEqual(self.f.spawn_count, 1)

    def test_inherited_getter_preserves_raw_timing_but_compares_parent_semantics(self):
        reads = []

        def plan(*args):
            reads.append(1)
            return self.f.f.native_result(variable_name="Mass", property_path="Test_C:Mass", setter=False,
                member_scope="inherited_actor_boolean", variable_guid="b" * 32, declaring_class_path=PARENT,
                declaring_blueprint_path=PARENT[:-2], expected_pin_type_text=self.f.expected_type,
                declaring_parent_validation={"success": True, "status": "compiled",
                    "dependency_audit": {"elapsed_ms": len(reads), "edges": []}})

        self.f.f.api.unreal.CubelessGraphLibrary.read_member_variable_plan = plan
        result = self.f.get()
        self.assertTrue(result["success"], result)
        self.assertTrue(result["data"]["declaring_parent_preserved"])
        self.assertEqual(result["data"]["member_preflight"]["declaring_parent_validation"]["dependency_audit"]["elapsed_ms"], 1)
        self.assertEqual(result["data"]["member_after"]["declaring_parent_validation"]["dependency_audit"]["elapsed_ms"], 2)


if __name__ == "__main__":
    unittest.main()
