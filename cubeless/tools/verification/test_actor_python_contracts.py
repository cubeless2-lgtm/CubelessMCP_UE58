"""Actor boundary/journal counterexamples with synthetic native calls, no UE."""

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

PROJECT = Path(__file__).resolve().parents[3]
PACKAGE = PROJECT / "Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58"
NATIVE = ("get_actors_in_level", "find_actors_by_name", "get_actor_properties", "spawn_actor", "delete_actor", "set_actor_transform", "spawn_blueprint_actor")


class ActorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cubeless-actor-unit-")
        self.addCleanup(self.temp.cleanup)
        self.calls = []
        self.overrides = {}
        self.level = "/Game/CubelessMCPTests/ActorMap"
        self.context = {"persistent_level_path": self.level, "editor_world_path": self.level + ".ActorMap"}
        self.actor = {"name": "ExactActor", "label": "Different Label", "class": "PointLight", "location": [1, 2, 3],
            "rotation": [0, 30, 0], "scale": [1, 1, 1], "actor_path": self.level + ".ActorMap:PersistentLevel.ExactActor", "level_path": self.level}
        library = types.SimpleNamespace(**{name: self.native(name) for name in NATIVE})
        unreal = types.SimpleNamespace(CubelessActorLibrary=library,
            Paths=types.SimpleNamespace(project_saved_dir=lambda: self.temp.name),
            SystemLibrary=types.SimpleNamespace(get_engine_version=lambda: "5.8.3-synthetic"))
        packages = {"unreal": unreal}
        for suffix in ("", ".compatibility", ".compatibility.ue58", ".operations"):
            name = "cubeless_mcp_ue58" + suffix
            module = types.ModuleType(name)
            module.__path__ = [str(PACKAGE.joinpath(*suffix.strip(".").split("."))) if suffix else str(PACKAGE)]
            packages[name] = module
        self.modules = patch.dict(sys.modules, packages)
        self.modules.start()
        self.addCleanup(self.modules.stop)
        for name in list(sys.modules):
            if name.startswith("cubeless_mcp_ue58.") and name not in packages:
                del sys.modules[name]
        self.api = importlib.import_module("cubeless_mcp_ue58.compatibility.ue58.actor")
        self.ops = importlib.import_module("cubeless_mcp_ue58.operations.actor")
        self.state = importlib.import_module("cubeless_mcp_ue58.operations.state")

    def result(self, name, **extra):
        mutated = name in NATIVE[3:]
        data = dict(self.context)
        if name in NATIVE[:2]:
            data.update(actors=[dict(self.actor)], actor_count=1)
        else:
            data.update(copy.deepcopy(self.actor))
        result = {"success": True, "stage": "complete", "data": data, "mutation_attempted": mutated,
            "changed": mutated, "change_state_known": True, "postcondition_passed": mutated, "dirty_before": False, "dirty_after": mutated,
            "before": {"exists": True, "dirty": False, "world_partition": False, "external_actors": False, "selected_actor_paths": [self.actor["actor_path"]]}, "after": {"exists": name != "delete_actor"},
            "verification": {"method": "synthetic_actor_readback"}}
        result.update(extra)
        return result

    def native(self, name):
        def call(*arguments):
            self.calls.append((name, arguments))
            result = self.overrides.get(name)
            if callable(result):
                result = result(*arguments)
            elif result is None:
                result = self.result(name)
            return result if isinstance(result, str) else json.dumps(result)
        return call

    def test_named_schemas_have_only_string_vector_adapters(self):
        tree = ast.parse((PACKAGE / "registration/actor_toolset.py").read_text(encoding="utf-8"))
        methods = {node.name: node for cls in tree.body if isinstance(cls, ast.ClassDef) for node in cls.body if isinstance(node, ast.FunctionDef)}
        self.assertEqual(set(methods), set(NATIVE) | {"create_actor"})
        for name, method in methods.items():
            args = [item.arg for item in method.args.args]
            defaults = dict(zip(args[len(args) - len(method.args.defaults):], map(ast.literal_eval, method.args.defaults)))
            self.assertIsNone(defaults["expected_level_path"])
            self.assertTrue(all("list" not in ast.unparse(item.annotation) for item in method.args.args))
            if name in {"spawn_actor", "create_actor"}:
                self.assertEqual([key for key in args if key not in defaults], ["name", "type"])
                self.assertEqual([defaults[key] for key in ("location_json", "rotation_json", "scale_json")], ["[0,0,0]", "[0,0,0]", "[1,1,1]"])
            if name == "spawn_blueprint_actor":
                self.assertEqual([key for key in args if key not in defaults], ["blueprint_name", "actor_name"])
                self.assertEqual([defaults[key] for key in ("location_json", "rotation_json", "scale_json")], ["[0,0,0]", "[0,0,0]", "[1,1,1]"])
            if name == "set_actor_transform":
                self.assertEqual([defaults[key] for key in ("location_json", "rotation_json", "scale_json")], ["null"] * 3)

    def test_six_bindings_use_exact_native_positional_contract(self):
        self.api.get_actors_in_level("L")
        self.api.find_actors_by_name("P", "L")
        self.api.get_actor_properties("A", "L")
        self.api.spawn_actor("A", "PointLight", "a", "b", "c", "L")
        self.api.delete_actor("A", "L")
        self.api.set_actor_transform("A", "a", "b", "c", "L")
        self.assertEqual(self.calls, [(NATIVE[0], ("L", 10000)), (NATIVE[1], ("P", "L", 10000)),
            (NATIVE[2], ("A", "L", 10000)), (NATIVE[3], ("A", "PointLight", "a", "b", "c", "L", 10000)),
            (NATIVE[4], ("A", "L", 10000)), (NATIVE[5], ("A", "a", "b", "c", "L", 10000))])

    def test_all_five_types_canonicalize_the_legacy_uppercase_bug(self):
        for kind in self.ops._TYPES.values():
            result = self.ops.spawn_actor("New", kind.upper(), operation_id=kind)
            self.assertTrue(result["success"], result)
            self.assertEqual(self.calls[-1][1][1], kind)

    def test_invalid_vectors_never_reach_native(self):
        for value in ("null", "[]", "[1,2]", "[1,2,3,4]", '["1",2,3]', "[true,2,3]", "[NaN,2,3]", "[1e999,2,3]", "[1e13,2,3]", "{}", "[0,0,0] trailing", None, [0, 0, 0]):
            with self.subTest(value=value):
                result = self.ops.spawn_actor("New", "PointLight", location_json=value)
                self.assertFalse(result["success"])
                self.assertEqual(result["changes"], [])
        self.assertEqual(self.calls, [])

    def test_unknown_class_and_oversize_names_do_not_dispatch(self):
        self.assertFalse(self.ops.spawn_actor("New", "Blueprint")["success"])
        self.assertFalse(self.ops.delete_actor("A" * 1024)["success"])
        self.assertFalse(self.ops.get_actor_properties("😀" * 512)["success"])
        self.assertEqual(self.calls, [])

    def test_partial_transform_preserves_null_and_exact_object_name(self):
        result = self.ops.set_actor_transform("ExactActor", location_json="[4, 5, 6]", operation_id="partial")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls[-1], ("set_actor_transform", ("ExactActor", "[4,5,6]", "null", "null", self.level, 10000)))
        self.assertEqual(result["data"]["before_snapshot"]["label"], "Different Label")
        self.assertFalse(result["compile"]["attempted"])
        self.assertFalse(result["save"]["attempted"])

    def test_spawn_checkpoint_is_durable_compact_and_pins_level(self):
        checkpoints = []
        def mutation(*args):
            checkpoints.append(self.state.get_operation("checkpoint"))
            return self.result("spawn_actor")
        self.overrides["spawn_actor"] = mutation
        result = self.ops.spawn_actor("New", "PointLight", operation_id="checkpoint")
        self.assertTrue(result["success"], result)
        record = checkpoints[0]
        self.assertEqual(record["state"], "started")
        self.assertEqual(record["result"]["stage"], "before_mutation_checkpoint")
        self.assertEqual(record["result"]["changes"], [])
        snapshot = record["result"]["data"]["before_snapshot"]
        self.assertNotIn("actors", snapshot)
        self.assertEqual(snapshot["requested_name_matches"], [])
        self.assertEqual(snapshot["loaded_actor_count"], 1)
        self.assertIs(snapshot["dirty"], False)
        self.assertEqual(snapshot["selected_actor_paths"], [self.actor["actor_path"]])
        self.assertEqual(self.calls[-1][1][-2], self.level)

    def test_checkpoint_failure_blocks_dispatch(self):
        with patch.object(self.state, "checkpoint", side_effect=OSError("disk full")):
            result = self.ops.delete_actor("ExactActor", operation_id="disk-full")
        self.assertFalse(result["success"])
        self.assertEqual(result["stage"], "before_mutation_checkpoint")
        self.assertEqual(result["changes"], [])
        self.assertEqual([item[0] for item in self.calls], ["get_actor_properties"])
        self.assertIn("before_snapshot", result["data"])

    def test_matching_completed_id_replays_without_even_a_snapshot(self):
        original = self.ops.spawn_actor("New", "PointLight", operation_id="replay")
        count = len(self.calls)
        replay = self.ops.spawn_actor("New", "PointLight", operation_id="replay")
        self.assertTrue(original["success"])
        self.assertTrue(replay["retry"]["replayed"])
        self.assertEqual(len(self.calls), count)

    def test_create_alias_uses_spawn_core_but_distinct_journal_identity(self):
        result = self.ops.create_actor("New", "PointLight", operation_id="alias")
        self.assertTrue(result["success"], result)
        self.assertEqual(self.calls[-1][0], "spawn_actor")
        self.assertEqual(result["data"]["operation"], "create_actor")
        self.assertEqual(result["changes"][0]["kind"], "create_actor")
        self.assertEqual(self.state.get_operation("alias")["operation"], "create_actor")
        count = len(self.calls)
        conflict = self.ops.spawn_actor("New", "PointLight", operation_id="alias")
        self.assertEqual(conflict["error_code"], "operation_id_conflict")
        self.assertEqual(len(self.calls), count)

    def test_blueprint_spawn_shares_guarded_core_and_durable_level_intent(self):
        observed = []
        def spawn(*args):
            observed.append(self.state.get_operation("blueprint-spawn"))
            return self.result("spawn_blueprint_actor")
        self.overrides["spawn_blueprint_actor"] = spawn
        result = self.ops.spawn_blueprint_actor("BP_Cube", "CubeInstance", operation_id="blueprint-spawn")
        self.assertTrue(result["success"], result)
        self.assertEqual([name for name, _ in self.calls], ["get_actors_in_level", "spawn_blueprint_actor"])
        self.assertEqual(self.calls[-1][1], ("/Game/Blueprints/BP_Cube.BP_Cube", "CubeInstance",
            "[0,0,0]", "[0,0,0]", "[1,1,1]", self.level, 10000))
        self.assertEqual(observed[0]["operation"], "spawn_blueprint_actor")
        self.assertEqual(observed[0]["result"]["stage"], "before_mutation_checkpoint")
        self.assertEqual(observed[0]["result"]["changes"], [])
        self.assertEqual(result["data"]["effective_expected_level_path"], self.level)
        self.assertEqual(result["changes"][0]["kind"], "spawn_blueprint_actor")
        self.assertFalse(result["compile"]["attempted"])
        self.assertFalse(result["save"]["attempted"])

    def test_blueprint_spawn_lost_response_keeps_intent_and_replay_never_respawns(self):
        self.overrides["spawn_blueprint_actor"] = "lost native JSON response"
        result = self.ops.spawn_blueprint_actor("BP_Cube", "CubeInstance", operation_id="blueprint-lost")
        self.assertEqual(result["error_code"], "invalid_native_actor_result")
        self.assertEqual(result["stage"], "native_mutation")
        self.assertEqual(result["changes"][0]["outcome"], "unknown_until_native_result")
        self.assertIn("before_snapshot", result["data"])
        self.assertFalse(result["retry"]["automatic_retry"])
        before = len(self.calls)
        replay = self.ops.spawn_blueprint_actor("BP_Cube", "CubeInstance", operation_id="blueprint-lost")
        self.assertTrue(replay["retry"]["replayed"])
        self.assertFalse(replay["success"])
        self.assertEqual(len(self.calls), before)

    def test_unknown_native_mutation_outcome_keeps_attempt_and_snapshot(self):
        self.overrides["delete_actor"] = "broken json"
        result = self.ops.delete_actor("ExactActor", operation_id="unknown")
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "invalid_native_actor_result")
        self.assertEqual(result["stage"], "native_mutation")
        self.assertEqual(result["changes"][0]["outcome"], "unknown_until_native_result")
        self.assertIn("before_snapshot", result["data"])
        self.assertFalse(result["retry"]["automatic_retry"])

    def test_native_preflight_rejection_clears_attempt_without_losing_snapshot(self):
        self.overrides["delete_actor"] = self.result("delete_actor", success=False, stage="preflight", mutation_attempted=False,
            changed=False, postcondition_passed=False, error_code="actor_class_unsupported", error="Blueprint actor is outside this slice")
        result = self.ops.delete_actor("ExactActor", operation_id="native-reject")
        self.assertEqual(result["error_code"], "actor_class_unsupported")
        self.assertEqual(result["changes"], [])
        self.assertIn("before_snapshot", result["data"])

    def test_blueprint_preflight_load_effects_survive_without_spawn_attempt(self):
        for known, changed in ((True, True), (False, None)):
            with self.subTest(change_state_known=known):
                native = self.result("spawn_blueprint_actor", success=False, stage="preflight",
                    mutation_attempted=False, change_state_known=known, changed=changed,
                    dirty_after=True if known else None, postcondition_passed=False,
                    error_code="blueprint_spawn_preflight_failed", error="Blueprint load changed or obscured the world")
                if not known:
                    native.pop("changed")
                self.overrides["spawn_blueprint_actor"] = native
                operation_id = "blueprint-load-effect-" + str(known)
                result = self.ops.spawn_blueprint_actor("BP_Cube", "CubeInstance", operation_id=operation_id)
                self.assertFalse(result["success"])
                self.assertEqual(result["stage"], "preflight")
                self.assertEqual(result["error_code"], "blueprint_spawn_preflight_failed")
                self.assertEqual(len(result["changes"]), 1)
                change = result["changes"][0]
                self.assertFalse(change["attempted"])
                self.assertIs(change["change_state_known"], known)
                self.assertIs(change["changed"], changed)
                self.assertEqual(change["outcome"], "observed_native_result" if known else "unknown_after_native_dispatch")
                self.assertIn("before_snapshot", result["data"])
                self.assertFalse(result["retry"]["automatic_retry"])
                self.assertEqual(self.state.get_operation(operation_id)["result"]["changes"], result["changes"])

    def test_known_partial_native_failure_preserves_changes(self):
        self.overrides["set_actor_transform"] = self.result("set_actor_transform", success=False, postcondition_passed=False,
            error_code="actor_postcondition_failed", error="readback differed", stage="postcondition")
        result = self.ops.set_actor_transform("ExactActor", location_json="[7,8,9]", operation_id="partial-failure")
        self.assertFalse(result["success"])
        self.assertTrue(result["changes"][0]["changed"])
        self.assertEqual(result["retry"]["duplicate_risk"], "partial_or_attempted_changes")

    def test_native_success_without_postcondition_is_not_success(self):
        self.overrides["spawn_actor"] = self.result("spawn_actor", postcondition_passed=False)
        result = self.ops.spawn_actor("New", "PointLight", operation_id="unverified")
        self.assertEqual(result["error_code"], "actor_postcondition_not_verified")
        self.assertTrue(result["changes"])

    def test_noop_transform_has_no_observed_mutation(self):
        self.overrides["set_actor_transform"] = self.result("set_actor_transform", mutation_attempted=False, changed=False, dirty_after=False)
        result = self.ops.set_actor_transform("ExactActor", operation_id="noop")
        self.assertTrue(result["success"], result)
        self.assertEqual(result["changes"], [])
        self.assertEqual(result["retry"]["duplicate_risk"], "none_observed")

    def test_unknown_basic_readback_is_not_misreported_as_unchanged(self):
        self.overrides["set_actor_transform"] = self.result("set_actor_transform", change_state_known=False, changed=False)
        result = self.ops.set_actor_transform("ExactActor", location_json="[7,8,9]", operation_id="unknown-state")
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "actor_change_state_unknown")
        self.assertIsNone(result["changes"][0]["changed"])
        self.assertEqual(result["changes"][0]["outcome"], "unknown_after_native_dispatch")

    def test_missing_or_changed_level_identity_blocks_mutation(self):
        for data in ({"name": "ExactActor"}, {**self.actor, "persistent_level_path": "/Engine/Transient"}, {**self.actor, **self.context}):
            self.overrides["get_actor_properties"] = self.result("get_actor_properties", data=data)
            self.calls.clear()
            result = self.ops.delete_actor("ExactActor", expected_level_path="/Game/AnotherMap")
            self.assertFalse(result["success"])
            self.assertEqual(result["changes"], [])
            self.assertEqual(len(self.calls), 1)

    def test_read_error_has_stage_and_no_changes(self):
        self.overrides["find_actors_by_name"] = "not json"
        result = self.ops.find_actors_by_name("")
        self.assertFalse(result["success"])
        self.assertEqual(result["stage"], "native_read")
        self.assertEqual(result["changes"], [])

    def test_read_helper_claiming_mutation_is_rejected(self):
        self.overrides["get_actor_properties"] = self.result("get_actor_properties", mutation_attempted=True)
        result = self.ops.get_actor_properties("ExactActor")
        self.assertEqual(result["error_code"], "unexpected_read_mutation")
        self.assertTrue(result["changes"])

    def test_busy_lease_prevents_snapshot_and_mutation(self):
        self.state._active_operation = "OtherOperation"
        result = self.ops.delete_actor("ExactActor", operation_id="busy")
        self.assertEqual(result["error_code"], "mutation_busy")
        self.assertEqual(self.calls, [])

    def test_process_interrupt_leaves_durable_started_state_and_blocks_reexecution(self):
        def interrupted(*args):
            raise KeyboardInterrupt("synthetic process stop after dispatch")
        self.overrides["spawn_actor"] = interrupted
        with self.assertRaises(KeyboardInterrupt):
            self.ops.spawn_actor("New", "PointLight", operation_id="crash")
        record = self.state.get_operation("crash")
        self.assertEqual(record["state"], "started")
        self.assertEqual(record["journal_observation"]["actual_changes"], "unknown")
        self.assertEqual(record["result"]["stage"], "before_mutation_checkpoint")
        count = len(self.calls)
        rerun = self.ops.spawn_actor("New", "PointLight", operation_id="crash")
        self.assertEqual(rerun["error_code"], "operation_incomplete")
        self.assertEqual(len(self.calls), count)

    def test_action_core_can_use_enclosing_journal_checkpoint_without_nested_mutate(self):
        checkpoints = []
        result = self.state.new_result("outer")
        with patch.object(self.ops, "mutate", side_effect=AssertionError("nested journal")):
            self.ops.spawn_actor_action(result, "New", "PointLight", "[0,0,0]", "[0,0,0]", "[1,1,1]",
                checkpoint_callback=lambda value: checkpoints.append(copy.deepcopy(value)))
        self.assertTrue(result["success"])
        self.assertEqual(len(checkpoints), 1)
        self.assertEqual(checkpoints[0]["stage"], "before_mutation_checkpoint")


if __name__ == "__main__":
    unittest.main(verbosity=2)
