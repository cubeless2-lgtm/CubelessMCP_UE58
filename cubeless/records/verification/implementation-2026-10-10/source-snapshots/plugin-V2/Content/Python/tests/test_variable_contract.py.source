"""Focused variable adapter/journal tests; fake engine, no UE execution."""

import importlib
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch


PACKAGE = Path(__file__).parents[1] / "cubeless_mcp_ue58"


class VariableContractTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.calls = []
        self.dispatched_checkpoints = []
        self.override = None
        methods = ("add_blueprint_variable", "set_blueprint_variable_default",
                   "inspect_blueprint_variables", "inspect_blueprint_variable_metadata")
        library = types.SimpleNamespace(**{name: self.native(name) for name in methods})
        modules = {"unreal": types.SimpleNamespace(CubelessVariableLibrary=library,
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
        self.ops = importlib.import_module("cubeless_mcp_ue58.operations.variables")
        self.state = importlib.import_module("cubeless_mcp_ue58.operations.state")

    @staticmethod
    def envelope(**overrides):
        result = {"success": True, "stage": "complete", "data": {}, "mutation_attempted": True,
            "changed": True, "change_state_known": True, "postcondition_passed": True,
            "compile_requested": False, "compile_attempted": False, "compiled": None,
            "save_requested": False, "save_attempted": False, "saved": False}
        result.update(overrides)
        return result

    def native(self, name):
        def call(*args):
            self.calls.append((name, args))
            if self.state._active_operation:
                self.dispatched_checkpoints.append(self.state.get_operation(self.state._active_operation))
            if isinstance(self.override, BaseException):
                raise self.override
            value = self.override
            if value is None:
                if name == "set_blueprint_variable_default":
                    compile, save, dry = args[-3:]
                    value = self.envelope(mutation_attempted=not dry, changed=not dry,
                        compile_requested=compile, compile_attempted=compile and not dry,
                        compiled=True if compile and not dry else None,
                        save_requested=save, save_attempted=save and not dry, saved=save and not dry,
                        data={"stored_default_after": "" if compile and not dry else args[2],
                              "verified_variable": {"default_value": 25.0}})
                elif name.startswith("inspect"):
                    value = self.envelope(mutation_attempted=False, changed=False,
                        data={"new_variables": [{"name": "Mass", "stored_default": "", "default_value": 25.0}],
                              "metadata_entries": [{"index": 0, "key": "ToolTip", "value": "Mass"}]})
                else:
                    value = self.envelope()
            return value if isinstance(value, str) else json.dumps(value)
        return call

    def test_add_default_normalization_hidden_fields_and_replay(self):
        guid = "0dfce8d0-59ba-4e4d-b7b6-96a91ff20135"
        result = self.ops.add_blueprint_variable("BP_Test", "Mass", "Float", default_value_json="10",
            metadata_json='{"ClampMin":0}', variable_guid=guid, is_editable=True, operation_id="add")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls[0], ("add_blueprint_variable", ("/Game/Blueprints/BP_Test", "Mass", "Float",
            False, "10", "", "", "", "", False, '{"ClampMin":0}', guid.replace("-", ""), True)))
        again = self.ops.add_blueprint_variable("BP_Test", "Mass", "Float", default_value_json=" 10 ",
            metadata_json='{"ClampMin":0}', variable_guid=guid.replace("-", ""), is_editable=True, operation_id="add")
        self.assertTrue(again["retry"]["replayed"])
        self.assertEqual(len(self.calls), 1)
        self.assertGreater(self.dispatched_checkpoints[0]["checkpoint"]["sequence"], 0)
        self.assertEqual(self.dispatched_checkpoints[0]["result"]["data"]["pending_mutation"]["dispatch_state_at_checkpoint"], "not_yet_dispatched")

    def test_add_none_and_json_null_share_unspecified_default(self):
        for value in (None, "null"):
            result = self.ops.add_blueprint_variable("BP_Test", "Mass", "Float", default_value_json=value, operation_id="null")
            self.assertTrue(result["success"], result)
        self.assertEqual(self.calls[0][1][4], "")
        self.assertEqual(len(self.calls), 1)

    def test_malformed_inputs_never_dispatch_or_create_journal(self):
        cases = ({"default_value_json": "NaN"}, {"metadata_json": '{"X":1,"X":2}'},
            {"metadata_json": '{"ToolTip":"a","Tooltip":"b"}'}, {"metadata_json": '{"X":[]}'},
            {"is_array": 1}, {"variable_guid": "not-a-guid"}, {"variable_guid": "0" * 32})
        for index, kwargs in enumerate(cases):
            with self.subTest(kwargs=kwargs):
                result = self.ops.add_blueprint_variable("BP_Test", "Mass", "Float", operation_id=f"bad-{index}", **kwargs)
                self.assertFalse(result["success"])
                self.assertEqual(result["changes"], [])
                self.assertEqual(self.state.get_operation(f"bad-{index}")["state"], "unknown")
        self.assertEqual(self.calls, [])

    def test_set_public_compile_save_defaults_and_consumed_stored_value(self):
        result = self.ops.set_blueprint_variable_default("BP_Test", "Mass", "25", operation_id="set")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls[0][1][-3:], (True, True, False))
        self.assertTrue(result["compile"]["compiled"])
        self.assertTrue(result["save"]["attempted"])
        self.assertTrue(result["save"]["saved"])
        self.assertEqual(result["data"]["stored_default_after"], "")

    def test_dry_run_preserves_requested_flags_without_attempts(self):
        result = self.ops.set_blueprint_variable_default("BP_Test", "Mass", "25", dry_run=True)
        self.assertTrue(result["success"], result)
        self.assertEqual(result["changes"], [])
        self.assertTrue(result["compile"]["requested"])
        self.assertTrue(result["save"]["requested"])
        self.assertFalse(result["compile"]["attempted"])
        self.assertFalse(result["save"]["attempted"])
        self.assertFalse(result["save"]["saved"])

    def test_lost_reply_retains_compile_save_unknown_and_never_reruns(self):
        self.override = RuntimeError("lost reply after mutation")
        result = self.ops.set_blueprint_variable_default("BP_Test", "Mass", "25", operation_id="lost")
        self.assertFalse(result["success"])
        self.assertTrue(result["data"]["mutation_state_unknown"])
        self.assertIsNone(result["compile"]["attempted"])
        self.assertIsNone(result["save"]["saved"])
        self.assertFalse(result["retry"]["automatic_retry"])
        again = self.ops.set_blueprint_variable_default("BP_Test", "Mass", "25", operation_id="lost")
        self.assertTrue(again["retry"]["replayed"])
        self.assertEqual(len(self.calls), 1)

    def test_known_partial_compile_or_save_failure_keeps_native_state(self):
        self.override = self.envelope(success=False, compile_requested=True, compile_attempted=True, compiled=True,
            save_requested=True, save_attempted=True, saved=False, error_code="save_failed", error="disk failure",
            data={"stored_default_after": "", "compile": {"status": "up_to_date", "compile_error_count": 0}})
        result = self.ops.set_blueprint_variable_default("BP_Test", "Mass", "25")
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "save_failed")
        self.assertTrue(result["changes"][0]["changed"])
        self.assertTrue(result["compile"]["compiled"])
        self.assertFalse(result["data"]["mutation_state_unknown"])
        self.assertFalse(result["save"]["saved"])

    def test_native_read_observations_and_noop(self):
        result = self.ops.inspect_blueprint_variables("BP_Test")
        self.assertTrue(result["success"], result)
        self.assertFalse(result["data"]["journaled"])
        self.assertEqual(result["changes"], [])
        self.assertEqual(result["data"]["new_variables"][0]["default_value"], 25.0)
        self.override = self.envelope(mutation_attempted=False, changed=False, data={"no_op": True})
        result = self.ops.set_blueprint_variable_default("BP_Test", "Mass", "25", compile=False, save=False)
        self.assertTrue(result["success"], result)
        self.assertEqual(result["changes"], [])

    def test_read_failure_or_unexpected_write_is_not_silently_empty(self):
        for override in (RuntimeError("load failed after PostLoad"), self.envelope()):
            self.override = override
            result = self.ops.inspect_blueprint_variable_metadata("BP_Test", "Mass")
            self.assertFalse(result["success"])
            self.assertTrue(result["changes"])
            self.assertTrue(result["data"]["mutation_state_unknown"])
            self.assertFalse(result["retry"]["automatic_retry"])


if __name__ == "__main__":
    unittest.main()
