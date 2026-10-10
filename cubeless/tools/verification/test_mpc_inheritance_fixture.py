"""Offline counterexamples for the fixture's inherited-value assertions."""
import copy
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

PATH = Path(__file__).resolve().parents[1] / "pipelines/mpc_inheritance_smoke.py"
SPEC = importlib.util.spec_from_file_location("mpc_inheritance_fixture", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class InheritanceReportTests(unittest.TestCase):
    def setUp(self):
        self.paths = {name: "/Game/Test/" + name + "." + name for name in ("base", "mid", "leaf")}
        self.result = {"success": True, "data": {"collection_semantics_version": "ue58_effective_collection_v1",
            "base_chain": [{"collection": self.paths[k]} for k in ("leaf", "mid", "base")],
            "include_runtime": False, "has_runtime_instance": False,
            "scalars": [{"name": "RootScalar", "id": "11111111-1111-1111-1111-111111111111", "asset_default": 0.25,
                         "declared_in": self.paths["base"], "inherited": True, "runtime_resolved": False}], "vectors": []}}

    def check(self, value):
        return MODULE.validate_values(value, "leaf", self.paths, ["RootScalar"])

    def test_exact_inherited_value_passes(self):
        self.check(self.result)

    def test_legacy_local_only_omission_fails(self):
        self.result["data"]["scalars"] = []
        with self.assertRaises(AssertionError): self.check(self.result)

    def test_inherited_owner_misreported_as_leaf_fails(self):
        self.result["data"]["scalars"][0].update(declared_in=self.paths["leaf"], inherited=False)
        with self.assertRaises(AssertionError): self.check(self.result)

    def test_wrong_effective_default_fails(self):
        self.result["data"]["scalars"][0]["asset_default"] = 0.5
        with self.assertRaises(AssertionError): self.check(self.result)

    def test_duplicate_parameter_fails(self):
        self.result["data"]["scalars"].append(copy.deepcopy(self.result["data"]["scalars"][0]))
        with self.assertRaises(AssertionError): self.check(self.result)

    def test_zero_guid_fails(self):
        self.result["data"]["scalars"][0]["id"] = "00000000-0000-0000-0000-000000000000"
        with self.assertRaises(AssertionError): self.check(self.result)

    def test_runtime_read_while_disabled_fails(self):
        self.result["data"]["scalars"][0].update(runtime_resolved=True, runtime_value=0.25)
        with self.assertRaises(AssertionError): self.check(self.result)

    def test_unknown_native_failure_never_passes(self):
        self.result["success"] = False
        with self.assertRaises(AssertionError): self.check(self.result)

    def test_real_user_project_rejected_before_client_construction(self):
        with patch.object(MODULE, "OfficialMCPClient") as client:
            with self.assertRaisesRegex(AssertionError, "restricted"):
                MODULE.main(["--endpoint", "http://unused.invalid", "--expected-project", str(MODULE.PROJECT_ROOT / "UnrealMCP5_8_3.uproject"),
                             "--phase", "author", "--output", str(MODULE.PROJECT_ROOT / "never-created-guard-test.json")])
            client.assert_not_called()

    def test_correct_isolated_project_passes_gate_without_network(self):
        class ReachedClient(RuntimeError): pass
        with patch.object(MODULE, "OfficialMCPClient", side_effect=ReachedClient) as client:
            with self.assertRaises(ReachedClient):
                MODULE.main(["--endpoint", "http://unused.invalid", "--expected-project", str(MODULE.ALLOWED_PROJECT),
                             "--phase", "author", "--output", str(MODULE.PROJECT_ROOT / "never-created-guard-test.json")])
            client.assert_called_once()

    def test_all_four_asset_files_are_inside_isolated_content(self):
        for name in ("MPC_Base", "MPC_Mid", "MPC_Leaf", "M_Inheritance"):
            actual = MODULE.fixture_asset_path("/Game/CubelessMCPTests/MPCInheritance_" + "a" * 32 + "/" + name)
            self.assertTrue(actual.is_relative_to((MODULE.ALLOWED_PROJECT.parent / "Content").resolve()))
            self.assertFalse(actual.is_relative_to((MODULE.PROJECT_ROOT / "Content").resolve()))

    def test_package_traversal_and_user_asset_rejected(self):
        for package in ("/Game/UserMaterial", "/Game/CubelessMCPTests/MPCInheritance_" + "a" * 32 + "/../MPC_Base"):
            with self.assertRaises(AssertionError): MODULE.fixture_asset_path(package)


if __name__ == "__main__":
    unittest.main()
