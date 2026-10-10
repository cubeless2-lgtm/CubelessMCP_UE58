"""Read-only before/after comparison for two Cubeless collection read paths."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import statistics
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import operation_result

PROJECT = Path(__file__).resolve().parents[3] / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
MATERIAL = "cubeless_mcp_ue58.registration.material_toolset.CubelessMaterialTools"
NATIVE = "CubelessMCPUE58Editor.CubelessNativeToolset"


def contract(result):
    result = copy.deepcopy(result)
    result.pop("operation_id", None)
    native = result.get("data", {}).get("native_outcome", {})
    native.pop("performance", None)
    native.pop("read_data_source", None)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--author-report", required=True, type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Refusing to overwrite evidence")
    parent = json.loads(args.author_report.read_text(encoding="utf-8"))
    assert parent["status"] == "pass" and parent["phase"] == "author"
    assert Path(parent["project_file"]).resolve() == PROJECT.resolve()
    baseline_bytes = args.baseline.read_bytes() if args.baseline else None
    baseline = json.loads(baseline_bytes) if baseline_bytes else None
    if baseline:
        assert baseline["status"] == "pass"
    root = parent["packages"]
    values = "get_material_parameter_collection_values"
    nodes = "list_material_collection_parameter_nodes"
    cases = [
        ("leaf_default", values, {"collection_path": root["leaf"], "include_runtime": False}, True),
        ("leaf_filtered", values, {"collection_path": root["leaf"], "parameter_names": ["RootScalar"], "include_runtime": False}, True),
        ("leaf_no_defaults", values, {"collection_path": root["leaf"], "include_asset_defaults": False, "include_runtime": False}, True),
        ("collection_nodes", nodes, {"material_path": root["material"]}, True),
        ("missing_collection", values, {"collection_path": root["leaf"] + "_Never", "include_runtime": False}, False),
        ("wrong_collection_type", values, {"collection_path": root["material"], "include_runtime": False}, False),
    ]
    report = {"schema": "cubeless_material_read_reuse_probe_v1", "status": "fail", "mutation": False,
              "author_report_sha256": hashlib.sha256(args.author_report.read_bytes()).hexdigest(), "cases": [],
              "conditions": "One warmup then 10 sequential samples per case in one nullrhi editor; separate I/J processes, fixed case/build order, cache/disk effects uncorrected; no general speedup inference"}
    client = OfficialMCPClient(args.endpoint)
    try:
        client.initialize()
        report["editor"] = operation_result(client.call_toolset(NATIVE, "GetEditorDiagnostics"))
        assert Path(report["editor"]["project_file"]).resolve() == PROJECT.resolve() and report["editor"]["dirty_packages"] == []
        if baseline:
            assert baseline["editor"]["instance_id"] != report["editor"]["instance_id"]
            assert report["author_report_sha256"] == baseline["author_report_sha256"]
            report["baseline_sha256"] = hashlib.sha256(baseline_bytes).hexdigest()
        report["schema_raw"] = client.call("describe_toolset", {"toolset_name": MATERIAL})
        schemas = {t["name"].rsplit(".", 1)[-1]: t["inputSchema"] for t in tool_payload(report["schema_raw"])["tools"]}
        for label, tool, parameters, success in cases:
            assert set(schemas[tool].get("required", [])) <= set(parameters)
            row = {"name": label, "tool": tool, "arguments": parameters, "samples": []}
            report["cases"].append(row)
            for index in range(11):
                start = time.perf_counter()
                raw = client.call_toolset(MATERIAL, tool, parameters)
                elapsed = time.perf_counter() - start
                sample = {"warmup": index == 0, "raw": raw, "http_seconds": elapsed}
                row["samples"].append(sample)
                value = operation_result(raw)
                sample["result"] = value
                assert value["success"] is success and value["changes"] == []
                normalized = contract(value)
                if index == 0:
                    row["contract"] = normalized
                else:
                    assert row["contract"] == normalized, "Read changed within process: " + label
                if baseline:
                    previous = next(c for c in baseline["cases"] if c["name"] == label)
                    assert previous["arguments"] == parameters and previous["contract"] == normalized, "I/J contract differs: " + label
                    counts = value["data"]["native_outcome"]["performance"]["observed_counts"]
                    if success:
                        assert counts["capture_state_calls"] == 2 and counts["captured_read_payload_reuses"] == 1 and counts.get("execute_dispatch_calls", 0) == 0
                        expected = "collection_values_read_calls" if tool == values else "collection_nodes_read_calls"
                        assert counts[expected] == 2
                        assert value["data"]["before"] == value["data"]["after"], "Independent after capture differs"
            row["median_http_seconds"] = statistics.median(s["http_seconds"] for s in row["samples"][1:])
            row["contract_verified"] = True
        report["final_editor"] = operation_result(client.call_toolset(NATIVE, "GetEditorDiagnostics"))
        assert report["final_editor"]["dirty_packages"] == []
        report["status"] = "pass"
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["calls"] = client.observations
        client.close()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
    print(json.dumps({"status": report["status"], "error": report.get("error"), "cases": len(report["cases"])}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
