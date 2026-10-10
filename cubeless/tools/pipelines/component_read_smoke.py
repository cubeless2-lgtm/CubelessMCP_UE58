"""Official-MCP local-SCS read/cold proof. No fixture creation or engine startup.

Only reads two named Cubeless tools plus independent official property/dirty
queries. No compile, save, mutation retry or cleanup is requested. Asset loading
may perform implicit PostLoad work, so uncertain observations stop the run.
"""
import argparse
import copy
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from verification.component_pipeline_report import PROJECT, fixture_contract, plan, read_observation, require, sha, verify


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True); parser.add_argument("--expected-project", type=Path, required=True)
    parser.add_argument("--phase", choices=["read", "reload"], required=True); parser.add_argument("--fixture-report", type=Path, required=True)
    parser.add_argument("--parent-report", type=Path); parser.add_argument("--output", type=Path, required=True); parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args(argv); output = args.output.resolve()
    require(args.expected_project.resolve() == PROJECT.resolve(), "Only exact UE58Verification is allowed")
    require(not output.exists(), "Refusing to overwrite evidence")
    fixture_bytes = args.fixture_report.read_bytes(); fixture = json.loads(fixture_bytes); bps, files = fixture_contract(fixture)
    parent_bytes = args.parent_report.read_bytes() if args.parent_report else None; parent = json.loads(parent_bytes) if parent_bytes else None
    require(args.phase != "reload" or parent is not None and parent.get("phase") == "read" and verify(parent, fixture=fixture, fixture_bytes=fixture_bytes)["passed"], "Reload requires passing first-read evidence")
    require(args.phase != "read" or parent is None, "First read cannot accept a parent replay")
    def file_snapshot():
        return [{**row, "sha256": sha(Path(row["path"]).read_bytes())} for row in files]
    before = file_snapshot(); require(before == files, "Prepared asset bytes changed before read; inspect prior attempt")
    report = {"schema": "cubeless_component_read_pipeline_v1", "status": "running", "phase": args.phase,
        "run_id": fixture["run_id"], "project_file": str(PROJECT.resolve()), "endpoint": args.endpoint,
        "fixture_report": str(args.fixture_report.resolve()), "fixture_sha256": sha(fixture_bytes), "asset_files_before": before,
        "started_ns": time.time_ns(), "stages": [], "tool_calls": [], "node_guids": {}, "property_values": {},
        "outcome_unknown": False, "automatic_retries": 0, "explicit_compile_calls": 0, "explicit_save_calls": 0,
        "guid_independently_verified": False, "packaging": "not_run", "full_legacy_component_parity": False,
        "scope": "Exact local/empty/inherited SCS and ten declared property cases; 9 generic official property baselines plus one public FName getter baseline. GUID stability only. No inherited effective/default-object/orphan completeness or PostLoad immutability claim."}
    if parent: report.update(parent_sha256=sha(parent_bytes), parent_report=str(args.parent_report.resolve()))
    client = OfficialMCPClient(args.endpoint, args.timeout); schemas = {}
    def persist():
        output.parent.mkdir(parents=True, exist_ok=True); temporary = output.with_suffix(output.suffix + ".partial")
        temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"); temporary.replace(output)
    def invoke(spec):
        descriptor = schemas[spec["toolset_suffix"]]
        choices = [row for row in descriptor["value"]["tools"] if row["name"].rsplit(".", 1)[-1] == spec["tool"]]
        require(len(choices) == 1, "Missing unique named tool: " + spec["tool"])
        inputs = choices[0].get("inputSchema", {}); mapped = {}
        for key, value in spec["arguments"].items():
            keys = [k for k in inputs.get("properties", {}) if canonical(k) == canonical(key)]
            require(len(keys) == 1, "Unknown/ambiguous live input: " + key); mapped[keys[0]] = value
        require(all(k in mapped for k in inputs.get("required", [])), "Unexpected live required argument; no fabricated default")
        call = {"toolset": descriptor["name"], "tool": spec["tool"], "arguments": mapped, "mutation": False, "started_ns": time.time_ns()}
        report["tool_calls"].append(call); persist()
        try:
            raw = client.call_toolset(descriptor["name"], spec["tool"], mapped, mutation=False); call["raw"] = raw
            value = operation_result(raw); call["decoded"] = value
            if spec["toolset_suffix"] == "CubelessComponentTools" and isinstance(value, dict):
                native = value.get("data", {}).get("native_outcome", {})
                if native.get("change_state_known") is False or any(c.get("change_state_known") is False for c in value.get("changes", [])):
                    report["outcome_unknown"] = True
            return value
        except Exception as error:
            call["error"] = str(error)
            # Although no write was requested, asset loads/reference resolution
            # could have run before a lost or malformed response.
            if spec["toolset_suffix"] in {"CubelessComponentTools", "ObjectTools"}: report["outcome_unknown"] = True
            raise
        finally:
            call["elapsed_seconds"] = (time.time_ns() - call["started_ns"]) / 1e9; persist()
    try:
        persist(); report["initialize"] = client.initialize(); report["top_level_schema"] = client.list_tools()
        catalog = tool_payload(client.call("list_toolsets")); require(type(catalog) is str, "Unexpected Registry catalog")
        names = re.findall(r"^- ([\w.]+):", catalog, re.MULTILINE)
        for suffix in ("CubelessNativeToolset", "CubelessComponentTools", "ObjectTools", "AssetTools"):
            matches = [n for n in names if n.endswith("." + suffix)]; require(len(matches) == 1, "Need unique live toolset: " + suffix)
            schemas[suffix] = {"name": matches[0], "value": tool_payload(client.call("describe_toolset", {"toolset_name": matches[0]}))}
        report["discovered_schemas"] = schemas
        deferred_failure = None
        for spec in plan(fixture):
            row = {"name": spec["name"], "passed": False, "call_index": len(report["tool_calls"])}; report["stages"].append(row); persist()
            value = invoke(spec); row["result"] = value
            if spec["check"] == "editor":
                require(value.get("success") is True and Path(value["project_file"]).resolve() == PROJECT.resolve() and value["engine_version"].startswith("5.8.3-") and value["game_thread"] is True and value["pie_or_sie_active"] is False and value["dirty_packages"] == [], "Wrong/nonidle/dirty editor")
                if spec["name"] == "preflight":
                    report["editor"] = copy.deepcopy(value)
                    if parent: require(value["instance_id"] != parent["editor"]["instance_id"], "Reload requires a new process")
            elif spec["check"] == "reject":
                try:
                    read_observation(value, success=False)
                    require(value["error_code"] == spec["expected"]["code"], "Wrong rejection")
                except (AssertionError, TypeError, KeyError): deferred_failure = "Unexpected negative result at " + spec["name"]
            elif spec["check"] in {"list", "property"}: read_observation(value)
            elif spec["check"] == "clean": require(value is False, "Prepared asset became dirty")
            elif spec["check"] == "official_property": require(type(value) is dict, "Official property read failed")
            if spec["name"] == "local_first": report["node_guids"] = {r["component_name"]: r["node_guid"] for r in value["data"]["components"]}
            if spec["check"] == "property":
                prop = bps["local"]["properties"][spec["expected"]["property_index"]]
                report["property_values"][prop["id"]] = value["data"]["property_value"]
            row["passed"] = True; persist()
            if report["outcome_unknown"]: raise AssertionError("Read/loading outcome uncertain; no automatic retry")
            if deferred_failure and spec["name"].startswith("after_"): raise AssertionError(deferred_failure + "; actual list readback retained")
        require(deferred_failure is None, "Negative result lacks follow-up")
        report["asset_files_after"] = file_snapshot(); require(report["asset_files_after"] == before, "Read changed disk bytes")
        report["status"] = "pass"
        report["independent_verification"] = verify(report, fixture=fixture, fixture_bytes=fixture_bytes, parent=parent, parent_bytes=parent_bytes)
        require(report["independent_verification"]["passed"], "Independent verification failed: " + str(report["independent_verification"]["errors"]))
    except Exception as error: report.update(status="failed", error=str(error))
    finally:
        report["finished_ns"] = time.time_ns(); report["http_observations"] = client.observations
        try: client.close()
        except Exception as close_error: report["session_close_error"] = str(close_error)
        # Preserve post-read disk observations even when a prior call failed.
        try: report["asset_files_final_observation"] = file_snapshot()
        except Exception as file_error: report["asset_files_final_error"] = str(file_error)
        persist()
    print(json.dumps({"status": report["status"], "stages": len(report["stages"]), "outcome_unknown": report["outcome_unknown"], "report": str(output)}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__": raise SystemExit(main())
