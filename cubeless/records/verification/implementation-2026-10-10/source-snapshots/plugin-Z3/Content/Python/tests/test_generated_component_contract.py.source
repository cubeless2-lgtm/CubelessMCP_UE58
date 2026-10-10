"""Y adapters with real journal/core and fake engine boundaries; no UE claim."""

import importlib
import unittest

import test_batch_contract as batch_fixture
import test_graph_identity_links as graph_fixture


CLASS = "/Game/Tests/BP_Component.BP_Component_C"


class GeneratedComponentArgumentsTests(unittest.TestCase):
    def setUp(self):
        self.f = batch_fixture.BatchContractTests()
        self.f.setUp()
        self.addCleanup(self.f.doCleanups)

    def arguments(self, kind=CLASS, **overrides):
        values = dict(blueprint_name="BP_Test", component_type=kind, component_name="MouseButtons",
            location_json="[0,0,0]", rotation_json="[0,0,0]", scale_json="[1,1,1]",
            parent_component=None, component_properties_json="{}", compile_blueprint=False)
        values.update(overrides)
        return self.f.component._add_arguments(**values)

    def test_exact_generated_class_and_old_aliases_share_core_and_replay(self):
        entry = {"type": "CubelessComponentTools.add_component_to_blueprint", "params": {
            "blueprint_name": "BP_Test", "component_type": CLASS, "component_name": "MouseButtons", "compile_blueprint": False}}
        result = self.f.execute([entry])
        self.assertTrue(result["success"], result)
        self.assertEqual(self.f.component_calls, [self.arguments()])
        replay = self.f.execute([entry])
        self.assertTrue(replay["success"], replay)
        self.assertEqual(len(self.f.component_calls), 1)
        self.assertEqual(self.arguments("USceneComponent"), self.arguments("/Script/Engine.SceneComponent"))

    def test_generated_options_and_aliases_are_rejected_before_dispatch(self):
        for overrides in ({"location_json": "[1,0,0]"}, {"parent_component": "DefaultSceneRoot"},
                          {"component_properties_json": '{"bVisible":false}'}):
            with self.subTest(overrides=overrides), self.assertRaises(self.f.state.OperationError) as caught:
                self.arguments(**overrides)
            self.assertEqual(caught.exception.code, "generated_component_options_unsupported")
        for kind in ("/Game/Tests/BP_Component.SKEL_BP_Component_C", "/Game/Tests/BP_Component", CLASS + ":Subobject"):
            with self.subTest(kind=kind), self.assertRaises(self.f.state.OperationError):
                self.arguments(kind)
        self.assertEqual(self.f.component_calls, [])


class GeneratedComponentGetterTests(unittest.TestCase):
    def setUp(self):
        self.f = graph_fixture.GraphIdentityTests()
        self.f.setUp()
        self.addCleanup(self.f.doCleanups)
        self.api = importlib.import_module("cubeless_mcp_ue58.compatibility.ue58.graph_variables")
        self.ops = importlib.import_module("cubeless_mcp_ue58.operations.graph_variables")
        self.expected_type = '(PinCategory="object",PinSubCategoryObject="' + CLASS + '")'
        native = self.f.api.unreal.CubelessGraphLibrary
        native.read_member_variable_plan = lambda path, name, setter: self.f.native_result(
            variable_name="Mass", property_path="Test_C:Mass", setter=setter,
            member_scope="local_scs_generated_component", scs_variable_guid="a" * 32,
            expected_pin_type_text=self.expected_type)
        self.actual_type = self.expected_type
        self.lose_reply = False
        self.spawn_count = 0

        def spawn(name, class_path):
            self.spawn_count += 1
            node = graph_fixture.Node(4, self.f)
            pin = graph_fixture.Pin(node, "Mass", "output", "object")
            pin.type_text = self.actual_type
            self.f.nodes.append(node)
            if self.lose_reply:
                raise RuntimeError("reply lost after creation")
            return node

        self.f.editor.add_get_member_variable_node = spawn

    def get(self):
        return self.ops.add_blueprint_variable_get_node("/Game/Test", "Mass", operation_id="component-get")

    def test_component_getter_checks_raw_type_without_equating_scs_and_member_guids(self):
        result = self.get()
        self.assertTrue(result["success"], result)
        self.assertTrue(result["data"]["component_output_type_verified"])
        self.assertNotEqual(result["data"]["node_guid"], result["data"]["member_preflight"]["scs_variable_guid"])
        self.assertEqual(result["data"]["node_id"], self.f.nodes[-1].get_path_name())
        self.assertTrue(self.get()["success"])
        self.assertEqual(self.spawn_count, 1)

    def test_matching_display_but_wrong_native_component_type_fails_with_after_snapshot(self):
        self.actual_type = self.expected_type.replace("BP_Component_C", "BP_Other_C")
        result = self.get()
        self.assertFalse(result["success"])
        self.assertFalse(result["data"]["component_output_type_verified"])
        self.assertEqual(result["data"]["after"]["node_count"], 4)
        self.assertEqual(self.spawn_count, 1)

    def test_lost_spawn_reply_preserves_actual_readback_and_never_retries(self):
        self.lose_reply = True
        result = self.get()
        self.assertFalse(result["success"])
        self.assertTrue(result["data"]["mutation_state_unknown"])
        self.assertEqual(result["data"]["after"]["node_count"], 4)
        self.assertTrue(result["changes"][0]["graph_changed"])
        self.assertFalse(self.get()["success"])
        self.assertEqual(self.spawn_count, 1)


if __name__ == "__main__":
    unittest.main()
