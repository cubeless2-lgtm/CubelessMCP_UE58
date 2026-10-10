"""Offline tampering checks bound to immutable actual B author/C cold reports.

No engine calls or asset writes. Fixture identities and source/copy hashes are
checked against the real records so synthetic rows are never runtime evidence.
"""
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import current_runtime_report as check


class CurrentRuntimeReportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        records = check.contract.RECORDS
        cls.author_bytes = (records / "current-runtime-fixture-B.json").read_bytes()
        cls.author = json.loads(cls.author_bytes)
        cls.cold_bytes = (records / "current-runtime-cold-C.json").read_bytes()
        cls.cold = json.loads(cls.cold_bytes)
        cls.manifest = check.read_json(cls.author["manifest"]["path"])

    def bad(self, mutate):
        value = copy.deepcopy(self.cold); mutate(value)
        self.assertFalse(check.verify(value, self.author, self.author_bytes)["passed"])

    def test_actual_paired_evidence_passes(self):
        result = check.verify(self.cold, self.author, self.author_bytes)
        self.assertTrue(result["passed"], result)
        self.assertFalse(result["shader_completion_verified"])
        self.assertEqual(result["dependency_evidence"], "paired_fresh_process_disk_registry")

    def test_author_missing_fresh_registry_edges_remains_failed(self):
        result = check.verify(self.author)
        self.assertFalse(result["passed"])
        self.assertIn("Connected runtime clone lacks MPC hard reference", result["errors"])

    def test_cold_needs_bound_author_bytes(self):
        self.assertFalse(check.verify(self.cold)["passed"])
        self.assertFalse(check.verify(self.cold, self.author, b"{}")["passed"])

    def test_missing_clone_hard_edge_cannot_be_reconciled(self):
        clone = self.manifest["clone_specs"][0]
        self.bad(lambda r: r["dependency_audit"].update(edges=[e for e in r["dependency_audit"]["edges"] if (e["from"], e["to"]) != (clone["target"], clone["collection"])]))

    def test_dependency_query_category_overlap_rejected(self):
        self.bad(lambda r: r["dependency_audit"]["query_options"]["hard_management"].update(include_game_package_references=True))

    def test_disconnected_registry_rows_do_not_prove_reachability(self):
        value = copy.deepcopy(self.cold["dependency_audit"])
        value["packages"].append({"package": "/Game/CubelessMCPTests/Unreachable"})
        with self.assertRaisesRegex(RuntimeError, "Disconnected"):
            check.independent_dependencies(value, self.manifest, self.author["new_asset_files"])

    def test_hidden_native_mcp_reference_is_rejected(self):
        value = copy.deepcopy(self.cold["dependency_audit"])
        value["edges"].append({"from": self.manifest["default_map"], "to": "/Script/CubelessMCPUE58Editor", "category": "hard_package"})
        with self.assertRaisesRegex(RuntimeError, "Forbidden"):
            check.independent_dependencies(value, self.manifest, self.author["new_asset_files"])

    def test_required_registry_file_identity_rejected(self):
        package = self.manifest["default_map"]
        self.bad(lambda r: next(x for x in r["dependency_audit"]["packages"] if x["package"] == package).update(sha256="0" * 64))

    def test_marker_actor_path_is_preserved(self):
        value = copy.deepcopy(self.author["map_readback"])
        row = next(a for a in value["actors"] if a["label"] == self.manifest["preserved_marker_actors"][0]["label"])
        row["path"] += "_Recreated"
        with self.assertRaisesRegex(RuntimeError, "marker actor identity"):
            check.map_evidence(value, self.manifest)

    def test_arbitrary_world_path_is_not_relaxed(self):
        self.bad(lambda r: r["map_readback"]["actors"][0].update(path="/Game/Other.Other:PersistentLevel.Cube"))

    def test_wrong_cube_material_rejected(self):
        self.bad(lambda r: r["map_readback"]["cubes"][0].update(material="/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"))

    def test_disconnected_custom_root_rejected(self):
        self.bad(lambda r: r["source_readback"]["custom"].update(base_color=None))

    def test_changed_mpc_base_rejected(self):
        self.bad(lambda r: r["source_readback"]["mpc_local_chain"]["leaf"].update(base=None))

    def test_changed_material_number_rejected(self):
        def mutate(r):
            n = next(n for n in r["source_readback"]["material_0"]["nodes"] if n.get("r") == .875)
            n["r"] = 0
        self.bad(mutate)

    def test_incomplete_actor_change_log_rejected(self):
        author = copy.deepcopy(self.author)
        author["changes"] = [c for c in author["changes"] if c["kind"] != "spawn_material_cube"]
        self.assertFalse(check.verify(author, _author_dependency_reconciliation=self.cold)["passed"])

    def test_failed_component_stage_rejected(self):
        author = copy.deepcopy(self.author)
        next(c for c in author["changes"] if c["kind"] == "spawn_material_cube")["completed"] = False
        self.assertFalse(check.verify(author, _author_dependency_reconciliation=self.cold)["passed"])

    def test_missing_blueprint_compile_rejected(self):
        self.bad(lambda r: r["compile"].pop())

    def test_contradictory_blueprint_compile_rejected(self):
        for value in (False, True):
            with self.subTest(returned=value):
                self.bad(lambda r: r["compile"][0].update(returned=value, errors=[{"error": "bad compile"}]))

    def test_same_process_or_hidden_save_rejected(self):
        self.bad(lambda r: r.update(process_id=self.author["process_id"]))
        self.bad(lambda r: r["save"].update(attempted=True))

    def test_overstated_shader_or_failed_preservation_rejected(self):
        self.bad(lambda r: r.update(shader_completion_verified=True))
        self.bad(lambda r: r["preservation"]["files"][0].update(source_unchanged=False))

    def test_old_map_dependency_rejected(self):
        value = copy.deepcopy(self.cold["dependency_audit"])
        value["packages"].append({"package": self.manifest["excluded_old_map"]})
        with self.assertRaisesRegex(RuntimeError, "old-map"):
            check.independent_dependencies(value, self.manifest, self.author["new_asset_files"])


if __name__ == "__main__": unittest.main()
