"""AA changed-boundary tests; fake engine, real adapters/actions/journals."""

import copy
import importlib
import json
import types
import unittest
from unittest.mock import patch

import test_batch_contract as batch_fixture
import test_graph_identity_links as graph_fixture


CLASS = "/Game/Tests/BP_Component.BP_Component_C"


class ExternalMemberContracts(unittest.TestCase):
    def setUp(self):
        self.f = graph_fixture.GraphIdentityTests()
        self.f.setUp()
        self.addCleanup(self.f.doCleanups)
        self.api = importlib.import_module("cubeless_mcp_ue58.compatibility.ue58.graph_variables")
        self.ops = importlib.import_module("cubeless_mcp_ue58.operations.graph_variables")
        self.source = {"class_path": CLASS, "package_name": CLASS.split(".")[0], "loaded": False,
                       "dirty": False, "status": None, "file": {"path": "fixture.uasset", "exists": True, "sha256": "source"}}
        self.api.external_source_state = lambda path: copy.deepcopy(self.source)
        self.calls, self.plan_calls = [], 0
        self.lose_reply, self.change_source = False, False

        def load(path):
            self.source.update(loaded=True, status="up_to_date")
            return types.SimpleNamespace(get_path_name=lambda: path)
        self.api.load_external_class = load
        self.value_type, self.self_type = "real:float", "object:" + CLASS

        def plan(path, name, setter, target):
            self.plan_calls += 1
            return self.f.native_result(variable_name=name, setter=setter, member_scope="external_generated_component",
                target_class_path=target, property_path="SKEL_Component_C:" + name, compiled_property_path=CLASS + ":" + name,
                declaring_class_path=target, declaring_blueprint_path=CLASS[:-2], variable_guid="a" * 32,
                expected_pin_type_text=self.value_type, expected_self_pin_type_text=self.self_type,
                external_source_validation={"success": True, "dependency_audit": {"edges": [], "elapsed_ms": self.plan_calls}})
        native = self.f.api.unreal.CubelessGraphLibrary
        native.read_external_member_variable_plan = plan
        original_identities = native.read_node_identities

        def identities(path):
            value = json.loads(original_identities(path))
            for row in value["nodes"]:
                if row["object_path"].endswith("Node4"):
                    row.update(self_context=False, variable_property_path="SKEL_Component_C:Mass",
                        variable_member_parent_path=CLASS, variable_member_guid="a" * 32)
            return json.dumps(value)
        native.read_node_identities = identities

        def spawn(name, class_path, setter):
            self.calls.append((name, class_path, setter))
            node = graph_fixture.Node(4, self.f)
            node.get_class = lambda: types.SimpleNamespace(get_name=lambda: "K2Node_VariableSet" if setter else "K2Node_VariableGet")
            value = graph_fixture.Pin(node, name, "input" if setter else "output")
            value.type_text = self.value_type
            target = graph_fixture.Pin(node, "self", "input", "object")
            target.type_text = self.self_type
            self.f.nodes.append(node)
            if self.change_source:
                self.source["file"]["sha256"] = "unexpected-save"
            if self.lose_reply:
                raise RuntimeError("lost spawn response")
            return node
        self.f.editor.add_get_member_variable_node = lambda name, path: spawn(name, path, False)
        self.f.editor.add_set_member_variable_node = lambda name, path: spawn(name, path, True)

    def call(self, setter=False, **kwargs):
        method = self.ops.add_blueprint_variable_set_node if setter else self.ops.add_blueprint_variable_get_node
        return method("/Game/Test", "Mass", operation_id="AA-member", target_class=CLASS, **kwargs)

    def test_external_setter_uses_exact_source_class_position_and_preserves_source(self):
        result = self.call(True, node_position_json="[40,-20]")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls, [("Mass", CLASS, True)])
        self.assertEqual((result["data"]["x"], result["data"]["y"]), (40, -20))
        self.assertFalse(result["data"]["external_source_before_load"]["loaded"])
        self.assertTrue(result["data"]["external_source_preserved"])
        self.assertFalse(result["data"]["self_context"])
        self.assertTrue(self.call(True, node_position_json="[40,-20]")["retry"]["replayed"])
        self.assertEqual(len(self.calls), 1)

    def test_source_change_or_lost_reply_keeps_after_and_never_replays_spawn(self):
        for mode in ("change_source", "lose_reply"):
            with self.subTest(mode=mode):
                self.f.nodes[:] = self.f.nodes[:3]
                self.source["file"]["sha256"] = "source"
                setattr(self, mode, True)
                method = self.ops.add_blueprint_variable_get_node
                result = method("/Game/Test", "Mass", operation_id=mode, target_class=CLASS)
                self.assertFalse(result["success"], result)
                self.assertTrue(result["data"]["mutation_state_unknown"])
                self.assertEqual(result["data"]["after"]["node_count"], 4)
                count = len(self.calls)
                self.assertFalse(method("/Game/Test", "Mass", operation_id=mode, target_class=CLASS)["success"])
                self.assertEqual(len(self.calls), count)
                setattr(self, mode, False)

    def test_old_positional_arguments_and_omitted_option_fingerprint_remain_exact(self):
        observed = []
        with patch.object(self.ops, "mutate", side_effect=lambda name, args, opid, action: observed.append((name, args, opid)) or {}):
            self.ops.add_blueprint_variable_get_node("/Game/Test", "Mass", "EventGraph", 5, 9, "prior-id")
            self.ops.add_blueprint_variable_get_node("/Game/Test", "Mass", "EventGraph", 5, 9, "prior-id",
                target_class="", graph_id="", graph_type=None, create_graph_if_missing=False, node_position_json=None)
            rejected = self.ops.add_blueprint_variable_get_node("/Game/Test", "Mass", "EventGraph", 5, 9, "conflict",
                node_position_json="[0,0]")
        self.assertEqual(observed[0], observed[1])
        self.assertEqual(observed[0][1], {"blueprint_name": "/Game/Test", "variable_name": "Mass", "graph_name": "EventGraph", "x": 5, "y": 9})
        self.assertEqual(rejected["error_code"], "node_position_conflict")


