"""Author the UE56 mouse host's reset and component state transfer through UE58 MCP.

The isolated fixture preserves reset-before-gate and real object identity. Input
events, a real PlayerController and full FluidNinja capture remain separate work.
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
from pipelines.blueprint_duplicate import asset_disk, asset_ref, ROOT, normalized_graph
from pipelines.blueprint_component_host import TOOLSETS

CALLER = "CFNL_UpdateCapture"
PROTOTYPE_DEFAULTS = {MEMBER: 0, "SampleValid": True, "MousePressed": True,
                      "SimulationPosition": [0, 0, 0], "OwnerActor": None, "Controller": None}


def pre_aa_graph_view(graph):
    """Compare old proof bytes without AA's two additive member observations.

    Only these node-level fields are projected out. Every original node GUID,
    property path, pin and link remains exact; raw current observations are saved.
    """
    return {**graph, "nodes": [{key: value for key, value in node.items()
        if key not in {"variable_member_parent_path", "variable_member_guid"}} for node in graph["nodes"]]}


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
    run = uuid.uuid4().hex
    folder = ROOT + "/StateTransfer_" + run
    prototype, host = folder + "/BP_StateComponent", folder + "/BP_StateHost"
    prototype_class = prototype + ".BP_StateComponent_C"
    require(not asset_disk(host).parent.exists(), "Fresh destination folder required")
    report = {"schema": "cubeless_blueprint_state_transfer_v1", "status": "running", "run_id": run,
        "project_file": str(PROJECT.resolve()), "package": host, "component_package": prototype,
        "component_class": prototype_class, "source_package": source, "caller": CALLER,
        "component_report": str(proof_path), "component_report_sha256": hashlib.sha256(proof_bytes).hexdigest(),
        "source_file_before": fingerprint(source_file), "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "recorder_sha256": hashlib.sha256(Path(__file__).with_name("recorded_calls.py").read_bytes()).hexdigest(),
        "prototype_defaults": PROTOTYPE_DEFAULTS, "sample_position": [10, 20, 30],
        "historical_graph_comparison": "Exact original fields; only additive node-level variable_member_parent_path/variable_member_guid projected out",
        "stages": [], "changes": [], "started_ns": time.time_ns(), "outcome_unknown": False,
        "automatic_retries": 0, "atomic_rollback": False, "explicit_full_compile_requests": 0, "explicit_save_requests": 0,
        "cold_reload": "not_run", "runtime_execution": "not_run", "packaging": "not_run", "full_legacy_pipeline_parity": False,
        "scope": "Component bool reset before gate; external bool/Vector/Engine object members; GetOwner/IsValid/GetPlayerController signatures; component timer call",
        "pending": ["nonnull PlayerController", "InputKey", "CustomEvent", "EndPlay", "transient variable flag",
                    "full original FluidNinja host", "Shipping"]}
    recorded = RecordedCalls(args.endpoint, out, report, TOOLSETS)
    call = recorded.call

    def inspect(label, package, name=CALLER):
        value = call(label, "graph", "inspect_graph", {"blueprint_name": package, "graph_name": name})
        no_explicit_compile_save(value)
        require(value["changes"] == [], "Inspection changed state")
        graph_view(value["data"])
        return value["data"]

    def command(package, group, tool, **params):
        return {"type": TOOLSETS[group].rsplit(".", 1)[-1] + "." + tool,
                "params": {"blueprint_name": package, **params}}

    def graphcmd(tool, **params):
        return command(host, "graph", tool, graph_name=CALLER, **params)

    def batch(label, commands):
        value = call(label, "batch", "execute_batch", {"commands_json": json.dumps(commands),
            "operation_id": run + "_" + label}, mutation=True)
        return value, batch_children(value, len(commands))

    def nodes_batch(label, commands):
        value, rows = batch(label, commands)
        require(not value["compile"]["entries"] and not value["save"]["entries"], "Unexpected explicit compile/save")
        return rows

    def compile_save(label, package, commands=()):
        report["explicit_full_compile_requests"] += 1
        report["explicit_save_requests"] += 1
        value, rows = batch(label, [*commands, command(package, "bp", "compile_and_save_blueprint", save=True)])
        require(rows[-1]["compile"].get("compiled") is True and rows[-1]["compile"].get("error_count") == 0
                and rows[-1]["save"].get("saved") is True
                and len(value["compile"]["entries"]) == len(value["save"]["entries"]) == 1,
                "Expected one final successful compile/save")
        for row in rows[:-1]: graph_mutation(row)
        require(fingerprint(asset_disk(package)) is not None, "Asset was not saved")
        return rows

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
        require(pre_aa_graph_view(source_graph) == pre_aa_graph_view(proof["saved_graph"]), "Source timer graph differs")
        source_cdo = call("source_cdo", "official_bp", "get_default_object", {"blueprint": asset_ref(source)})
        source_values = call("source_defaults", "objects", "get_properties", {"instance": source_cdo, "properties": [MEMBER]})
        require(field(source_values, MEMBER) == 0, "Source component default differs")
        cloned = call("clone_component", "bp", "duplicate_blueprint_to_temp", {"blueprint_name": source,
            "target_path": prototype, "required_root": ROOT, "overwrite_existing": False, "allow_dirty_source": False,
            "compile": False, "save": False, "dry_run": False, "operation_id": run + "_clone"}, mutation=True)
        require(cloned["data"].get("created") is True and cloned["validation"].get("source_preserved") == "passed"
                and not cloned["save"]["saved"] and not asset_disk(prototype).exists(), "Unsaved source-preserving clone required")
        report["implicit_postduplicate_compile"] = cloned["compile"].get("implicit_postduplicate_compile")
        rows = nodes_batch("prototype_members", [
            command(prototype, "bp", "add_blueprint_variable", variable_name="SampleValid", variable_type="Boolean", default_value_json="true", is_exposed=True),
            command(prototype, "bp", "add_blueprint_variable", variable_name="MousePressed", variable_type="Boolean", default_value_json="true", is_exposed=True),
            command(prototype, "bp", "add_blueprint_variable", variable_name="SimulationPosition", variable_type="Vector", default_value_json="[0,0,0]"),
            command(prototype, "bp", "add_blueprint_variable", variable_name="OwnerActor", variable_type="Object", type_object="/Script/Engine.Actor"),
            command(prototype, "bp", "add_blueprint_variable", variable_name="Controller", variable_type="Object", type_object="/Script/Engine.Controller")])
        for row in rows: variable_success(row)
        compile_save("prototype_compile_save", prototype)
        prototype_graph = inspect("prototype_timer", prototype, FUNCTION)
        require(normalized_graph(pre_aa_graph_view(prototype_graph), prototype) == normalized_graph(pre_aa_graph_view(source_graph), source), "Cloned timer graph differs")
        prototype_file = fingerprint(asset_disk(prototype))
        report.update(component_file=prototype_file, component_saved_graph=prototype_graph)
        recorded.persist()
        created = call("create_host", "bp", "create_blueprint", {"name": "BP_StateHost", "parent_class": "Actor",
            "package_path": folder, "operation_id": run + "_host"}, mutation=True)
        require(not created["save"]["saved"] and not asset_disk(host).exists(), "Host saved unexpectedly")
        report["host_implicit_factory_compile"] = created["compile"].get("implicit_factory_compile")
        events = inspect("host_factory", host, "EventGraph")
        rows = nodes_batch("host_prepare", [
            command(host, "component", "add_component_to_blueprint", component_type=prototype_class, component_name="Buttons", compile_blueprint=False),
            command(host, "component", "add_component_to_blueprint", component_type=prototype_class, component_name="Capture", compile_blueprint=False),
            command(host, "bp", "add_blueprint_variable", variable_name="RequestCapture", variable_type="Boolean", default_value_json="false", is_exposed=True),
            command(host, "bp", "add_blueprint_variable", variable_name="SamplePosition", variable_type="Vector", default_value_json="[10,20,30]"),
            graphcmd("resolve_blueprint_graph", graph_type="function", create_if_missing=True),
            graphcmd("add_blueprint_function_parameter", graph_type="function", parameter_name="DeltaSeconds", parameter_type="float", direction="input")])
        for row in rows[:2]: component_success(row, mutation=True)
        for row in rows[2:4]: variable_success(row)
        for row in rows[4:]: function_success(row)
        initial = inspect("caller_entry", host)
        entries, initial_edges = graph_view(initial)
        require(len(entries) == 1 and not initial_edges, "Fresh caller must have one unlinked entry")
        entry = next(iter(entries.values()))
        definitions = []

        def member(key, name, setter=False, external=False):
            params = {"variable_name": name, "x": 300 * (len(definitions) % 6), "y": 220 * (len(definitions) // 6)}
            if external: params.update(target_class=prototype_class, graph_type="function")
            definitions.append((key, graphcmd("add_blueprint_variable_set_node" if setter else "add_blueprint_variable_get_node", **params)))

        def function(key, owner, name, defaults=None):
            definitions.append((key, graphcmd("add_blueprint_call_function_node", graph_type="function", function_class=owner,
                function_name=name, param_defaults_json=json.dumps({} if defaults is None else defaults),
                node_position_json=json.dumps([300 * (len(definitions) % 6), 220 * (len(definitions) // 6)]))))

        for key in ("Buttons", "Capture", "RequestCapture", "SamplePosition"): member(key, key)
        member("ResetValid", "SampleValid", True, True)
        member("ResetPressed", "MousePressed", True, True)
        for key in ("GateRequest", "GateOwner"):
            definitions.append((key, graphcmd("add_branch_node", x=600, y=200 if key == "GateRequest" else 500)))
        function("Owner", "/Script/Engine.ActorComponent", "GetOwner")
        function("ValidOwner", "/Script/Engine.KismetSystemLibrary", "IsValid", {"Object": None})
        function("Player", "/Script/Engine.GameplayStatics", "GetPlayerController")
        member("SetPosition", "SimulationPosition", True, True)
        member("SetOwner", "OwnerActor", True, True)
        member("SetController", "Controller", True, True)
        function("Advance", prototype_class, FUNCTION)
        member("SetValid", "SampleValid", True, True)
        member("ButtonPressed", "MousePressed", False, True)
        member("SetPressed", "MousePressed", True, True)
        rows = nodes_batch("state_nodes", [cmd for _, cmd in definitions])
        for row in rows: no_explicit_compile_save(row)
        before = inspect("before_defaults", host)
        nodes, edges = graph_view(before)
        require(not edges and len(nodes) == len(definitions) + 1, "Fresh state nodes must be unlinked")
        chosen = {key: nodes[row["data"].get("object_path") or row["data"]["node_id"]]
                  for (key, _), row in zip(definitions, rows)}
        chosen["Entry"] = entry
        report["node_roles"] = {key: node["node_id"] for key, node in chosen.items()}
        rejected = call("reject_wrong_bool_literal", "graph", "set_blueprint_pin_default", {"blueprint_name": host,
            "node_id": chosen["SetValid"]["node_guid"], "pin_name": "SampleValid", "value_json": '"invalid-bool"',
            "graph_name": CALLER, "graph_type": "function", "operation_id": run + "_bad_literal"}, mutation=True, expect_success=False)
        require(rejected.get("success") is False and rejected.get("changes") == [], "Invalid literal must fail before mutation")
        no_explicit_compile_save(rejected)
        require(inspect("rejected_literal_unchanged", host) == before, "Rejected literal changed graph")
        rows = nodes_batch("state_pin_defaults", [graphcmd("set_blueprint_pin_default", graph_type="function",
            node_id=chosen[key]["node_guid"], pin_name=pin, value_json=value) for key, pin, value in (
                ("ResetValid", "SampleValid", "false"), ("ResetPressed", "MousePressed", "false"),
                ("SetValid", "SampleValid", "true"), ("SetController", "Controller", "null"))])
        for row in rows: no_explicit_compile_save(row)
        literals = inspect("literal_readback", host)
        literal_nodes, literal_edges = graph_view(literals)
        require(not literal_edges, "Pin literals unexpectedly linked nodes")
        for role, pin_name, expected in (("ResetValid", "SampleValid", "false"), ("ResetPressed", "MousePressed", "false"), ("SetValid", "SampleValid", "true")):
            pins = [p for p in literal_nodes[chosen[role]["node_id"]]["pins"] if p["direction"] == "input" and p["name"] == pin_name]
            require(len(pins) == 1 and pins[0]["value"].casefold() == expected, "Boolean pin default differs")
        links = []

        def edge(s, sp, t, tp): links.append((chosen[s], sp, chosen[t], tp))

        chain = ["Entry", "ResetValid", "ResetPressed", "GateRequest", "GateOwner", "SetPosition", "SetOwner", "SetController", "Advance", "SetValid", "SetPressed"]
        for s, t in zip(chain, chain[1:]): edge(s, "then", t, "execute")
        edge("RequestCapture", "RequestCapture", "GateRequest", "Condition")
        edge("ValidOwner", "ReturnValue", "GateOwner", "Condition")
        edge("Buttons", "Buttons", "Owner", "self")
        edge("Owner", "ReturnValue", "ValidOwner", "Object")
        edge("Owner", "ReturnValue", "SetOwner", "OwnerActor")
        edge("SamplePosition", "SamplePosition", "SetPosition", "SimulationPosition")
        edge("Player", "ReturnValue", "SetController", "Controller")
        edge("Entry", "DeltaSeconds", "Advance", "DeltaSeconds")
        edge("Buttons", "Buttons", "ButtonPressed", "self")
        edge("ButtonPressed", "MousePressed", "SetPressed", "MousePressed")
        for key in ("ResetValid", "ResetPressed", "SetPosition", "SetOwner", "SetController", "Advance", "SetValid", "SetPressed"):
            edge("Capture", "Capture", key, "self")
        commands = [graphcmd("connect_blueprint_graph_pins", graph_type="function",
            source_node_id=s["node_guid"], source_pin=sp, target_node_id=t["node_guid"], target_pin=tp) for s, sp, t, tp in links]
        compile_save("connect_compile_save", host, commands)
        saved = inspect("saved_host", host)
        final_nodes, final_edges = graph_view(saved)
        expected_edges = set()
        for s, sp, t, tp in links:
            expected_edges |= {(s["node_id"], "output", sp, t["node_id"], tp), (t["node_id"], "input", tp, s["node_id"], sp)}
        require(final_edges == expected_edges and {k: n["node_guid"] for k, n in final_nodes.items()} ==
                {k: n["node_guid"] for k, n in nodes.items()}, "Saved topology/identity differs")
        require(inspect("events_preserved", host, "EventGraph") == events, "Host factory graph changed")
        require(inspect("prototype_preserved", prototype, FUNCTION) == prototype_graph
                and fingerprint(asset_disk(prototype)) == prototype_file, "Prototype changed during external access")
        require(inspect("source_preserved", source, FUNCTION) == source_graph and fingerprint(source_file) == proof["asset_file"], "Source changed")
        require(call("source_defaults_preserved", "objects", "get_properties", {"instance": source_cdo, "properties": [MEMBER]}) == source_values, "Source CDO changed")
        audit = call("runtime_dependencies", "native", "AuditPackageDependencies", {"packageNames": [host], "bRecursive": True, "maxPackages": 256})
        report["dependency_audit"] = audit
        require(audit.get("success") is True and audit.get("passed") is True
                and audit["incomplete_reasons"] == [] and audit["dirty_scanned_packages"] == []
                and audit["violations"] == [], "Runtime dependency audit failed")
        final = call("final_editor", "native", "GetEditorDiagnostics")
        require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == []
                and proof_path.read_bytes() == proof_bytes and not report["outcome_unknown"], "Dirty/changed/uncertain result")
        report.update(status="pass", saved_graph=saved, source_graph=source_graph, event_graph=events,
            asset_file=fingerprint(asset_disk(host)), source_file=fingerprint(source_file),
            node_count=len(final_nodes), physical_edge_count=len(final_edges) // 2,
            source_and_prototype_preserved=True, reset_before_gate_topology_verified=True)
    except Exception as error:
        report.update(status="failed", error=str(error), failed_stage=report.get("stage", "discovery"), do_not_retry=True)
    finally:
        previous = report.get("stage")
        if report.get("save_policy_before") == "SoC_Never":
            try:
                policy = call("final_save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
                report["save_policy_after"] = policy.get("SaveOnCompile")
                require(report["save_policy_after"] == "SoC_Never", "Save policy changed")
            except Exception as error:
                report["final_save_policy_error"] = str(error)
                if report["status"] != "failed": report.update(error=str(error), failed_stage="final_save_policy")
                report["status"] = "failed"
            finally: report["stage"] = previous
        report.update(component_file_after=fingerprint(asset_disk(prototype)), asset_after=fingerprint(asset_disk(host)),
                      source_file_after=fingerprint(source_file))
        recorded.finish()
    summary = {key: report.get(key) for key in ("status", "package", "failed_stage", "outcome_unknown")}
    if report.get("error"): summary.update(error=report["error"][:240], report=str(out))
    print(json.dumps(summary, ensure_ascii=True))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
