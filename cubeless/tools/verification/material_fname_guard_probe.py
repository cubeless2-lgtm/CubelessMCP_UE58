"""Exercise bounded L guards only after matching the exact guarded DLL on disk.

Uses existing owned fixtures for rejected requests and independent readback.
Does not send these source-confirmed fatal inputs to older builds.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient
from pipelines.blueprint_mutation_smoke import operation_result

GUARDED_DLL = "ebc15581509c6d91f5042dd46fa578fcdf4491a181346f411e0de2c28007d504"
NATIVE = "CubelessMCPUE58Editor.CubelessNativeToolset"
MATERIAL = "cubeless_mcp_ue58.registration.material_toolset.CubelessMaterialTools"
OBJECTS = "editor_toolset.toolsets.object.ObjectTools"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--expected-pid", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    records = root / "cubeless/records/verification/implementation-2026-10-10"
    project = root / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
    dll = project.parent / "Plugins/CubelessMCP_UE58/Binaries/Win64/UnrealEditor-CubelessMCPUE58Editor.dll"
    assert not args.output.exists()
    assert hashlib.sha256(dll.read_bytes()).hexdigest() == GUARDED_DLL, "Guarded L DLL is required"
    read = lambda name: json.loads((records / name).read_text(encoding="utf-8"))
    custom, material = read("custom-material-author-K.json"), read("material-author-J.json")
    assert custom["status"] == material["status"] == "pass"
    client = OfficialMCPClient(args.endpoint)
    report = {"schema": "cubeless_fname_guard_probe_v1", "status": "fail", "stages": [],
              "guarded_dll_sha256": GUARDED_DLL, "expected_pid": args.expected_pid,
              "automatic_retries": 0, "outcome_unknown": False, "expected_asset_changes": False}
    run = uuid.uuid4().hex

    def checkpoint():
        args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

    def call(label, toolset, tool, params, mutation=False):
        stage = {"name": label, "toolset": toolset, "tool": tool, "arguments": params, "mutation": mutation}
        report["stages"].append(stage)
        checkpoint()
        try:
            stage["raw"] = client.call_toolset(toolset, tool, params, mutation=mutation)
            checkpoint()
            stage["result"] = operation_result(stage["raw"])
            return stage["result"]
        except Exception:
            if mutation: report["outcome_unknown"] = True
            raise

    def snapshot(label):
        output = {}
        for kind, package, node, properties in [
            ("custom", custom["material_package"], custom["nodes"]["default"], ["Code", "Inputs", "AdditionalDefines", "Description"]),
            ("collection", material["assets"]["material"], material["fixture"]["nodes"]["collection"], ["ParameterName", "Collection"]),
        ]:
            output[kind] = {"properties": call(label + "_" + kind, OBJECTS, "get_properties", {"instance": {"refPath": node}, "properties": properties}),
                            "nodes": call(label + "_" + kind + "_graph", MATERIAL, "list_material_nodes", {"material_path": package})["data"]["nodes"]}
        return output

    try:
        client.initialize()
        diag = call("preflight", NATIVE, "GetEditorDiagnostics", {})
        assert diag["process_id"] == args.expected_pid and Path(diag["project_file"]).resolve() == project.resolve()
        assert diag["dirty_packages"] == [] and not diag["pie_or_sie_active"]
        report["schemas"] = {n: client.call("describe_toolset", {"toolset_name": n}) for n in (NATIVE, MATERIAL, OBJECTS)}
        before = snapshot("before")
        long_name = "X" * 1024
        aggregate = "(" + ",".join('(InputName="Name%d")' % i for i in range(60)) + ")"
        assert len(aggregate) >= 1024
        cases = [
            ("property_name", "Custom", custom["material_package"], custom["selectors"]["default"], long_name, "value", "property name"),
            ("direct_fname", "CollectionParameter", material["assets"]["material"], material["fixture"]["selectors"]["collection"], "ParameterName", long_name, "fname value"),
            ("nested_fname", "Custom", custom["material_package"], custom["selectors"]["default"], "Inputs", '((InputName="' + long_name + '"))', "raw property import"),
            ("long_valid_aggregate", "Custom", custom["material_package"], custom["selectors"]["default"], "Inputs", aggregate, "raw property import"),
        ]
        for mode in ("add", "set"):
            for key, cls, package, selector, prop, value, reason in cases:
                label = mode + "_" + key
                params = {"material_path": package, "operation_id": run + "_" + label}
                if mode == "add":
                    params.update(expression_class=cls, properties_json=json.dumps({prop: value}))
                    tool = "add_material_node"
                else:
                    params.update(node_id=selector, property_name=prop, value_json=json.dumps(value))
                    tool = "set_material_node_property"
                result = call(label, MATERIAL, tool, params, True)
                after = snapshot(label + "_after")
                assert after == before, "Rejected input changed native properties/graph"
                assert result["success"] is False and result["error_code"] == "material_mutation_target_rejected"
                outcome = result["data"]["native_outcome"]
                assert outcome["mutation_attempted"] is False and outcome["changed"] is False and outcome["change_state_known"] is True
                assert reason in result["error"].lower() and "name_size" in result["error"].lower(), result["error"]
        mpc_args = {"collection_path": material["assets"]["collection"], "include_runtime": False}
        bad = call("long_mpc_filter", MATERIAL, "get_material_parameter_collection_values", dict(mpc_args, parameter_names=[long_name]))
        assert bad["success"] is False and bad["error_code"] == "material_preflight_failed"
        assert bad["data"]["error_code"] == "collection_parameter_filter_invalid" and "NAME_SIZE" in bad["error"]
        for label, filters in [("empty_filter", []), ("short_filter", ["ContractScalar"])]:
            valid = call(label, MATERIAL, "get_material_parameter_collection_values", dict(mpc_args, parameter_names=filters))
            assert valid["success"] is True
        audit = call("long_package_name", NATIVE, "AuditPackageDependencies", {"packageNames": ["/Game/" + long_name]})
        assert audit["success"] is False and "NAME_SIZE" in json.dumps(audit)
        assert snapshot("final") == before
        diag = call("final_clean", NATIVE, "GetEditorDiagnostics", {})
        assert diag["process_id"] == args.expected_pid and diag["dirty_packages"] == []
        report["status"] = "pass"
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["calls"] = client.observations
        report["limitations"] = ["Long valid name-bearing aggregates remain unsupported", "No universal loader/importer safety claim", "Long ordinary FString success tested separately, not by this rejection probe"]
        checkpoint()
        client.close()
    print(json.dumps({"status": report["status"], "error": report.get("error"), "stages": len(report["stages"])}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__": raise SystemExit(main())
