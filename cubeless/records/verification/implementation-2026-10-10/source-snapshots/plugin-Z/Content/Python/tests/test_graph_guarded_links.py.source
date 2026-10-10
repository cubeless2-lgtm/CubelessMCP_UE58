"""Focused guarded-link and failed-dispatch observations; no Unreal execution."""

import json
import sys
import types
import unittest
from unittest.mock import patch

import test_graph_identity_links as fixture
import test_batch_contract as batch_fixture


class GuardedLinkTests(unittest.TestCase):
    def setUp(self):
        self.f = fixture.GraphIdentityTests()
        self.f.setUp()
        self.addCleanup(self.f.doCleanups)
        self.calls, self.checkpoints, self.mode = [], [], "normal"
        for pin in (self.f.output, self.f.target, self.f.other): pin.category = "exec"
        self.f.link(self.f.output, self.f.other)
        unreal = sys.modules["unreal"]
        unreal.CubelessGraphLibrary.read_connection_plan = self.plan
        unreal.CubelessGraphLibrary.replace_exec_link = self.native
        unreal.CubelessFunctionLibrary = types.SimpleNamespace(resolve_blueprint_graph=self.resolve)

    def resolve(self, *args):
        return json.dumps(dict(success=True, mutation_attempted=False, change_state_known=True,
            changed=False, postcondition_passed=True, compile_requested=False, compile_attempted=False,
            compiled=None, save_requested=False, save_attempted=False, saved=False,
            data={"graph": {"graph_name": "EventGraph", "object_path": fixture.GRAPH}}))

    def plan(self, *args):
        linked = self.f.target in self.f.output.links and self.f.output in self.f.target.links
        return self.f.native_result(direct_connection_allowed=not self.f.output.links,
            already_linked=linked, type_exactly_equal=True, exact_standard_k2_schema=True,
            schema_response_name="break_others_a" if self.f.output.links else "make", schema_message="test")

    def native(self, *args):
        self.calls.append(args)
        self.checkpoints.append(self.f.state.get_operation(self.f.state._active_operation))
        response = dict(success=True, stage="complete", mutation_attempted=True,
            change_state_known=True, changed=True, engine_return=True, postcondition_passed=True,
            compiled=False, saved=False, data={"replacement_guard_verified": True})
        if self.f.output.links != [self.f.other] or self.f.other.links != [self.f.output] or self.f.target.links:
            response.update(success=False, stage="preflight", mutation_attempted=False, changed=False,
                engine_return=None, postcondition_passed=False, error_code="connection_replacement_guard_mismatch",
                error="native guard rejected")
            response["data"]["replacement_guard_verified"] = False
            return json.dumps(response)
        self.f.output.links.remove(self.f.other)
        self.f.other.links.remove(self.f.output)
        if self.mode != "partial-false": self.f.link(self.f.output, self.f.target)
        if self.mode == "lost": return "{"
        if self.mode == "partial-false":
            response.update(success=False, stage="readback", engine_return=False, postcondition_passed=False,
                error_code="connection_replacement_api_failed", error="native returned false after removing old link")
        return json.dumps(response)

    def arguments(self):
        return dict(blueprint_name="/Game/Test", source_node_id=self.f.nodes[0].guid,
            source_pin="Mass", target_node_id=self.f.nodes[1].guid, target_pin="Mass", graph_name="EventGraph",
            allow_pin_link_replacement=True, expected_existing_source_link_count=1,
            expected_existing_source_link_target_node_id=self.f.nodes[2].guid,
            expected_existing_source_link_target_pin="Mass", expected_schema_response="break_others_a")

    def replace(self, identity="replace", **overrides):
        return self.f.ops.connect_blueprint_graph_pins(**dict(self.arguments(), **overrides), operation_id=identity)

    def assert_batch_observation(self, result, unknown):
        batch = batch_fixture.BatchContractTests()
        batch.setUp()
        self.addCleanup(batch.doCleanups)
        row = {"response": result, "mutation_attempted": True}
        batch.batch._observation(row)
        self.assertEqual(row["mutation_state_unknown"], unknown)
        self.assertTrue(row["mutation_performed"])

    def test_replacement_exact_delta_replay_and_new_id_stale_guard(self):
        result = self.replace()
        self.assertTrue(result["success"], result)
        self.assertTrue(result["data"]["replacement_guard_verified"])
        self.assertTrue(result["data"]["expected_existing_link_removed"])
        self.assertEqual(result["data"]["replaced_source_link_count"], 1)
        self.assertEqual(len(result["data"]["edge_delta"]["removed"]), 2)
        self.assertEqual(len(result["data"]["edge_delta"]["added"]), 2)
        self.assertEqual(result["data"]["delta"]["updated_count"], 1)
        self.assertFalse(self.checkpoints[0]["result"]["changes"][0]["attempted"])
        self.assertEqual(self.calls[0][-2:], (1, "break_others_a"))
        self.assertTrue(self.replace()["retry"]["replayed"])
        stale = self.replace("new-id")
        self.assertEqual(stale["error_code"], "connection_replacement_guard_mismatch")
        self.assertEqual(stale["changes"], [])
        self.assertEqual(len(self.calls), 1)

    def test_missing_group_strict_count_occupied_target_and_compile_reject(self):
        cases = [(dict(expected_schema_response=None), "connection_replacement_guard_required"),
            (dict(allow_pin_link_replacement=False), "connection_replacement_guard_required"),
            (dict(expected_existing_source_link_count=True), "connection_replacement_guard_invalid"),
            (dict(compile=True), "connection_compile_save_unsupported")]
        for index, (overrides, code) in enumerate(cases):
            with self.subTest(code=code):
                result = self.replace("invalid-" + str(index), **overrides)
                self.assertEqual(result["error_code"], code)
                self.assertEqual(result["changes"], [])
                if overrides.get("compile"):
                    self.assertTrue(result["compile"]["requested"])
                    self.assertFalse(result["compile"]["attempted"])
        other_output = fixture.Pin(self.f.nodes[2], "OtherOutput", "output", "exec")
        self.f.link(other_output, self.f.target)
        result = self.replace("occupied-target")
        self.assertEqual(result["error_code"], "connection_replacement_guard_mismatch")
        self.assertEqual(self.calls, [])

    def test_native_rechecks_guard_after_checkpoint(self):
        original = self.f.state.checkpoint
        def changed_after_checkpoint(result):
            original(result)
            self.f.output.links.clear()
            self.f.other.links.clear()
        with patch.object(self.f.state, "checkpoint", side_effect=changed_after_checkpoint):
            result = self.replace()
        self.assertEqual(result["error_code"], "connection_replacement_guard_mismatch")
        self.assertFalse(result["changes"][0]["attempted"])
        self.assertTrue(result["changes"][0]["changed"])
        self.assertTrue(result["data"]["observed_graph_change_state_known"])
        self.assertFalse(result["data"]["mutation_state_unknown"])
        self.assertEqual(self.f.target.links, [])

    def test_false_native_return_retains_known_partial_graph_and_no_retry(self):
        self.mode = "partial-false"
        result = self.replace()
        self.assertEqual(result["error_code"], "connection_replacement_api_failed")
        self.assertTrue(result["data"]["observed_graph_changed"])
        self.assertFalse(result["data"]["mutation_state_unknown"])
        self.assertEqual(len(result["data"]["edge_delta"]["removed"]), 2)
        self.assertEqual(result["data"]["edge_delta"]["added"], [])
        self.assertTrue(self.replace()["retry"]["replayed"])
        self.assertEqual(len(self.calls), 1)
        self.assert_batch_observation(result, False)

    def test_lost_response_retains_after_but_dispatch_uncertainty(self):
        self.mode = "lost"
        result = self.replace()
        self.assertEqual(result["error_code"], "invalid_native_connection_result")
        self.assertTrue(result["data"]["observed_graph_change_state_known"])
        self.assertTrue(result["data"]["mutation_state_unknown"])
        self.assertTrue(result["data"]["topology_preserved"])
        self.assertTrue(self.replace()["retry"]["replayed"])
        self.assertEqual(len(self.calls), 1)
        self.assert_batch_observation(result, True)

    def test_old_direct_api_false_and_exception_keep_after_observation(self):
        self.f.output.links.clear(); self.f.other.links.clear()
        for index, raises in enumerate((False, True)):
            with self.subTest(raises=raises):
                self.f.output.links.clear(); self.f.target.links.clear()
                def dispatch(output, target):
                    self.f.link(output, target)
                    if raises: raise RuntimeError("lost direct response")
                    return False
                with patch.object(self.f.api, "connect", side_effect=dispatch):
                    result = self.f.connect("direct-failure-" + str(index))
                self.assertFalse(result["success"])
                self.assertIn("after", result["data"])
                self.assertTrue(result["data"]["observed_graph_changed"])
                self.assertEqual(result["data"]["mutation_state_unknown"], raises)

    def test_full_native_pin_type_change_fails_even_with_equal_display_category(self):
        self.f.output.links.clear(); self.f.other.links.clear()
        self.f.after_connect = lambda: setattr(self.f.other, "type_text", "unexpected native type")
        result = self.f.connect("pin-type-changed")
        self.assertEqual(result["error_code"], "postcondition_failed")
        self.assertFalse(result["data"]["nonlink_state_preserved"])
        self.assertFalse(result["data"]["mutation_state_unknown"])

    def test_prior_plain_connection_journal_replays_with_unchanged_identity_and_arguments(self):
        arguments = {"blueprint_name": "/Game/Test", "source_node_id": self.f.nodes[0].guid,
            "source_pin_name": "Mass", "target_node_id": self.f.nodes[1].guid,
            "target_pin_name": "Mass", "graph_name": "EventGraph"}
        old = self.f.state.mutate("connect_graph_pins", arguments, "prior-version",
            lambda result: result.update(success=True, stage="complete", data={"prior_version_result": True}))
        replay = self.f.ops.connect_graph_pins(**arguments, operation_id="prior-version")
        self.assertTrue(old["success"])
        self.assertTrue(replay["retry"]["replayed"])
        self.assertTrue(replay["data"]["prior_version_result"])
        record = self.f.state.get_operation("prior-version")
        self.assertEqual(record["operation"], "connect_graph_pins")
        self.assertEqual(record["arguments"], arguments)
        self.assertEqual(self.f.connect_calls, 0)
        self.assertEqual(self.calls, [])


if __name__ == "__main__": unittest.main()
