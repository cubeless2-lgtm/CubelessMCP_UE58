"""Verify generic Add/Set cannot introduce unaudited Custom shader includes."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import operation_result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--author-report", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    assert not args.output.exists()
    parent = json.loads(args.author_report.read_text(encoding="utf-8"))
    assert parent["status"] == "pass" and parent["schema"] == "cubeless_custom_material_pipeline_v1"
    project = Path(__file__).resolve().parents[3] / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
    assert Path(parent["project_file"]).resolve() == project.resolve()
    client = OfficialMCPClient(args.endpoint)
    report = {"status": "fail", "schema": "cubeless_generic_custom_guard_probe_v1", "automatic_retries": 0,
              "outcome_unknown": False, "expected_asset_changes": False, "stages": [],
              "parent_sha256": hashlib.sha256(args.author_report.read_bytes()).hexdigest()}
    material = "cubeless_mcp_ue58.registration.material_toolset.CubelessMaterialTools"
    native = "CubelessMCPUE58Editor.CubelessNativeToolset"
    run = uuid.uuid4().hex
    def call(label, toolset, tool, params, mutation=False):
        stage = {"name": label, "toolset": toolset, "tool": tool, "arguments": params, "mutation": mutation}
        report["stages"].append(stage)
        try:
            stage["raw"] = client.call_toolset(toolset, tool, params, mutation=mutation)
            stage["result"] = operation_result(stage["raw"])
            return stage["result"]
        except Exception:
            if mutation:
                report["outcome_unknown"] = True
            raise
    try:
        client.initialize()
        report["editor"] = call("preflight", native, "GetEditorDiagnostics", {})
        assert Path(report["editor"]["project_file"]).resolve() == project.resolve() and report["editor"]["dirty_packages"] == []
        catalog = tool_payload(client.call("list_toolsets"))
        objects = next(line.split(":", 1)[0][2:] for line in catalog.splitlines() if line.startswith("- ") and ".ObjectTools:" in line)
        report["object_schema"] = client.call("describe_toolset", {"toolset_name": objects})
        read_args = {"instance": {"refPath": parent["nodes"]["default"]}, "properties": ["Code", "Inputs", "AdditionalDefines", "IncludeFilePaths"]}
        before = call("independent_before", objects, "get_properties", read_args)
        graph = call("graph_before", material, "list_material_nodes", {"material_path": parent["material_package"]})
        assert graph["success"] is True
        cases = [
            ("code_comment_split", "Code", '#/**/include "/Engine/Private/Common.ush"\nreturn 0;'),
            ("code_digraph", "Code", '%:include "/Engine/Private/Common.ush"\nreturn 0;'),
            ("include_array", "IncludeFilePaths", '("/Engine/Private/Common.ush")'),
            ("define", "AdditionalDefines", '((DefineName="X",DefineValue="#include bad"))'),
        ]
        for mode in ("add", "set"):
            for key, prop, value in cases:
                label = mode + "_" + key
                params = {"material_path": parent["material_package"], "operation_id": run + "_" + label}
                if mode == "add":
                    tool = "add_material_node"
                    params.update(expression_class="Custom", properties_json=json.dumps({prop: value}))
                else:
                    tool = "set_material_node_property"
                    params.update(node_id=parent["selectors"]["default"], property_name=prop, value_json=json.dumps(value))
                result = call(label, material, tool, params, True)
                observed = call(label + "_readback", objects, "get_properties", read_args)
                after = call(label + "_graph", material, "list_material_nodes", {"material_path": parent["material_package"]})
                assert observed == before and after["data"]["nodes"] == graph["data"]["nodes"], "Generic rejected input changed graph"
                assert result["success"] is False and result["error_code"] == "material_mutation_target_rejected"
                outcome = result["data"]["native_outcome"]
                assert outcome["mutation_attempted"] is False and outcome["changed"] is False and outcome["change_state_known"] is True
                assert "shader" in result["error"].lower(), "Failed for another reason; dependency guard not proven"
        report["final_editor"] = call("final_clean", native, "GetEditorDiagnostics", {})
        assert report["final_editor"]["dirty_packages"] == []
        report["status"] = "pass"
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["calls"] = client.observations
        client.close()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2, ensure_ascii=False)
    print(json.dumps({"status": report["status"], "error": report.get("error"), "stages": len(report["stages"])}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
