"""Official HTTP Material Add/Set edge fixtures, restricted to UE58Verification.

Creates two Constants in a fresh Material. No compiler, retry, process control,
original fixture edits, cleanup or packaged-runtime claims.
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
from official_mcp_client import MCPError, OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from verification.material_add_set_edges_report import ALLOWED_PROJECT, FINAL_DESC, INITIAL_DESC, POSITION_CASES, PROPERTIES, VALUE_CASES, mutation, null_observation, readback, rejected, require, verify


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", required=True); p.add_argument("--expected-project", type=Path, required=True)
    p.add_argument("--phase", choices=["author", "reload"], required=True); p.add_argument("--parent-report", type=Path)
    p.add_argument("--output", type=Path, required=True); p.add_argument("--timeout", type=float, default=180)
    a = p.parse_args(argv)
    require(a.expected_project.resolve() == ALLOWED_PROJECT.resolve(), "Only the exact UE58Verification project is allowed")
    output = a.output.resolve(); require(not output.exists(), "Refusing to overwrite evidence")
    parent_bytes = a.parent_report.read_bytes() if a.phase == "reload" and a.parent_report else None
    parent = json.loads(parent_bytes) if parent_bytes else None
    require(a.phase != "reload" or parent is not None and verify(parent, phase="author")["passed"], "Reload requires passing actual author evidence")
    run = parent["run_id"] if parent else uuid.uuid4().hex
    root = "/Game/CubelessMCPTests/MaterialEdges_" + run; package = root + "/M_Edges"
    report = {"schema": "cubeless_material_add_set_edges_v1", "status": "running", "phase": a.phase, "run_id": run,
              "endpoint": a.endpoint, "project_file": str(a.expected_project.resolve()), "material_package": package,
              "material_object": package + ".M_Edges", "nodes": dict(parent["nodes"]) if parent else {}, "selectors": dict(parent["selectors"]) if parent else {},
              "stages": [], "tool_calls": [], "started_ns": time.time_ns(), "automatic_retries": 0, "outcome_unknown": False,
              "atomic_rollback": False, "full_material_parity": False, "packaging": "not_run",
              "null_position_must_reject": False, "null_position_limit": "UE JsonObjectConverter may skip null before the custom array converter; empty-path observation is not a null-rejection proof",
              "compile": {"explicit_calls": 0, "shader_completion_verified": False}, "save": {"explicit_calls": 0, "saved": False}}
    report["input_fingerprint"] = hashlib.sha256(json.dumps({k: report[k] for k in ("phase", "run_id", "project_file", "endpoint", "material_package")}, sort_keys=True).encode()).hexdigest()
    if parent:
        report.update(author_report_sha256=hashlib.sha256(parent_bytes).hexdigest(), author_editor=parent["editor"])
    schemas = {}; client = OfficialMCPClient(a.endpoint, a.timeout)

    def persist():
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = output.with_suffix(output.suffix + ".partial")
        temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"); temporary.replace(output)

    def invoke(toolset, tool, arguments=None, *, mutation_call=False, probe=False):
        rows = [r for r in schemas[toolset]["tools"] if r["name"].rsplit(".", 1)[-1] == tool]
        require(len(rows) == 1, "Live schema has no unique tool: " + tool)
        props = rows[0].get("inputSchema", {}).get("properties", {}); mapped = {}
        for key, value in (arguments or {}).items():
            matches = [name for name in props if canonical(name) == canonical(key)]
            require(len(matches) == 1, "Unknown/ambiguous live argument: " + tool + "." + key)
            mapped[matches[0]] = value
        # Deliberately do not synthesize [] or node_position: omission must be
        # exercised through the real server's declared/default behavior.
        call = {"toolset": toolset, "tool": tool, "arguments": mapped, "mutation": mutation_call, "started_ns": time.time_ns()}
        report["tool_calls"].append(call); persist()
        try:
            raw = client.call_toolset(toolset, tool, mapped, mutation=mutation_call); call["raw"] = raw
            if raw.get("isError") and probe:
                call["decoded"] = None
                if mutation_call: report["outcome_unknown"] = True
                return {"raw": raw, "decoded": None}
            value = operation_result(raw); call["decoded"] = value
            return {"raw": raw, "decoded": value} if probe else value
        except Exception as error:
            call["error"] = str(error)
            if mutation_call: report["outcome_unknown"] = True
            raise
        finally:
            call["elapsed_seconds"] = (time.time_ns() - call["started_ns"]) / 1e9; persist()

    def step(name, action, check=lambda v: None):
        row = {"name": name, "passed": False, "call_indices": [], "started_ns": time.time_ns()}
        report["stages"].append(row); first = len(report["tool_calls"]); persist()
        try:
            value = action(); row["result"] = value; check(value); row["passed"] = True; return value
        except Exception as error:
            row["error"] = str(error); raise
        finally:
            row["call_indices"] = list(range(first, len(report["tool_calls"]))); row["elapsed_seconds"] = (time.time_ns() - row["started_ns"]) / 1e9; persist()

    try:
        persist(); report["initialize"] = client.initialize(); report["top_level_tools"] = client.list_tools()
        names = re.findall(r"^- ([\w.]+):", tool_payload(client.call("list_toolsets")), flags=re.MULTILINE)
        def discover(suffix):
            matches = [name for name in names if name.endswith("." + suffix)]
            require(len(matches) == 1, "Need unique toolset: " + suffix)
            name = matches[0]; schemas[name] = tool_payload(client.call("describe_toolset", {"toolset_name": name})); return name
        native = discover("CubelessNativeToolset"); cubeless = discover("CubelessMaterialTools")
        official = discover("MaterialTools"); assets = discover("AssetTools"); objects = discover("ObjectTools")
        report["discovered_schemas"] = schemas
        def editor_check(value):
            require(value.get("success") is True and Path(value["project_file"]).resolve() == ALLOWED_PROJECT.resolve(), "Wrong editor project")
            require(str(value["engine_version"]).startswith("5.8.3") and value.get("game_thread") is True and value.get("pie_or_sie_active") is False and not value.get("dirty_packages"), "Editor is wrong version or not clean/idle")
        report["editor"] = step("preflight", lambda: invoke(native, "GetEditorDiagnostics"), editor_check)
        if parent:
            require(report["editor"]["instance_id"] != parent["editor"]["instance_id"], "Reload needs a new editor process")
        def props(kind):
            return invoke(objects, "get_properties", {"instance": {"refPath": report["nodes"][kind]}, "properties": PROPERTIES})
        def change(tool, args, identifier, *, probe=False):
            return invoke(cubeless, tool, {"material_path": package, **args, "operation_id": run + "_" + identifier}, mutation_call=True, probe=probe)
        if not parent:
            for key, position in POSITION_CASES.items():
                step("position_" + key, lambda key=key, position=position: invoke(cubeless, "add_material_node", {"material_path": "", "expression_class": "Constant", "node_position": position, "operation_id": run + "_position_" + key}, probe=True), lambda v: rejected(v, "position"))
            null_result = step("position_null_observation", lambda: invoke(cubeless, "add_material_node", {"material_path": "", "expression_class": "Constant", "node_position": None, "operation_id": run + "_position_null"}, probe=True), null_observation)
            report["null_position_observation"] = null_observation(null_result)
            step("post_position_editor", lambda: invoke(native, "GetEditorDiagnostics"), editor_check)
            step("guard_absent", lambda: invoke(assets, "exists", {"path": package}), lambda v: require(v is False, "Fixture path exists"))
            step("create_material", lambda: invoke(official, "create_material", {"folder_path": root, "asset_name": "M_Edges"}, mutation_call=True), lambda v: require(v == {"refPath": report["material_object"]}, "Material factory returned unexpected reference"))
            for kind in ("default", "position"):
                arguments = {"expression_class": "Constant", "properties_json": json.dumps({"R": 0.125, "Desc": INITIAL_DESC})}
                if kind == "position": arguments["node_position"] = [-700, 550]
                value = step("add_" + kind, lambda arguments=arguments, kind=kind: change("add_material_node", arguments, "add_" + kind), mutation)
                report["nodes"][kind] = value["data"]["object_path"]; report["selectors"][kind] = value["data"]["node_key"]
                require(report["nodes"][kind].startswith(report["material_object"] + ":"), "Expression escaped fixture")
                step("read_" + kind, lambda kind=kind: props(kind), lambda v, kind=kind: readback(v, 0.125, INITIAL_DESC, [0, 0] if kind == "default" else [-700, 550]))
            set_args = {"node_id": report["selectors"]["position"], "property_name": "R", "value_json": "0.875"}
            step("set_number", lambda: change("set_material_node_property", set_args, "number"), mutation)
            step("read_number", lambda: props("position"), lambda v: readback(v, 0.875, INITIAL_DESC, [-700, 550]))
            set_args = {"node_id": report["selectors"]["position"], "property_name": "Desc", "value_json": json.dumps(FINAL_DESC)}
            step("set_quoted", lambda: change("set_material_node_property", set_args, "quoted"), mutation)
            step("read_quoted", lambda: props("position"), lambda v: readback(v, 0.875, FINAL_DESC, [-700, 550]))
            for key, value in VALUE_CASES.items():
                result = step("value_" + key, lambda key=key, value=value: change("set_material_node_property", {"node_id": report["selectors"]["position"], "property_name": "R", "value_json": value}, "negative_" + key, probe=True))
                # Read even an unexpected successful response before reporting
                # failure. A read is not a retry of the rejected mutation.
                step("read_after_" + key, lambda: props("position"), lambda v: readback(v, 0.875, FINAL_DESC, [-700, 550]))
                step("reject_" + key, lambda result=result, key=key: rejected(result, "parse" if key in {"trailing", "multiple"} else "property"))
        else:
            step("reload_default", lambda: props("default"), lambda v: readback(v, 0.125, INITIAL_DESC, [0, 0]))
            step("reload_position", lambda: props("position"), lambda v: readback(v, 0.875, FINAL_DESC, [-700, 550]))
        step("official_expression_array", lambda: invoke(official, "get_expressions", {"material_or_function": {"refPath": report["material_object"]}}), lambda v: require(isinstance(v, list) and len(v) == 2 and {x.get("refPath") if isinstance(x, dict) else x for x in v} == set(report["nodes"].values()), "Official array-return read differs"))
        if not parent:
            report["save"]["explicit_calls"] = 1
            step("save_once", lambda: invoke(assets, "save_assets", {"asset_paths": [package]}, mutation_call=True), lambda v: require(v is True, "Save failed")); report["save"]["saved"] = True
            step("saved_default", lambda: props("default"), lambda v: readback(v, 0.125, INITIAL_DESC, [0, 0]))
            step("saved_position", lambda: props("position"), lambda v: readback(v, 0.875, FINAL_DESC, [-700, 550]))
        step("reload_clean" if parent else "saved_clean", lambda: invoke(assets, "is_dirty", {"asset_path": package}), lambda v: require(v is False, "Saved/reloaded package is dirty"))
        step("final_editor", lambda: invoke(native, "GetEditorDiagnostics"), editor_check)
        report["status"] = "pass"
        report["independent_contract"] = verify(report, phase=a.phase, parent=parent, parent_bytes=parent_bytes)
        require(report["independent_contract"]["passed"], "Independent contract failed: " + str(report["independent_contract"]["errors"]))
    except Exception as error:
        report.update(status="fail", error=str(error), failed_stage=report["stages"][-1]["name"] if report["stages"] else "discovery")
        if isinstance(error, MCPError): report["outcome_unknown"] = report["outcome_unknown"] or error.outcome_unknown
    finally:
        report["http_observations"] = client.observations; report["finished_ns"] = time.time_ns(); client.close(); persist()
    print(json.dumps({"status": report["status"], "output": str(output), "stage": report.get("failed_stage"), "error": report.get("error")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
