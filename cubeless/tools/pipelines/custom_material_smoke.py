"""Official MCP Custom Material author/cold fixture in exact UE58Verification.

No retry, cleanup, process control, external HLSL includes or rendering claim.
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
from verification.custom_material_report import ALLOWED_PROJECT, ALIAS_ARGUMENTS, DEFAULT_CODE, NEGATIVES, PROPERTIES, expression_set, inputs, mutation, properties, ref, field, rejection, require, verify


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True); parser.add_argument("--expected-project", type=Path, required=True)
    parser.add_argument("--phase", choices=["author", "reload"], required=True); parser.add_argument("--parent-report", type=Path)
    parser.add_argument("--output", type=Path, required=True); parser.add_argument("--timeout", type=float, default=180)
    args = parser.parse_args(argv)
    require(args.expected_project.resolve() == ALLOWED_PROJECT.resolve(), "Only exact UE58Verification is allowed")
    output = args.output.resolve(); require(not output.exists(), "Refusing to overwrite evidence")
    parent_raw = args.parent_report.read_bytes() if args.phase == "reload" and args.parent_report else None
    parent = json.loads(parent_raw) if parent_raw else None
    require(args.phase != "reload" or parent is not None and parent.get("phase") == "author" and verify(parent)["passed"], "Cold phase requires passing author evidence and saved bytes")
    run = parent["run_id"] if parent else uuid.uuid4().hex
    folder = "/Game/CubelessMCPTests/Custom_" + run; package = folder + "/M_Custom"; obj = package + ".M_Custom"
    report = {"schema": "cubeless_custom_material_pipeline_v1", "status": "running", "phase": args.phase, "run_id": run,
              "project_file": str(args.expected_project.resolve()), "endpoint": args.endpoint, "material_package": package, "material_object": obj,
              "nodes": dict(parent["nodes"]) if parent else {}, "selectors": dict(parent["selectors"]) if parent else {},
              "stages": [], "tool_calls": [], "automatic_retries": 0, "outcome_unknown": False, "atomic_rollback": False,
              "full_legacy_custom_parity": False, "external_includes_supported": False, "packaging": "not_run", "shader_completion_verified": False,
              "compile": {"explicit_calls": 0, "status": "not_requested"}, "save": {"explicit_calls": 0, "saved": False}, "started_ns": time.time_ns(),
              "scope": "Default/alias Custom creation, inputs/defines, no-change invalid inputs, durable replay, official property/wiring reads, single recompile return/save and optional new-process reload. No include dependencies, shader completion, MaterialFunction parity or rendering proof."}
    if parent: report.update(author_sha256=hashlib.sha256(parent_raw).hexdigest(), asset_file=parent["asset_file"])
    schemas = {}; client = OfficialMCPClient(args.endpoint, args.timeout)
    def persist():
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = output.with_suffix(output.suffix + ".partial")
        temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"); temporary.replace(output)
    def invoke(toolset, tool, arguments=None, mutation_call=False):
        candidates = [row for row in schemas[toolset]["tools"] if row["name"].rsplit(".", 1)[-1] == tool]
        require(len(candidates) == 1, "Unique live tool missing: " + tool)
        schema = candidates[0].get("inputSchema", {}); mapped = {}
        for key, value in (arguments or {}).items():
            names = [name for name in schema.get("properties", {}) if canonical(name) == canonical(key)]
            require(len(names) == 1, "Unknown/ambiguous live input: " + key); mapped[names[0]] = value
        require(all(key in mapped for key in schema.get("required", [])), "Required live inputs missing; no synthesized defaults")
        call = {"toolset": toolset, "tool": tool, "arguments": mapped, "mutation": mutation_call, "started_ns": time.time_ns()}
        report["tool_calls"].append(call); persist()
        try:
            raw = client.call_toolset(toolset, tool, mapped, mutation=mutation_call); call["raw"] = raw
            value = operation_result(raw); call["decoded"] = value; return value
        except Exception as error:
            call["error"] = str(error)
            if mutation_call: report["outcome_unknown"] = True
            raise
        finally:
            call["elapsed_seconds"] = (time.time_ns() - call["started_ns"]) / 1e9; persist()
    def step(name, action, check=lambda v: None):
        row = {"name": name, "passed": False, "call_indices": []}; report["stages"].append(row)
        first = len(report["tool_calls"]); persist()
        try:
            value = action(); row["result"] = value; check(value); row["passed"] = True; return value
        except Exception as error: row["error"] = str(error); raise
        finally: row["call_indices"] = list(range(first, len(report["tool_calls"]))); persist()
    try:
        persist(); report["initialize"] = client.initialize(); report["top_level_tools"] = client.list_tools()
        names = re.findall(r"^- ([\w.]+):", tool_payload(client.call("list_toolsets")), flags=re.MULTILINE)
        def discover(suffix):
            matches = [name for name in names if name.endswith("." + suffix)]; require(len(matches) == 1, "Need unique toolset: " + suffix)
            name = matches[0]; schemas[name] = tool_payload(client.call("describe_toolset", {"toolset_name": name})); return name
        native = discover("CubelessNativeToolset"); cubeless = discover("CubelessMaterialTools")
        official = discover("MaterialTools"); assets = discover("AssetTools"); objects = discover("ObjectTools"); report["discovered_schemas"] = schemas
        def editor_check(value):
            require(value.get("success") is True and Path(value["project_file"]).resolve() == ALLOWED_PROJECT.resolve() and str(value["engine_version"]).startswith("5.8.3-") and value.get("game_thread") is True and value.get("pie_or_sie_active") is False and value.get("dirty_packages") == [], "Wrong/dirty/nonidle editor")
        report["editor"] = step("preflight", lambda: invoke(native, "GetEditorDiagnostics"), editor_check)
        if parent: require(report["editor"]["instance_id"] != parent["editor"]["instance_id"], "Cold reload requires a new editor process")
        def custom(kind): return {"material_path": package, **({"code": DEFAULT_CODE} if kind == "default" else ALIAS_ARGUMENTS), "operation_id": run + "_" + kind}
        def props(kind): return invoke(objects, "get_properties", {"instance": {"refPath": report["nodes"][kind]}, "properties": PROPERTIES})
        def nodes_read(): return invoke(official, "get_expressions", {"material_or_function": {"refPath": obj}})
        def node_check(value): expression_set(value, report["nodes"])
        def inputs_read(): return invoke(official, "get_expression_inputs", {"material_or_function": {"refPath": obj}, "expression": {"refPath": report["nodes"]["alias"]}})
        def root_read(): return invoke(official, "get_property_input", {"material": {"refPath": obj}, "material_property": "MP_BaseColor"})
        def root_check(value): require(ref(field(value, "expression")) == report["nodes"]["alias"], "Independent root connection differs")
        if not parent:
            step("guard_absent", lambda: invoke(assets, "exists", {"path": package}), lambda v: require(v is False, "Fixture already exists"))
            step("create_material", lambda: invoke(official, "create_material", {"folder_path": folder, "asset_name": "M_Custom"}, True), lambda v: require(v == {"refPath": obj}, "Created object reference differs"))
            for kind in ("default", "alias"):
                added = step("add_" + kind, lambda kind=kind: invoke(cubeless, "add_custom_material_node", custom(kind), True), mutation)
                report["nodes"][kind] = added["data"]["object_path"]; report["selectors"][kind] = added["data"]["node_key"]
                require(report["nodes"][kind].startswith(obj + ":"), "Custom node escaped fixture")
                step("read_" + kind, lambda kind=kind: props(kind), lambda v, kind=kind: properties(v, kind))
            def replay_check(value): mutation(value); require(value["retry"].get("replayed") is True and value["data"]["object_path"] == report["nodes"]["alias"], "Replay created another expression")
            step("replay_alias", lambda: invoke(cubeless, "add_custom_material_node", custom("alias"), True), replay_check)
            step("replay_readback", nodes_read, node_check)
            step("conflict", lambda: invoke(cubeless, "add_custom_material_node", {**custom("alias"), "code": DEFAULT_CODE}, True), lambda v: rejection(v, "operation_id_conflict"))
            step("conflict_readback", nodes_read, node_check)
            for key, (overrides, code) in NEGATIVES.items():
                value = step("negative_" + key, lambda key=key, overrides=overrides: invoke(cubeless, "add_custom_material_node", {"material_path": package, "code": DEFAULT_CODE, **overrides, "operation_id": run + "_negative_" + key}, True))
                # Read actual state even if the invalid operation unexpectedly succeeded.
                step("nodes_after_" + key, nodes_read, node_check)
                observed = step("properties_after_" + key, lambda: props("alias"), lambda v: properties(v, "alias"))
                require(observed == next(s["result"] for s in report["stages"] if s["name"] == "read_alias"), "Rejected creation changed an existing Custom node")
                step("reject_" + key, lambda value=value, code=code: rejection(value, code))
            for pin in ("Signal", "Bias"):
                step("connect_" + pin, lambda pin=pin: invoke(cubeless, "connect_material_nodes", {"material_path": package, "from_node_id": report["selectors"]["default"], "to_node_id": report["selectors"]["alias"], "to_input": pin, "operation_id": run + "_connect_" + pin}, True), mutation)
            step("independent_inputs", inputs_read, lambda v: inputs(v, report["nodes"]))
            step("connect_root", lambda: invoke(cubeless, "connect_material_property", {"material_path": package, "from_node_id": report["selectors"]["alias"], "property": "BaseColor", "operation_id": run + "_root"}, True), mutation)
            step("independent_root", root_read, root_check)
            report["compile"]["explicit_calls"] = 1
            step("compile_once", lambda: invoke(official, "recompile", {"material_or_function": {"refPath": obj}}, True))
            report["compile"]["status"] = "returned_without_reported_errors"
            report["save"]["explicit_calls"] = 1
            step("save_once", lambda: invoke(assets, "save_assets", {"asset_paths": [package]}, True), lambda v: require(v is True, "Save failed")); report["save"]["saved"] = True
            for kind in ("default", "alias"): step("saved_" + kind, lambda kind=kind: props(kind), lambda v, kind=kind: properties(v, kind))
            step("saved_nodes", nodes_read, node_check)
        else:
            for kind in ("default", "alias"): step("reload_" + kind, lambda kind=kind: props(kind), lambda v, kind=kind: properties(v, kind))
            step("reload_nodes", nodes_read, node_check)
            step("independent_inputs", inputs_read, lambda v: inputs(v, report["nodes"]))
            step("independent_root", root_read, root_check)
        step("reload_clean" if parent else "saved_clean", lambda: invoke(assets, "is_dirty", {"asset_path": package}), lambda v: require(v is False, "Saved/reloaded fixture is dirty"))
        path = ALLOWED_PROJECT.parent / "Content" / (package[len("/Game/"):] + ".uasset")
        observed_file = {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        if parent: require(observed_file == parent["asset_file"], "Saved bytes changed since author")
        report["asset_file"] = observed_file
        step("final_editor", lambda: invoke(native, "GetEditorDiagnostics"), editor_check)
        report["status"] = "pass"; report["independent_contract"] = verify(report, parent=parent, parent_bytes=parent_raw)
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
