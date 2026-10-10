"""Focused function adapter/journal cases; fake native responses, no Unreal execution."""

import importlib
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch


PACKAGE = Path(__file__).parents[1] / "cubeless_mcp_ue58"


class FunctionContractTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.calls, self.checkpoints = [], []
        self.override = None
        library = types.SimpleNamespace(**{name: self.native(name) for name in (
            "resolve_blueprint_graph", "add_blueprint_function_parameter", "add_blueprint_call_function_node")})
        modules = {"unreal": types.SimpleNamespace(CubelessFunctionLibrary=library,
            Paths=types.SimpleNamespace(project_saved_dir=lambda: temporary.name),
            SystemLibrary=types.SimpleNamespace(get_engine_version=lambda: "5.8.3-offline-test"))}
        for suffix in ("", ".operations", ".compatibility", ".compatibility.ue58"):
            name = "cubeless_mcp_ue58" + suffix
            module = types.ModuleType(name)
            module.__path__ = [str(PACKAGE.joinpath(*suffix.strip(".").split("."))) if suffix else str(PACKAGE)]
            modules[name] = module
        self.module_patch = patch.dict(sys.modules, modules)
        self.module_patch.start()
        self.addCleanup(self.module_patch.stop)
        for name in list(sys.modules):
            if name.startswith("cubeless_mcp_ue58.") and name not in modules:
                del sys.modules[name]
        self.ops = importlib.import_module("cubeless_mcp_ue58.operations.functions")
        self.state = importlib.import_module("cubeless_mcp_ue58.operations.state")

    @staticmethod
    def envelope(**updates):
        value = {"success": True, "stage": "complete", "data": {"graph": {"graph_name": "Timer"}},
            "mutation_attempted": True, "change_state_known": True, "changed": True, "postcondition_passed": True,
            "compile_requested": False, "compile_attempted": False, "compiled": None,
            "save_requested": False, "save_attempted": False, "saved": False}
        value.update(updates)
        return value

    def native(self, name):
        def call(*args):
            self.calls.append((name, args))
            self.checkpoints.append(self.state.get_operation(self.state._active_operation))
            if isinstance(self.override, BaseException):
                raise self.override
            value = self.override if self.override is not None else self.envelope()
            return value if isinstance(value, str) else json.dumps(value)
        return call

    def test_selectors_and_unspecified_default_normalize_and_replay(self):
        guid = "11111111-2222-3333-4444-555555555555"
        result = self.ops.add_blueprint_function_parameter("BP_Test", "Delta", "Float", default_value_json="null",
            graph_name="Timer", graph_id=guid, operation_id="parameter")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls[0][1], ("/Game/Blueprints/BP_Test", "Delta", "Float", "input", "", "", False,
            "Timer", guid.replace("-", ""), "function", False))
        replay = self.ops.add_blueprint_function_parameter("BP_Test", "Delta", "Float", graph_name="Timer",
            graph_id=guid.replace("-", ""), operation_id="parameter")
        self.assertTrue(replay["retry"]["replayed"])
        self.assertEqual(len(self.calls), 1)
        self.assertEqual(self.checkpoints[0]["result"]["data"]["pending_mutation"]["dispatch_state_at_checkpoint"], "not_yet_dispatched")
        call = self.ops.add_blueprint_call_function_node("BP_Test", "/Script/Engine.KismetMathLibrary", "Abs",
            param_defaults_json="null", node_position_json="null", operation_id="call")
        self.assertTrue(call["success"])
        replay = self.ops.add_blueprint_call_function_node("BP_Test", "/Script/Engine.KismetMathLibrary", "Abs", operation_id="call")
        self.assertTrue(replay["retry"]["replayed"])
        self.assertEqual(self.calls[-1][1][3:5], ("{}", "[0,0]"))

    def test_malformed_inputs_and_forbidden_optins_never_dispatch(self):
        cases = ({"param_defaults_json": '{"X":1,"X":2}'}, {"param_defaults_json": '{"X":1,"x":2}'},
            {"node_position_json": "[true,0]"}, {"node_position_json": "[0,1000001]"},
            {"graph_id": "0" * 32}, {"allow_editor_only": True}, {"allow_latent": True},
            {"param_defaults_json": '{"X":NaN}'})
        for i, arguments in enumerate(cases):
            result = self.ops.add_blueprint_call_function_node("BP_Test", "/Script/Engine.KismetMathLibrary", "Abs",
                operation_id=f"invalid-{i}", **arguments)
            self.assertFalse(result["success"], arguments)
            self.assertEqual(result["changes"], [])
            self.assertEqual(self.state.get_operation(f"invalid-{i}")["state"], "unknown")
        self.assertEqual(self.calls, [])

    def test_lost_reply_is_unknown_and_not_reexecuted(self):
        self.override = RuntimeError("native reply lost after structural edit")
        result = self.ops.resolve_blueprint_graph("BP_Test", "Timer", graph_type="function", create_if_missing=True, operation_id="lost")
        self.assertFalse(result["success"])
        self.assertTrue(result["data"]["mutation_state_unknown"])
        self.assertIsNone(result["changes"][0]["changed"])
        self.assertFalse(result["retry"]["automatic_retry"])
        replay = self.ops.resolve_blueprint_graph("BP_Test", "Timer", graph_type="function", create_if_missing=True, operation_id="lost")
        self.assertTrue(replay["retry"]["replayed"])
        self.assertEqual(len(self.calls), 1)

    def test_resolve_noop_and_known_partial_native_failure(self):
        self.override = self.envelope(mutation_attempted=False, changed=False, data={"resolved": True, "created": False})
        result = self.ops.resolve_blueprint_graph("BP_Test")
        self.assertTrue(result["success"])
        self.assertEqual(result["changes"], [])
        self.override = self.envelope(success=False, stage="verification", error_code="function_pin_postcondition_failed",
            error="Pin differs", postcondition_passed=False, data={"graph": {"graph_name": "Timer"}, "implicit_skeleton_compile_possible": True})
        result = self.ops.add_blueprint_function_parameter("BP_Test", "Delta", "Float", graph_name="Timer")
        self.assertFalse(result["success"])
        self.assertEqual(result["stage"], "verification")
        self.assertTrue(result["changes"][0]["changed"])
        self.assertTrue(result["data"]["implicit_skeleton_compile_possible"])
        self.assertFalse(result["compile"]["attempted"])

    def test_checkpoint_failure_prevents_native_and_unrequested_save_is_retained(self):
        with patch.object(self.state, "checkpoint", side_effect=self.state.OperationError("journal_checkpoint_failed", "disk")):
            result = self.ops.resolve_blueprint_graph("BP_Test", "Timer", graph_type="function", create_if_missing=True)
        self.assertFalse(result["success"])
        self.assertEqual(self.calls, [])
        self.override = self.envelope(save_attempted=True, saved=True)
        result = self.ops.add_blueprint_call_function_node("BP_Test", "/Script/Engine.KismetMathLibrary", "Abs")
        self.assertFalse(result["success"])
        self.assertTrue(result["save"]["saved"])
        self.assertTrue(result["data"]["mutation_state_unknown"])
        self.assertEqual(result["error_code"], "unexpected_function_compile_save")


if __name__ == "__main__":
    unittest.main()
