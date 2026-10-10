"""Synthetic N pipeline evidence and tamper tests, with no engine/network calls."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verification import component_pipeline_report as v


def fixture():
    run = "a" * 32; root = "/Game/CubelessMCPTests/Components_" + run
    result = {"schema": "cubeless_component_read_fixture_v1", "success": True, "stage": "complete", "run_id": run,
        "root": root, "project_file": str(v.PROJECT.resolve()), "engine_version": "5.8.3-synthetic", "dirty_before": [], "dirty_after": [],
        "compile": {"success": True, "explicit_calls": 3, "results": [{"success": True, "compiled": True, "saved": False}] * 3, "implicit_engine_compilation_possible": True},
        "save": {"saved": True, "explicit_calls": 1}, "automatic_retries": 0, "atomic_rollback": False, "outcome_unknown": False,
        "status": "pass", "dirty_final": [], "preservation": {"passed": True, "changed_or_missing": []}, "save_on_compile": {"value": 0, "changed_by_fixture": False},
        "blueprints": {}, "asset_files": []}
    for role, name in zip(v.ROLES, ("BP_Local", "BP_Empty", "BP_Inherited")):
        package = root + "/" + name
        result["blueprints"][role] = {"package": package, "object_path": package + "." + name, "has_scs": role != "empty", "expected_components": [], "properties": []}
        result["asset_files"].append({"package": package, "path": str(v.PROJECT.parent / "Content" / (package[6:] + ".uasset")), "sha256": "b" * 64})
    local = result["blueprints"]["local"]
    for name, kind, parent in (("DefaultSceneRoot", "SceneComponent", ""), ("MeshChild", "StaticMeshComponent", "DefaultSceneRoot"), ("LightChild", "PointLightComponent", "MeshChild")):
        row = {"component_name": name, "variable_name": name, "component_class": kind, "component_class_path": "/Script/Engine." + kind,
            "template_name": name + "_GEN_VARIABLE", "template_path": local["package"] + ".BP_Local_C:" + name + "_GEN_VARIABLE", "parent_component_name": parent,
            "relative_transform": {"location": [12,34,56] if name == "MeshChild" else [0,0,0], "rotation": [10,20,30] if name == "MeshChild" else [0,0,0], "scale": [1.25,.75,2] if name == "MeshChild" else [1,1,1]}}
        if name == "MeshChild": row["static_mesh"] = "/Engine/BasicShapes/Cube.Cube"
        local["expected_components"].append(row)
    for (component, name), value in v.FIXED_VALUES.items():
        official = {name: value}
        if name == "Mobility": official = {name: "Movable"}
        elif name == "StaticMesh": official = {name: {"refPath": value}}
        elif name == "RelativeLocation" or name == "RelativeScale3D": official = {name: dict(zip(("x","y","z"), value))}
        elif name == "RelativeRotation": official = {name: dict(zip(("pitch","yaw","roll"), value))}
        elif name == "AttachSocketName": official = None
        local["properties"].append({"id": component + "_" + name, "component_name": component, "property_name": name, "property_type": None,
            "property_encoding": "legacy_ue_export_text_bounded_scalar_array" if name == "ComponentTags" else "legacy_typed_json",
            "declared_expected_value": copy.deepcopy(value), "independent_readback_value": copy.deepcopy(value), "official_property_baseline": official,
            "official_property_status": "not_accessible_by_generic_tool" if official is None else "observed"})
    result["blueprints"]["inherited"].update(parent_blueprint=local["object_path"], inherited_components_excluded=True)
    return result


def envelope(data, success=True):
    data = copy.deepcopy(data)
    data.update(journaled=False, full_legacy_component_parity=False, loading_side_effects_verified=False)
    data["native_outcome"] = {"success": success, "mutation_attempted": False, "compile_requested": False, "save_requested": False,
        "saved": False, "changed": False, "change_state_known": True, "loaded_during_read": False, "implicit_postload_work_possible": True,
        "dirty_before": False, "dirty_after": False, "blueprint_status_before": 0, "blueprint_status_after": 0,
        "verification_scope": "bounded local SCS and dirty/status only", "postcondition_passed": success,
        "performance": {"local_scs_nodes_visited": data.get("local_reachable_node_count", 3)}}
    return {"success": success, "changes": [], "compile": {"requested": False, "attempted": False},
        "save": {"requested": False, "attempted": False, "saved": False}, "retry": {"automatic_retry": False},
        "validation": {"independent_value_check": "not_run"}, "data": data}


def sample_report(fix, phase="read", parent=None):
    raw = json.dumps(fix).encode()
    report = {"schema": "cubeless_component_read_pipeline_v1", "status": "pass", "phase": phase, "fixture_sha256": v.sha(raw),
        "run_id": fix["run_id"], "project_file": str(v.PROJECT.resolve()), "automatic_retries": 0, "outcome_unknown": False,
        "explicit_compile_calls": 0, "explicit_save_calls": 0, "packaging": "not_run", "full_legacy_component_parity": False,
        "guid_independently_verified": False, "stages": [], "tool_calls": [], "node_guids": {}, "property_values": {},
        "asset_files_before": copy.deepcopy(fix["asset_files"]), "asset_files_after": copy.deepcopy(fix["asset_files"])}
    if parent: report["parent_sha256"] = v.sha(json.dumps(parent).encode())
    guids = dict(zip(("DefaultSceneRoot", "MeshChild", "LightChild"), ("1" * 32, "2" * 32, "3" * 32)))
    for spec in v.plan(fix):
        kind, e = spec["check"], spec["expected"]
        if kind == "editor":
            value = {"success": True, "project_file": str(v.PROJECT.resolve()), "engine_version": "5.8.3-synthetic", "game_thread": True,
                "pie_or_sie_active": False, "dirty_packages": [], "instance_id": "reload-instance" if parent else "first-instance"}
            if spec["name"] == "preflight": report["editor"] = copy.deepcopy(value)
        elif kind == "clean": value = False
        elif kind == "list":
            bp = fix["blueprints"][e["role"]]
            rows = [{**copy.deepcopy(row), "node_guid": guids[row["component_name"]]} for row in bp["expected_components"] if not e["selected"] or row["component_name"] == e["selected"]]
            value = envelope({"blueprint_path": bp["object_path"], "has_simple_construction_script": bp["has_scs"], "components": sorted(rows, key=lambda r: r["variable_name"]),
                "component_count": len(rows), "local_reachable_node_count": len(bp["expected_components"]),
                "inherited_effective_defaults_verified": False, "native_cdo_components_included": False, "orphan_allnodes_entries_inspected": False})
            if spec["name"] == "local_first": report["node_guids"] = copy.deepcopy(guids)
        elif kind in {"property", "official_property"}:
            prop = fix["blueprints"]["local"]["properties"][e["property_index"]]
            if kind == "official_property": value = copy.deepcopy(prop["official_property_baseline"])
            else:
                raw_value = "(CubelessComponentFixture,ReadOnlyProof)" if prop["property_name"] == "ComponentTags" else copy.deepcopy(prop["independent_readback_value"])
                value = envelope({"blueprint_path": fix["blueprints"]["local"]["object_path"], "component_name": prop["component_name"],
                    "property_name": prop["property_name"], "property_type": "SyntheticExpectedType", "property_encoding": prop["property_encoding"], "property_value": raw_value})
                report["property_values"][prop["id"]] = raw_value
        else:
            value = envelope({}, success=False); value.update(error_code=e["code"], error="Expected synthetic rejection")
        raw = {"structuredContent": {"returnValue": json.dumps(value) if kind == "official_property" else copy.deepcopy(value)}}
        call = {"toolset": "fake." + spec["toolset_suffix"], "tool": spec["tool"], "arguments": spec["arguments"], "mutation": False, "raw": raw, "decoded": copy.deepcopy(value)}
        report["stages"].append({"name": spec["name"], "passed": True, "call_index": len(report["tool_calls"]), "result": copy.deepcopy(value)})
        report["tool_calls"].append(call)
    return report


class VerifierTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixture(); self.raw = json.dumps(self.fixture).encode(); self.report = sample_report(self.fixture)

    def check(self, report=None, **kwargs): return v.verify(report or self.report, fixture=self.fixture, fixture_bytes=self.raw, check_files=False, **kwargs)

    def edit(self, stage, action):
        row = next(r for r in self.report["stages"] if r["name"] == stage); action(row["result"])
        call = self.report["tool_calls"][row["call_index"]]; call["decoded"] = copy.deepcopy(row["result"])
        call["raw"]["structuredContent"]["returnValue"] = copy.deepcopy(row["result"])

    def test_positive_first_and_new_process_cold(self):
        self.assertTrue(self.check()["passed"], self.check())
        cold = sample_report(self.fixture, "reload", self.report)
        self.assertTrue(self.check(cold, parent=self.report, parent_bytes=json.dumps(self.report).encode())["passed"])

    def test_named_tools_only_and_zero_compile_save(self):
        tools = {r["tool"] for r in v.plan(self.fixture)}
        self.assertEqual(tools, {"GetEditorDiagnostics", "list_blueprint_components", "get_component_property", "get_properties", "is_dirty"})
        self.assertEqual(len(v.plan(self.fixture)), 51)

    def test_raw_cannot_be_replaced_by_cached_success(self):
        self.report["tool_calls"][1]["raw"]["isError"] = True
        self.assertFalse(self.check()["passed"])

    def test_call_arguments_and_readonly_provenance_bound(self):
        self.report["tool_calls"][1]["arguments"]["blueprint_name"] = "/Game/Other"
        self.assertFalse(self.check()["passed"])

    def test_wrong_parent_or_mesh_transform_fails(self):
        self.edit("local_first", lambda value: value["data"]["components"][1].update(parent_component_name="Wrong"))
        self.assertFalse(self.check()["passed"])

    def test_filtered_list_cannot_hide_traversal_count(self):
        self.edit("local_exact_filter", lambda value: value["data"].update(local_reachable_node_count=1))
        self.assertFalse(self.check()["passed"])

    def test_no_inherited_cdo_or_guid_independent_claim(self):
        self.edit("inherited_scope", lambda value: value["data"].update(inherited_effective_defaults_verified=True))
        self.assertFalse(self.check()["passed"])

    def test_dirty_read_failure_cannot_pass(self):
        self.edit("local_repeat", lambda value: value["data"]["native_outcome"].update(dirty_after=True))
        self.assertFalse(self.check()["passed"])

    def test_unknown_native_read_cannot_pass(self):
        self.edit("local_repeat", lambda value: value["data"]["native_outcome"].update(change_state_known=False))
        self.assertFalse(self.check()["passed"])

    def test_unexpected_save_flag_fails(self):
        self.edit("local_repeat", lambda value: value["save"].update(saved=True))
        self.assertFalse(self.check()["passed"])

    def test_property_number_bool_confusion_fails(self):
        index = next(i for i, p in enumerate(self.fixture["blueprints"]["local"]["properties"]) if p["property_name"] == "bVisible")
        self.edit("property_" + str(index), lambda value: value["data"].update(property_value=0))
        self.assertFalse(self.check()["passed"])

    def test_legacy_array_json_array_is_not_equal_to_text(self):
        index = next(i for i, p in enumerate(self.fixture["blueprints"]["local"]["properties"]) if p["property_name"] == "ComponentTags")
        self.edit("property_" + str(index), lambda value: value["data"].update(property_value=["CubelessComponentFixture", "ReadOnlyProof"]))
        self.assertFalse(self.check()["passed"])

    def test_malformed_legacy_array_quote_is_not_a_valid_text_encoding(self):
        index = next(i for i, p in enumerate(self.fixture["blueprints"]["local"]["properties"]) if p["property_name"] == "ComponentTags")
        self.edit("property_" + str(index), lambda value: value["data"].update(property_value='("CubelessComponentFixture,ReadOnlyProof)'))
        self.assertFalse(self.check()["passed"])

    def test_independent_official_property_difference_fails(self):
        self.edit("official_0", lambda value: value.update(RelativeLocation={"x": 0, "y": 0, "z": 0}))
        self.assertFalse(self.check()["passed"])

    def test_missing_property_cannot_accept_unrelated_error(self):
        self.edit("negative_missing_property", lambda value: value.update(error_code="transport_failure"))
        self.assertFalse(self.check()["passed"])

    def test_guids_must_remain_stable_after_negative(self):
        self.edit("after_node_budget", lambda value: value["data"]["components"][0].update(node_guid="f" * 32))
        self.assertFalse(self.check()["passed"])

    def test_manifest_path_escape_and_coherent_false_expected_value_fail(self):
        self.fixture["asset_files"][0]["path"] = str(v.ROOT / "UserAsset.uasset")
        with self.assertRaises(AssertionError): v.fixture_contract(self.fixture)
        fix = fixture(); fix["blueprints"]["local"]["properties"][0].update(declared_expected_value=[0,0,0], independent_readback_value=[0,0,0])
        with self.assertRaises(AssertionError): v.fixture_contract(fix)

    def test_cold_requires_new_process_and_exact_bytes(self):
        cold = sample_report(self.fixture, "reload", self.report)
        cold["asset_files_after"][0]["sha256"] = "c" * 64
        self.assertFalse(self.check(cold, parent=self.report, parent_bytes=json.dumps(self.report).encode())["passed"])
        self.report["phase"] = "reload"; self.assertFalse(self.check()["passed"])

    def test_same_editor_cannot_supply_cold_proof(self):
        cold = sample_report(self.fixture, "reload", self.report)
        for row in cold["stages"]:
            if row["name"] in {"preflight", "final_editor"}:
                row["result"]["instance_id"] = self.report["editor"]["instance_id"]
                call = cold["tool_calls"][row["call_index"]]
                call["decoded"] = copy.deepcopy(row["result"]); call["raw"]["structuredContent"]["returnValue"] = copy.deepcopy(row["result"])
        cold["editor"] = copy.deepcopy(self.report["editor"])
        self.assertFalse(self.check(cold, parent=self.report, parent_bytes=json.dumps(self.report).encode())["passed"])

    def test_fixture_implicit_save_and_preservation_claims_cannot_be_ignored(self):
        for change in (lambda f: f["save_on_compile"].update(value=1),
                       lambda f: f["compile"]["results"][0].update(saved=True),
                       lambda f: f["preservation"].update(changed_or_missing=["ExistingUserAsset"]),
                       lambda f: f.update(dirty_final=["/Game/Unexpected"])):
            fix = fixture(); change(fix)
            with self.assertRaises(AssertionError): v.fixture_contract(fix)

    def test_final_disk_observation_cannot_differ_from_earlier_snapshot(self):
        self.report["asset_files_final_observation"] = copy.deepcopy(self.report["asset_files_after"])
        self.report["asset_files_final_observation"][0]["sha256"] = "d" * 64
        self.assertFalse(self.check()["passed"])

    def test_drop_stage_and_false_scope_summary_fail(self):
        self.report["stages"].pop()
        self.assertFalse(self.check()["passed"])
        self.report = sample_report(self.fixture); self.report["guid_independently_verified"] = True
        self.assertFalse(self.check()["passed"])


class ExternalPipelineTests(unittest.TestCase):
    def run_pipeline(self, tamper=None):
        from pipelines import component_read_smoke as pipeline
        with tempfile.TemporaryDirectory(prefix="cubeless-component-pipeline-") as directory:
            project = Path(directory) / "CubelessMCPVerification.uproject"
            with patch.object(v, "PROJECT", project), patch.object(pipeline, "PROJECT", project):
                fix = fixture()
                for row in fix["asset_files"]:
                    path = Path(row["path"]); path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(b"synthetic asset bytes, never passed to UE")
                    row["sha256"] = v.sha(path.read_bytes())
                source = Path(directory) / "fixture.json"; source.write_text(json.dumps(fix), encoding="utf-8")
                expected = sample_report(fix)
                if tamper: tamper(expected)
                schemas = {}
                for spec in v.plan(fix):
                    name = "fake." + spec["toolset_suffix"]
                    methods = schemas.setdefault(name, {})
                    row = methods.setdefault(spec["tool"], {"name": spec["tool"], "inputSchema": {"properties": {}, "required": []}})
                    row["inputSchema"]["properties"].update({k: {} for k in spec["arguments"]})
                class FakeClient:
                    closed = False
                    def __init__(self, *args): self.observations = []; self.index = 0
                    def initialize(self): return {"protocolVersion": "synthetic"}
                    def list_tools(self): return {"tools": []}
                    def call(self, name, arguments=None):
                        value = "\n".join("- " + key + ": synthetic" for key in schemas) if name == "list_toolsets" else {"tools": list(schemas[arguments["toolset_name"]].values())}
                        return {"structuredContent": {"returnValue": value}}
                    def call_toolset(self, name, tool, arguments, mutation=False):
                        call = expected["tool_calls"][self.index]; self.index += 1
                        assert (name, tool, arguments, mutation) == (call["toolset"], call["tool"], call["arguments"], False)
                        return copy.deepcopy(call["raw"])
                    def close(self): type(self).closed = True
                output = Path(directory) / "result.json"
                with patch.object(pipeline, "OfficialMCPClient", FakeClient):
                    code = pipeline.main(["--endpoint", "http://never-called.invalid", "--expected-project", str(project), "--phase", "read", "--fixture-report", str(source), "--output", str(output)])
                return code, json.loads(output.read_text(encoding="utf-8")), FakeClient.closed

    def test_actual_external_source_success_and_finally_session_close(self):
        code, report, closed = self.run_pipeline()
        self.assertEqual(code, 0, report.get("error")); self.assertEqual(len(report["stages"]), 51)
        self.assertTrue(closed); self.assertEqual(report["asset_files_before"], report["asset_files_final_observation"])

    def test_unexpected_negative_gets_one_readback_then_stops_and_closes(self):
        def edit(report):
            row = next(r for r in report["stages"] if r["name"] == "negative_missing_component")
            call = report["tool_calls"][row["call_index"]]
            call["raw"]["structuredContent"]["returnValue"]["success"] = True
        code, report, closed = self.run_pipeline(edit)
        self.assertEqual(code, 1); self.assertEqual(report["stages"][-1]["name"], "after_missing_component")
        self.assertTrue(closed); self.assertIn("actual list readback retained", report["error"])

    def test_uncertain_native_read_stops_and_preserves_post_disk_observation(self):
        def edit(report):
            report["tool_calls"][1]["raw"]["structuredContent"]["returnValue"]["data"]["native_outcome"]["change_state_known"] = False
        code, report, closed = self.run_pipeline(edit)
        self.assertEqual(code, 1); self.assertEqual(len(report["stages"]), 2)
        self.assertTrue(report["outcome_unknown"]); self.assertTrue(closed)
        self.assertEqual(report["asset_files_before"], report["asset_files_final_observation"])


if __name__ == "__main__": unittest.main()
