"""Author a generated ActorComponent attachment and external timer call via UE58 MCP.

Extracts the MouseButtons attachment/getter/call chain from the legacy event and
timer host pipelines. A fresh native-Actor host isolates this boundary; inherited
Tick, hardware input and the original FluidNinja host remain separate work.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from pipelines.recorded_calls import RecordedCalls
from pipelines.component_authoring import PROJECT, RECORDS, SETTINGS, fingerprint, require, component_success, field
from pipelines.blueprint_variable_graph import TOOLSETS as GRAPH_TOOLSETS, graph_view, no_explicit_compile_save, graph_mutation, batch_children
from pipelines.blueprint_function_timer import FUNCTION, MEMBER, function_success
from pipelines.blueprint_duplicate import asset_disk, asset_ref, ROOT

TOOLSETS = {**GRAPH_TOOLSETS, "component": "cubeless_mcp_ue58.registration.component_toolset.CubelessComponentTools"}
CALLER = "CFNL_AdvanceComponent"
COMPONENT = "MouseButtons"


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", required=True)
    p.add_argument("--expected-project", required=True, type=Path)
    p.add_argument("--expected-pid", required=True, type=int)
    p.add_argument("--component-report", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    args = p.parse_args(argv)
    out, source_report = args.output.resolve(), args.component_report.resolve()
    require(args.expected_project.resolve() == PROJECT.resolve(), "Exact isolated project required")
    require(out.is_relative_to(RECORDS.resolve()) and not out.exists(), "Fresh owned report required")
    require(source_report.is_relative_to(RECORDS.resolve()) and source_report.is_file(), "Owned component proof required")
    source_bytes = source_report.read_bytes()
    parent = json.loads(source_bytes.decode("utf-8-sig"))
    require(parent.get("schema") == "cubeless_blueprint_function_timer_v2" and parent.get("status") == "pass"
            and parent.get("outcome_unknown") is False and parent.get("parent_class") == "ActorComponent"
            and Path(parent["project_file"]).resolve() == PROJECT.resolve(), "Successful saved component timer proof required")
    component_package = parent["package"]
    component_disk = asset_disk(component_package)
    require(fingerprint(component_disk) == parent["asset_file"], "Component changed since its author proof")
    component_class = component_package + "." + component_package.rsplit("/", 1)[1] + "_C"
    run = uuid.uuid4().hex
    package = ROOT + "/ComponentHost_" + run + "/BP_ComponentHost"
    disk = asset_disk(package)
    require(not disk.parent.exists(), "Destination must be fresh")
    report = {"schema": "cubeless_blueprint_component_host_v1", "status": "running", "run_id": run,
        "project_file": str(PROJECT.resolve()), "package": package, "component_package": component_package,
        "component_class": component_class, "component_name": COMPONENT, "caller": CALLER, "callee": FUNCTION,
        "component_report": str(source_report), "component_report_sha256": hashlib.sha256(source_bytes).hexdigest(),
        "component_file_before": fingerprint(component_disk), "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "recorder_sha256": hashlib.sha256(Path(__file__).with_name("recorded_calls.py").read_bytes()).hexdigest(),
        "stages": [], "changes": [], "started_ns": time.time_ns(), "outcome_unknown": False,
        "automatic_retries": 0, "atomic_rollback": False, "explicit_full_compile_requests": 0, "explicit_save_requests": 0,
        "cold_reload": "not_run", "runtime_execution": "not_run", "packaging": "not_run", "full_legacy_pipeline_parity": False,
        "scope": "Saved generated non-scene ActorComponent on local SCS, its object getter and own compiled external timer call; fresh native Actor host",
        "pending": ["inherited Tick", "hardware input", "external variable setters", "scene component subclasses", "full original host"]}
    recorded = RecordedCalls(args.endpoint, out, report, TOOLSETS)
    call = recorded.call

    def inspect(label, graph_name=CALLER, target=package):
        value = call(label, "graph", "inspect_graph", {"blueprint_name": target, "graph_name": graph_name})
        no_explicit_compile_save(value)
        require(value["changes"] == [], "Graph inspection changed state")
        graph_view(value["data"])
        return value["data"]

    def components(label):
        value = call(label, "component", "list_blueprint_components", {"blueprint_name": package})
        component_success(value)
        rows = value["data"]["components"]
        require(len(rows) == len({row["component_name"] for row in rows}), "Duplicate component identity")
        return {row["component_name"]: row for row in rows}

    def command(group, tool, **params):
        return {"type": TOOLSETS[group].rsplit(".", 1)[-1] + "." + tool, "params": {"blueprint_name": package, **params}}

    try:
        recorded.discover()
        editor = call("preflight", "native", "GetEditorDiagnostics")
        require(Path(editor["project_file"]).resolve() == PROJECT.resolve() and editor["process_id"] == args.expected_pid
                and editor["engine_version"].startswith("5.8.3-") and editor["game_thread"] is True
                and not editor["pie_or_sie_active"] and editor["dirty_packages"] == [], "Wrong/dirty editor")
        report["editor"] = editor
        policy = call("save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
        report["save_policy_before"] = policy.get("SaveOnCompile")
        require(report["save_policy_before"] == "SoC_Never", "Never required; settings not changed")
        source_graph = inspect("source_timer", FUNCTION, component_package)
        require(source_graph == parent["saved_graph"], "Source timer differs from author evidence")
        source_cdo = call("source_cdo", "official_bp", "get_default_object", {"blueprint": asset_ref(component_package)})
        source_values = call("source_default", "objects", "get_properties", {"instance": source_cdo, "properties": [MEMBER]})
        require(field(source_values, MEMBER) == 0, "Source default must be zero")
        created = call("create_host", "bp", "create_blueprint", {"name": "BP_ComponentHost", "parent_class": "Actor",
            "package_path": package.rsplit("/", 1)[0], "operation_id": run + "_create"}, mutation=True)
        require(created["save"]["saved"] is False and not disk.exists(), "Creation saved unexpectedly")
        report["implicit_factory_compile"] = created["compile"].get("implicit_factory_compile")
        events = inspect("factory_events", "EventGraph")
        before_components = components("factory_components")
        require(set(before_components) == {"DefaultSceneRoot"}, "Fresh host must retain one scene root")
        invalid = call("reject_non_scene_transform", "component", "add_component_to_blueprint", {
            "blueprint_name": package, "component_type": component_class, "component_name": COMPONENT,
            "location_json": "[1,0,0]", "compile_blueprint": False, "operation_id": run + "_invalid"}, mutation=True, expect_success=False)
        require(invalid.get("success") is False and invalid.get("changes") == [], "Non-scene transform must fail without mutation")
        no_explicit_compile_save(invalid)
        require(components("invalid_unchanged") == before_components, "Invalid component request changed SCS")
        prepared = call("attach_function_parameter", "batch", "execute_batch", {"commands_json": json.dumps([
            command("component", "add_component_to_blueprint", component_type=component_class, component_name=COMPONENT, compile_blueprint=False),
            command("graph", "resolve_blueprint_graph", graph_name=CALLER, graph_type="function", create_if_missing=True),
            command("graph", "add_blueprint_function_parameter", graph_name=CALLER, graph_type="function", parameter_name="DeltaSeconds", parameter_type="float", direction="input")]),
            "operation_id": run + "_prepare"}, mutation=True)
        children = batch_children(prepared, 3)
        component_success(children[0], mutation=True)
        for child in children[1:]: function_success(child)
        require(not prepared["compile"]["entries"] and not prepared["save"]["entries"], "Unexpected full compile/save in preparation")
        attached = components("attached_components")
        require(set(attached) == {"DefaultSceneRoot", COMPONENT} and attached["DefaultSceneRoot"] == before_components["DefaultSceneRoot"], "Original scene root changed")
        component_row = attached[COMPONENT]
        require(component_row["component_class_path"] == component_class and component_row["parent_component_name"] == ""
                and "relative_transform" not in component_row, "Non-scene component class/root contract differs")
        initial = inspect("caller_entry")
        nodes, edges = graph_view(initial)
        require(len(nodes) == 1 and not edges, "Fresh caller must have one unlinked entry")
        entry = next(iter(nodes.values()))
        authored = call("getter_external_call", "batch", "execute_batch", {"commands_json": json.dumps([
            command("graph", "add_blueprint_variable_get_node", graph_name=CALLER, variable_name=COMPONENT, x=0, y=180),
            command("graph", "add_blueprint_call_function_node", graph_name=CALLER, graph_type="function", function_class=component_class,
                function_name=FUNCTION, param_defaults_json="{}", node_position_json="[320,0]")]), "operation_id": run + "_nodes"}, mutation=True)
        children = batch_children(authored, 2)
        graph_mutation(children[0]); function_success(children[1])
        require(not authored["compile"]["entries"] and not authored["save"]["entries"], "Node batch explicitly compiled/saved")
        get_data, call_data = [child["data"] for child in children]
        require(call_data["function_class"] == component_class and call_data["function_name"] == FUNCTION
                and call_data["self_context"] is False and call_data["is_static"] is False
                and call_data["target_component_class"] == component_class, "Expected external generated instance call")
        authored_graph = inspect("authored_caller")
        nodes, edges = graph_view(authored_graph)
        getter, invoke = nodes[get_data["node_id"]], nodes[call_data["object_path"]]
        require(len(nodes) == 3 and not edges and nodes[entry["node_id"]] == entry
                and getter["variable_name"] == COMPONENT and getter["self_context"] is True
                and invoke["function_path"] == call_data["function_path"]
                and invoke["function_name"] == FUNCTION, "Observed getter/call identity differs")
        links = [(getter, COMPONENT, invoke, "self"), (entry, "DeltaSeconds", invoke, "DeltaSeconds"), (entry, "then", invoke, "execute")]
        commands = [command("graph", "connect_blueprint_graph_pins", graph_name=CALLER, graph_type="function",
            source_node_id=s["node_guid"], source_pin=sp, target_node_id=t["node_guid"], target_pin=tp) for s, sp, t, tp in links]
        commands.append(command("bp", "compile_and_save_blueprint", save=True))
        report.update(explicit_full_compile_requests=1, explicit_save_requests=1)
        finished = call("connect_compile_save", "batch", "execute_batch", {"commands_json": json.dumps(commands), "operation_id": run + "_finish"}, mutation=True)
        children = batch_children(finished, 4)
        for child in children[:3]: graph_mutation(child)
        object_plan = children[0]["data"]["connection_plan"]
        require(object_plan["direct_connection_allowed"] is True and object_plan["type_exactly_equal"] is True
                and object_plan["source_pin_type"]["type_object"] == object_plan["target_pin_type"]["type_object"] == component_class,
                "Object link did not preserve the exact generated component class")
        require(children[-1]["compile"].get("compiled") is True and children[-1]["compile"].get("error_count") == 0
                and children[-1]["save"].get("saved") is True and len(finished["compile"]["entries"]) == len(finished["save"]["entries"]) == 1,
                "Expected one successful full compile/save")
        saved = fingerprint(disk)
        require(saved is not None, "Host was not saved")
        saved_graph = inspect("saved_host")
        final_nodes, final_edges = graph_view(saved_graph)
        expected_edges = set()
        for s, sp, t, tp in links:
            expected_edges |= {(s["node_id"], "output", sp, t["node_id"], tp), (t["node_id"], "input", tp, s["node_id"], sp)}
        require(final_edges == expected_edges and {k: n["node_guid"] for k,n in final_nodes.items()} == {k: n["node_guid"] for k,n in nodes.items()}, "Saved topology/identity differs")
        require(components("saved_components") == attached and inspect("events_preserved", "EventGraph") == events, "Compile/save changed component tree or EventGraph")
        require(inspect("source_preserved", FUNCTION, component_package) == source_graph and fingerprint(component_disk) == report["component_file_before"], "Source component changed")
        require(call("source_default_preserved", "objects", "get_properties", {"instance": source_cdo, "properties": [MEMBER]}) == source_values, "Source CDO changed")
        final = call("final_editor", "native", "GetEditorDiagnostics")
        require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == [] and fingerprint(disk) == saved
                and source_report.read_bytes() == source_bytes and not report["outcome_unknown"], "Dirty/changed/uncertain result")
        report.update(status="pass", saved_graph=saved_graph, components=attached, event_graph=events, source_graph=source_graph,
            asset_file=saved, component_file=report["component_file_before"], external_instance_call_node_verified=True,
            component_source_preserved=True, node_count=len(final_nodes), physical_edge_count=len(final_edges)//2,
            object_connection_plan=object_plan, component_add=prepared["data"]["batch"]["results"][0]["response"]["data"])
    except Exception as error:
        report.update(status="failed", error=str(error), failed_stage=report.get("stage", "discovery"), do_not_retry=True)
    finally:
        previous_stage = report.get("stage")
        if report.get("save_policy_before") == "SoC_Never":
            try:
                policy = call("final_save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
                report["save_policy_after"] = policy.get("SaveOnCompile")
                require(report["save_policy_after"] == "SoC_Never", "Save policy changed")
            except Exception as error:
                report["final_save_policy_error"] = str(error)
                if report["status"] != "failed": report.update(error=str(error), failed_stage="final_save_policy")
                report["status"] = "failed"
            finally: report["stage"] = previous_stage
        report.update(component_file_after=fingerprint(component_disk), asset_after=fingerprint(disk))
        recorded.finish()
    print(json.dumps({key: report.get(key) for key in ("status", "package", "failed_stage", "error", "outcome_unknown")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
