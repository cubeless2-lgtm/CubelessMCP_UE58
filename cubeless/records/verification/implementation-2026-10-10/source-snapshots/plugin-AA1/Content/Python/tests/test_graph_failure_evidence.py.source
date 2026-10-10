"""External failure-injection tests; fake graph APIs are not UE validation.

Exercise real operation/journal code with a mutation followed by readback failure.
Run with the same unittest discovery command as test_operation_journal.py.
"""

import copy
import importlib.util
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import Mock, patch


class GraphFailureEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.api = types.ModuleType("cubeless_mcp_ue58.compatibility.ue58.graph")
        modules = {}
        for name in ("cubeless_mcp_ue58", "cubeless_mcp_ue58.operations",
                     "cubeless_mcp_ue58.compatibility", "cubeless_mcp_ue58.compatibility.ue58"):
            module = types.ModuleType(name)
            module.__path__ = []
            modules[name] = module
        modules["unreal"] = types.SimpleNamespace(
            Paths=types.SimpleNamespace(project_saved_dir=lambda: self.temp.name),
            SystemLibrary=types.SimpleNamespace(get_engine_version=lambda: "5.8.3-test-stub"))
        modules[self.api.__name__] = self.api
        modules["cubeless_mcp_ue58.compatibility.ue58"].graph = self.api
        modules["cubeless_mcp_ue58.compatibility.ue58.blueprint"] = types.SimpleNamespace(ensure_editor_idle=lambda: None)
        self.modules_patch = patch.dict(sys.modules, modules)
        self.modules_patch.start()
        self.addCleanup(self.modules_patch.stop)
        self.state = self.load("state")
        self.graph = self.load("graph")
        self.api.graph_editor = Mock(return_value=object())
        self.api.find_node = Mock(side_effect=lambda editor, node_id: node_id)
        self.api.node_id = lambda node: node
        self.api.connection_plan = Mock(return_value={"direct_connection_allowed": True, "already_linked": False,
            "type_exactly_equal": True, "exact_standard_k2_schema": True, "schema_response_name": "make"})

    def load(self, name):
        fullname = "cubeless_mcp_ue58.operations." + name
        path = Path(__file__).parents[1] / "cubeless_mcp_ue58/operations" / (name + ".py")
        spec = importlib.util.spec_from_file_location(fullname, path)
        module = importlib.util.module_from_spec(spec)
        sys.modules[fullname] = module
        spec.loader.exec_module(module)
        return module

    def failing_graph_readback(self):
        before = {"graph_path": "/Game/Test.Test:EventGraph", "nodes": [{"node_id": "original", "pins": []}]}
        self.api.graph_info = Mock(side_effect=[before, RuntimeError("injected readback failure")])
        return before, copy.deepcopy(before)

    def assert_failure_evidence(self, result, expected, mutation):
        self.assertFalse(result["success"])
        self.assertEqual(result["stage"], "readback")
        self.assertEqual(result["error"], "injected readback failure")
        self.assertEqual(result["data"]["before"], expected)
        self.assertNotIn("after", result["data"])
        self.assertEqual(result["changes"][0]["kind"], mutation)
        self.assertTrue(result["changes"][0]["attempted"])
        self.assertEqual(result["retry"]["duplicate_risk"], "partial_or_attempted_changes")
        recorded = self.state.get_operation(result["operation_id"])
        self.assertEqual(recorded["state"], "complete")
        self.assertEqual(recorded["result"]["data"]["before"], expected)
        self.assertNotIn("journal_observation", recorded)

    def test_branch_add_preserves_before_when_later_graph_readback_fails(self):
        before, expected = self.failing_graph_readback()
        def add(editor, x, y):
            before["nodes"].append({"node_id": "new", "pins": []})
            return "new"
        self.api.add_branch = Mock(side_effect=add)
        self.api.node_info = Mock(return_value={"node_id": "new"})
        result = self.graph.add_branch_node("/Game/Test", "EventGraph", 100, 200, "add-failure")
        self.assert_failure_evidence(result, expected, "add_branch")
        self.api.add_branch.assert_called_once()

    def test_branch_at_node_limit_rejects_before_api_or_checkpoint(self):
        before = {"graph_path": "/Game/Test.Test:EventGraph", "nodes": [{"node_id": str(i), "pins": []} for i in range(2048)]}
        self.api.graph_info = Mock(return_value=before)
        self.api.add_branch = Mock()
        callback = Mock()
        result = self.state.new_result("limit-core")
        with self.assertRaises(self.state.OperationError) as caught:
            self.graph.add_branch_node_action(result, "/Game/Test", "EventGraph", 0, 0, callback)
        self.assertEqual(caught.exception.code, "graph_node_limit_reached")
        self.assertEqual(result["changes"], [])
        self.api.add_branch.assert_not_called()
        callback.assert_not_called()
        before["nodes"] = before["nodes"][:1]
        before["total_node_count"] = 2048  # Other nodes are comments, still counted by native.
        with self.assertRaises(self.state.OperationError) as caught:
            self.graph.add_branch_node_action(result, "/Game/Test", "EventGraph", 0, 0, callback)
        self.assertEqual(caught.exception.code, "graph_node_limit_reached")
        self.api.add_branch.assert_not_called()

    def test_move_preserves_original_coordinates_when_readback_fails(self):
        before = {"node_id": "original", "x": 10, "y": 20, "pins": []}
        expected = copy.deepcopy(before)
        self.api.node_info = Mock(side_effect=[before, RuntimeError("injected readback failure")])
        def move(node, x, y):
            before.update(x=x, y=y)
        self.api.move_node = Mock(side_effect=move)
        result = self.graph.set_node_position("/Game/Test", "original", 100, 200, "EventGraph", 10, 20, "move-failure")
        self.assert_failure_evidence(result, expected, "move_node")
        self.api.move_node.assert_called_once()

    def test_delete_preserves_removed_node_when_readback_fails(self):
        before, expected = self.failing_graph_readback()
        self.api.node_info = Mock(return_value=before["nodes"][0])
        self.api.remove_node = Mock(side_effect=lambda editor, node: before["nodes"].clear())
        result = self.graph.delete_node("/Game/Test", "original", "EventGraph", False, "delete-failure")
        self.assert_failure_evidence(result, expected, "delete_node")
        self.api.remove_node.assert_called_once()

    def test_connect_preserves_original_links_when_readback_fails(self):
        before = {"graph_path": "/Game/Test.Test:EventGraph", "nodes": [
            {"node_id": "original", "pins": [{"name": "then", "direction": "output", "category": "exec", "links": []}]},
            {"node_id": "target", "pins": [{"name": "execute", "direction": "input", "category": "exec", "links": []}]}]}
        expected = copy.deepcopy(before)
        self.api.graph_info = Mock(side_effect=[before, RuntimeError("injected readback failure")])
        source = types.SimpleNamespace(list_connected_pins=lambda: [])
        target = types.SimpleNamespace(list_connected_pins=lambda: [])
        self.api.find_pin = Mock(side_effect=[source, target])
        self.api.pin_category = Mock(return_value="exec")
        def connect(output, input):
            before["nodes"][0]["pins"].append({"name": "then", "links": [{"node_id": "target"}]})
            return True
        self.api.connect = Mock(side_effect=connect)
        result = self.graph.connect_graph_pins("/Game/Test", "original", "then", "target", "execute", "EventGraph", "connect-failure")
        self.assert_failure_evidence(result, expected, "connect_exec_pins")
        self.api.connect.assert_called_once_with(source, target)

    def test_interrupted_journal_distinguishes_initial_checkpoint_from_actual_failure(self):
        def crash(result):
            result["stage"] = "readback"
            result["changes"].append({"kind": "already_attempted_mutation", "attempted": True})
            raise KeyboardInterrupt("simulated process interruption")
        with self.assertRaises(KeyboardInterrupt):
            self.state.mutate("example", {}, "interrupted-observation", crash)
        path = self.state._journal_path("interrupted-observation")
        bytes_before = path.read_bytes()
        observed = self.state.get_operation("interrupted-observation")
        self.assertEqual(observed["result"]["stage"], "preflight")
        self.assertEqual(observed["result"]["changes"], [])
        meaning = observed["journal_observation"]
        self.assertEqual(meaning["recorded_result_scope"], "last_initial_checkpoint")
        self.assertFalse(meaning["recorded_stage_is_actual_failure_stage"])
        for field in ("actual_failure_stage", "actual_changes", "actual_completion"):
            self.assertEqual(meaning[field], "unknown")
        self.assertFalse(meaning["automatic_retry"])
        self.assertEqual(path.read_bytes(), bytes_before)


if __name__ == "__main__":
    unittest.main()