class PinDefaultContracts(unittest.TestCase):
    def setUp(self):
        self.f = graph_fixture.GraphIdentityTests()
        self.f.setUp()
        self.addCleanup(self.f.doCleanups)
        self.ops = importlib.import_module("cubeless_mcp_ue58.operations.pin_defaults")
        self.f.api.sequence_editor = lambda *args, **kwargs: (self.f.editor, {"object_path": graph_fixture.GRAPH, "graph_name": "EventGraph"})
        self.values = {"target": "1.0", "other": "2.0"}
        self.f.target.get_pin_value = lambda: self.values["target"]
        self.f.other.get_pin_value = lambda: self.values["other"]
        self.f.target.type_text = 'number'
        self.calls, self.corrupt_other, self.lose_reply, self.no_op = 0, False, False, False
        self.clear_float_flag = False

        def set_default(graph_path, node_path, pin_name, value_json, direction):
            self.calls += 1
            self.assertEqual(node_path, self.f.nodes[1].get_path_name())
            previous_type = self.f.target.type_text
            if self.clear_float_flag:
                self.f.target.type_text = 'real:float;legacy_single=false'
            if not self.no_op:
                self.values["target"] = str(json.loads(value_json))
            if self.corrupt_other:
                self.values["other"] = "bad"
            if self.lose_reply:
                raise RuntimeError("lost reply")
            return json.dumps(dict(success=True, stage="complete", mutation_attempted=not self.no_op, changed=not self.no_op,
                change_state_known=True, postcondition_passed=True, compile_requested=False, compile_attempted=False,
                compiled=False, save_requested=False, save_attempted=False, saved=False,
                data={"node_id": "2" * 32,
                    "legacy_float_serialization_flag_cleared": self.clear_float_flag,
                    "previous": {"pin_type_text": previous_type},
                    "pin": {"name": pin_name, "default_value": self.values["target"], "pin_type_text": self.f.target.type_text}}))
        self.f.api.unreal.CubelessGraphLibrary.set_blueprint_pin_default = set_default

    def call(self, opid="AA-pin"):
        return self.ops.set_blueprint_pin_default("/Game/Test", self.f.nodes[1].guid, "Mass", "0.0", operation_id=opid)

    def test_named_pin_default_preserves_other_state_noop_and_replay(self):
        result = self.call()
        self.assertTrue(result["success"], result)
        self.assertTrue(result["data"]["other_graph_state_preserved"])
        self.assertEqual(self.values, {"target": "0.0", "other": "2.0"})
        self.assertTrue(self.call()["retry"]["replayed"])
        self.assertEqual(self.calls, 1)
        self.no_op = True
        result = self.call("new-noop")
        self.assertTrue(result["success"], result)
        self.assertEqual(result["changes"], [])
        self.assertEqual(result["data"]["delta"]["skipped_count"], 1)

    def test_lost_reply_and_unrelated_change_cannot_be_success(self):
        self.corrupt_other = True
        result = self.call("corrupt")
        self.assertFalse(result["success"], result)
        self.assertFalse(result["data"]["other_graph_state_preserved"])
        self.corrupt_other = False
        self.lose_reply = True
        result = self.call("lost")
        self.assertFalse(result["success"], result)
        self.assertTrue(result["data"]["mutation_state_unknown"])
        self.assertIn("graph_after", result["data"])
        count = self.calls
        self.assertFalse(self.call("lost")["success"])
        self.assertEqual(self.calls, count)

    def test_native_verified_legacy_float_bit_change_preserves_other_types(self):
        self.clear_float_flag = True
        self.f.target.type_text = 'real:float;legacy_single=true'
        result = self.call('legacy-float')
        self.assertTrue(result['success'], result)
        self.assertTrue(result['data']['other_graph_state_preserved'])
        self.assertEqual(result['data']['graph_before']['nodes'][1]['pins'][0]['native_pin_type_text'],
                         'real:float;legacy_single=true')
        self.assertEqual(result['data']['graph_after']['nodes'][1]['pins'][0]['native_pin_type_text'],
                         'real:float;legacy_single=false')


class AdditiveBatchContracts(unittest.TestCase):
    def test_old_normalization_and_new_named_pin_action(self):
        fixture = batch_fixture.BatchContractTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        command = {"type": "CubelessBlueprintGraphTools.add_blueprint_variable_get_node", "params": {"blueprint_name": "/Game/Test", "variable_name": "Mass"}}
        values = fixture.batch._normalize(json.dumps([command]), True)
        self.assertEqual(values[0]["params"], {"blueprint_name": "/Game/Test", "variable_name": "Mass", "graph_name": "EventGraph", "x": 0, "y": 0})
        command["params"].update(target_class=None, graph_id="", graph_type=None, node_position_json=None, create_graph_if_missing=False)
        self.assertEqual(fixture.batch._normalize(json.dumps([command]), True), values)
        self.assertIs(fixture.batch._CAPABILITIES["CubelessBlueprintGraphTools.set_blueprint_pin_default"]["action"],
                      fixture.pin_defaults.set_blueprint_pin_default_action)


if __name__ == "__main__":
    unittest.main()
