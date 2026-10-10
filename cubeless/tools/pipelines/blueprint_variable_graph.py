"""Author a bounded member-variable graph through the official UE58 MCP server.

This extracts getter/setter and direct-link behavior from the legacy mouse timer.
It does not port its function graph, parameter, math call or caller gating. The
root owns editor launch and cold readback; this client never retries mutations.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from pipelines.component_authoring import PROJECT, RECORDS, SETTINGS, fingerprint, require, success, field
from pipelines.blueprint_variables import typed_equal, variable_success

TOOLSETS = {
    "native": "CubelessMCPUE58Editor.CubelessNativeToolset",
    "bp": "cubeless_mcp_ue58.registration.toolsets.CubelessBlueprintTools",
    "graph": "cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools",
    "batch": "cubeless_mcp_ue58.registration.batch_toolset.CubelessBatchTools",
    "assets": "editor_toolset.toolsets.asset.AssetTools",
    "objects": "editor_toolset.toolsets.object.ObjectTools",
    "official_bp": "editor_toolset.toolsets.blueprint.BlueprintTools",
}
DEFAULTS = {"Source": 3.0, "Target": 0.0, "Gate": True}


def node_identity(row):
    """Stable identity/type/position contract, independent of localized titles."""
    return {key: row.get(key) for key in ("node_id", "node_guid", "class", "x", "y",
        "variable_name", "self_context", "variable_property_path", "event_name", "event_member_parent_path")}


def graph_view(data):
    rows = data["nodes"]
    require(data["node_count"] == len(rows), "Incomplete graph row count")
    nodes = {row["node_id"]: row for row in rows}
    require(len(nodes) == len(rows) and len({uuid.UUID(r["node_guid"]).hex for r in rows}) == len(rows)
            and all(uuid.UUID(r["node_guid"]).int for r in rows), "Invalid or duplicated node identities")
    edges = set()
    for row in rows:
        keys = {(p["name"], p["direction"]) for p in row["pins"]}
        require(len(keys) == len(row["pins"]), "Duplicate pin identities")
        for pin in row["pins"]:
            require(pin["direction"] in {"input", "output"}, "Invalid pin direction")
            for link in pin["links"]:
                edge = (row["node_id"], pin["direction"], pin["name"], link["node_id"], link["pin_name"])
                require(edge not in edges, "Duplicate observed link")
                edges.add(edge)
    for source, direction, pin, target, target_pin in edges:
        require(target in nodes, "Dangling graph link")
        reverse = (target, "input" if direction == "output" else "output", target_pin, source, pin)
        require(reverse in edges, "One-sided graph link")
    return nodes, edges


def no_explicit_compile_save(value):
    require(value["compile"].get("requested") is False and value["compile"].get("attempted") is False
            and value["save"].get("requested") is False and value["save"].get("attempted") is False
            and value["save"].get("saved") is False, "Unexpected intermediate full compile/save")


def graph_mutation(value):
    success(value); no_explicit_compile_save(value)
    require(value["data"].get("postcondition_verified") is True and value["data"].get("verified_success") is True,
            "Graph postcondition not verified")


def batch_children(value, count):
    success(value); batch = value["data"]["batch"]
    require(batch["executed"] == batch["succeeded"] == count and batch["failed"] == 0
            and batch["not_run"] == [] and batch["mutation_state_unknown"] is False, "Batch incomplete or uncertain")
    children = [row["response"] for row in batch["results"]]
    require(len(children) == count, "Missing batch responses")
    return children


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", required=True); p.add_argument("--expected-project", required=True, type=Path)
    p.add_argument("--expected-pid", required=True, type=int); p.add_argument("--output", required=True, type=Path)
    a = p.parse_args(argv); out = a.output.resolve()
    require(a.expected_project.resolve() == PROJECT.resolve(), "Exact isolated project required")
    require(out.is_relative_to(RECORDS.resolve()) and not out.exists(), "Use a fresh verification report")
    run = uuid.uuid4().hex; folder = "/Game/CubelessMCPTests/VariableGraph_" + run
    package = folder + "/BP_VariableGraph"; asset_ref = package + ".BP_VariableGraph"
    disk = PROJECT.parent / "Content" / (package[6:] + ".uasset")
    require(not disk.parent.exists(), "Owned UUID folder already exists")
    config = PROJECT.parent / "Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini"
    report = {"schema": "cubeless_blueprint_variable_graph_v1", "status": "running", "run_id": run,
        "package": package, "project_file": str(PROJECT.resolve()), "expected_defaults": DEFAULTS,
        "stages": [], "changes": [], "started_ns": time.time_ns(), "automatic_retries": 0,
        "outcome_unknown": False, "atomic_rollback": False, "cold_reload": "not_run", "packaging": "not_run",
        "runtime_execution": "not_run", "full_legacy_pipeline_parity": False, "full_legacy_node_parity": False,
        "legacy": {"id": "script:Python/scripts/analysis/build_cube_fluid_ninja_mouse_timer.py",
            "sha256": "696ea2d51014152d55cde30a3f7e7720ad4b230a1153ea6e9be6e8036319ae29",
            "scope": "member getter/setter and typed link subset; function graph/parameter/math call/caller gating remain pending"},
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), "config_before": fingerprint(config)}
    c = OfficialMCPClient(a.endpoint); schemas = {}

    def persist():
        out.parent.mkdir(parents=True, exist_ok=True); temp = out.with_suffix(out.suffix + ".partial")
        temp.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"); temp.replace(out)

    def call(label, group, tool, arguments=None, *, mutation=False, expect_success=True):
        report["stage"] = label
        choices = [t for t in schemas[group]["tools"] if t["name"].rsplit(".", 1)[-1] == tool]
        require(len(choices) == 1, "Missing unique tool " + tool); schema = choices[0]["inputSchema"]; mapped = {}
        for key, value in (arguments or {}).items():
            keys = [k for k in schema.get("properties", {}) if canonical(k) == canonical(key)]
            require(len(keys) == 1, "Missing input " + key); mapped[keys[0]] = value
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
                data = value.get("data", {})
                if (any(x.get("change_state_known") is False for x in value["changes"])
                        or data.get("mutation_state_unknown") or data.get("batch", {}).get("mutation_state_unknown")):
                    report["outcome_unknown"] = True
            if group in {"native", "bp", "graph", "batch"} and expect_success: success(value)
            return value
        except Exception as error:
            row["error"] = str(error)
            if row["state"] == "submitted" and mutation: report["outcome_unknown"] = True
            raise
        finally:
            row["elapsed_seconds"] = (time.time_ns() - row["started_ns"]) / 1e9; persist()

    def inspect(label):
        value = call(label, "graph", "inspect_graph", {"blueprint_name": package})
        no_explicit_compile_save(value); require(value["changes"] == [], "Graph read reported changes")
        graph_view(value["data"]); return value["data"]

    def command(group, tool, **params):
        return {"type": TOOLSETS[group].rsplit(".", 1)[-1] + "." + tool, "params": {"blueprint_name": package, **params}}

    try:
        persist(); c.initialize()
        schemas.update({k: tool_payload(c.call("describe_toolset", {"toolset_name": v})) for k, v in TOOLSETS.items()})
        report["schemas"] = schemas
        required = {"add_blueprint_variable_get_node", "add_blueprint_variable_set_node", "inspect_graph", "connect_graph_pins", "add_branch_node"}
        require(required.issubset({t["name"].rsplit(".", 1)[-1] for t in schemas["graph"]["tools"]}), "T graph tools not registered")
        editor = call("preflight", "native", "GetEditorDiagnostics")
        require(Path(editor["project_file"]).resolve() == PROJECT.resolve() and editor["process_id"] == a.expected_pid
                and editor["game_thread"] is True and editor["engine_version"].startswith("5.8.3-")
                and not editor["pie_or_sie_active"] and editor["dirty_packages"] == [], "Wrong/dirty editor")
        report["editor"] = editor
        policy = call("save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
        report["save_policy_before"] = policy.get("SaveOnCompile")
        require(report["save_policy_before"] == "SoC_Never", "Requires Never; settings are not changed")
        require(call("absent", "assets", "exists", {"path": package}) is False, "Asset exists")
        created = call("create", "bp", "create_blueprint", {"name": "BP_VariableGraph", "parent_class": "Actor",
            "package_path": folder, "operation_id": run + "_create"}, mutation=True)
        require(created["save"]["saved"] is False and not disk.exists(), "Creation saved unexpectedly")
        report["implicit_factory_compile"] = created["compile"].get("implicit_factory_compile")
        baseline = inspect("factory_graph"); baseline_nodes, baseline_edges = graph_view(baseline)
        events = [n for n in baseline["nodes"] if n["class"] == "K2Node_Event"
            and n.get("event_name") == "ReceiveBeginPlay" and n.get("event_member_parent_path") == "/Script/Engine.Actor"]
        require(len(events) == 1 and not baseline_edges, "Fresh unlinked Actor BeginPlay member not identified uniquely")
        begin = events[0]; report["factory_graph"] = baseline
        commands = [command("bp", "add_blueprint_variable", variable_name=name,
            variable_type="Boolean" if type(value) is bool else "Float", default_value_json=json.dumps(value), is_exposed=True)
            for name, value in DEFAULTS.items()]
        commands += [command("graph", "add_blueprint_variable_get_node", variable_name="Source", x=-500, y=100),
            command("graph", "add_blueprint_variable_get_node", variable_name="Gate", x=-100, y=300),
            command("graph", "add_blueprint_variable_set_node", variable_name="Target", x=0, y=0),
            command("graph", "add_branch_node", x=400, y=0)]
        authored = call("members_and_nodes_batch", "batch", "execute_batch", {"commands_json": json.dumps(commands),
            "operation_id": run + "_nodes"}, mutation=True)
        children = batch_children(authored, 7)
        for child in children[:3]: variable_success(child)
        for child in children[3:]: graph_mutation(child)
        require(not authored["compile"]["entries"] and not authored["save"]["entries"] and not disk.exists(), "Intermediate batch compiled/saved")
        source, gate, target, branch = [child["data"] for child in children[3:]]
        before_links = inspect("authored_nodes"); nodes, edges = graph_view(before_links)
        created_ids = {node["node_id"] for node in (source, gate, target, branch)}
        require(len(created_ids) == 4 and set(nodes) == set(baseline_nodes) | created_ids and edges == baseline_edges,
                "Unexpected authored nodes or links")
        require(all(nodes[key] == row for key, row in baseline_nodes.items()), "Factory node changed during member authoring")
        for row, name, kind in ((source, "Source", "K2Node_VariableGet"), (gate, "Gate", "K2Node_VariableGet"), (target, "Target", "K2Node_VariableSet")):
            actual = nodes[row["node_id"]]
            require(actual["class"] == kind and actual["variable_name"] == name and actual["self_context"] is True
                    and actual["variable_property_path"] == row["member_preflight"]["property_path"]
                    and node_identity(actual) == node_identity(row), "Wrong member identity")
        require(nodes[branch["node_id"]]["class"] == "K2Node_IfThenElse", "Branch identity differs")
        report["authored_graph"] = before_links
        missing = call("reject_missing_member", "graph", "add_blueprint_variable_get_node", {"blueprint_name": package,
            "variable_name": "MissingMember", "operation_id": run + "_missing"}, mutation=True, expect_success=False)
        require(missing.get("success") is False and missing.get("stage") == "preflight"
                and missing.get("error_code") == "graph_member_not_declared" and missing.get("changes") == [], "Missing member did not reject before change")
        no_explicit_compile_save(missing)
        wrong = call("reject_bool_to_float", "graph", "connect_graph_pins", {"blueprint_name": package,
            "source_node_id": gate["node_guid"], "source_pin_name": "Gate", "target_node_id": target["node_guid"],
            "target_pin_name": "Target", "operation_id": run + "_incompatible"}, mutation=True, expect_success=False)
        require(wrong.get("success") is False and wrong.get("stage") == "preflight"
                and wrong.get("error_code") == "conversion_contract_not_implemented" and wrong.get("changes") == []
                and wrong["data"]["connection_plan"]["direct_connection_allowed"] is False, "Incompatible link did not reject before change")
        no_explicit_compile_save(wrong)
        require(inspect("rejected_graph_unchanged") == before_links, "Negative probe changed graph")
        links = [(source, "Source", target, "Target"), (gate, "Gate", branch, "Condition"),
                 (begin, "then", target, "execute"), (target, "then", branch, "execute")]
        commands = [command("graph", "connect_graph_pins", source_node_id=s["node_guid"], source_pin_name=sp,
            target_node_id=t["node_guid"], target_pin_name=tp) for s, sp, t, tp in links]
        commands += [command("bp", "compile_and_save_blueprint", save=True)]
        linked = call("guid_links_compile_save_batch", "batch", "execute_batch", {"commands_json": json.dumps(commands),
            "operation_id": run + "_links_save"}, mutation=True)
        children = batch_children(linked, 5)
        for child in children[:4]:
            graph_mutation(child); plan = child["data"]["connection_plan"]
            require(plan["direct_connection_allowed"] is True and plan["type_exactly_equal"] is True
                    and child["data"]["topology_preserved"] is True, "Connection required conversion or changed other topology")
        compiled = children[-1]; success(compiled)
        native_compile = compiled["data"].get("compile", compiled["data"])
        require(compiled["compile"].get("compiled") is True and compiled["compile"].get("error_count") == 0
                and native_compile.get("validation_pass") is True and compiled["save"].get("saved") is True
                and len(linked["compile"]["entries"]) == len(linked["save"]["entries"]) == 1, "Expected one validated full compile/save")
        saved = fingerprint(disk); require(saved is not None, "Graph asset not saved")
        final_graph = inspect("saved_graph"); final_nodes, final_edges = graph_view(final_graph)
        expected_edges = set(edges)
        for s, sp, t, tp in links:
            expected_edges |= {(s["node_id"], "output", sp, t["node_id"], tp), (t["node_id"], "input", tp, s["node_id"], sp)}
        require({k: node_identity(v) for k, v in final_nodes.items()} == {k: node_identity(v) for k, v in nodes.items()}
                and final_edges == expected_edges, "Final graph identity or exact bidirectional edge set differs")
        cdo = call("official_default_object", "official_bp", "get_default_object", {"blueprint": {"refPath": asset_ref}})
        require(isinstance(cdo, dict) and isinstance(cdo.get("refPath"), str) and cdo["refPath"].startswith(package + "."), "CDO escaped owned asset")
        values = call("official_cdo_defaults", "objects", "get_properties", {"instance": cdo, "properties": list(DEFAULTS)})
        observed = {name: field(values, name) for name in DEFAULTS}
        require(all(typed_equal(observed[name], wanted) for name, wanted in DEFAULTS.items()), "Compiled default differs; runtime graph has not executed")
        require(call("asset_clean", "assets", "is_dirty", {"asset_path": package}) is False and fingerprint(disk) == saved, "Readback changed saved asset")
        final = call("final_editor", "native", "GetEditorDiagnostics")
        require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == [] and not report["outcome_unknown"], "Editor changed/dirty or outcome uncertain")
        report.update(status="pass", saved_graph=final_graph, expected_edges=[list(e) for e in sorted(expected_edges)],
            asset_file=saved, generated_cdo=cdo, cdo_values=observed, explicit_full_compile_requests=1, explicit_save_requests=1,
            created_member_nodes=3, created_branch_nodes=1, direct_connections=4, guid_selectors_verified=True,
            errors_verified=[missing["error_code"], wrong["error_code"]], graph_execution_scope="Compiled topology only; BeginPlay was not executed")
    except Exception as error:
        report.update(status="failed", error=str(error), failed_stage=report.get("stage", "discovery"), do_not_retry=True)
    finally:
        original_stage = report.get("stage")
        if report.get("editor") and report.get("save_policy_before") == "SoC_Never":
            try:
                policy = call("final_save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
                report["save_policy_after"] = policy.get("SaveOnCompile")
                report["save_policy_unchanged"] = report["save_policy_after"] == report["save_policy_before"]
                require(report["save_policy_unchanged"], "SaveOnCompile changed")
            except Exception as error:
                report["final_save_policy_error"] = str(error)
                if report.get("status") != "failed": report.update(error=str(error), failed_stage="final_save_policy")
                report["status"] = "failed"
            finally: report["stage"] = original_stage
        report.update(asset_after=fingerprint(disk), config_after=fingerprint(config), finished_ns=time.time_ns(), http_observations=c.observations)
        report["config_changed"] = report["config_after"] != report["config_before"]
        report["config_observation_scope"] = "Ini bytes observed only; SaveOnCompile checked separately."
        persist(); c.close()
    print(json.dumps({k: report.get(k) for k in ("status", "package", "failed_stage", "error", "outcome_unknown")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
