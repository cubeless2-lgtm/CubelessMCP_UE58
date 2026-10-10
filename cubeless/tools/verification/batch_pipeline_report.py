"""Independent checks for bounded Blueprint batch live-MCP evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import re

AUTHOR_STAGES = ["preflight", "batch_lifecycle", "standalone_create", "standalone_compile_save", "standalone_inspect", "replay_lifecycle", "conflicting_id", "independent_batch_identity", "unsupported_batch", "unsupported_prefix_absent", "stop_true", "stop_true_dirty_readback", "preserve_stop_true", "stop_false", "stop_false_clean_readback", "graph_add_batch", "graph_edit_batch", "graph_independent_readback", "graph_compile_save_batch", "final_batch_identity", "final_true_identity", "final_false_identity", "final_standalone_identity", "final_editor"]
RELOAD_STAGES = ["preflight", "reload_read_batch", "reload_editor"]


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def identity(value, package, dirty=False):
    require(value.get("success") is True, "Blueprint read failed")
    data = value.get("data", {})
    require(data.get("path") == package and data.get("parent_class") == "/Script/Engine.Actor", "Blueprint identity/parent differs")
    require(data.get("dirty") is dirty, "Blueprint dirty state differs")


def batch_result(value, *, total, successes=None, failure_index=None, stop=True):
    summary = value.get("data", {}).get("batch", {})
    require(summary.get("manifest_version") == "blueprint-graph-v1", "Unknown batch manifest")
    require(summary.get("total") == total and summary.get("stop_on_error") is stop, "Batch input summary differs")
    executed = failure_index + 1 if failure_index is not None and stop else total
    successes = executed - int(failure_index is not None) if successes is None else successes
    require(summary.get("executed") == executed and summary.get("dispatched_count") == executed, "Batch executed/dispatched count differs")
    require(summary.get("succeeded") == successes and summary.get("failed") == int(failure_index is not None), "Batch success/failure counts differ")
    require(summary.get("not_run") == list(range(executed, total)), "Batch skipped indices differ")
    require(summary.get("forced_stop") is False and summary.get("mutation_state_unknown") is False and summary.get("command_may_still_be_in_flight") is False, "Unexpected forced stop/uncertain state")
    require(value.get("success") is (failure_index is None) and summary.get("success") is (failure_index is None), "Batch success contradicts children")
    require(value.get("retry", {}).get("automatic_retry") is False, "Automatic batch retry permitted")
    rows = summary.get("results", [])
    require(len(rows) == executed and [row.get("index") for row in rows] == list(range(executed)), "Child row coverage/order differs")
    for index, row in enumerate(rows):
        expected = index != failure_index
        require(row.get("phase") == "returned" and row.get("status") == ("success" if expected else "error") and row.get("response", {}).get("success") is expected, "Child outcome differs: " + str(index))
        require(row.get("mutation_state_unknown") is False and row.get("command_may_still_be_in_flight") is False, "Child result is uncertain")
        require(row.get("response", {}).get("retry", {}).get("automatic_retry") is False, "Child retry policy differs")
    if failure_index is not None:
        row = rows[failure_index]
        require(row.get("error_code") == "asset_already_exists" and row.get("response", {}).get("changes") == [], "Expected known duplicate-asset guard")
        require(row.get("mutation_attempted") is False and row.get("mutation_performed") is False, "Rejected duplicate mutated state")
        require(summary.get("partial_mutation") is True, "Created prefix must be recorded as partial mutation")
        require(summary.get("stopped_at_index") == (failure_index if stop else -1), "Wrong stop index")
    transaction = summary.get("transaction", {})
    require(transaction.get("atomic_rollback") is False and transaction.get("disk_save_rollback") is False and transaction.get("group_undo_verified") is False, "Batch overstates rollback/Undo proof")
    return [row["response"] for row in rows]


def saved(value):
    compiled, saved_state, data = value.get("compile", {}), value.get("save", {}), value.get("data", {})
    require(value.get("success") is True and compiled.get("requested") is True and compiled.get("attempted") is True and compiled.get("compiled") is True, "Explicit Blueprint compile did not pass")
    require(compiled.get("error_count") == 0 and compiled.get("status") in {"up_to_date", "up_to_date_with_warnings"} and data.get("validation_pass") is True and data.get("compile_status") == "passed" and data.get("compile_error_count") == 0 and data.get("compile_errors") == [], "Compile diagnostics contradict success")
    require(value.get("validation", {}).get("status") == "compile_passed", "Compile validation status differs")
    require(saved_state.get("requested") is True and saved_state.get("attempted") is True and saved_state.get("saved") is True and saved_state.get("status") == "saved", "Explicit Blueprint save did not pass")
    require(data.get("package_dirty_after") is False and data.get("after", {}).get("dirty") is False, "Saved Blueprint remains dirty")


def graph(value):
    require(value.get("success") is True and isinstance(value.get("data", {}).get("nodes"), list), "Graph read failed")
    result = {key: value["data"].get(key) for key in ("graph_name", "node_count", "nodes")}
    nodes = result["nodes"]
    require(result["node_count"] == len(nodes) and len({node["node_id"] for node in nodes}) == len(nodes), "Graph node count/identity differs")
    by_id = {node["node_id"]: node for node in nodes}
    for node in nodes:
        for pin in node["pins"]:
            for link in pin["links"]:
                require(link["node_id"] in by_id, "Dangling graph link references absent node")
                peer_pins = [peer for peer in by_id[link["node_id"]]["pins"] if peer["name"] == link["pin_name"]]
                require(len(peer_pins) == 1 and {"node_id": node["node_id"], "pin_name": pin["name"]} in peer_pins[0]["links"], "Graph connection lacks exact reverse link")
    return result


def verify(report, *, require_phase=None, parent=None, parent_bytes=None):
    errors = []
    def check(fn):
        try:
            fn()
        except (AssertionError, KeyError, TypeError, ValueError, IndexError, StopIteration) as error:
            errors.append(str(error))
    def run():
        phase = report.get("phase")
        require(report.get("schema") == "cubeless_blueprint_batch_pipeline_v1" and phase in {"author", "reload"}, "Unexpected batch pipeline schema/phase")
        require(require_phase is None or phase == require_phase, "Wrong requested phase")
        require(report.get("status") == "pass" and report.get("outcome_unknown") is False and report.get("automatic_retries") == 0, "Phase failed or outcome uncertain")
        run_id = report.get("run_id", "")
        require(re.fullmatch("[a-f0-9]{32}", run_id) is not None, "Invalid run ID")
        root = "/Game/CubelessMCPTests/Batch_" + run_id
        assets = {key: root + "/" + name for key, name in {"batch": "BP_Batch", "standalone": "BP_Standalone", "stop_true": "BP_StopTrue", "stop_false": "BP_StopFalse", "unsupported": "BP_UnsupportedNever"}.items()}
        require(report.get("assets") == assets and report.get("asset_root") == root, "Fixture ownership differs")
        values = {key: report.get(key) for key in ("phase", "run_id", "project_file", "endpoint", "assets")}
        require(report.get("input_fingerprint") == hashlib.sha256(json.dumps(values, sort_keys=True).encode()).hexdigest(), "Input fingerprint differs")
        rows = report.get("stages", [])
        require([row.get("name") for row in rows] == (AUTHOR_STAGES if phase == "author" else RELOAD_STAGES) and all(row.get("passed") is True for row in rows), "Required stage coverage/order/results differ")
        lookup = {row["name"]: row["result"] for row in rows}
        editor = report.get("editor", {})
        require(lookup["preflight"] == editor and editor.get("success") is True and editor.get("game_thread") is True and editor.get("pie_or_sie_active") is False and not editor.get("dirty_packages"), "Invalid editor preflight")
        require(str(editor.get("engine_version", "")).startswith("5.8.3") and Path(editor["project_file"]).resolve() == Path(report["project_file"]).resolve(), "Wrong project or engine")
        require(report.get("undo", {}).get("status") == "not_run" and report.get("full_legacy_batch_parity") is False, "Unsupported Undo/parity claim")
        if phase == "author":
            expected_tools = {
                "batch_lifecycle": ["create_blueprint", "compile_and_save_blueprint", "inspect_blueprint"],
                "replay_lifecycle": ["create_blueprint", "compile_and_save_blueprint", "inspect_blueprint"],
                "stop_true": ["create_blueprint", "create_blueprint"],
                "stop_false": ["create_blueprint", "create_blueprint", "compile_and_save_blueprint", "inspect_blueprint"],
                "graph_add_batch": ["add_branch_node", "add_branch_node", "inspect_graph"],
                "graph_edit_batch": ["set_node_position", "connect_graph_pins", "inspect_graph", "delete_node", "inspect_graph"],
                "graph_compile_save_batch": ["compile_and_save_blueprint", "inspect_graph"],
            }
            graph_names = {"add_branch_node", "set_node_position", "connect_graph_pins", "delete_node", "inspect_graph"}
            for stage, tools in expected_tools.items():
                actual = lookup[stage].get("data", {}).get("batch", {}).get("results", [])
                qualified = [("CubelessBlueprintGraphTools." if name in graph_names else "CubelessBlueprintTools.") + name for name in tools]
                require([row.get("type") for row in actual] == qualified, "Actual child command order differs: " + stage)
            lifecycle = batch_result(lookup["batch_lifecycle"], total=3)
            saved(lifecycle[1]); identity(lifecycle[2], assets["batch"])
            saved(lookup["standalone_compile_save"]); identity(lookup["standalone_inspect"], assets["standalone"])
            require(lookup["standalone_create"].get("success") is True, "Standalone creation failed")
            replay = lookup["replay_lifecycle"]
            batch_result(replay, total=3)
            require(replay.get("retry", {}).get("replayed") is True and replay["data"]["batch"] == lookup["batch_lifecycle"]["data"]["batch"], "Duplicate ID was not exact historical replay")
            conflict = lookup["conflicting_id"]
            require(conflict.get("success") is False and conflict.get("error_code") == "operation_id_conflict" and conflict.get("changes") == [], "Same-ID conflict did not reject without mutation")
            identity(lookup["independent_batch_identity"], assets["batch"])
            unsupported = lookup["unsupported_batch"]
            require(unsupported.get("success") is False and unsupported.get("error_code") == "batch_unsupported_command" and unsupported.get("data", {}).get("admission_rejected") is True and unsupported["data"].get("actions_executed") == 0 and unsupported.get("changes") == [], "Unsupported batch was not rejected up front")
            require(lookup["unsupported_prefix_absent"] is False, "Unsupported batch created a prefix asset")
            stop_true = batch_result(lookup["stop_true"], total=4, failure_index=1, stop=True)
            require(lookup["stop_true"].get("compile", {}).get("entries") == [] and lookup["stop_true"].get("save", {}).get("entries") == [], "Stop=True executed the skipped compile/save")
            identity(lookup["stop_true_dirty_readback"], assets["stop_true"], True)
            saved(lookup["preserve_stop_true"])
            stop_false = batch_result(lookup["stop_false"], total=4, failure_index=1, stop=False)
            saved(stop_false[2]); identity(stop_false[3], assets["stop_false"])
            identity(lookup["stop_false_clean_readback"], assets["stop_false"])
            added = batch_result(lookup["graph_add_batch"], total=3)
            source, target = added[0]["data"]["node_id"], added[1]["data"]["node_id"]
            require(report["fixture"] == {"remaining_node_id": source, "deleted_node_id": target}, "Graph node ownership differs")
            edits = batch_result(lookup["graph_edit_batch"], total=5)
            for child in (added[0], added[1], edits[0], edits[1], edits[3]):
                data = child.get("data", {})
                require(data.get("contract_version") == "mutation_result_v1" and data.get("postcondition_verified") is True and data.get("verified_success") is True, "Graph child lacks verified mutation contract")
            connected = graph(edits[2])["nodes"]
            source_node = next(node for node in connected if node["node_id"] == source)
            target_node = next(node for node in connected if node["node_id"] == target)
            require(target_node["x"] == 600 and target_node["y"] == 200, "Batch move readback differs")
            then = next(pin for pin in source_node["pins"] if pin["name"] == "then")
            require(any(link["node_id"] == target and link["pin_name"] == "execute" for link in then["links"]), "Batch connection readback differs")
            deleted = graph(edits[4])
            require(target not in {node["node_id"] for node in deleted["nodes"]} and source in {node["node_id"] for node in deleted["nodes"]}, "Batch deletion readback differs")
            require(deleted == graph(lookup["graph_independent_readback"]), "Independent graph readback differs")
            final = batch_result(lookup["graph_compile_save_batch"], total=2)
            saved(final[0]); require(graph(final[1]) == report.get("saved_graph") == deleted, "Saved graph differs from independently read deletion result")
            for key in ("batch", "stop_true", "stop_false", "standalone"):
                identity(lookup["final_" + {"stop_true": "true", "stop_false": "false"}.get(key, key) + "_identity"], assets[key])
            require(not lookup["final_editor"].get("dirty_packages"), "Author ended with dirty packages")
            perf = report.get("performance", {})
            require(perf.get("comparison_action_count") == 3 and perf.get("explicit_compile_calls_each") == 1 and perf.get("explicit_save_calls_each") == 1 and perf.get("batch_tool_calls") == 1 and perf.get("standalone_tool_calls") == 3 and perf.get("samples_each") == 1, "Performance workloads/call counts differ")
            require(perf.get("batch_elapsed_seconds") == rows[1]["elapsed_seconds"] and perf.get("standalone_elapsed_seconds") == sum(row["elapsed_seconds"] for row in rows[2:5]), "Performance elapsed values differ from stages")
        else:
            require(parent is not None and parent_bytes is not None and verify(parent, require_phase="author")["passed"], "Actual passing parent bytes required")
            require(report.get("author_report_sha256") == hashlib.sha256(parent_bytes).hexdigest(), "Parent fingerprint differs")
            require(editor["instance_id"] != parent["editor"]["instance_id"] and report.get("author_editor") == parent["editor"], "Cold reload reused/misidentified author process")
            require(report["assets"] == parent["assets"] and report["fixture"] == parent["fixture"], "Reload fixture differs")
            results = batch_result(lookup["reload_read_batch"], total=5)
            require([row.get("type") for row in lookup["reload_read_batch"]["data"]["batch"]["results"]] == ["CubelessBlueprintTools.inspect_blueprint"] * 4 + ["CubelessBlueprintGraphTools.inspect_graph"], "Reload child command order differs")
            for key, result in zip(("batch", "standalone", "stop_true", "stop_false"), results[:4]):
                identity(result, assets[key])
            require(graph(results[4]) == parent["saved_graph"], "Cold graph differs from saved state")
            require(not lookup["reload_read_batch"]["compile"].get("requested") and not lookup["reload_read_batch"]["save"].get("requested") and not lookup["reload_editor"].get("dirty_packages"), "Read-only reload requested compile/save or left dirty packages")
    check(run)
    return {"schema": "cubeless_batch_pipeline_verification_v1", "passed": not errors, "errors": errors, "phase": report.get("phase"), "full_legacy_batch_parity": False, "scope": "Independent stage/child/readback contract; execution authenticity requires original live MCP evidence"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path); parser.add_argument("--phase", choices=["author", "reload"]); parser.add_argument("--parent-report", type=Path); parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    raw = args.parent_report.read_bytes() if args.parent_report else None
    result = verify(json.loads(args.report.read_text(encoding="utf-8-sig")), require_phase=args.phase, parent=json.loads(raw) if raw else None, parent_bytes=raw)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("x", encoding="utf-8") as stream: json.dump(result, stream, indent=2)
    print(json.dumps(result)); return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
