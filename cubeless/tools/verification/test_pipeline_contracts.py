"""Offline counterexamples for report gates and uncertain HTTP mutation handling.

Synthetic reports are explicitly test fixtures, not Unreal execution evidence.
"""
import copy
import hashlib
import io
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import MCPError, OfficialMCPClient, tool_payload
from verification.pipeline_report import verify
from pipelines.blueprint_mutation_smoke import strict_mutation


def author_fixture():
    names = ["preflight", "invalid_create_rejected", "create", "duplicate_create_rejected", "add_first_branch", "add_second_branch", "connect", "readback_connection", "stale_position_rejected", "readback_stale_unchanged", "move", "readback_position", "linked_delete_rejected", "readback_rejected_delete", "delete", "readback_deleted", "compile_save", "saved_readback", "dependency_audit"]
    report = {"synthetic": True, "schema": "cubeless_blueprint_pipeline_v1", "phase": "author", "status": "pass", "outcome_unknown": False,
              "automatic_retries": 0, "run_id": "a" * 32, "endpoint": "http://synthetic.invalid/mcp", "asset_path": "/Game/CubelessMCPTests/Run_" + "a" * 32 + "/BP_MutationContract", "project_file": "fixture.uproject", "editor": {"instance_id": "synthetic-A"},
              "compile": {"compiled": True}, "save": {"saved": True}, "reload": {"status": "not_run"}, "packaging": {"status": "not_run"},
              "stages": [{"name": name, "passed": True, "result": {}} for name in names]}
    report["stages"][-3]["result"] = {"compile": {"compiled": True}, "save": {"saved": True}}
    report["stages"][-1]["result"] = {"passed": True}
    update_fingerprint(report)
    return report


def update_fingerprint(report):
    report["input_fingerprint"] = hashlib.sha256(json.dumps({key: report.get(key) for key in ("phase", "run_id", "endpoint", "project_file", "asset_path")}, sort_keys=True).encode()).hexdigest()


class ContractTests(unittest.TestCase):
    def test_complete_synthetic_fixture(self):
        self.assertTrue(verify(author_fixture())["passed"])

    def test_missing_compile_stage_rejected(self):
        value = author_fixture()
        value["stages"] = [x for x in value["stages"] if x["name"] != "compile_save"]
        self.assertFalse(verify(value)["passed"])

    def test_forged_save_summary_does_not_mask_stage_failure(self):
        value = author_fixture()
        value["stages"][-3]["result"]["save"]["saved"] = False
        self.assertFalse(verify(value)["passed"])

    def test_unknown_outcome_is_not_pass(self):
        value = author_fixture()
        value["outcome_unknown"] = True
        self.assertFalse(verify(value)["passed"])

    def test_stale_input_fingerprint_rejected(self):
        value = author_fixture()
        value["project_file"] = "unrelated_project.uproject"
        self.assertFalse(verify(value)["passed"])

    def test_unsaved_fails(self):
        value = author_fixture()
        value["save"]["saved"] = False
        self.assertFalse(verify(value)["passed"])

    def test_missing_independent_readback_fails(self):
        value = author_fixture()
        value["stages"] = [x for x in value["stages"] if x["name"] != "readback_connection"]
        self.assertFalse(verify(value)["passed"])

    def test_author_cannot_claim_reload_or_package(self):
        for key in ("reload", "packaging"):
            value = author_fixture()
            value[key]["status"] = "passed"
            self.assertFalse(verify(value)["passed"])

    def test_same_editor_cannot_be_cold_reload(self):
        value = author_fixture()
        value.update(phase="reload", author_editor={"instance_id": "synthetic-A"}, author_report_sha256="synthetic", reload={"status": "passed"})
        value["stages"] = [{"name": name, "passed": True, "result": {"passed": True}} for name in ("preflight", "new_process", "reload_readback", "reload_graph", "dependency_audit")]
        update_fingerprint(value)
        self.assertFalse(verify(value)["passed"])
        value["editor"]["instance_id"] = "synthetic-B"
        self.assertTrue(verify(value)["passed"])

    def test_missing_delta_contract_rejected(self):
        with self.assertRaises(AssertionError):
            strict_mutation({"success": True, "data": {"contract_version": "mutation_result_v1", "verified_success": True, "postcondition_verified": True, "before": {}, "after": {}, "verification": {"status": "passed", "method": "synthetic"}, "delta": {}}})

    def test_false_boolean_is_preserved(self):
        self.assertIs(tool_payload({"structuredContent": {"returnValue": False}}), False)
        self.assertIs(tool_payload({"content": [{"type": "text", "text": '{"returnValue":false}'}]}), False)

    def test_tool_error_is_not_empty_success(self):
        with self.assertRaises(MCPError):
            tool_payload({"isError": True, "content": []})

    def test_timeout_mutation_not_retried(self):
        class TimeoutOpener:
            calls = 0
            def open(self, request, timeout):
                self.calls += 1
                raise TimeoutError("synthetic timeout after mutation dispatch")
        client = OfficialMCPClient("http://127.0.0.1:1/mcp")
        client.opener = TimeoutOpener()
        with self.assertRaises(MCPError) as caught:
            client.call_toolset("Synthetic.Tools", "mutate", mutation=True)
        self.assertTrue(caught.exception.outcome_unknown)
        self.assertEqual(client.opener.calls, 1)


if __name__ == "__main__":
    stream = io.StringIO()
    outcome = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(ContractTests))
    report = {"schema": "cubeless_external_pipeline_offline_tests_v1", "scope": "Synthetic report counterexamples and timeout handling; no engine calls/assets", "tests_run": outcome.testsRun, "passed": outcome.wasSuccessful(), "failures": len(outcome.failures), "errors": len(outcome.errors), "details": stream.getvalue()}
    if len(sys.argv) > 1:
        path = Path(sys.argv[1])
        if path.exists():
            raise SystemExit("Refusing to overwrite evidence")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items() if key != "details"}))
    raise SystemExit(0 if outcome.wasSuccessful() else 1)
