"""Author inherited Tick, preserved parent flow and gated component calls via official MCP.

This ports a concrete subset of the legacy mouse timer host. It creates isolated
parent/child fixtures; real game-world ticking, hardware input and FluidNinja
resources are outside this execution. No uncertain mutation is retried.
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
from pipelines.component_authoring import PROJECT, RECORDS, SETTINGS, fingerprint, require, field, component_success
from pipelines.blueprint_variable_graph import graph_view, no_explicit_compile_save, batch_children, graph_mutation
from pipelines.blueprint_function_timer import FUNCTION, MEMBER, function_success
from pipelines.blueprint_variables import variable_success
from pipelines.blueprint_duplicate import asset_disk, asset_ref, ROOT
from pipelines.blueprint_component_host import TOOLSETS, COMPONENT
from pipelines.blueprint_gated_timer import GATES

PARENT_MARKER, CHILD_MARKER = "ParentDelta", "ChildDelta"
EVENT = "ReceiveTick"


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", required=True)
    p.add_argument("--expected-project", type=Path, required=True)
    p.add_argument("--expected-pid", type=int, required=True)
    p.add_argument("--component-report", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args(argv)
    out, proof_path = args.output.resolve(), args.component_report.resolve()
    require(args.expected_project.resolve() == PROJECT.resolve(), "Exact isolated project required")
    require(out.is_relative_to(RECORDS.resolve()) and not out.exists(), "Fresh owned report required")
    require(proof_path.is_relative_to(RECORDS.resolve()) and proof_path.is_file(), "Owned source proof required")
    proof_bytes = proof_path.read_bytes()
    proof = json.loads(proof_bytes.decode("utf-8-sig"))
    require(proof.get("schema") == "cubeless_blueprint_function_timer_v2" and proof.get("status") == "pass"
            and proof.get("outcome_unknown") is False and proof.get("parent_class") == "ActorComponent"
            and Path(proof["project_file"]).resolve() == PROJECT.resolve(), "Saved ActorComponent timer proof required")
    source = proof["package"]
    source_file = asset_disk(source)
    require(fingerprint(source_file) == proof["asset_file"], "Source component differs from proof")
    source_class = source + "." + source.rsplit("/", 1)[1] + "_C"
    run = uuid.uuid4().hex
    folder = ROOT + "/InheritedTick_" + run
    parent, child = folder + "/BP_TickParent", folder + "/BP_TickHost"
    parent_class = parent + ".BP_TickParent_C"
    require(not asset_disk(parent).parent.exists(), "Fresh destination folder required")
    report = {"schema": "cubeless_blueprint_inherited_tick_v1", "status": "running", "run_id": run,
        "project_file": str(PROJECT.resolve()), "parent_package": parent, "package": child,
        "component_package": source, "component_class": source_class, "parent_class": parent_class,
        "gate_order": list(GATES), "event": EVENT, "parent_marker": PARENT_MARKER, "child_marker": CHILD_MARKER,
        "component_report": str(proof_path), "component_report_sha256": hashlib.sha256(proof_bytes).hexdigest(),
        "component_file_before": fingerprint(source_file), "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "recorder_sha256": hashlib.sha256(Path(__file__).with_name("recorded_calls.py").read_bytes()).hexdigest(),
        "stages": [], "changes": [], "started_ns": time.time_ns(), "outcome_unknown": False,
        "automatic_retries": 0, "atomic_rollback": False, "explicit_full_compile_requests": 0, "explicit_save_requests": 0,
        "cold_reload": "not_run", "runtime_execution": "not_run", "packaging": "not_run",
        "full_legacy_pipeline_parity": False, "parent_call_is_new_extension": True,
        "scope": "Saved Actor Blueprint parent; exact Tick override/parent call; inherited Boolean gates; guarded old-flow reconnect; local generated ActorComponent timer call",
        "pending": ["actual game-world ticking", "hardware input", "external variable setters", "press/release reset",
                    "full original FluidNinja host", "Shipping"]}
    recorded = RecordedCalls(args.endpoint, out, report, TOOLSETS)
    call = recorded.call

    def inspect(label, package, name="EventGraph"):
        value = call(label, "graph", "inspect_graph", {"blueprint_name": package, "graph_name": name})
        no_explicit_compile_save(value)
        require(value["changes"] == [], "Inspection changed state")
        graph_view(value["data"])
        return value["data"]

    def command(package, group, tool, **params):
        return {"type": TOOLSETS[group].rsplit(".", 1)[-1] + "." + tool,
                "params": {"blueprint_name": package, **params}}

    def batch(label, commands):
        value = call(label, "batch", "execute_batch", {"commands_json": json.dumps(commands),
            "operation_id": run + "_" + label}, mutation=True)
        rows = batch_children(value, len(commands))
        return value, rows

    def graphcmd(package, tool, **params):
        return command(package, "graph", tool, graph_name="EventGraph", **params)

    def link(package, s, sp, t, tp, **extra):
        return graphcmd(package, "connect_blueprint_graph_pins", graph_type="event",
            source_node_id=s["node_guid"], source_pin=sp, target_node_id=t["node_guid"], target_pin=tp, **extra)

    def edges_for(links):
        edges = set()
        for s, sp, t, tp in links:
            edges |= {(s["node_id"], "output", sp, t["node_id"], tp),
                      (t["node_id"], "input", tp, s["node_id"], sp)}
        return edges

    def one(nodes, predicate):
        rows = [v for v in nodes.values() if predicate(v)]
        require(len(rows) == 1, "Exact unique graph node required")
        return rows[0]

    def checked_save(value, rows):
        require(rows[-1]["compile"].get("compiled") is True and rows[-1]["compile"].get("error_count") == 0
                and rows[-1]["save"].get("saved") is True
                and len(value["compile"]["entries"]) == len(value["save"]["entries"]) == 1,
                "Expected one final successful compile/save")
        for row in rows[:-1]: graph_mutation(row)

    try:
        recorded.discover()
        editor = call("preflight", "native", "GetEditorDiagnostics")
        require(Path(editor["project_file"]).resolve() == PROJECT.resolve() and editor["process_id"] == args.expected_pid
                and editor["engine_version"].startswith("5.8.3-") and editor["game_thread"] is True
                and not editor["pie_or_sie_active"] and editor["dirty_packages"] == [], "Wrong/dirty editor")
        report["editor"] = editor
        policy = call("save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
        report["save_policy_before"] = policy.get("SaveOnCompile")
        require(report["save_policy_before"] == "SoC_Never", "Never save policy required; settings not changed")
        source_graph = inspect("source_timer", source, FUNCTION)
        require(source_graph == proof["saved_graph"], "Source timer graph differs")
        source_cdo = call("source_cdo", "official_bp", "get_default_object", {"blueprint": asset_ref(source)})
        source_values = call("source_defaults", "objects", "get_properties", {"instance": source_cdo, "properties": [MEMBER]})
        require(field(source_values, MEMBER) == 0, "Source component default differs")
        created = call("create_parent", "bp", "create_blueprint", {"name": "BP_TickParent", "parent_class": "Actor",
            "package_path": folder, "operation_id": run + "_parent"}, mutation=True)
        require(not created["save"]["saved"] and not asset_disk(parent).exists(), "Parent saved unexpectedly")
        report["parent_implicit_factory_compile"] = created["compile"].get("implicit_factory_compile")
        parent_factory = inspect("parent_factory", parent)
        rejected = call("reject_missing_event", "graph", "add_blueprint_event_node", {
            "blueprint_name": parent, "event_name": "CubelessMissingEvent", "graph_name": "EventGraph",
            "graph_type": "event", "operation_id": run + "_missing_event"}, mutation=True, expect_success=False)
        require(rejected.get("success") is False and rejected.get("changes") == []
                and rejected["data"]["native_outcome"]["mutation_attempted"] is False,
                "Missing event must fail before node creation")
        no_explicit_compile_save(rejected)
        require(inspect("missing_event_unchanged", parent) == parent_factory, "Rejected event changed graph")
        commands = [command(parent, "bp", "add_blueprint_variable", variable_name=key, variable_type="Boolean",
            default_value_json="false", is_exposed=True) for key in GATES]
        commands += [command(parent, "bp", "add_blueprint_variable", variable_name=PARENT_MARKER, variable_type="Float", default_value_json="0"),
            graphcmd(parent, "add_blueprint_event_node", event_name=EVENT, graph_type="event"),
            graphcmd(parent, "add_blueprint_variable_set_node", variable_name=PARENT_MARKER, x=400, y=0)]
        prepared, rows = batch("parent_nodes", commands)
        for row in rows[:4]: variable_success(row)
        function_success(rows[4]); graph_mutation(rows[5])
        require(not prepared["compile"]["entries"] and not prepared["save"]["entries"], "Intermediate parent compile/save")
        parent_before = inspect("parent_before_links", parent)
        nodes, existing = graph_view(parent_before)
        event = nodes[rows[4]["data"]["object_path"]]
        setter = one(nodes, lambda n:n["class"]=="K2Node_VariableSet" and n.get("variable_name")==PARENT_MARKER)
        parent_links = [(event, "then", setter, "execute"), (event, "DeltaSeconds", setter, PARENT_MARKER)]
        report["explicit_full_compile_requests"] += 1; report["explicit_save_requests"] += 1
        finished, rows = batch("parent_compile_save", [link(parent,*v) for v in parent_links]
            + [command(parent,"bp","compile_and_save_blueprint",save=True)])
        checked_save(finished, rows)
        parent_saved = inspect("parent_saved", parent)
        require(graph_view(parent_saved)[1] == existing | edges_for(parent_links), "Parent topology differs")
        parent_file = fingerprint(asset_disk(parent))
        require(parent_file is not None, "Parent not saved")
        report.update(parent_saved_graph=parent_saved, parent_file=parent_file, parent_factory_graph=parent_factory)
        recorded.persist()
        created = call("create_inherited_child", "bp", "create_blueprint", {"name":"BP_TickHost", "parent_class":parent_class,
            "package_path":folder, "operation_id":run+"_child"}, mutation=True)
        require(not created["save"]["saved"] and not asset_disk(child).exists(), "Child saved unexpectedly")
        report["child_implicit_factory_compile"] = created["compile"].get("implicit_factory_compile")
        child_factory = inspect("child_factory", child)
        prepared, rows = batch("child_state_and_event", [
            command(child,"component","add_component_to_blueprint",component_type=source_class,component_name=COMPONENT,compile_blueprint=False),
            command(child,"bp","add_blueprint_variable",variable_name=CHILD_MARKER,variable_type="Float",default_value_json="0"),
            graphcmd(child,"add_blueprint_event_node",event_name=EVENT,graph_type="event")])
        component_success(rows[0],mutation=True);variable_success(rows[1]);function_success(rows[2])
        event_path=rows[2]["data"]["object_path"]
        parent_call=call("child_parent_call","graph","add_blueprint_parent_function_call",{
            "blueprint_name":child,"event_node_id":event_path,"graph_name":"EventGraph","graph_type":"event",
            "node_position_json":"[300,0]","operation_id":run+"_parent_call"},mutation=True)
        function_success(parent_call)
        commands=[graphcmd(child,"add_blueprint_sequence_node",graph_type="event",node_position_json="[600,0]"),
            graphcmd(child,"add_blueprint_variable_set_node",variable_name=CHILD_MARKER,x=900,y=-200),
            graphcmd(child,"add_blueprint_variable_get_node",variable_name=COMPONENT,x=1500,y=600),
            graphcmd(child,"add_blueprint_call_function_node",graph_type="event",function_class=source_class,
                function_name=FUNCTION,node_position_json="[1800,300]")]
        for i,key in enumerate(GATES):
            commands += [graphcmd(child,"add_blueprint_variable_get_node",variable_name=key,x=700+350*i,y=500),
                graphcmd(child,"add_branch_node",x=900+350*i,y=300)]
        authored, rows=batch("child_nodes",commands)
        require(not prepared["compile"]["entries"] and not authored["compile"]["entries"]
                and not prepared["save"]["entries"] and not authored["save"]["entries"],"Intermediate child compile/save")
        for i,row in enumerate(rows):
            function_success(row) if i==3 else graph_mutation(row)
        before=inspect("child_before_links",child)
        nodes, initial_edges=graph_view(before)
        event=nodes[event_path];parent_node=nodes[parent_call["data"]["object_path"]]
        sequence=one(nodes,lambda n:n["class"]=="K2Node_ExecutionSequence")
        marker=one(nodes,lambda n:n["class"]=="K2Node_VariableSet" and n.get("variable_name")==CHILD_MARKER)
        component=one(nodes,lambda n:n["class"]=="K2Node_VariableGet" and n.get("variable_name")==COMPONENT)
        invoke=nodes[rows[3]["data"]["object_path"]]
        gates=[nodes[rows[4+2*i]["data"]["node_id"]] for i in range(3)]
        branches=[nodes[rows[5+2*i]["data"]["node_id"]] for i in range(3)]
        original_links=[(event,"then",parent_node,"execute"),(event,"DeltaSeconds",parent_node,"DeltaSeconds"),
                        (parent_node,"then",marker,"execute"),(event,"DeltaSeconds",marker,CHILD_MARKER)]
        linked, results=batch("retain_existing_flow",[link(child,*v) for v in original_links])
        for result in results:graph_mutation(result)
        original=inspect("before_guarded_insert",child)
        require(graph_view(original)[1]==initial_edges | edges_for(original_links),"Existing flow differs")
        replacement=link(child,parent_node,"then",sequence,"execute",allow_pin_link_replacement=True,
            expected_existing_source_link_count=1,expected_existing_source_link_target_node_id=marker["node_guid"],
            expected_existing_source_link_target_pin="execute",expected_schema_response="break_others_a")
        remaining=[(sequence,"then_0",marker,"execute"),(sequence,"then_1",branches[0],"execute"),
            (component,COMPONENT,invoke,"self"),(event,"DeltaSeconds",invoke,"DeltaSeconds"),
            *[(gates[i],GATES[i],branches[i],"Condition") for i in range(3)],
            (branches[0],"then",branches[1],"execute"),(branches[1],"then",branches[2],"execute"),
            (branches[2],"then",invoke,"execute")]
        report["explicit_full_compile_requests"]+=1;report["explicit_save_requests"]+=1
        completed, results=batch("child_replace_connect_save",[replacement]+[link(child,*v) for v in remaining]
            +[command(child,"bp","compile_and_save_blueprint",save=True)])
        checked_save(completed,results)
        require(results[0]["data"].get("replacement_guard_verified") is True,"Guarded replacement not verified")
        saved=inspect("child_saved",child)
        final_nodes,final_edges=graph_view(saved)
        expected=(initial_edges | edges_for(original_links))-edges_for([(parent_node,"then",marker,"execute")])
        expected |= edges_for([(parent_node,"then",sequence,"execute")]+remaining)
        require(final_edges==expected and {k:v["node_guid"] for k,v in final_nodes.items()}==
                {k:v["node_guid"] for k,v in nodes.items()},"Saved child topology/identity differs")
        require(inspect("parent_preserved",parent)==parent_saved and fingerprint(asset_disk(parent))==parent_file,"Parent changed")
        require(inspect("component_preserved",source,FUNCTION)==source_graph and fingerprint(source_file)==proof["asset_file"],"Source changed")
        require(call("source_defaults_preserved","objects","get_properties",{"instance":source_cdo,"properties":[MEMBER]})==source_values,"Source CDO changed")
        final=call("final_editor","native","GetEditorDiagnostics")
        require(final["instance_id"]==editor["instance_id"] and final["dirty_packages"]==[]
                and proof_path.read_bytes()==proof_bytes and not report["outcome_unknown"],"Dirty/changed/uncertain result")
        report.update(status="pass",saved_graph=saved,child_factory_graph=child_factory,source_graph=source_graph,
            asset_file=fingerprint(asset_disk(child)),component_file=fingerprint(source_file),
            node_count=len(final_nodes),physical_edge_count=len(final_edges)//2,
            parent_call=parent_call["data"],inherited_getters=[rows[4+2*i]["data"] for i in range(3)],
            parent_and_source_preserved=True,guarded_old_flow_reconnected=True)
    except Exception as error:
        report.update(status="failed",error=str(error),failed_stage=report.get("stage","discovery"),do_not_retry=True)
    finally:
        previous=report.get("stage")
        if report.get("save_policy_before")=="SoC_Never":
            try:
                policy=call("final_save_policy","objects","get_properties",{"instance":{"refPath":SETTINGS},"properties":["SaveOnCompile"]})
                report["save_policy_after"]=policy.get("SaveOnCompile")
                require(report["save_policy_after"]=="SoC_Never","Save policy changed")
            except Exception as error:
                report["final_save_policy_error"]=str(error)
                if report["status"]!="failed":report.update(error=str(error),failed_stage="final_save_policy")
                report["status"]="failed"
            finally:report["stage"]=previous
        report.update(parent_file_after=fingerprint(asset_disk(parent)),asset_after=fingerprint(asset_disk(child)),
                      component_file_after=fingerprint(source_file))
        recorded.finish()
    summary = {key: report.get(key) for key in ("status", "package", "failed_stage", "outcome_unknown")}
    failed = next((s for s in reversed(report["stages"]) if s["name"] == report.get("failed_stage")), {})
    if report.get("error"):
        summary.update(error_code=failed.get("result", {}).get("error_code"), error=report["error"][:240], report=str(out))
    print(json.dumps(summary))
    return 0 if report["status"]=="pass" else 1


if __name__=="__main__":
    raise SystemExit(main())
