"""Meaningful offline tampering tests bound to actual installed-L evidence."""
import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import installed_current_runtime_proof as proof


class InstalledCurrentProofTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.paths = {k: proof.prior.contract.RECORDS / v for k, v in proof.DEFAULTS.items()}
        cls.preparation = proof.read_json(cls.paths["preparation"])
        cls.prior_D = proof.read_json(cls.paths["prior_D"])
        cls.author = proof.read_json(cls.prior_D["artifacts"]["author"]["path"])
        cls.manifest = proof.read_json(cls.prior_D["manifest"]["path"])
        cls.package = proof.read_json(cls.paths["package"])
        cls.engine = Path(cls.package["unrealpak"]["path"]).resolve().parents[2]
        cls.summary = proof.build(cls.paths)

    def test_actual_installed_L_recomputed(self):
        self.assertTrue(self.summary["success_within_scope"], self.summary.get("error"))
        self.assertTrue(proof.verify(self.summary)["passed"])
        self.assertEqual(self.summary["preparation"]["protected_count"], 57)
        self.assertEqual(len(self.summary["preparation"]["assets"]), 23)
        self.assertEqual(self.summary["package"]["membership"]["required_package_count"], 22)
        self.assertTrue(self.summary["runtime"]["same_game_executable_as_D"])

    def test_generated_rebuild_changes_are_not_source_changes(self):
        result = self.summary["preparation"]
        self.assertGreater(len(result["generated_changes"]), 0)
        self.assertTrue(all(proof.generated_plugin_file(p) and not proof.protected_plugin_file(p) for p in result["generated_changes"]))
        self.assertFalse(proof.generated_plugin_file("Content/Python/operations.py"))
        self.assertTrue(proof.protected_plugin_file("Content/Python/operations.py"))

    def test_cpp_descriptor_cannot_be_treated_as_generated(self):
        for p in ("Source/Mod/Operations.cpp", "Source/Mod/Mod.Build.cs", "CubelessMCP_UE58.uplugin"):
            self.assertTrue(proof.protected_plugin_file(p)); self.assertFalse(proof.generated_plugin_file(p))

    def bad_preparation(self, mutation):
        changed = copy.deepcopy(self.preparation); mutation(changed)
        with self.assertRaises(RuntimeError): proof.preparation_evidence(changed, self.manifest, self.author)

    def test_missing_asset_and_duplicate_copy_rejected(self):
        self.bad_preparation(lambda r: r["copied_assets"].pop())
        self.bad_preparation(lambda r: r["copied_assets"].__setitem__(0, r["copied_assets"][1]))

    def test_wrong_asset_hash_and_copy_destination_rejected(self):
        self.bad_preparation(lambda r: r["copied_assets"][0].update(sha256="0" * 64))
        self.bad_preparation(lambda r: r["copied_assets"][0].update(destination=r["copied_assets"][0]["source"]))

    def test_wrong_protected_plugin_hash_rejected(self):
        self.bad_preparation(lambda r: next(x for x in r["plugin_files"] if x["relative"].endswith(".py")).update(sha256="0" * 64))

    def test_omitted_protected_file_cannot_hide_as_generated(self):
        self.bad_preparation(lambda r: next(x for x in r["plugin_files"] if x["relative"].endswith(".py")).update(relative="Intermediate/FalsePlaceholder.bin"))

    def test_actor_M_or_gameplay_claim_rejected(self):
        self.bad_preparation(lambda r: r.update(actor_M_included=True))
        self.bad_preparation(lambda r: r.update(gameplay_cpp_added=True))

    def test_relative_path_traversal_and_drive_rejected(self):
        for path in ("../Outside.cpp", "C:/Outside.cpp", "/Absolute.cpp", "Source/../Outside.cpp"):
            with self.subTest(path=path):
                with self.assertRaises(RuntimeError): proof.safe_relative(path)

    def test_editor_target_and_module_type_required(self):
        descriptor = proof.read_json(proof.PROJECT); plugin = proof.read_json(proof.PLUGIN / "CubelessMCP_UE58.uplugin")
        proof.descriptor_check(descriptor, plugin)
        a = copy.deepcopy(descriptor); a["Plugins"][2]["TargetAllowList"] = ["Editor", "Game"]
        with self.assertRaises(RuntimeError): proof.descriptor_check(a, plugin)
        b = copy.deepcopy(plugin); b["Modules"][0]["Type"] = "Runtime"
        with self.assertRaises(RuntimeError): proof.descriptor_check(descriptor, b)
        b = copy.deepcopy(plugin); b["Plugins"][0]["TargetAllowList"] = ["Game"]
        with self.assertRaises(RuntimeError): proof.descriptor_check(descriptor, b)

    def test_installed_but_disabled_is_not_this_proof(self):
        descriptor = proof.read_json(proof.PROJECT); plugin = proof.read_json(proof.PLUGIN / "CubelessMCP_UE58.uplugin")
        descriptor["Plugins"][2]["Enabled"] = False
        with self.assertRaises(RuntimeError): proof.descriptor_check(descriptor, plugin)

    def test_exact_cleanup_display_with_written_success_only(self):
        prefix = "LogShaderCompilers: Display: Failed to delete old shader autogen file: "
        name = str(proof.PROJECT.parent / "Intermediate/ShaderAutogen/PCD3D_SM5/AutogenShaderHeaders.ush")
        after = "LogShaderCompilers: Display: Shader autogen file written: " + name
        self.assertTrue(proof.cleanup_observation(prefix + name, after, self.engine))
        for before, following in [(prefix.replace("Display", "Warning") + name, after), (prefix.replace("delete old", "save") + name, after), (prefix + name, ""), (prefix + name, after + ".wrong"), (prefix + name.replace("UE58InstalledCurrentRuntimeProof", "OtherProject"), after)]:
            self.assertFalse(proof.cleanup_observation(before, following, self.engine))

    def test_cleanup_and_shader_scope_are_kept_separate(self):
        observed = self.summary["build"]
        self.assertEqual(len(observed["shader_autogen_cleanup_observations"]), 2)
        self.assertEqual(observed["shader_diagnostics"], [])
        self.assertFalse(observed["shader_completion_verified"])

    def test_wrong_stage_or_pak_hash_rejected(self):
        changed = copy.deepcopy(self.package); changed["stage"] = str(proof.prior.STAGE)
        with self.assertRaises(RuntimeError): proof.package_evidence(changed, self.manifest)
        changed = copy.deepcopy(self.package); changed["pak_listings"][0]["pak_sha256_after"] = "0" * 64
        with self.assertRaises(RuntimeError): proof.package_evidence(changed, self.manifest)

    def test_omitted_pak_membership_or_marker_forgery_rejected(self):
        changed = copy.deepcopy(self.summary); changed["package"]["membership"]["exact_entries"].pop(next(iter(changed["package"]["membership"]["exact_entries"])))
        self.assertFalse(proof.verify(changed)["passed"])
        changed = copy.deepcopy(self.summary); changed["runtime"]["marker"]["sha256"] = "0" * 64
        self.assertFalse(proof.verify(changed)["passed"])

    def test_overstated_complete_migration_or_actor_scope_rejected(self):
        for key in ("full_migration_complete", "actor_M_included", "shader_completion_verified"):
            changed = copy.deepcopy(self.summary); changed[key] = True
            self.assertFalse(proof.verify(changed)["passed"])

    def test_frozen_D_verifier_not_modified(self):
        self.assertEqual(proof.sha(Path(proof.prior.__file__)), proof.D_VERIFIER_SHA)


if __name__ == "__main__": unittest.main()
