"""Synthetic positive and tampered-evidence tests; never invokes an engine."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verification import actor_pipeline_report as v


def sample_fixture():
    run = "a" * 32; package = "/Game/CubelessMCPTests/Actors_" + run + "/Map_Actors"; obj = package + ".Map_Actors"
    actor = {"name": "WorldSettings", "class_path": "/Script/Engine.WorldSettings", "actor_path": obj + ":PersistentLevel.WorldSettings"}
    return {"schema": "cubeless_actor_map_fixture_v1", "success": True, "stage": "complete", "run_id": run,
        "project_file": str(v.PROJECT.resolve()), "map_package": package, "map_object": obj,
        "save": {"requested": True, "attempted": True, "saved": True}, "dirty_before": [], "dirty_after": [],
        "compile": {"requested": False}, "automatic_retry": False, "atomic_rollback": False, "packaging": "not_run",
        "protected_actor": actor, "baseline_actors": [actor], "asset_file": {"path": str(v.PROJECT.parent / "Content" / (package[6:] + ".umap")), "sha256": "b" * 64}}


def sample_report(fixture, phase="author", parent=None):
    raw = json.dumps(fixture).encode(); package = fixture["map_package"]
    def actor(name, kind): return {"name": name, "class": kind, "class_path": "/Script/Engine." + kind, "actor_path": fixture["map_object"] + ":PersistentLevel." + name,
        "level_path": package, "location": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1], "label": ""}
    model = v.actor_map(parent["saved_actors"]) if parent else {"WorldSettings": actor("WorldSettings", "WorldSettings")}
    report = {"schema": "cubeless_actor_pipeline_v1", "status": "pass", "phase": phase, "run_id": fixture["run_id"],
        "map_package": package, "project_file": str(v.PROJECT.resolve()), "fixture_sha256": v.sha(raw), "initial_map_sha256": fixture["asset_file"]["sha256"],
        "outcome_unknown": False, "automatic_retries": 0, "atomic_rollback": False, "packaging": "not_run", "full_legacy_actor_parity": False,
        "stages": [], "tool_calls": [], "save_calls": 1 if phase == "author" else 0,
        "asset_file": {"path": fixture["asset_file"]["path"], "sha256": "c" * 64}}
    if parent: report["author_sha256"] = v.sha(json.dumps(parent).encode())
    def envelope(data): return {"success": True, "data": data, "changes": [], "compile": {"requested": False}, "save": {"requested": False, "saved": False}, "validation": {"native_postcondition_passed": True}, "retry": {"automatic_retry": False, "replayed": False}}
    for spec in v.plan(fixture, phase):
        kind, e = spec["check"], spec["expected"]
        if kind in {"editor", "dirty_editor"}:
            value = {"success": True, "project_file": str(v.PROJECT.resolve()), "game_thread": True, "pie_or_sie_active": False,
                "engine_version": "5.8.3-synthetic", "instance_id": "cold" if parent else "author", "dirty_packages": [package] if kind == "dirty_editor" else []}
            if spec["name"] == "preflight": report["editor"] = copy.deepcopy(value)
        elif kind == "void": value = None
        elif kind == "map": value = package
        elif kind == "true": value = True
        elif kind == "false": value = False
        elif kind == "label": value = e["label"]
        elif kind == "label_set": model[e["name"]]["label"] = e["label"]; value = True
        elif kind == "reject": value = envelope({}); value.update(success=False, error_code=e["error"], error="synthetic expected failure")
        elif kind in {"list", "baseline", "find"}:
            rows = list(model.values()) if kind != "find" else [model[n] for n in e["names"]]
            value = envelope({"actors": copy.deepcopy(rows), "actor_count": len(rows), "persistent_level_path": package})
            if spec["name"] == "saved_actors": report["saved_actors"] = copy.deepcopy(rows)
        elif kind == "inspect": value = envelope({**copy.deepcopy(model[e["name"]]), "persistent_level_path": package})
        else:
            before = copy.deepcopy(list(model.values())); name = e["name"]; delta = {"failed_count": 0, "created_count": 0, "updated_count": 0, "removed_count": 0, "skipped_count": 0}
            if kind == "spawn": model[name] = actor(name, e["kind"]); delta["created_count"] = 1
            elif kind == "transform": model[name]["location"] = [40,50,60]; delta["updated_count"] = 1
            elif kind == "noop": delta["skipped_count"] = 1
            deleted = model.pop(name) if kind == "delete" else None
            if deleted: delta["removed_count"] = 1
            data = {**copy.deepcopy(model.get(name, {})), "persistent_level_path": package, "contract_version": "mutation_result_v1", "verified_success": True, "postcondition_verified": True,
                "operation": e.get("operation", spec["tool"]), "before": {"actors": before}, "after": {"actors": copy.deepcopy(list(model.values()))}, "delta": delta,
                "native_outcome": {"change_state_known": True, "postcondition_passed": True, "skipped_noop": kind == "noop", "mutation_attempted": kind != "noop"}}
            if deleted: data.update(deleted_actor=deleted, deleted=True)
            value = envelope(data); value["retry"]["replayed"] = kind == "replay"
        call = {"toolset": "fake." + spec["toolset_suffix"], "tool": spec["tool"], "arguments": spec["arguments"], "mutation": spec["mutation"],
            "raw": {"structuredContent": {"returnValue": copy.deepcopy(value)}}, "decoded": copy.deepcopy(value)}
        report["stages"].append({"name": spec["name"], "passed": True, "call_index": len(report["tool_calls"]), "result": copy.deepcopy(value)})
        report["tool_calls"].append(call)
    return report


class ActorVerifierTests(unittest.TestCase):
    def setUp(self):
        self.fixture = sample_fixture(); self.raw = json.dumps(self.fixture).encode(); self.report = sample_report(self.fixture)

    def verify(self, report=None, **extra): return v.verify(report or self.report, fixture=self.fixture, fixture_bytes=self.raw, check_files=False, **extra)

    def tamper_result(self, stage, change):
        row = next(r for r in self.report["stages"] if r["name"] == stage); change(row["result"])
        call = self.report["tool_calls"][row["call_index"]]; call["decoded"] = copy.deepcopy(row["result"]); call["raw"]["structuredContent"]["returnValue"] = copy.deepcopy(row["result"])

    def test_positive_author_and_new_process_reload(self):
        self.assertTrue(self.verify()["passed"], self.verify())
        cold = sample_report(self.fixture, "reload", self.report)
        self.assertTrue(self.verify(cold, parent=self.report, parent_bytes=json.dumps(self.report).encode())["passed"])

    def test_seven_named_tools_are_exercised_and_no_executor(self):
        names = {r["tool"] for r in v.plan(self.fixture, "author") if r["toolset_suffix"] == "CubelessActorTools"}
        self.assertEqual(names, {"get_actors_in_level", "find_actors_by_name", "get_actor_properties", "spawn_actor", "create_actor", "delete_actor", "set_actor_transform"})
        self.assertEqual(sum(r["tool"] == "save_assets" for r in v.plan(self.fixture, "author")), 1)

    def test_raw_result_cannot_be_replaced_by_cached_success(self):
        self.report["tool_calls"][4]["raw"]["isError"] = True
        self.assertFalse(self.verify()["passed"])

    def test_tool_provenance_and_arguments_are_bound(self):
        self.report["tool_calls"][4]["arguments"]["name"] = "OtherActor"
        self.assertFalse(self.verify()["passed"])

    def test_false_success_wrong_spawn_class_rejected(self):
        self.tamper_result("spawn_PointLight", lambda r: r["data"].update(class_path="/Script/CubelessMCPUE58Editor.Bad"))
        self.assertFalse(self.verify()["passed"])

    def test_name_search_must_not_match_label(self):
        self.tamper_result("find_label_only", lambda r: r["data"].update(actor_count=1))
        self.assertFalse(self.verify()["passed"])

    def test_partial_transform_must_preserve_rotation_and_scale(self):
        self.tamper_result("partial_transform", lambda r: r["data"]["after"]["actors"][-1].update(scale=[2,2,2]))
        self.assertFalse(self.verify()["passed"])

    def test_noop_must_not_dispatch_mutation(self):
        self.tamper_result("noop_transform", lambda r: r["data"]["native_outcome"].update(mutation_attempted=True))
        self.assertFalse(self.verify()["passed"])

    def test_replay_requires_historical_replay_flag(self):
        self.tamper_result("replay_transform", lambda r: r["retry"].update(replayed=False))
        self.assertFalse(self.verify()["passed"])

    def test_protected_delete_cannot_accept_any_error(self):
        self.tamper_result("negative_protected", lambda r: r.update(error_code="transport_failed"))
        self.assertFalse(self.verify()["passed"])

    def test_negative_followup_checks_actual_state(self):
        self.tamper_result("after_duplicate", lambda r: r["data"]["actors"][-1].update(location=[900,0,0]))
        self.assertFalse(self.verify()["passed"])

    def test_missing_actor_or_dropped_stage_rejected(self):
        self.report["stages"].pop(5)
        self.assertFalse(self.verify()["passed"])

    def test_disk_owner_escape_rejected(self):
        self.report["asset_file"]["path"] = str(v.PROJECT.parent / "Content/UserMap.umap")
        self.assertFalse(self.verify()["passed"])

    def test_same_process_cannot_claim_cold_reload(self):
        cold = sample_report(self.fixture, "reload", self.report)
        for row, call in zip(cold["stages"], cold["tool_calls"]):
            if row["name"] in {"preflight", "final_editor"}:
                row["result"]["instance_id"] = "author"; call["decoded"] = copy.deepcopy(row["result"]); call["raw"]["structuredContent"]["returnValue"] = copy.deepcopy(row["result"])
        cold["editor"]["instance_id"] = "author"
        self.assertFalse(self.verify(cold, parent=self.report, parent_bytes=json.dumps(self.report).encode())["passed"])

    def cold_with_extra_actor(self, name):
        cold = sample_report(self.fixture, "reload", self.report)
        extra = copy.deepcopy(cold["saved_actors"][0]); extra.update(name=name, class_path="/Script/Engine.Actor", actor_path=self.fixture["map_object"] + ":PersistentLevel." + name)
        extra["class"] = "Actor"
        for row, call in zip(cold["stages"], cold["tool_calls"]):
            if row["name"] in {"reload_actors", "saved_actors"}:
                row["result"]["data"]["actors"].append(copy.deepcopy(extra)); row["result"]["data"]["actor_count"] += 1
                call["decoded"] = copy.deepcopy(row["result"]); call["raw"]["structuredContent"]["returnValue"] = copy.deepcopy(row["result"])
        cold["saved_actors"].append(extra)
        return cold

    def test_cold_system_actor_difference_does_not_claim_persistence(self):
        cold = self.cold_with_extra_actor("TransientSystemActor")
        result = self.verify(cold, parent=self.report, parent_bytes=json.dumps(self.report).encode())
        self.assertTrue(result["passed"], result)

    def test_cold_unexpected_owned_actor_is_rejected(self):
        cold = self.cold_with_extra_actor("Cubeless_Unexpected")
        self.assertFalse(self.verify(cold, parent=self.report, parent_bytes=json.dumps(self.report).encode())["passed"])

    def run_synthetic_pipeline(self, bad_negative=False):
        from pipelines import actor_mutation_smoke as pipeline
        with tempfile.TemporaryDirectory(prefix="cubeless-actor-pipeline-") as directory:
            project = Path(directory) / "CubelessMCPVerification.uproject"
            with patch.object(v, "PROJECT", project), patch.object(pipeline, "PROJECT", project):
                fixture = sample_fixture(); disk = Path(fixture["asset_file"]["path"])
                disk.parent.mkdir(parents=True); disk.write_bytes(b"synthetic map")
                fixture["asset_file"]["sha256"] = v.sha(disk.read_bytes())
                fixture_path = Path(directory) / "fixture.json"; fixture_path.write_text(json.dumps(fixture), encoding="utf-8")
                golden = sample_report(fixture); specs = v.plan(fixture, "author"); schemas = {}
                for spec in specs:
                    methods = schemas.setdefault(spec["toolset_suffix"], {})
                    methods.setdefault(spec["tool"], set()).update(spec["arguments"])
                if bad_negative:
                    index = next(i for i, s in enumerate(specs) if s["name"] == "negative_duplicate")
                    value = golden["tool_calls"][index]["raw"]["structuredContent"]["returnValue"]
                    value.update(success=True, changes=[{"attempted": True}])
                class Client:
                    def __init__(self, *args): self.observations = []; self.index = 0
                    def initialize(self): return {"protocolVersion": "synthetic"}
                    def list_tools(self): return {"tools": []}
                    def call(self, name, args=None):
                        if name == "list_toolsets": return {"structuredContent": "\n".join("- fake." + key + ": synthetic" for key in schemas)}
                        suffix = args["toolset_name"].split(".")[-1]
                        return {"structuredContent": {"tools": [{"name": tool, "inputSchema": {"properties": {k: {} for k in keys}, "required": []}} for tool, keys in schemas[suffix].items()]}}
                    def call_toolset(self, *args, **kwargs):
                        raw = golden["tool_calls"][self.index]["raw"]; self.index += 1; return copy.deepcopy(raw)
                output = Path(directory) / "report.json"
                with patch.object(pipeline, "OfficialMCPClient", Client):
                    code = pipeline.main(["--endpoint", "http://localhost:1/mcp", "--expected-project", str(project), "--phase", "author", "--fixture-report", str(fixture_path), "--output", str(output)])
                return code, json.loads(output.read_text(encoding="utf-8"))

    def test_pipeline_handles_official_string_and_void_results(self):
        code, report = self.run_synthetic_pipeline()
        self.assertEqual(code, 0, report.get("error"))
        self.assertTrue(report["independent_verification"]["passed"])

    def test_unexpected_negative_acceptance_gets_readback_then_stops(self):
        code, report = self.run_synthetic_pipeline(True)
        self.assertEqual(code, 1)
        self.assertEqual(report["stages"][-1]["name"], "after_duplicate")
        self.assertEqual(report["save_calls"], 0)
        self.assertNotIn("negative_missing", [r["name"] for r in report["stages"]])


if __name__ == "__main__": unittest.main(verbosity=2)
