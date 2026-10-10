"""Positive regression: bounded FName guards must not cap ordinary FString Code/Description.

Authors one new owned Material, saves once, and supports a separate cold read.
No process control or automatic retry; no full Custom/shader/rendering claim.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import operation_result
from verification.material_add_set_edges_report import field, mutation

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
NATIVE = "CubelessMCPUE58Editor.CubelessNativeToolset"
MATERIAL = "cubeless_mcp_ue58.registration.material_toolset.CubelessMaterialTools"
OFFICIAL = "editor_toolset.toolsets.material.MaterialTools"
ASSETS = "editor_toolset.toolsets.asset.AssetTools"
OBJECTS = "editor_toolset.toolsets.object.ObjectTools"
CODE_A = "/*" + "A" * 2048 + "*/\nreturn float3(0.1,0.2,0.3);"
CODE_B = "/*" + "B" * 3072 + "*/\nreturn float3(0.2,0.3,0.4);"
DESCRIPTION = "D" * 2048


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", required=True)
    p.add_argument("--phase", choices=["author", "reload"], required=True)
    p.add_argument("--parent-report", type=Path)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args(); assert not a.output.exists()
    parent = json.loads(a.parent_report.read_text(encoding="utf-8")) if a.parent_report else None
    assert a.phase != "reload" or parent and parent["status"] == "pass" and parent["phase"] == "author"
    run = parent["run_id"] if parent else uuid.uuid4().hex
    folder = "/Game/CubelessMCPTests/LongString_" + run
    package = folder + "/M_LongString"; obj = package + ".M_LongString"
    disk = PROJECT.parent / ("Content/" + package[len("/Game/"):] + ".uasset")
    report = {"schema": "cubeless_material_long_fstring_v1", "status": "fail", "phase": a.phase,
              "run_id": run, "package": package, "project_file": str(PROJECT), "stages": [],
              "automatic_retries": 0, "outcome_unknown": False, "save_calls": 0,
              "compile_calls": 0, "shader_completion_verified": False, "packaging": "not_run"}
    if parent:
        report.update(node=parent["node"], selector=parent["selector"], parent_sha256=hashlib.sha256(a.parent_report.read_bytes()).hexdigest())
        assert hashlib.sha256(disk.read_bytes()).hexdigest() == parent["asset_sha256"]
    c = OfficialMCPClient(a.endpoint)
    def persist(): a.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    def call(label, toolset, tool, args, mut=False):
        row = {"name": label, "toolset": toolset, "tool": tool, "arguments": args, "mutation": mut}
        report["stages"].append(row); persist()
        try:
            row["raw"] = c.call_toolset(toolset, tool, args, mutation=mut); persist()
            row["result"] = operation_result(row["raw"]); return row["result"]
        except Exception:
            if mut: report["outcome_unknown"] = True
            raise
    def properties(label, expected):
        value = call(label, OBJECTS, "get_properties", {"instance": {"refPath": report["node"]}, "properties": ["Code", "Description", "Desc"]})
        assert field(value, "Code") == expected and field(value, "Description") == DESCRIPTION and field(value, "Desc") == DESCRIPTION
    try:
        persist(); c.initialize()
        report["schemas"] = {n: tool_payload(c.call("describe_toolset", {"toolset_name": n})) for n in [NATIVE, MATERIAL, OFFICIAL, ASSETS, OBJECTS]}
        diag = call("preflight", NATIVE, "GetEditorDiagnostics", {})
        assert Path(diag["project_file"]).resolve() == PROJECT.resolve() and diag["dirty_packages"] == [] and not diag["pie_or_sie_active"]
        report["editor"] = diag
        if parent:
            assert diag["instance_id"] != parent["editor"]["instance_id"], "Cold proof requires a new process"
            properties("cold_properties", CODE_B)
        else:
            assert call("absent", ASSETS, "exists", {"path": package}) is False
            assert call("create", OFFICIAL, "create_material", {"folder_path": folder, "asset_name": "M_LongString"}, True) == {"refPath": obj}
            added = call("add_long_strings", MATERIAL, "add_custom_material_node", {"material_path": package, "code": CODE_A, "description": DESCRIPTION, "operation_id": run + "_add"}, True)
            mutation(added); report.update(node=added["data"]["object_path"], selector=added["data"]["node_key"])
            properties("added_properties", CODE_A)
            changed = call("set_long_code", MATERIAL, "set_material_node_property", {"material_path": package, "node_id": report["selector"], "property_name": "Code", "value_json": json.dumps(CODE_B), "operation_id": run + "_set"}, True)
            mutation(changed); properties("changed_properties", CODE_B)
            linked = call("connect", MATERIAL, "connect_material_property", {"material_path": package, "from_node_id": report["selector"], "property": "BaseColor", "operation_id": run + "_link"}, True)
            mutation(linked)
            report["compile_calls"] = 1; errors = call("compile", OFFICIAL, "recompile", {"material_or_function": {"refPath": obj}}, True)
            # Official MaterialTools returns void, raising if the native errors array is nonempty.
            assert errors is None, errors
            report["save_calls"] = 1; assert call("save", ASSETS, "save_assets", {"asset_paths": [package]}, True) is True
            properties("saved_properties", CODE_B)
        assert call("clean_asset", ASSETS, "is_dirty", {"asset_path": package}) is False
        assert call("final_clean", NATIVE, "GetEditorDiagnostics", {})["dirty_packages"] == []
        report["asset_sha256"] = hashlib.sha256(disk.read_bytes()).hexdigest()
        if parent: assert report["asset_sha256"] == parent["asset_sha256"]
        report["status"] = "pass"
    except Exception as e: report["error"] = str(e)
    finally:
        report["calls"] = c.observations; persist(); c.close()
    print(json.dumps({"status": report["status"], "error": report.get("error"), "stages": len(report["stages"])}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__": raise SystemExit(main())
