"""Offline dispatcher counterexamples; no Unreal processes or assets are used."""
from __future__ import annotations

import contextlib
import copy
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from pipelines import verification_dispatcher as dispatcher
from verification.test_pipeline_contracts import author_fixture, update_fingerprint


class DispatcherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cubeless dispatcher tests ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def plan(self, profile="bootstrap-offline", *extras, output="run.json"):
        args = dispatcher.make_parser().parse_args(["--profile", profile, "--output", str(self.root / output), *map(str, extras)])
        return dispatcher.build_plan(args)

    def write(self, name, value):
        path = self.root / name
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def test_exact_source_manifest_and_profile_fingerprints(self):
        expected = {"manifest": "0d45de1b90aab8faef004356a52e7024b78cdcf06793a8a7c6f4b8b082b400c2",
                    "profiles": "881f4da6d8d7ba62103ff10019fda25ef91288c9aaf73520a288c2171cc36329"}
        for name, value in expected.items():
            self.assertEqual(hashlib.sha256(json.dumps(dispatcher.LEGACY[name], sort_keys=True, separators=(",", ":")).encode()).hexdigest(), value)
        self.assertEqual(dispatcher.LEGACY["source"]["sha256"], "2568476fee409ea43f774035bf673f2a513d6de39cca3f7fc4d254783c3549c9")
        self.assertEqual(len(dispatcher.LEGACY_MANIFEST), 42)
        self.assertEqual(len(dispatcher.LEGACY_PROFILES), 9)
        self.assertEqual(dispatcher.LEGACY_PROFILES["full"], [x["key"] for x in dispatcher.LEGACY_MANIFEST])

    def test_plan_never_runs_children_or_creates_output(self):
        output = self.root / "nonexistent" / "plan.json"
        with patch.object(dispatcher.subprocess, "run") as runner, contextlib.redirect_stdout(io.StringIO()):
            code = dispatcher.main(["--profile", "full", "--plan", "json", "--output", str(output)])
        self.assertEqual(code, 0)
        runner.assert_not_called()
        self.assertFalse(output.parent.exists())

    def test_all_nine_legacy_profiles_are_incomplete_without_execution(self):
        for index, profile in enumerate(dispatcher.LEGACY_PROFILES):
            plan = self.plan(profile, output=f"legacy-{index}.json")
            with patch.object(dispatcher.subprocess, "run") as runner:
                code, report = dispatcher.run_plan(plan)
            runner.assert_not_called()
            self.assertEqual((code, report["status"]), (2, "incomplete"))
            self.assertEqual([row["key"] for row in report["gates"]], dispatcher.LEGACY_PROFILES[profile])
            self.assertTrue(all(row["status"] == "pending" for row in report["gates"]))
            self.assertFalse(report["full_migration_complete"])
            self.assertEqual(report["legacy_gates_closed"], [])

    def test_missing_phase_inputs_are_incomplete_and_no_child_runs(self):
        plan = self.plan("blueprint-reload-evidence")
        with patch.object(dispatcher.subprocess, "run") as runner:
            code, report = dispatcher.run_plan(plan)
        runner.assert_not_called()
        self.assertEqual(code, 2)
        self.assertEqual(len(report["incomplete_gates"]), 3)

    def test_fail_fast_preserves_first_child_exit_and_later_not_run(self):
        plan = self.plan()
        with patch.object(dispatcher.subprocess, "run", return_value=subprocess.CompletedProcess([], 23)) as runner:
            code, report = dispatcher.run_plan(plan)
        self.assertEqual(code, 23)
        self.assertEqual(runner.call_count, 1)
        self.assertEqual(report["first_failed_gate"], "pipeline_contract_unit")
        self.assertEqual(report["gates"][0]["child_exit_code"], 23)
        self.assertTrue(all(row["status"] == "not_run" for row in report["gates"][1:]))
        self.assertEqual(json.loads((self.root / "run.json").read_text())["status"], "failed")

    def test_missing_process_executable_has_durable_failure(self):
        with patch.object(dispatcher.subprocess, "run", side_effect=FileNotFoundError("synthetic executable missing")):
            code, report = dispatcher.run_plan(self.plan())
        self.assertEqual(code, 127)
        self.assertEqual(report["gates"][0]["error_type"], "FileNotFoundError")
        self.assertTrue((self.root / "run.json").is_file())

    def test_no_shell_paths_with_spaces_are_single_arguments(self):
        author = self.write("author with spaces.json", author_fixture())
        plan = self.plan("blueprint-author-evidence", "--author-report", author)
        with patch.object(dispatcher.subprocess, "run", return_value=subprocess.CompletedProcess([], 8)) as runner:
            dispatcher.run_plan(plan)
        args, kwargs = runner.call_args
        self.assertIn(str(author), args[0])
        self.assertNotIn("shell", kwargs)
        self.assertEqual(kwargs["stdin"], subprocess.DEVNULL)
        self.assertEqual(args[0][:2], [sys.executable, "-B"])

    def test_author_summary_pass_without_stage_evidence_rejected_by_real_verifier(self):
        author = self.write("forged.json", {"status": "pass", "phase": "author", "compile": {"compiled": True}, "save": {"saved": True}})
        code, report = dispatcher.run_plan(self.plan("blueprint-author-evidence", "--author-report", author))
        self.assertNotEqual(code, 0)
        self.assertEqual(report["gates"][0]["child_exit_code"], 1)
        child_result = Path(report["plan"]["gates"][0]["result_file"])
        self.assertFalse(dispatcher.read_json(child_result)["passed"])

    def test_correct_synthetic_author_only_claims_contract_scope(self):
        author = self.write("author.json", author_fixture())
        code, report = dispatcher.run_plan(self.plan("blueprint-author-evidence", "--author-report", author))
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "passed_limited_scope")
        self.assertFalse(report["full_migration_complete"])
        self.assertFalse(report["packaging_independence_proven"])
        self.assertIn("source authenticity", report["gates"][0]["scope"])

    def test_reload_and_author_use_distinct_required_phases(self):
        author = self.write("author.json", author_fixture())
        plan = self.plan("blueprint-reload-evidence", "--author-report", author, "--reload-report", author)
        self.assertEqual(plan["gates"][0]["argv"][-3], "author")
        self.assertEqual(plan["gates"][1]["argv"][-3], "reload")
        code, report = dispatcher.run_plan(plan)
        self.assertEqual(code, 1)
        self.assertEqual(report["first_failed_gate"], "blueprint_reload_report")

    def test_reload_link_requires_actual_author_bytes_and_identity(self):
        value = author_fixture()
        author = self.write("author.json", value)
        later = {"run_id": value["run_id"], "asset_path": value["asset_path"], "project_file": value["project_file"],
                 "author_report_sha256": dispatcher.digest(author), "author_editor": value["editor"]}
        reload_path = self.write("reload.json", later)
        gate = self.plan("blueprint-reload-evidence", "--author-report", author, "--reload-report", reload_path)["gates"][-1]
        self.assertTrue(dispatcher.verify_lifecycle_link(gate)["passed"])
        later["author_report_sha256"] = "counterfeit"
        self.write("reload.json", later)
        self.assertFalse(dispatcher.verify_lifecycle_link(gate)["passed"])
        later["author_report_sha256"] = dispatcher.digest(author)
        later["asset_path"] = "/Game/Unrelated"
        self.write("reload.json", later)
        self.assertFalse(dispatcher.verify_lifecycle_link(gate)["passed"])

    def test_cleanup_has_independent_phase_and_parent_link(self):
        value = author_fixture()
        author = self.write("author.json", value)
        cleanup = self.write("cleanup.json", value)
        plan = self.plan("blueprint-cleanup-evidence", "--author-report", author, "--cleanup-report", cleanup)
        self.assertEqual(plan["gates"][1]["argv"][-3], "cleanup")
        self.assertFalse(dispatcher.verify_lifecycle_link(plan["gates"][-1])["passed"])

    def test_overwrite_report_and_child_evidence_are_refused(self):
        plan = self.plan()
        (self.root / "run.json").write_text("preserve me")
        with patch.object(dispatcher.subprocess, "run") as runner, self.assertRaises(FileExistsError):
            dispatcher.run_plan(plan)
        runner.assert_not_called()
        self.assertEqual((self.root / "run.json").read_text(), "preserve me")
        plan2 = self.plan(output="other.json")
        Path(plan2["child_directory"]).mkdir()
        with self.assertRaises(FileExistsError):
            dispatcher.run_plan(plan2)
        self.assertFalse(Path(plan2["report_path"]).exists())

    def test_input_change_during_verifier_is_failed(self):
        author = self.write("author.json", author_fixture())
        plan = self.plan("blueprint-author-evidence", "--author-report", author)
        gate = plan["gates"][0]
        def mutate(*args, **kwargs):
            author.write_text("changed during read")
            Path(gate["result_file"]).write_text(json.dumps({"schema": gate["expected_schema"], "passed": True}))
            return subprocess.CompletedProcess([], 0)
        with patch.object(dispatcher.subprocess, "run", side_effect=mutate):
            code, report = dispatcher.run_plan(plan)
        self.assertEqual(code, 1)
        self.assertIn("changed during validation", report["gates"][0]["error"])

    def test_success_exit_without_independent_result_is_failed(self):
        author = self.write("author.json", author_fixture())
        with patch.object(dispatcher.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)):
            code, report = dispatcher.run_plan(self.plan("blueprint-author-evidence", "--author-report", author))
        self.assertNotEqual(code, 0)
        self.assertEqual(report["gates"][0]["child_exit_code"], 0)

    def test_wrong_child_result_schema_is_rejected(self):
        author = self.write("author.json", author_fixture())
        plan = self.plan("blueprint-author-evidence", "--author-report", author)
        gate = plan["gates"][0]
        def wrong(*args, **kwargs):
            Path(gate["result_file"]).write_text(json.dumps({"schema": "unrelated", "passed": True}))
            return subprocess.CompletedProcess([], 0)
        with patch.object(dispatcher.subprocess, "run", side_effect=wrong):
            code, report = dispatcher.run_plan(plan)
        self.assertEqual(code, 1)
        self.assertIn("schema mismatch", report["gates"][0]["error"])

    def test_package_profile_requires_fresh_inputs_not_old_status_report(self):
        plan = self.plan("package-static-evidence")
        self.assertIn("--stage", plan["gates"][0]["argv"])
        self.assertIn("--unrealpak", plan["gates"][0]["argv"])
        with patch.object(dispatcher.subprocess, "run") as runner:
            code, report = dispatcher.run_plan(plan)
        runner.assert_not_called()
        self.assertEqual(code, 2)

    def test_package_output_cannot_modify_staging(self):
        stage = self.root / "stage"
        stage.mkdir()
        plan = self.plan("package-static-evidence", "--stage", stage, output="stage/report.json")
        with self.assertRaises(ValueError):
            dispatcher.run_plan(plan)
        self.assertEqual(list(stage.iterdir()), [])

    def test_marker_profile_runs_verifier_and_keeps_limits(self):
        fixture = self.write("fixture.json", {})
        baseline = self.write("baseline.json", {})
        launch = self.write("launch.json", {})
        plan = self.plan("runtime-marker-evidence", "--fixture-report", fixture, "--baseline", baseline, "--launch-report", launch)
        self.assertIn("verify", plan["gates"][0]["argv"])
        self.assertNotIn("prepare", plan["gates"][0]["argv"])
        code, report = dispatcher.run_plan(plan)
        self.assertEqual(code, 1)
        self.assertFalse(report["packaging_independence_proven"])

    def test_unknown_profile_rejected(self):
        args = dispatcher.make_parser().parse_args([])
        args.profile = "made-up"
        with self.assertRaises(ValueError):
            dispatcher.build_plan(args)

    def test_material_missing_phase_inputs_are_pending_without_execution(self):
        plan = self.plan("material-reload-evidence")
        with patch.object(dispatcher.subprocess, "run") as runner:
            code, report = dispatcher.run_plan(plan)
        runner.assert_not_called()
        self.assertEqual(code, 2)
        self.assertEqual(len(report["incomplete_gates"]), 2)
        self.assertEqual(report["legacy_gates_closed"], [])

    def test_material_reload_passes_actual_author_bytes_to_independent_verifier(self):
        author = self.write("material author.json", {})
        reload = self.write("material reload.json", {})
        plan = self.plan("material-reload-evidence", "--author-report", author, "--reload-report", reload)
        first, second = plan["gates"]
        self.assertIn("material_pipeline_report.py", first["argv"][2])
        self.assertEqual(first["argv"][first["argv"].index("--phase") + 1], "author")
        self.assertEqual(second["argv"][second["argv"].index("--phase") + 1], "reload")
        self.assertEqual(second["argv"][second["argv"].index("--parent-report") + 1], str(author))
        self.assertEqual(len(second["required_inputs"]), 3)

    def test_material_pass_summary_cannot_replace_readback_evidence(self):
        author = self.write("forged material.json", {"schema": "cubeless_material_pipeline_v1", "status": "pass", "phase": "author"})
        code, report = dispatcher.run_plan(self.plan("material-author-evidence", "--author-report", author))
        self.assertEqual(code, 1)
        self.assertEqual(report["first_failed_gate"], "material_author_report")
        child = dispatcher.read_json(report["plan"]["gates"][0]["result_file"])
        self.assertFalse(child["passed"])
        self.assertFalse(child["full_material_migration_complete"])


if __name__ == "__main__":
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(DispatcherTests))
    report = {"schema": "cubeless_dispatcher_offline_tests_v1", "tests_run": result.testsRun, "passed": result.wasSuccessful(),
              "failures": len(result.failures), "errors": len(result.errors), "scope": "Synthetic/offline; no Unreal launch or asset mutations", "details": stream.getvalue()}
    if len(sys.argv) > 1:
        path = Path(sys.argv[1])
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("x", encoding="utf-8") as output:
            json.dump(report, output, indent=2)
    print(json.dumps(report))
    raise SystemExit(0 if result.wasSuccessful() else 1)
