"""Offline guard counterexamples only; does not import or execute Unreal."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location("component_fixture", Path(__file__).with_name("create_component_read_fixture.py"))
fixture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fixture)


class FixtureGuards(unittest.TestCase):
    def test_actual_user_project_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "isolated"):
            fixture.checked_project(fixture.PROJECT_ROOT / "UnrealMCP5_8_3.uproject")

    def test_exact_isolated_project_is_accepted(self):
        fixture.checked_project(fixture.ALLOWED_PROJECT)

    def test_other_isolated_sibling_is_rejected(self):
        with self.assertRaises(ValueError):
            fixture.checked_project(fixture.ALLOWED_PROJECT.parent.parent / "Wrong" / fixture.ALLOWED_PROJECT.name)

    def test_report_escape_and_bad_run_id_rejected(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(fixture, "RECORDS", Path(folder)):
            for output, run_id in ((Path(folder) / ".." / "outside.json", "a" * 32), (Path(folder) / "ok.json", "../asset"), (Path(folder) / "ok.json", "A" * 32)):
                with self.subTest(output=output, run_id=run_id), self.assertRaises(ValueError):
                    fixture.checked_options(output, run_id)

    def test_existing_evidence_cannot_be_reused(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(fixture, "RECORDS", Path(folder)):
            evidence = Path(folder) / "existing.json"
            evidence.write_text("existing evidence", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "overwritten"):
                fixture.checked_options(evidence, "a" * 32)
            self.assertEqual(evidence.read_text(), "existing evidence")

    def test_wrong_project_author_fails_before_any_asset_api(self):
        calls = []
        fake = types.SimpleNamespace(Paths=types.SimpleNamespace(get_project_file_path=lambda: str(fixture.PROJECT_ROOT / "UnrealMCP5_8_3.uproject"), convert_relative_path_to_full=lambda value: value),
                                     get_editor_subsystem=lambda *args: calls.append("asset subsystem"),
                                     EditorLoadingAndSavingUtils=types.SimpleNamespace(get_dirty_content_packages=lambda: [], get_dirty_map_packages=lambda: []))
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(fixture, "RECORDS", Path(folder)), mock.patch.dict(sys.modules, {"unreal": fake}):
            report = fixture.author(Path(folder) / "wrong-project.json", "b" * 32, 123)
            self.assertFalse(report["success"])
            self.assertEqual(report["actions"], [])
            self.assertEqual(calls, [])

    def test_boolean_is_not_numeric_readback(self):
        self.assertFalse(fixture.close_value(True, 1))
        self.assertFalse(fixture.close_value(0, False))
        self.assertTrue(fixture.close_value([12.0, 34.0], [12, 34]))


if __name__ == "__main__":
    unittest.main()
