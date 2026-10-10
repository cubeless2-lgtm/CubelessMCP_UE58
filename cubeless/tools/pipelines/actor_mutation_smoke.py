"""Actor author/cold smoke over the official UE MCP endpoint, no server startup.

Requires root-prepared create_actor_smoke_map.py evidence. No generic executor,
process control, cleanup, mutation retry, gameplay C++ or runtime coverage claim.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, MCPError, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from verification.actor_pipeline_report import PROJECT, fixture_contract, plan, require, sha, verify


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True); parser.add_argument("--expected-project", type=Path, required=True)
    parser.add_argument("--phase", choices=["author", "reload"], required=True); parser.add_argument("--fixture-report", type=Path, required=True)
    parser.add_argument("--parent-report", type=Path); parser.add_argument("--output", type=Path, required=True); parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args(argv); output = args.output.resolve()
    require(args.expected_project.resolve() == PROJECT.resolve(), "Only exact UE58Verification is allowed")
    require(not output.exists(), "Refusing to overwrite evidence")
    fixture_bytes = args.fixture_report.read_bytes(); fixture = json.loads(fixture_bytes); package, disk = fixture_contract(fixture)
    parent_bytes = args.parent_report.read_bytes() if args.parent_report else None; parent = json.loads(parent_bytes) if parent_bytes else None
    require(args.phase != "reload" or parent is not None and parent.get("phase") == "author" and verify(parent, fixture=fixture, fixture_bytes=fixture_bytes)["passed"], "Reload requires passing saved author evidence")
    require(args.phase != "author" or parent is None, "Author must not accept a parent replay")
    require(disk.is_file(), "Prepared map file missing")
    initial_hash = sha(disk.read_bytes())
    if args.phase == "author": require(initial_hash == fixture["asset_file"]["sha256"], "Prepared map bytes changed; inspect prior attempt")
    report = {"schema": "cubeless_actor_pipeline_v1", "status": "running", "phase": args.phase, "run_id": fixture["run_id"],
        "project_file": str(PROJECT.resolve()), "endpoint": args.endpoint, "map_package": package, "fixture_sha256": sha(fixture_bytes),
        "fixture_report": str(args.fixture_report.resolve()), "initial_map_sha256": initial_hash,
        "started_ns": time.time_ns(), "stages": [], "tool_calls": [], "outcome_unknown": False, "automatic_retries": 0,
        "atomic_rollback": False, "packaging": "not_run", "full_legacy_actor_parity": False, "save_calls": 0,
        "scope": "Six public Actor contracts plus create_actor alias in one saved isolated map. Cold phase changes editor context to load the map but never invokes actor writes, compile, save or cleanup."}
    if parent: report.update(author_sha256=sha(parent_bytes), author_report=str(args.parent_report.resolve()))
    client = OfficialMCPClient(args.endpoint, args.timeout); schemas = {}
    def persist():
        output.parent.mkdir(parents=True, exist_ok=True); temporary = output.with_suffix(output.suffix + ".partial")
        temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"); temporary.replace(output)
    def invoke(spec):
        schema = schemas[spec["toolset_suffix"]]; choices = [row for row in schema["value"]["tools"] if row["name"].rsplit(".", 1)[-1] == spec["tool"]]
        require(len(choices) == 1, "Missing unique live named tool: " + spec["tool"])
        inputs = choices[0].get("inputSchema", {}); mapped = {}
        for key, value in spec["arguments"].items():
            keys = [k for k in inputs.get("properties", {}) if canonical(k) == canonical(key)]
            require(len(keys) == 1, "Live input missing/ambiguous: " + key); mapped[keys[0]] = value
        require(all(k in mapped for k in inputs.get("required", [])), "Unexpected live required arguments; no default is fabricated")
        call = {"toolset": schema["name"], "tool": spec["tool"], "arguments": mapped, "mutation": spec["mutation"], "started_ns": time.time_ns()}
        report["tool_calls"].append(call); persist()
        try:
            raw = client.call_toolset(schema["name"], spec["tool"], mapped, mutation=spec["mutation"])
            call["raw"] = raw
            call["decoded"] = operation_result(raw) if spec["toolset_suffix"].startswith("Cubeless") else tool_payload(raw)
            decoded = call["decoded"]
            if spec["mutation"] and isinstance(decoded, dict):
                native = decoded.get("data", {}).get("native_outcome", {})
                if native.get("change_state_known") is False or any(str(change.get("outcome", "")).startswith("unknown") for change in decoded.get("changes", [])):
                    report["outcome_unknown"] = True
            return call["decoded"]
        except Exception as error:
            call["error"] = str(error)
            if spec["mutation"]: report["outcome_unknown"] = True
            raise
        finally:
            call["elapsed_seconds"] = (time.time_ns() - call["started_ns"]) / 1e9; persist()
    try:
        persist(); report["initialize"] = client.initialize(); report["top_level_schema"] = client.list_tools()
        catalog = tool_payload(client.call("list_toolsets")); require(isinstance(catalog, str), "Unexpected registry catalog")
        names = re.findall(r"^- ([\w.]+):", catalog, re.MULTILINE)
        for suffix in ("CubelessNativeToolset", "CubelessActorTools", "SceneTools", "ActorTools", "AssetTools"):
            matches = [n for n in names if n.endswith("." + suffix)]; require(len(matches) == 1, "Need one live toolset: " + suffix)
            schemas[suffix] = {"name": matches[0], "value": tool_payload(client.call("describe_toolset", {"toolset_name": matches[0]}))}
        report["discovered_schemas"] = schemas
        deferred_failure = None
        for spec in plan(fixture, args.phase):
            row = {"name": spec["name"], "passed": False, "call_index": len(report["tool_calls"])}; report["stages"].append(row); persist()
            value = invoke(spec); row["result"] = value
            if spec["name"] == "preflight":
                require(value.get("success") is True and Path(value["project_file"]).resolve() == PROJECT.resolve() and value["engine_version"].startswith("5.8.3-") and value.get("game_thread") is True and value.get("pie_or_sie_active") is False and value.get("dirty_packages") == [], "Wrong/dirty/nonidle editor")
                report["editor"] = value
                if parent: require(value["instance_id"] != parent["editor"]["instance_id"], "Reload requires a fresh editor process")
            elif spec["check"] == "map": require(value == package, "Wrong current map")
            elif spec["check"] == "reject":
                # Always capture one actual actor read after a returned expected
                # rejection, even if it was unexpectedly accepted. Never retry.
                valid = isinstance(value, dict) and value.get("success") is False and value.get("error_code") == spec["expected"]["error"] and value.get("changes") == []
                if not valid: deferred_failure = "Unexpected negative result at " + spec["name"]
            elif spec["check"] in {"true", "label_set"}: require(value is True, "Boolean operation failed")
            elif spec["check"] == "false": require(value is False, "Map is dirty")
            elif spec["check"] not in {"label", "void"}: require(isinstance(value, dict) and value.get("success") is True, "Actor operation failed: " + str(value))
            if spec["name"] == "before_save_editor": require(set(value["dirty_packages"]) == {package}, "Unrelated dirty work; will not save")
            if spec["name"] == "save_map_once": report["save_calls"] += 1
            if spec["name"] == "saved_actors": report["saved_actors"] = value["data"]["actors"]
            row["passed"] = True; persist()
            if deferred_failure and spec["check"] == "list": raise AssertionError(deferred_failure + "; actual follow-up readback recorded")
        require(deferred_failure is None, "Negative call failed without a following readback")
        report["asset_file"] = {"path": str(disk.resolve()), "sha256": sha(disk.read_bytes())}
        if parent: require(report["asset_file"] == parent["asset_file"], "Cold process changed disk bytes")
        report["status"] = "pass"
        report["independent_verification"] = verify(report, fixture=fixture, fixture_bytes=fixture_bytes, parent=parent, parent_bytes=parent_bytes)
        require(report["independent_verification"]["passed"], "Independent contract failed: " + str(report["independent_verification"]["errors"]))
    except Exception as error:
        report.update(status="failed", error=str(error))
        if isinstance(error, MCPError): report["outcome_unknown"] = report["outcome_unknown"] or error.outcome_unknown
    finally:
        report["finished_ns"] = time.time_ns(); report["http_observations"] = client.observations; persist()
    print(json.dumps({"status": report["status"], "report": str(output), "stages": len(report["stages"]), "outcome_unknown": report["outcome_unknown"]}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__": raise SystemExit(main())
