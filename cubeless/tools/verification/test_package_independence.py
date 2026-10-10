"""Synthetic checker counterexamples. Does not launch UnrealPak, an editor, or a game."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import check_package_independence as checker


class PackageEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="cubeless-checker-test-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.stage = self.root / "stage"
        self.stage.mkdir()
        (self.stage / "Fixture.exe").write_bytes(b"synthetic-not-executable")
        self.receipt = self.root / "Fixture.target"
        self.receipt.write_text(json.dumps({"TargetName": "Fixture", "TargetType": "Game", "Platform": "Win64",
            "Configuration": "Shipping", "BuildProducts": [{"Path": "$(ProjectDir)/Binaries/Win64/Fixture.exe", "Type": "Executable"}]}))
        self.unrealpak = self.root / "UnrealPak.exe"
        self.unrealpak.write_bytes(b"synthetic-never-launched")

    def inspect(self, **options):
        defaults = {"stage": self.stage, "unrealpak": self.unrealpak, "runtime_log": None, "marker": "",
                    "listing_dir": self.root / "listings", "receipt_paths": [self.receipt]}
        defaults.update(options)
        return checker.inspect(**defaults)

    def pak(self, name="Content.pak"):
        path = self.stage / name
        path.write_bytes(b"synthetic-pak-never-parsed")
        return path

    @staticmethod
    def listing(*names, total=None):
        lines = [f'LogPakFile: Display: "{name}" offset: 0, size: 1 bytes, sha1: 00, compression: None.' for name in names]
        lines.append(f"LogPakFile: Display: {len(names) if total is None else total} files (1 bytes), (0 filtered bytes).")
        return ("\n".join(lines) + "\n").encode()

    def run_result(self, output, exit_code=0):
        return subprocess.CompletedProcess([], exit_code, stdout=output, stderr=b"")

    def test_arbitrary_nonempty_directory_is_incomplete(self):
        result = self.inspect(receipt_paths=[])
        self.assertEqual(result["status"], "incomplete")

    def test_missing_executable_cannot_pass_receipt_alone(self):
        (self.stage / "Fixture.exe").unlink()
        self.assertEqual(self.inspect()["status"], "incomplete")

    def test_editor_target_cannot_substitute_for_game_receipt(self):
        data = json.loads(self.receipt.read_text())
        data["TargetType"] = "Editor"
        self.receipt.write_text(json.dumps(data))
        self.assertEqual(self.inspect()["status"], "incomplete")

    def test_clean_static_scope_does_not_claim_packaging_proof(self):
        result = self.inspect()
        self.assertEqual(result["status"], "static_pass")
        self.assertFalse(result["packaging_independence_proven"])
        self.assertFalse(result["full_migration_complete"])

    def test_every_pak_is_directly_listed_and_second_violation_fails(self):
        first, second = self.pak("A.pak"), self.pak("B.pak")
        outputs = [self.run_result(self.listing("../../../Fixture/Content/Test.uasset")),
                   self.run_result(self.listing("../../../Fixture/Plugins/CubelessMCP_UE58/Content/Python/init_unreal.py"))]
        with patch.object(checker.subprocess, "run", side_effect=outputs) as runner:
            result = self.inspect()
        self.assertEqual([call.args[0][1] for call in runner.call_args_list], [str(first), str(second)])
        self.assertEqual(result["pak_coverage"]["complete_listings"], 2)
        self.assertEqual(result["status"], "fail")

    def test_missing_unrealpak_cannot_accept_old_listings(self):
        self.pak()
        with patch.object(checker.subprocess, "run") as runner:
            result = self.inspect(unrealpak=None)
        runner.assert_not_called()
        self.assertEqual(result["status"], "incomplete")

    def test_truncated_listing_with_valid_first_entry_is_incomplete(self):
        self.pak()
        with patch.object(checker.subprocess, "run", return_value=self.run_result(self.listing("safe.uasset", total=2))):
            result = self.inspect()
        self.assertEqual(result["status"], "incomplete")

    def test_unknown_extensions_are_scanned(self):
        self.pak()
        with patch.object(checker.subprocess, "run", return_value=self.run_result(self.listing("../../../Fixture/run.pyw"))):
            result = self.inspect()
        self.assertEqual(result["status"], "fail")

    def test_failed_listing_does_not_count_as_complete_coverage(self):
        self.pak()
        with patch.object(checker.subprocess, "run", return_value=self.run_result(self.listing("safe.uasset"), 1)):
            result = self.inspect()
        self.assertEqual(result["pak_coverage"]["direct_listing_records"], 1)
        self.assertEqual(result["pak_coverage"]["complete_listings"], 0)
        self.assertEqual(result["status"], "incomplete")

    def test_pak_changed_while_listing_cannot_pass(self):
        pak = self.pak()
        def listing_changed(*args, **kwargs):
            pak.write_bytes(b"changed")
            return self.run_result(self.listing("safe.uasset"))
        with patch.object(checker.subprocess, "run", side_effect=listing_changed):
            result = self.inspect()
        self.assertEqual(result["status"], "incomplete")

    def test_timeout_retains_partial_evidence(self):
        self.pak()
        timeout = subprocess.TimeoutExpired("UnrealPak", 300, output=b"partial listing", stderr=b"timeout detail")
        with patch.object(checker.subprocess, "run", side_effect=timeout):
            result = self.inspect()
        self.assertTrue(result["pak_listings"][0]["timed_out"])
        self.assertEqual(Path(result["pak_listings"][0]["listing"]).read_bytes(), b"partial listingtimeout detail")
        self.assertEqual(result["status"], "incomplete")

    def test_existing_evidence_is_not_overwritten(self):
        self.pak()
        with patch.object(checker.subprocess, "run", return_value=self.run_result(self.listing("safe.uasset"))) as runner:
            first = self.inspect()
            second = self.inspect()
        self.assertEqual(runner.call_count, 1)
        self.assertEqual(first["status"], "static_pass")
        self.assertEqual(second["status"], "incomplete")

    def test_iostore_is_explicitly_incomplete(self):
        (self.stage / "Content.utoc").write_bytes(b"synthetic")
        self.assertEqual(self.inspect()["status"], "incomplete")

    def test_invalid_json_receipt_cannot_pass(self):
        self.receipt.write_text("not json")
        self.assertEqual(self.inspect()["status"], "incomplete")

    def test_explicitly_inactive_project_plugin_reference_is_recorded_without_false_failure(self):
        (self.stage / "Fixture.uproject").write_text(json.dumps({"Plugins": [
            {"Name": "ModelContextProtocol", "Enabled": False},
            {"Name": "ToolsetRegistry", "Enabled": True, "TargetAllowList": ["Editor"]}]}))
        result = self.inspect()
        self.assertEqual(result["status"], "static_pass")
        row = next(item for item in result["receipts"] if item["path"].endswith("Fixture.uproject"))
        self.assertEqual(row["inactive_plugin_references"], ["ModelContextProtocol", "ToolsetRegistry"])

    def test_game_target_plugin_reference_is_still_rejected(self):
        (self.stage / "Fixture.uproject").write_text(json.dumps({"Plugins": [
            {"Name": "ModelContextProtocol", "Enabled": True, "TargetAllowList": ["Game"]}]}))
        self.assertEqual(self.inspect()["status"], "fail")

    def test_runtime_failure_fails_top_level_even_if_static_scan_passes(self):
        log = self.root / "runtime.log"
        log.write_text("LogBlueprintUserMessages: FIXTURE_READY\nFatal error: broken")
        result = self.inspect(runtime_log=log, marker="FIXTURE_READY", runtime_exit_code=0)
        self.assertEqual(result["static_status"], "pass")
        self.assertEqual(result["status"], "fail")

    def test_runtime_marker_echoed_only_in_command_line_cannot_pass(self):
        log = self.root / "runtime.log"
        log.write_text("LogInit: Command Line: -Marker=FIXTURE_READY")
        self.assertEqual(self.inspect(runtime_log=log, marker="FIXTURE_READY", runtime_exit_code=0)["status"], "fail")

    def test_runtime_without_exit_code_is_incomplete(self):
        log = self.root / "runtime.log"
        log.write_text("LogBlueprintUserMessages: FIXTURE_READY")
        self.assertEqual(self.inspect(runtime_log=log, marker="FIXTURE_READY")["status"], "incomplete")

    def test_fixture_success_is_still_not_packaging_independence_proof(self):
        log = self.root / "runtime.log"
        log.write_text("LogWindows: Failed to load 'aqProf.dll'\nLogBlueprintUserMessages: FIXTURE_READY")
        result = self.inspect(runtime_log=log, marker="FIXTURE_READY", runtime_exit_code=0)
        self.assertEqual(result["status"], "fixture_pass")
        self.assertFalse(result["packaging_independence_proven"])

    def test_listing_evidence_inside_stage_is_rejected(self):
        self.pak()
        self.assertEqual(self.inspect(listing_dir=self.stage / "evidence")["status"], "incomplete")


if __name__ == "__main__":
    unittest.main(verbosity=2)
