"""Offline D proof counterexamples; no Unreal processes or asset writes."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import current_runtime_proof as proof


class CurrentRuntimeProofTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.paths = {k: proof.contract.RECORDS / v for k, v in proof.DEFAULTS.items()}
        cls.manifest = proof.read_json(proof.contract.RECORDS / "operations-current-runtime-manifest-A.json")
        cls.summary = proof.build(cls.paths)
        cls.static = proof.read_json(cls.paths["package"])

    def test_actual_D_proof_recomputed(self):
        self.assertTrue(self.summary["success_within_scope"], self.summary.get("error"))
        checked = proof.verify_summary(self.summary)
        self.assertTrue(checked["passed"], checked)
        self.assertEqual(self.summary["package"]["membership"]["required_package_count"], 22)
        self.assertEqual(self.summary["prior_C"]["package_status"], "incomplete")
        self.assertEqual(self.summary["author_standalone"]["passed"], False)

    def test_summary_status_cannot_hide_missing_member(self):
        changed = copy.deepcopy(self.summary)
        changed["package"]["membership"]["exact_entries"].pop(next(iter(changed["package"]["membership"]["exact_entries"])))
        self.assertFalse(proof.verify_summary(changed)["passed"])

    def test_summary_cannot_upgrade_C_or_shader_scope(self):
        changed = copy.deepcopy(self.summary); changed["prior_C"]["package_status"] = "pass"
        self.assertFalse(proof.verify_summary(changed)["passed"])
        for key in ("shader_completion_verified", "full_migration_complete"):
            changed = copy.deepcopy(self.summary); changed[key] = True
            self.assertFalse(proof.verify_summary(changed)["passed"])

    def test_wrong_raw_fingerprint_rejected(self):
        changed = copy.deepcopy(self.summary); changed["artifacts"]["build_log"]["sha256"] = "0" * 64
        self.assertFalse(proof.verify_summary(changed)["passed"])

    def test_static_pass_cannot_hide_iostore(self):
        changed = copy.deepcopy(self.static); changed["containers"].append("Project/Content/Paks/global.utoc")
        with self.assertRaisesRegex(RuntimeError, "container"):
            proof.static_evidence(changed, self.manifest)

    def test_static_pass_cannot_hide_incomplete_listing(self):
        changed = copy.deepcopy(self.static); changed["pak_listings"][0]["status"] = "incomplete"
        with self.assertRaisesRegex(RuntimeError, "Pak bytes/listing"):
            proof.static_evidence(changed, self.manifest)

    def test_wrong_pak_fingerprint_rejected(self):
        changed = copy.deepcopy(self.static); changed["pak_listings"][0]["pak_sha256_after"] = "0" * 64
        with self.assertRaisesRegex(RuntimeError, "Pak bytes/listing"):
            proof.static_evidence(changed, self.manifest)

    def test_missing_exact_package_and_name_prefix_alias_rejected(self):
        entries = list(proof.expected_entries(self.manifest).values())
        self.assertEqual(proof.membership(entries, self.manifest)["required_package_count"], 22)
        for changed in (entries[1:], [entries[0] + ".bak"] + entries[1:]):
            with self.assertRaisesRegex(RuntimeError, "exact cooked package"):
                proof.membership(changed, self.manifest)

    def test_old_map_and_old_map_payload_rejected(self):
        entries = list(proof.expected_entries(self.manifest).values())
        old = proof.contract.PROJECT.stem + "/" + proof.contract.relative_asset(self.manifest["excluded_old_map"], True)
        for extra in (old, old.removesuffix(".umap") + ".uexp"):
            with self.assertRaisesRegex(RuntimeError, "Old marker map"):
                proof.membership(entries + [extra], self.manifest)

    def test_listing_count_mount_and_python_payload_rejected(self):
        def listing(entry, count=1, mount="../../../"):
            return f'Listing Fixture.pak with mount point "{mount}"\n"{entry}" offset: 0, size: 5 bytes\n{count} files (5 bytes), (5 filtered bytes).'
        ordinary = "CubelessCurrentRuntimeProof/Content/CubelessMCPTests/Test.uasset"
        self.assertEqual(proof.parse_listing(listing(ordinary), 1), [ordinary])
        for text, count in [(listing(ordinary, 2), 1), (listing(ordinary, mount="../../Other"), 1), (listing("Project/Content/Python/startup.py"), 1), (listing("../Outside.uasset"), 1), (listing("Project/Plugins/CubelessMCP_UE58/Content/x.uasset"), 1)]:
            with self.subTest(text=text):
                with self.assertRaises(RuntimeError): proof.parse_listing(text, count)

    def test_build_log_failure_and_wrong_map_rejected(self):
        text = self.paths["build_log"].read_text(encoding="utf-8-sig")
        mutations = [text.replace("BUILD SUCCESSFUL", "BUILD FAILED"), text.replace("ExitCode=0 (Success)", "ExitCode=1 (Error)"), text.replace(self.manifest["default_map"], "/Game/WrongMap"), text.replace("-clientconfig=Shipping", "-clientconfig=Development"), text.replace("BUILD SUCCESSFUL", "LogMaterial: Warning: Failed to compile Material\nBUILD SUCCESSFUL")]
        with tempfile.TemporaryDirectory(prefix="cubeless-proof-unit-") as temp:
            path = Path(temp) / "synthetic-not-runtime.log"
            for text in mutations:
                with self.subTest(text=text[-150:]):
                    path.write_text(text, encoding="utf-8")
                    with self.assertRaises(RuntimeError): proof.build_observation(path, self.manifest, proof.STAGE.parent)

    def test_missing_evidence_role_rejected(self):
        paths = dict(self.paths); paths.pop("baseline")
        self.assertFalse(proof.build(paths)["success_within_scope"])

    def test_editor_only_interpretation_bound_to_actual_source(self):
        source = self.summary["dependency_query_source"]
        self.assertEqual(source["source"]["sha256"], "0200285704dd44d2a43e4143d9cb78c5918c9a8f5381bc8dc946aca1534aeafe")
        self.assertEqual((source["line_start"], source["line_end"]), (10355, 10373))
        self.assertIn("includes editor-only as well as game", source["interpretation"])

    def test_game_filtered_query_cannot_claim_both_categories(self):
        source = self.summary["dependency_query_source"]
        options = copy.deepcopy(source["recorded_options"])
        options["hard_package"]["include_game_package_references"] = True
        with self.assertRaisesRegex(RuntimeError, "no-Game-filter"):
            proof.dependency_query_source(self.static, options)


if __name__ == "__main__": unittest.main()
