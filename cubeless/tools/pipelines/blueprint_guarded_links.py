"""Insert a Sequence before an existing gated caller using exact old-edge guards.

Ports the documented timer_host replacement/reconnect boundary. The current
legacy C++ source lacks the expected guards its historical pipelines require.
No full host, inherited Tick, data replacement or atomic rollback is claimed.
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
from pipelines.component_authoring import PROJECT, RECORDS, SETTINGS, fingerprint, require, field
from pipelines.blueprint_variable_graph import TOOLSETS, graph_view, no_explicit_compile_save, graph_mutation, batch_children
from pipelines.blueprint_variables import typed_equal
from pipelines.blueprint_duplicate import asset_disk, asset_ref, normalized_graph, ROOT
from pipelines.blueprint_gated_timer import CALLER, FUNCTION, DEFAULTS


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", required=True)
    p.add_argument("--expected-project", required=True, type=Path)
    p.add_argument("--expected-pid", required=True, type=int)
    p.add_argument("--parent-report", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    args = p.parse_args(argv)
    out, parent_path = args.output.resolve(), args.parent_report.resolve()
    require(args.expected_project.resolve() == PROJECT.resolve(), "Exact isolated project required")
    require(out.is_relative_to(RECORDS.resolve()) and not out.exists(), "Fresh verification report required")
    require(parent_path.is_relative_to(RECORDS.resolve()) and parent_path.is_file(), "Owned parent proof required")
    parent_bytes = parent_path.read_bytes()
    parent = json.loads(parent_bytes.decode("utf-8-sig"))
    require(parent.get("schema") == "cubeless_blueprint_gated_timer_v1" and parent.get("status") == "pass"
            and parent.get("outcome_unknown") is False and parent.get("expected_defaults") == DEFAULTS
            and Path(parent["project_file"]).resolve() == PROJECT.resolve(), "Successful own gated timer author proof required")
    source = parent["package"]
    source_file = asset_disk(source)
    require(fingerprint(source_file) == parent["asset_file"], "Parent asset differs from proof")
    run = uuid.uuid4().hex
    package = ROOT + "/GatedTimer_" + run + "/BP_GatedTimer"
    disk = asset_disk(package)
    require(not disk.parent.exists(), "Fresh destination required")
    report = {"schema": "cubeless_blueprint_guarded_links_v1", "status": "running", "run_id": run,
        "project_file": str(PROJECT.resolve()), "source_package": source, "package": package,
        "parent_report": str(parent_path), "parent_report_sha256": hashlib.sha256(parent_bytes).hexdigest(),
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "recorder_sha256": hashlib.sha256(Path(__file__).with_name("recorded_calls.py").read_bytes()).hexdigest(),
        "source_file_before": fingerprint(source_file), "expected_defaults": DEFAULTS,
        "stages": [], "changes": [], "started_ns": time.time_ns(), "outcome_unknown": False,
        "automatic_retries": 0, "atomic_rollback": False, "cold_reload": "not_run",
        "runtime_execution": "not_run", "packaging": "not_run", "full_legacy_pipeline_parity": False,
        "scope": "Exact one-source/one-old/empty-new exec replacement, followed by explicit old-flow reconnect on a fresh clone; no inherited Tick or external component references"}
    recorded = RecordedCalls(args.endpoint, out, report, TOOLSETS)
    call = recorded.call

    def inspect(label, name=CALLER, target=package):
        value = call(label, "graph", "inspect_graph", {"blueprint_name": target, "graph_name": name})
        no_explicit_compile_save(value)
        require(value["changes"] == [], "Inspection changed graph")
        graph_view(value["data"])
        return value["data"]

    def command(group, tool, **params):
        return {"type": TOOLSETS[group].rsplit(".", 1)[-1] + "." + tool, "params": {"blueprint_name": package, **params}}

    def reject(label, inputs, operation_id, before):
        value = call(label, "graph", "connect_blueprint_graph_pins", {**inputs, "operation_id": operation_id}, mutation=True, expect_success=False)
        require(value.get("success") is False and value.get("error_code") == "connection_replacement_guard_mismatch"
                and value.get("changes") == [], "Expected unchanged stale/mismatched guard rejection")
        no_explicit_compile_save(value)
        require(inspect(label + "_unchanged") == before, "Rejected guard changed the graph")
        return value

    try:
        recorded.discover()
        editor = call("preflight", "native", "GetEditorDiagnostics")
        require(Path(editor["project_file"]).resolve() == PROJECT.resolve() and editor["process_id"] == args.expected_pid
                and editor["engine_version"].startswith("5.8.3-") and editor["game_thread"] is True
                and not editor["pie_or_sie_active"] and editor["dirty_packages"] == [], "Wrong/dirty editor")
        report["editor"] = editor
        policy = call("save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
        report["save_policy_before"] = policy.get("SaveOnCompile")
        require(report["save_policy_before"] == "SoC_Never", "Never required; no settings changed")
        require(inspect("source_caller", target=source) == parent["saved_graph"], "Source differs from parent graph proof")
        clone = call("clone_unsaved", "bp", "duplicate_blueprint_to_temp", {"blueprint_name": source, "target_path": package,
            "required_root": ROOT, "dry_run": False, "compile": False, "save": False, "operation_id": run + "_clone"}, mutation=True)
        require(clone["data"].get("created") is True and not clone["save"].get("saved") and not disk.exists(), "Clone saved unexpectedly")
        report["implicit_postduplicate_compile"] = clone["compile"].get("implicit_postduplicate_compile")
        original = inspect("cloned_caller")
        require(normalized_graph(original, package) == normalized_graph(parent["saved_graph"], source), "Clone caller content differs")
        timer, events = inspect("cloned_timer", FUNCTION), inspect("cloned_events", "EventGraph")
        nodes, old_edges = graph_view(original)
        entries = [node for node in nodes.values() if node["class"] == "K2Node_FunctionEntry"]
        require(len(entries) == 1, "Unique entry required")
        entry = entries[0]
        entry_links = [edge for edge in old_edges if edge[0] == entry["node_id"] and edge[1:3] == ("output", "then")]
        require(len(entry_links) == 1, "Exactly one outgoing entry edge required")
        old_target = nodes[entry_links[0][3]]
        old_pin = entry_links[0][4]
        require(old_target["class"] == "K2Node_ExecutionSequence" and old_pin == "execute", "Expected existing Sequence endpoint")
        added = call("insert_sequence", "graph", "add_blueprint_sequence_node", {"blueprint_name": package,
            "graph_name": CALLER, "graph_type": "function", "node_position_json": "[-250,0]", "operation_id": run + "_sequence"}, mutation=True)
        graph_mutation(added)
        before = inspect("before_replacement")
        all_nodes, before_edges = graph_view(before)
        inserted = all_nodes[added["data"]["object_path"]]
        require(set(all_nodes) == set(nodes) | {inserted["node_id"]} and before_edges == old_edges,
                "Sequence creation changed previous topology")
        require({(pin["name"], pin["direction"]) for pin in inserted["pins"]}
                == {("execute", "input"), ("then_0", "output"), ("then_1", "output")}, "Default two-output Sequence differs")
        inputs = {"blueprint_name": package, "graph_name": CALLER, "graph_type": "function",
            "source_node_id": entry["node_guid"], "source_pin": "then", "target_node_id": inserted["node_guid"], "target_pin": "execute",
            "allow_pin_link_replacement": True, "expected_existing_source_link_count": 1,
            "expected_existing_source_link_target_node_id": old_target["node_guid"], "expected_existing_source_link_target_pin": old_pin,
            "expected_schema_response": "break_others_a", "compile": False, "save_asset": False}
        wrong = next(node for node in nodes.values() if node["class"] == "K2Node_VariableSet")
        reject("wrong_old_endpoint", {**inputs, "expected_existing_source_link_target_node_id": wrong["node_guid"]}, run + "_wrong", before)
        replaced = call("replace", "graph", "connect_blueprint_graph_pins", {**inputs, "operation_id": run + "_replace"}, mutation=True)
        graph_mutation(replaced)
        require(replaced["data"].get("replacement_guard_verified") is True
                and replaced["data"].get("expected_existing_link_removed") is True
                and replaced["data"].get("replaced_source_link_count") == 1, "Replacement guard/removal not verified")
        after = inspect("after_replacement")
        _, after_edges = graph_view(after)
        removed = {(entry["node_id"], "output", "then", old_target["node_id"], old_pin),
                   (old_target["node_id"], "input", old_pin, entry["node_id"], "then")}
        created = {(entry["node_id"], "output", "then", inserted["node_id"], "execute"),
                   (inserted["node_id"], "input", "execute", entry["node_id"], "then")}
        require(after_edges == ((before_edges - removed) | created), "Replacement removed/added unexpected edges")
        replay = call("same_id_replay", "graph", "connect_blueprint_graph_pins", {**inputs, "operation_id": run + "_replace"}, mutation=True)
        require(replay["retry"].get("replayed") is True and replay["success"] is True, "Same request ID did not replay")
        reject("new_id_stale_guard", inputs, run + "_stale", after)
        commands = [command("graph", "connect_blueprint_graph_pins", graph_name=CALLER, graph_type="function",
            source_node_id=inserted["node_guid"], source_pin="then_0", target_node_id=old_target["node_guid"], target_pin=old_pin),
            command("bp", "compile_and_save_blueprint", save=True)]
        finished = call("reconnect_compile_save", "batch", "execute_batch", {"commands_json": json.dumps(commands), "operation_id": run + "_finish"}, mutation=True)
        children = batch_children(finished, 2)
        graph_mutation(children[0])
        require(children[1]["compile"].get("compiled") is True and children[1]["compile"].get("error_count") == 0
                and children[1]["save"].get("saved") is True
                and len(finished["compile"]["entries"]) == len(finished["save"]["entries"]) == 1, "Expected one successful full compile/save")
        saved = fingerprint(disk)
        require(saved is not None, "No saved clone")
        final_graph = inspect("saved_caller")
        final_nodes, final_edges = graph_view(final_graph)
        reconnected = {(inserted["node_id"], "output", "then_0", old_target["node_id"], old_pin),
                       (old_target["node_id"], "input", old_pin, inserted["node_id"], "then_0")}
        require(final_edges == after_edges | reconnected
                and {key: node["node_guid"] for key, node in final_nodes.items()} == {key: node["node_guid"] for key, node in all_nodes.items()}, "Final topology/identity differs")
        require(inspect("timer_preserved", FUNCTION) == timer and inspect("events_preserved", "EventGraph") == events, "Other graph changed")
        require(inspect("source_preserved", target=source) == parent["saved_graph"] and fingerprint(source_file) == report["source_file_before"], "Source changed")
        cdo = call("cdo", "official_bp", "get_default_object", {"blueprint": asset_ref(package)})
        values = call("defaults", "objects", "get_properties", {"instance": cdo, "properties": list(DEFAULTS)})
        require(all(typed_equal(field(values, key), value) for key, value in DEFAULTS.items()), "Defaults changed")
        final = call("final_editor", "native", "GetEditorDiagnostics")
        require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == [] and fingerprint(disk) == saved
                and parent_path.read_bytes() == parent_bytes and not report["outcome_unknown"], "Dirty/changed/uncertain result")
        report.update(status="pass", saved_graph=final_graph, timer_graph=timer, event_graph=events, asset_file=saved,
            explicit_full_compile_requests=1, explicit_save_requests=1, replacement_guard_verified=True,
            wrong_old_endpoint_rejected=True, stale_new_id_rejected=True, same_id_replayed=True,
            removed_directed_edges=[list(edge) for edge in sorted(removed)], added_directed_edges=[list(edge) for edge in sorted(created | reconnected)],
            source_preserved=True, node_count=len(final_nodes), physical_edge_count=len(final_edges)//2)
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
            finally:
                report["stage"] = previous_stage
        report.update(source_file_after=fingerprint(source_file), asset_after=fingerprint(disk))
        recorded.finish()
    print(json.dumps({key: report.get(key) for key in ("status", "package", "failed_stage", "error", "outcome_unknown")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
