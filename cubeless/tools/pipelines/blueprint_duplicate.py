"""Clone one proven, owned ActorComponent timer through the official MCP server.

Preserves the legacy builder's parent-report and source-file boundary. It does
not port the complete mouse host, reset methods or caller gating. No retries.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from pipelines.component_authoring import PROJECT, RECORDS, SETTINGS, fingerprint, require, success, field
from pipelines.blueprint_variables import typed_equal
from pipelines.blueprint_variable_graph import TOOLSETS, graph_view, no_explicit_compile_save
from pipelines.blueprint_function_timer import FUNCTION, MEMBER, DEFAULTS, function_success

PARENT = {"refPath": "/Script/Engine.ActorComponent"}
ROOT = "/Game/CubelessMCPTests"


def asset_ref(package):
    return {"refPath": package + "." + package.rsplit("/", 1)[1]}


def asset_disk(package):
    require(package.startswith(ROOT + "/"), "Asset escaped the owned test root")
    return PROJECT.parent / "Content" / (package[6:] + ".uasset")


def normalized_graph(data, package):
    """Ignore regenerated GUIDs, preserving exact graph/member/function content."""
    graph_view(data)
    name = package.rsplit("/", 1)[1]
    own_asset = package + "." + name
    own_classes = (package + ".SKEL_" + name + "_C", own_asset + "_C")
    reference_fields = {"node_id", "graph_path", "variable_property_path", "function_path",
                        "function_owner_path", "event_member_parent_path"}

    def normalize(value, key=None):
        if isinstance(value, list):
            return [normalize(item, key) for item in value]
        if isinstance(value, dict):
            return {field: normalize(item, field) for field, item in value.items() if field != "node_guid"}
        if isinstance(value, str) and key in reference_fields:
            for prefix in own_classes:
                if value == prefix or value.startswith(prefix + ":"):
                    return "${OwnClass}" + value[len(prefix):]
            if value == own_asset or value.startswith(own_asset + ":"):
                return "${Blueprint}" + value[len(own_asset):]
        return value

    result = normalize(data)
    require(result["graph_path"].startswith("${Blueprint}:"), "Graph belongs to another Blueprint")
    result["nodes"].sort(key=lambda row: row["node_id"])
    for row in result["nodes"]:
        require(row["node_id"].startswith("${Blueprint}:"), "Node belongs to another Blueprint")
        row["pins"].sort(key=lambda pin: (pin["direction"], pin["name"]))
        for pin in row["pins"]:
            pin["links"].sort(key=lambda link: (link["node_id"], link["pin_name"]))
    return result


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", required=True)
    p.add_argument("--expected-project", required=True, type=Path)
    p.add_argument("--expected-pid", required=True, type=int)
    p.add_argument("--parent-report", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    a = p.parse_args(argv)
    parent_path, out = a.parent_report.resolve(), a.output.resolve()
    require(a.expected_project.resolve() == PROJECT.resolve(), "Exact isolated project required")
    require(parent_path.is_relative_to(RECORDS.resolve()) and parent_path.is_file(), "Owned parent report required")
    require(out.is_relative_to(RECORDS.resolve()) and not out.exists(), "Use a fresh verification report")
    parent_bytes = parent_path.read_bytes()
    parent = json.loads(parent_bytes.decode("utf-8-sig"))
    require(parent.get("schema") == "cubeless_blueprint_function_timer_v2" and parent.get("status") == "pass"
            and Path(parent["project_file"]).resolve() == PROJECT.resolve()
            and parent.get("parent_class") == "ActorComponent" and parent.get("official_parent") == PARENT
            and parent.get("outcome_unknown") is False and parent.get("expected_defaults") == DEFAULTS,
            "A successful, certain ActorComponent timer author report is required")
    source = parent["package"]
    require(re.fullmatch(ROOT + r"/FunctionTimer_[0-9a-f]{32}/BP_FunctionTimer", source), "Unexpected source ownership")
    source_file = asset_disk(source)
    require(fingerprint(source_file) == parent["asset_file"], "Source differs from its parent proof")
    run = uuid.uuid4().hex
    target = ROOT + "/Duplicate_" + run + "/BP_FunctionTimerClone"
    target_file = asset_disk(target)
    require(not target_file.parent.exists(), "Unique destination folder already exists")
    report = {"schema": "cubeless_blueprint_duplicate_v1", "status": "running", "run_id": run,
        "project_file": str(PROJECT.resolve()), "source_package": source, "package": target,
        "parent_report": str(parent_path), "parent_report_sha256": hashlib.sha256(parent_bytes).hexdigest(),
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "source_file_before": fingerprint(source_file), "stages": [], "changes": [], "started_ns": time.time_ns(),
        "outcome_unknown": False, "automatic_retries": 0, "atomic_rollback": False,
        "explicit_full_compile_requests": 0, "explicit_save_requests": 0,
        "runtime_execution": "not_run", "cold_reload": "not_run", "packaging": "not_run",
        "full_legacy_pipeline_parity": False, "caller_gating_verified": False,
        "legacy_scope": "Parent report/source SHA guarded clone boundary from mouse_buttons_endplay builder; no full host/reset pipeline"}
    c = OfficialMCPClient(a.endpoint)
    schemas = {}

    def persist():
        out.parent.mkdir(parents=True, exist_ok=True)
        temp = out.with_suffix(out.suffix + ".partial")
        temp.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        temp.replace(out)

    def call(label, group, tool, arguments=None, *, mutation=False):
        report["stage"] = label
        choices = [t for t in schemas[group]["tools"] if t["name"].rsplit(".", 1)[-1] == tool]
        require(len(choices) == 1, "Missing unique tool " + tool)
        schema, mapped = choices[0]["inputSchema"], {}
        for key, value in (arguments or {}).items():
            keys = [k for k in schema.get("properties", {}) if canonical(k) == canonical(key)]
            require(len(keys) == 1, "Missing input " + key)
            mapped[keys[0]] = value
        require(all(k in mapped for k in schema.get("required", [])), "Unexpected required argument")
        row = {"name": label, "toolset": TOOLSETS[group], "tool": tool, "arguments": mapped,
               "mutation": mutation, "state": "submitted", "started_ns": time.time_ns()}
        report["stages"].append(row); persist()
        try:
            row["raw"] = c.call_toolset(TOOLSETS[group], tool, mapped, mutation=mutation)
            value = operation_result(row["raw"]); row.update(result=value, state="responded")
            if isinstance(value, dict) and "changes" in value:
                report["changes"].append({"stage": label, "observations": value["changes"]})
                row.update(compile=value.get("compile"), save=value.get("save"))
                if (any(change.get("change_state_known") is False for change in value["changes"])
                        or value.get("data", {}).get("mutation_state_unknown")):
                    report["outcome_unknown"] = True
            if group in {"native", "bp", "graph"}: success(value)
            return value
        except Exception as error:
            row["error"] = str(error)
            if row["state"] == "submitted" and mutation: report["outcome_unknown"] = True
            raise
        finally:
            row["elapsed_seconds"] = (time.time_ns() - row["started_ns"]) / 1e9; persist()

    def graph(label, package, name):
        value = call(label, "graph", "inspect_graph", {"blueprint_name": package, "graph_name": name})
        no_explicit_compile_save(value)
        require(value["changes"] == [], "Graph inspection reported changes")
        graph_view(value["data"])
        return value["data"]

    def default(label, package):
        cdo = call(label + "_object", "official_bp", "get_default_object", {"blueprint": asset_ref(package)})
        require(isinstance(cdo, dict) and cdo.get("refPath", "").startswith(package + "."), "CDO escaped asset")
        values = call(label + "_values", "objects", "get_properties", {"instance": cdo, "properties": [MEMBER]})
        value = field(values, MEMBER)
        require(typed_equal(value, 0.0), "Timer CDO default differs")
        return {"object": cdo, "values": {MEMBER: value}}

    try:
        persist(); c.initialize()
        schemas.update({k: tool_payload(c.call("describe_toolset", {"toolset_name": TOOLSETS[k]}))
                        for k in ("native", "bp", "graph", "assets", "objects", "official_bp")})
        report["schemas"] = schemas
        editor = call("preflight", "native", "GetEditorDiagnostics")
        require(Path(editor["project_file"]).resolve() == PROJECT.resolve() and editor["process_id"] == a.expected_pid
                and editor["game_thread"] is True and editor["engine_version"].startswith("5.8.3-")
                and not editor["pie_or_sie_active"] and editor["dirty_packages"] == [], "Wrong/dirty editor")
        report["editor"] = editor
        policy = call("save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
        report["save_policy_before"] = policy.get("SaveOnCompile")
        require(report["save_policy_before"] == "SoC_Never", "Requires Never; settings are not changed")
        require(call("source_parent", "official_bp", "get_parent", {"blueprint": asset_ref(source)}) == PARENT, "Source parent differs")
        require(call("target_absent", "assets", "exists", {"path": target}) is False, "Destination exists")
        original = {FUNCTION: graph("source_function", source, FUNCTION), "EventGraph": graph("source_events", source, "EventGraph")}
        require(original[FUNCTION] == parent["saved_graph"] and original["EventGraph"] == parent["factory_event_graph"], "Source graph/GUID differs from parent proof")
        report["source_graphs"] = original
        report["source_default"] = default("source_default", source)
        args = {"blueprint_name": source, "target_path": target, "required_root": ROOT,
                "overwrite_existing": False, "allow_dirty_source": False, "compile": False, "save": True}
        dry = call("dry_run", "bp", "duplicate_blueprint_to_temp", {**args, "dry_run": True, "operation_id": run + "_dry"}, mutation=True)
        require(dry["data"].get("created") is False and dry["data"].get("dry_run") is True
                and dry["data"].get("source_package") == source and dry["data"].get("target_package") == target
                and dry["changes"] == []
                and dry["compile"].get("requested") is False and dry["compile"].get("attempted") is False
                and dry["save"].get("requested") is True and dry["save"].get("attempted") is False
                and dry["save"].get("saved") is False, "Dry-run mutated/compiled/saved or lost request state")
        require(dry["validation"].get("source_preserved") == "passed", "Dry-run did not verify source preservation")
        require(not target_file.exists() and fingerprint(source_file) == report["source_file_before"], "Dry-run changed disk")
        cloned = call("duplicate", "bp", "duplicate_blueprint_to_temp", {**args, "dry_run": False, "operation_id": run + "_clone"}, mutation=True)
        require(cloned["data"].get("created") is True and cloned["data"].get("dry_run") is False
                and cloned["data"].get("source_package") == source and cloned["data"].get("target_package") == target
                and cloned["data"].get("deleted_existing_target") is False
                and cloned["compile"].get("requested") is False and cloned["compile"].get("attempted") is False
                and cloned["save"].get("requested") is True and cloned["save"].get("saved") is True
                and cloned["save"].get("attempted") is True, "Clone outcome differs")
        require(all(cloned["validation"].get(key) == "passed" for key in ("source_preserved", "clone_content", "save_state")),
                "Clone/source/save validation incomplete")
        report["implicit_postduplicate_compile"] = cloned["compile"].get("implicit_postduplicate_compile")
        report["explicit_save_requests"] = 1
        saved = fingerprint(target_file); require(saved is not None, "Clone is not on disk")
        require(call("clone_parent", "official_bp", "get_parent", {"blueprint": asset_ref(target)}) == PARENT, "Clone parent differs")
        clones = {FUNCTION: graph("clone_function", target, FUNCTION), "EventGraph": graph("clone_events", target, "EventGraph")}
        for name in original:
            require(normalized_graph(original[name], source) == normalized_graph(clones[name], target), "Clone graph content differs: " + name)
            require(graph("source_unchanged_" + name, source, name) == original[name], "Source graph/GUID changed")
        report["clone_graphs"] = clones
        report["clone_default"] = default("clone_default", target)
        resolved = call("clone_function_identity", "graph", "resolve_blueprint_graph", {"blueprint_name": target,
            "graph_name": FUNCTION, "graph_type": "function", "operation_id": run + "_resolve"})
        function_success(resolved, unchanged=True)
        report["clone_function_graph"] = resolved["data"]["graph"]
        require(resolved["data"]["resolved"] is True and resolved["data"]["created"] is False
                and report["clone_function_graph"]["object_path"] == clones[FUNCTION]["graph_path"], "Clone graph resolution differs")
        final = call("final_editor", "native", "GetEditorDiagnostics")
        require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == []
                and fingerprint(source_file) == report["source_file_before"] and fingerprint(target_file) == saved
                and parent_path.read_bytes() == parent_bytes and not report["outcome_unknown"], "Changed files, dirty state or uncertain result")
        report.update(status="pass", asset_file=saved, source_preserved=True, clone_content_verified=True,
                      official_parent=PARENT, expected_defaults=DEFAULTS)
    except Exception as error:
        report.update(status="failed", error=str(error), failed_stage=report.get("stage", "discovery"), do_not_retry=True)
    finally:
        original_stage = report.get("stage")
        if report.get("editor") and report.get("save_policy_before") == "SoC_Never":
            try:
                final_policy = call("final_save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
                report["save_policy_after"] = final_policy.get("SaveOnCompile")
                require(report["save_policy_after"] == "SoC_Never", "SaveOnCompile changed")
            except Exception as error:
                report["final_save_policy_error"] = str(error)
                if report["status"] != "failed": report.update(error=str(error), failed_stage="final_save_policy")
                report["status"] = "failed"
            finally: report["stage"] = original_stage
        report.update(source_file_after=fingerprint(source_file), asset_after=fingerprint(target_file),
                      finished_ns=time.time_ns(), http_observations=c.observations)
        persist(); c.close()
    print(json.dumps({key: report.get(key) for key in ("status", "package", "failed_stage", "error", "outcome_unknown")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
