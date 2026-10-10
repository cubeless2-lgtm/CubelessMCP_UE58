"""Offline tampering checks against immutable, real UE58 I author evidence.

Reload records derived here are synthetic validator fixtures, never engine proof.
No Unreal process, HTTP endpoint or existing asset is modified.
"""
import copy
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verification import mpc_inheritance_report as verifier

RECORDS = Path(__file__).resolve().parents[2] / "records/verification/implementation-2026-10-10"
AUTHOR = RECORDS / "mpc-inheritance-author-I.json"
AUTHOR_SHA256 = "9b6d34011cd629049249abe75300e2de66b26ffb12bd00824c851f96ab16dd52"


def stage(report, name):
    return next(row for row in report["stages"] if row["name"] == name)["result"]


def effective(report, index):
    return [row["result"] for row in report["stages"] if row["name"].startswith("effective_")][index]


def sync_effective(report, index, change):
    """Change every redundant copy, so an expected-value check must detect it."""
    data = effective(report, index)["data"]
    for target in (data, data["before"], data["after"]):
        change(target)
    if index == 2:
        report["effective_leaf_manifest"] = copy.deepcopy({k: data[k] for k in ("collection", "scalars", "vectors", "base_chain")})


def sync_nodes(report, change):
    data = stage(report, "inherited_collection_nodes")["data"]
    for target in (data, data["before"]["effective_collection_nodes"], data["after"]["effective_collection_nodes"]):
        change(target)


def synthetic_reload(author):
    value = copy.deepcopy(author)
    value.update(phase="reload", save={"attempts": 0, "saved": False}, reload="passed")
    value["editor"]["instance_id"] = "synthetic-new-process-for-validator-test"
    rows = [copy.deepcopy(row) for row in author["stages"] if not row["mutation"] and not row["name"].startswith("absent_")]
    rows[0]["result"] = copy.deepcopy(value["editor"])
    for index, row in enumerate(rows):
        if row["name"].startswith("effective_"):
            row["name"] = row["name"].rsplit("_", 1)[0] + "_" + str(index)
    value["stages"] = rows
    raw = json.dumps(author).encode()
    value["parent_report_sha256"] = hashlib.sha256(raw).hexdigest()
    return value, raw


class InheritanceReportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = AUTHOR.read_bytes()
        if hashlib.sha256(raw).hexdigest() != AUTHOR_SHA256:
            raise AssertionError("Immutable I author regression evidence changed")
        cls.baseline = json.loads(raw)

    def setUp(self):
        self.report = copy.deepcopy(self.baseline)

    def fails(self, value=None, **kwargs):
        result = verifier.verify(value if value is not None else self.report, check_files=False, **kwargs)
        self.assertEqual(result["status"], "fail", "Tampered evidence was accepted")
        self.assertTrue(result["errors"])

    def test_actual_author_and_synthetic_cold_report(self):
        self.assertEqual(verifier.verify(self.report, check_files=False)["status"], "pass")
        reload, raw = synthetic_reload(self.report)
        self.assertEqual(verifier.verify(reload, self.report, raw, check_files=False)["status"], "pass")

    def test_partial_actual_g2_cannot_pass(self):
        partial = json.loads((RECORDS / "mpc-inheritance-author-G2.json").read_bytes())
        self.fails(partial)
        partial["status"] = "pass"
        self.fails(partial)

    def test_type_partition_even_when_all_duplicate_snapshots_agree(self):
        def swap(data):
            data["vectors"].append(data["scalars"].pop(0))
            data["scalar_count"] -= 1; data["vector_count"] += 1
        sync_effective(self.report, 2, swap)
        self.fails()

    def test_defaults_types_filter_and_scope(self):
        changes = [
            (2, lambda d: d["scalars"][0].update(asset_default=True)),
            (2, lambda d: d["vectors"][0]["asset_default"].update(r=.99)),
            (2, lambda d: d.update(include_asset_defaults=False)),
            (2, lambda d: d.update(effective_parameter_count_before_filter=3)),
            (2, lambda d: d.update(scalar_count=88)),
            (2, lambda d: d.update(rendered_runtime_verified=True)),
            (3, lambda d: d["scalars"][0].update(asset_default=.25)),
            (3, lambda d: d.update(include_asset_defaults=True)),
        ]
        for index, change in changes:
            with self.subTest(index=index, change=change):
                self.report = copy.deepcopy(self.baseline)
                sync_effective(self.report, index, change); self.fails()
        filtered = stage(self.report, "filter_inherited_scalar")["data"]
        filtered["scalars"] = []
        self.fails()

    def test_chain_origin_and_guid(self):
        changes = [lambda d: d["base_chain"].reverse(),
                   lambda d: d["base_chain"][0].update(local_scalar_count=1),
                   lambda d: d["scalars"][0].update(declared_in=self.baseline["objects"]["leaf"]),
                   lambda d: d["scalars"][0].update(inherited=False),
                   lambda d: d["scalars"][0].update(id="00000000-0000-0000-0000-000000000000"),
                   lambda d: d["scalars"][0].update(id=d["vectors"][0]["id"]),
                   lambda d: d["scalars"][0].update(runtime_resolved=True, runtime_value=.25)]
        for change in changes:
            with self.subTest(change=change):
                self.report = copy.deepcopy(self.baseline); sync_effective(self.report, 2, change); self.fails()

    def test_independent_leaf_readback_binding(self):
        for change in (lambda r: r["VectorParameters"][0]["defaultValue"].update(r=.9),
                       lambda r: r["VectorParameters"][0].update(id="12345678-1234-1234-1234-123456789012"),
                       lambda r: r.update(Base={"refPath": self.baseline["objects"]["base"]}),
                       lambda r: r.update(VectorParameters=[]),
                       lambda r: r.update(vector_parameters=r["VectorParameters"])):
            with self.subTest(change=change):
                self.report = copy.deepcopy(self.baseline); change(stage(self.report, "independent_leaf_properties")); self.fails()

    def test_expression_collection_reference_type_guid_and_counts(self):
        changes = [lambda d: d["nodes"][0].update(collection=self.baseline["objects"]["mid"]),
                   lambda d: d["nodes"][0].update(collection_parameter_type="vector"),
                   lambda d: d["nodes"][0].update(collection_parameter_found=False),
                   lambda d: d["nodes"][0].update(parameter_id=d["nodes"][1]["parameter_id"]),
                   lambda d: d["nodes"][0].update(base_chain=[]),
                   lambda d: d.update(mismatched_id_count=1),
                   lambda d: d.update(material="/Game/Other.Other")]
        for change in changes:
            with self.subTest(change=change):
                self.report = copy.deepcopy(self.baseline); sync_nodes(self.report, change); self.fails()

    def test_metadata_scope_and_collection_match(self):
        for change in (lambda d: d.update(effective_values_verified=True),
                       lambda d: d.update(rendered_runtime_verified=True),
                       lambda d: d.update(parameter_ids_verified=True),
                       lambda d: d["collections"][0].update(collection=self.baseline["objects"]["base"]),
                       lambda d: d["collections"][0].update(effective_parameter_count=2),
                       lambda d: d.update(unique_chain_parameter_count=2)):
            with self.subTest(change=change):
                self.report = copy.deepcopy(self.baseline)
                data = stage(self.report, "list_material_nodes_metadata")["data"]
                for d in (data, data["before"], data["after"]): change(d["ue58_collection_semantics"])
                self.fails()

    def test_read_contradictions_are_not_passes(self):
        changes = [lambda r: r.update(changes=[{"changed": True}]),
                   lambda r: r["compile"].update(requested=True),
                   lambda r: r["save"].update(saved=True),
                   lambda r: r["data"]["native_outcome"].update(changed=True),
                   lambda r: r["data"]["native_outcome"].update(mutation_attempted=True),
                   lambda r: r["data"]["native_outcome"].update(dirty_after=False),
                   lambda r: r["validation"].update(native_postcondition_passed=False),
                   lambda r: r["data"]["after"].update(scalar_count=90)]
        for change in changes:
            with self.subTest(change=change):
                self.report = copy.deepcopy(self.baseline); change(effective(self.report, 0)); self.fails()

    def test_failed_missing_reordered_mutating_stages(self):
        for change in (lambda r: r["stages"].pop(4), lambda r: r["stages"].reverse(),
                       lambda r: r["stages"][4].update(passed=False), lambda r: r["stages"][0].update(mutation=True),
                       lambda r: r.update(outcome_unknown=True), lambda r: r.update(automatic_retries=1),
                       lambda r: r["save"].update(attempts=2), lambda r: r["coverage"].update(packaging="passed")):
            with self.subTest(change=change):
                self.report = copy.deepcopy(self.baseline); change(self.report); self.fails()

    def test_parent_bytes_and_process_are_bound(self):
        reload, raw = synthetic_reload(self.report)
        unrelated = b'{"unrelated":true}'
        reload["parent_report_sha256"] = hashlib.sha256(unrelated).hexdigest()
        self.fails(reload, parent=self.report, parent_bytes=unrelated)
        reload, raw = synthetic_reload(self.report)
        reload["editor"]["instance_id"] = self.report["editor"]["instance_id"]
        reload["stages"][0]["result"] = copy.deepcopy(reload["editor"])
        self.fails(reload, parent=self.report, parent_bytes=raw)
        reload, raw = synthetic_reload(self.report)
        self.fails(reload, parent=self.report, parent_bytes=None)
        reload["phase"] = "reload"
        self.fails(reload, parent=reload, parent_bytes=json.dumps(reload).encode())

    def test_disk_hashes_are_checked_without_touching_real_assets(self):
        with tempfile.TemporaryDirectory(prefix="cubeless-mpc-validator-") as temp:
            project = Path(temp) / "Fixture.uproject"
            self.report["project_file"] = str(project)
            self.report["editor"]["project_file"] = str(project)
            self.report["stages"][0]["result"] = copy.deepcopy(self.report["editor"])
            for key, package in self.report["packages"].items():
                path = project.parent / "Content" / (package[len("/Game/"):] + ".uasset")
                path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(key.encode())
                self.report["asset_files"][key] = {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
            with patch.object(verifier, "PROJECT", project):
                self.assertEqual(verifier.verify(self.report)["status"], "pass")
                path.write_bytes(b"changed temporary fixture")
                self.assertEqual(verifier.verify(self.report)["status"], "fail")
                self.report["asset_files"]["base"]["sha256"] = "not-sha256"
                self.fails()


if __name__ == "__main__":
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(InheritanceReportTests))
    record = {"schema": "cubeless_mpc_inheritance_verifier_tests_v1", "passed": result.wasSuccessful(), "tests_run": result.testsRun, "author_evidence": str(AUTHOR), "author_sha256": AUTHOR_SHA256, "scope": "Offline author-report tampering and synthetic cold-phase checks; real reload is not proved by these tests", "details": stream.getvalue()}
    if len(sys.argv) > 1:
        with Path(sys.argv[1]).open("x", encoding="utf-8") as output: json.dump(record, output, indent=2)
    print(json.dumps(record)); raise SystemExit(0 if result.wasSuccessful() else 1)
