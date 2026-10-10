"""External Python tests for retry protection; these do not validate UE behavior.

Run: python -B -m unittest discover -s <plugin>/Content/Python/tests -v
"""

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch


class JournalTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        fake_unreal = types.SimpleNamespace(
            Paths=types.SimpleNamespace(project_saved_dir=lambda: self.temp.name),
            SystemLibrary=types.SimpleNamespace(get_engine_version=lambda: "5.8.3-test-stub"))
        self.module_patch = patch.dict(sys.modules, {"unreal": fake_unreal})
        self.module_patch.start()
        self.addCleanup(self.module_patch.stop)
        path = Path(__file__).parents[1] / "cubeless_mcp_ue58/operations/state.py"
        spec = importlib.util.spec_from_file_location("cubeless_journal_test_subject", path)
        self.state = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.state)

    def test_identical_request_replays_without_repeating_mutation(self):
        calls = []
        def action(result):
            calls.append(1)
            result["success"] = True
            result["stage"] = "complete"
        self.state.mutate("create", {"path": "/Game/Test"}, "same-id", action)
        replay = self.state.mutate("create", {"path": "/Game/Test"}, "same-id", action)
        self.assertEqual(calls, [1])
        self.assertTrue(replay["retry"]["replayed"])
        conflict = self.state.mutate("create", {"path": "/Game/Other"}, "same-id", action)
        self.assertEqual(conflict["error_code"], "operation_id_conflict")
        self.assertEqual(calls, [1])

    def test_exclusive_create_race_does_not_overwrite_other_editor_journal(self):
        original_open = Path.open
        foreign = '{"state":"started","owner":"another editor"}'
        def racing_open(path, mode="r", *args, **kwargs):
            if mode == "x" and path.name == "race-id.json":
                with original_open(path, "w", encoding="utf-8") as stream:
                    stream.write(foreign)
                raise FileExistsError("another process won exclusive create")
            return original_open(path, mode, *args, **kwargs)
        calls = []
        with patch.object(Path, "open", racing_open):
            result = self.state.mutate("create", {}, "race-id", lambda record: calls.append(1))
        self.assertFalse(result["success"])
        self.assertEqual(calls, [])
        self.assertEqual(self.state._journal_path("race-id").read_text(encoding="utf-8"), foreign)

    def test_interrupted_mutation_is_not_rerun(self):
        def crash(result):
            result["changes"].append({"attempted": True})
            raise KeyboardInterrupt("editor interrupted")
        with self.assertRaises(KeyboardInterrupt):
            self.state.mutate("create", {}, "interrupted-id", crash)
        calls = []
        result = self.state.mutate("create", {}, "interrupted-id", lambda record: calls.append(1))
        self.assertEqual(result["error_code"], "operation_incomplete")
        self.assertEqual(calls, [])

    def test_finalize_failure_keeps_started_record_and_reports_uncertain_outcome(self):
        calls = []
        def action(result):
            calls.append(1)
            result["success"] = True
        with patch.object(self.state.os, "replace", side_effect=OSError("disk failure")):
            result = self.state.mutate("create", {}, "disk-failure", action)
        self.assertEqual(calls, [1])
        self.assertEqual(result["error_code"], "journal_finalize_failed")
        self.assertFalse(result["success"])
        self.assertEqual(self.state.get_operation("disk-failure")["state"], "started")

    def test_path_traversal_is_rejected_before_mutation(self):
        calls = []
        result = self.state.mutate("create", {}, "../../escape", lambda record: calls.append(1))
        self.assertEqual(result["error_code"], "invalid_operation_id")
        self.assertEqual(calls, [])

    def test_checkpoint_survives_interrupt_without_claiming_actual_changes(self):
        def action(result):
            result["stage"] = "before_material_mutation"
            result["data"]["before"] = {"nodes": ["observed-before"]}
            self.assertEqual(self.state.checkpoint(result), 1)
            result["data"]["before"]["nodes"].append("not-persisted")
            result["stage"] = "after_possible_mutation"
            raise KeyboardInterrupt("synthetic process interruption")
        with self.assertRaises(KeyboardInterrupt):
            self.state.mutate("material", {}, "checkpoint-interrupt", action)
        record = self.state.get_operation("checkpoint-interrupt")
        self.assertEqual(record["result"]["data"]["before"]["nodes"], ["observed-before"])
        self.assertEqual(record["journal_observation"]["recorded_result_scope"], "last_durable_checkpoint")
        self.assertEqual(record["journal_observation"]["checkpoint_stage"], "before_material_mutation")
        self.assertEqual(record["journal_observation"]["actual_changes"], "unknown")
        self.assertFalse(record["journal_observation"]["recorded_stage_is_actual_failure_stage"])
        calls = []
        blocked = self.state.mutate("material", {}, "checkpoint-interrupt", lambda result: calls.append(1))
        self.assertEqual(blocked["error_code"], "operation_incomplete")
        self.assertEqual(calls, [])

    def test_checkpoint_failure_stops_before_next_mutation(self):
        calls = []
        def action(result):
            with patch.object(self.state, "_write_record", side_effect=OSError("checkpoint disk failure")):
                self.state.checkpoint(result)
            calls.append("mutation must not execute")
        result = self.state.mutate("material", {}, "checkpoint-failure", action)
        self.assertEqual(result["error_code"], "journal_checkpoint_failed")
        self.assertEqual(calls, [])
        self.assertEqual(result["changes"], [])

    def test_checkpoint_outside_active_operation_is_rejected(self):
        with self.assertRaises(self.state.OperationError) as caught:
            self.state.checkpoint(self.state.new_result("not-active"))
        self.assertEqual(caught.exception.code, "journal_checkpoint_not_owned")
        self.assertFalse(self.state._journal_path("not-active").exists())

    def test_multiple_checkpoints_preserve_request_identity_and_increment(self):
        observed = []
        def action(result):
            initial = self.state.get_operation("two-checkpoints")
            for index in (1, 2):
                result["stage"] = "checkpoint-" + str(index)
                observed.append(self.state.checkpoint(result))
                current = self.state.get_operation("two-checkpoints")
                self.assertEqual(current["fingerprint"], initial["fingerprint"])
                self.assertEqual(current["arguments"], {"sample": 42})
            result["success"] = True
            result["stage"] = "complete"
        result = self.state.mutate("material", {"sample": 42}, "two-checkpoints", action)
        self.assertEqual(observed, [1, 2])
        self.assertTrue(result["success"])
        self.assertEqual(self.state.get_operation("two-checkpoints")["state"], "complete")

    def test_initial_journal_scope_is_backward_compatible(self):
        def action(result):
            raise KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            self.state.mutate("old", {}, "initial-only", action)
        observation = self.state.get_operation("initial-only")["journal_observation"]
        self.assertEqual(observation["recorded_result_scope"], "last_initial_checkpoint")
        self.assertEqual(observation["checkpoint_sequence"], 0)

    def test_initial_encoder_failure_leaves_no_journal_and_never_dispatches(self):
        calls = []
        # Fingerprint encoding remains valid; only initial record encoding fails.
        original = self.state.json.dumps
        def encoding(value, *args, **kwargs):
            if isinstance(value, dict) and value.get("schema_version") == 1:
                raise TypeError("initial record cannot be encoded")
            return original(value, *args, **kwargs)
        with patch.object(self.state.json, "dumps", side_effect=encoding):
            result = self.state.mutate("create", {}, "encode-initial-failure", lambda record: calls.append(1))
        self.assertFalse(result["success"])
        self.assertEqual(calls, [])
        self.assertFalse(self.state._journal_path("encode-initial-failure").exists())
        self.assertIsNone(self.state._active_operation)

    def test_finalize_encoder_failure_preserves_started_and_prevents_replay(self):
        calls = []
        def action(result):
            calls.append(1)
            result["data"]["not_json_serializable"] = object()
            result["success"] = True
        result = self.state.mutate("create", {}, "encode-finalize-failure", action)
        self.assertEqual(result["error_code"], "journal_finalize_failed")
        self.assertEqual(calls, [1])
        path = self.state._journal_path("encode-finalize-failure")
        self.assertEqual(self.state.get_operation("encode-finalize-failure")["state"], "started")
        self.assertEqual(list(path.parent.glob(path.name + ".*.tmp")), [])
        replay = self.state.mutate("create", {}, "encode-finalize-failure", lambda record: calls.append(2))
        self.assertEqual(replay["error_code"], "operation_incomplete")
        self.assertEqual(calls, [1])

    def test_checkpoint_encoder_failure_keeps_previous_checkpoint_without_temp_file(self):
        observed = []
        def action(result):
            result["stage"] = "good-checkpoint"
            self.state.checkpoint(result)
            before = self.state._journal_path(result["operation_id"]).read_bytes()
            result["data"]["not_json_serializable"] = object()
            try:
                self.state.checkpoint(result)
            except self.state.OperationError as error:
                observed.append(error.code)
                path = self.state._journal_path(result["operation_id"])
                self.assertEqual(path.read_bytes(), before)
                self.assertEqual(list(path.parent.glob(path.name + ".*.tmp")), [])
                raise
            self.fail("Checkpoint encoding failure should abort before mutation")
        result = self.state.mutate("create", {}, "encode-checkpoint-failure", action)
        self.assertEqual(observed, ["journal_checkpoint_failed"])
        self.assertEqual(result["error_code"], "journal_finalize_failed")
        record = self.state.get_operation("encode-checkpoint-failure")
        self.assertEqual(record["result"]["stage"], "good-checkpoint")
        self.assertEqual(record["journal_observation"]["checkpoint_sequence"], 1)


if __name__ == "__main__":
    unittest.main()
