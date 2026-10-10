"""Independently validate Material smoke evidence; never execute engine calls."""
import argparse
import hashlib
import json
from pathlib import Path
import re

AUTHOR_STAGES = ["preflight", "guard_absent_material", "create_material", "guard_absent_collection", "create_collection", "set_collection_defaults",
    "create_expression_source", "create_expression_delete", "create_expression_add", "create_expression_collection", "set_expression_source", "set_expression_delete", "set_expression_collection",
    "resolve", "initial_nodes", "analyze", "collection_nodes", "collection_values", "connect_A", "replay_connect_A", "connect_B", "independent_connection_readback",
    "before_invalid_pin", "invalid_pin_rejected", "invalid_pin_readback", "connect_property", "independent_property_readback", "layout", "delete", "deleted_readback",
    "independent_deleted_link_readback", "compile_once", "save_once", "saved_graph_readback", "saved_clean_material", "saved_clean_collection"]
RELOAD_STAGES = ["preflight", "resolve_reloaded", "reloaded_nodes", "reloaded_collection_values", "reloaded_property_readback", "reload_clean_material", "reload_clean_collection"]


def verify(report, *, require_phase=None, parent=None, parent_bytes=None):
    errors = []
    def require(condition, message):
        if not condition:
            errors.append(message)
    require(report.get("schema") in {"cubeless_material_pipeline_v1", "cubeless_material_pipeline_v2"}, "Unexpected schema")
    extended = report.get("schema") == "cubeless_material_pipeline_v2"
    phase = report.get("phase")
    require(phase in {"author", "reload"}, "Unknown phase")
    require(require_phase is None or phase == require_phase, "Wrong requested phase")
    require(report.get("status") == "pass", "Phase did not pass")
    require(report.get("outcome_unknown") is False and report.get("automatic_retries") == 0, "Unknown/retried mutation outcome")
    run = report.get("run_id", "")
    require(isinstance(run, str) and re.fullmatch("[a-f0-9]{32}", run) is not None, "Invalid run identity")
    root = "/Game/CubelessMCPTests/Material_" + str(run)
    assets = {"material": root + "/M_Contract", "collection": root + "/MPC_Contract"}
    require(report.get("assets") == assets and report.get("asset_root") == root, "Fixture ownership paths mismatch")
    require(report.get("objects") == {kind: path + "." + path.rsplit("/", 1)[1] for kind, path in assets.items()}, "Actual UObject references differ from exact fixture object paths")
    values = {key: report.get(key) for key in ("phase", "run_id", "project_file", "endpoint", "assets")}
    require(report.get("input_fingerprint") == hashlib.sha256(json.dumps(values, sort_keys=True).encode()).hexdigest(), "Input fingerprint mismatch")
    rows = report.get("stages", [])
    names = [row.get("name") for row in rows]
    expected_stages = list(AUTHOR_STAGES if phase == "author" else RELOAD_STAGES)
    if extended and phase == "author":
        position = expected_stages.index("resolve")
        expected_stages[position:position] = ["cubeless_add", "replay_cubeless_add", "cubeless_set", "independent_added_property", "invalid_added_property_rejected", "independent_rejected_property"]
        expected_stages.insert(expected_stages.index("saved_clean_material"), "saved_added_property")
    elif extended:
        expected_stages.insert(expected_stages.index("reload_clean_material"), "reloaded_added_property")
    require(names == expected_stages, "Stage order/coverage differs from required phase")
    require(all(row.get("passed") is True for row in rows), "A recorded stage did not pass")
    lookup = {row.get("name"): row.get("result") for row in rows}
    editor = report.get("editor", {})
    require(bool(editor.get("instance_id")) and editor.get("game_thread") is True and editor.get("pie_or_sie_active") is False, "Invalid editor preflight evidence")
    require(str(editor.get("engine_version", "")).startswith("5.8.3"), "Wrong engine version")
    def same_project_path(left, right):
        return isinstance(left, str) and bool(left) and isinstance(right, str) and bool(right) and Path(left).resolve() == Path(right).resolve()
    require(same_project_path(editor.get("project_file"), report.get("project_file")), "Editor/project identity differs")
    require(lookup.get("preflight") == editor and not editor.get("dirty_packages"), "Preflight result differs or includes dirty packages")
    require(report.get("packaging", {}).get("status") == "not_run", "Material smoke cannot prove packaged runtime")
    require(report.get("cleanup", {}).get("status") == "not_run", "Material smoke cannot prove fixture cleanup")
    fixture = report.get("fixture", {})
    nodes = fixture.get("nodes", {})
    for name in (("source", "delete", "add", "collection", "added") if extended else ("source", "delete", "add", "collection")):
        require(isinstance(nodes.get(name), str) and nodes[name].startswith(assets["material"] + "."), "Expression fixture identity missing: " + name)

    def native_result(name):
        value = lookup.get(name)
        return value if isinstance(value, dict) else {}

    def graph(name):
        value = native_result(name)
        require(value.get("success") is True, "Graph read failed: " + name)
        return {key: value.get("data", {}).get(key) for key in ("material", "graph_type", "node_count", "nodes", "material_properties")}

    def field(value, key):
        if not isinstance(value, dict):
            return None
        matches = [item for name, item in value.items() if name.replace("_", "").lower() == key.replace("_", "").lower()]
        return matches[0] if len(matches) == 1 else None

    def object_path(value):
        return value.get("refPath") if isinstance(value, dict) else value

    def wiring(name, expected_b):
        value = lookup.get(name)
        require(isinstance(value, list), "Missing official expression-input readback: " + name)
        entries = {field(row, "input_name"): row for row in value or [] if isinstance(row, dict)} if isinstance(value, list) else {}
        require(set(entries) == {"A", "B"}, "Official Add input set differs: " + name)
        require(object_path(field(entries.get("A"), "expression")) == nodes.get("source"), "Official A connection differs: " + name)
        raw_b = field(entries.get("B"), "expression")
        require(object_path(raw_b) == expected_b if expected_b else raw_b == "None", "Official B connection differs from exact UE58 readback: " + name)

    def root_wiring(name):
        require(object_path(field(lookup.get(name), "expression")) == nodes.get("add"), "Official BaseColor connection differs: " + name)

    def collection_values(name):
        value = native_result(name)
        require(value.get("success") is True, "MPC values query failed: " + name)
        data = value.get("data", {})
        scalars, vectors = data.get("scalars"), data.get("vectors")
        def close(actual, expected):
            return isinstance(actual, (float, int)) and not isinstance(actual, bool) and abs(actual - expected) < 1e-5
        scalar = scalars[0] if isinstance(scalars, list) and len(scalars) == 1 and isinstance(scalars[0], dict) else {}
        vector = vectors[0] if isinstance(vectors, list) and len(vectors) == 1 and isinstance(vectors[0], dict) else {}
        require(scalar.get("name") == "CubelessScalar" and close(scalar.get("asset_default"), 0.25), "MPC scalar default differs: " + name)
        require(vector.get("name") == "CubelessVector", "MPC vector identity differs: " + name)
        channels = vector.get("asset_default") if isinstance(vector.get("asset_default"), dict) else {}
        require(all(close(channels.get(key), expected) for key, expected in {"r": 0.1, "g": 0.2, "b": 0.3, "a": 1.0}.items()), "MPC vector default differs: " + name)

    if phase == "author":
        require(report.get("compile", {}).get("explicit_calls") == 1 and report.get("compile", {}).get("passed") is True, "Exactly one final explicit compile must pass")
        require(report.get("save", {}).get("explicit_calls") == 1 and report.get("save", {}).get("saved") is True and lookup.get("save_once") is True, "Exactly one final save must pass")
        require(report.get("reload", {}).get("status") == "not_run", "Author cannot claim cold reload")
        require(lookup.get("guard_absent_material") is False and lookup.get("guard_absent_collection") is False, "Fixture already existed")
        require(lookup.get("saved_clean_material") is False and lookup.get("saved_clean_collection") is False, "Saved fixture dirty flags missing")
        for kind in ("material", "collection"):
            require(object_path(lookup.get("create_" + kind)) == report.get("objects", {}).get(kind), "Factory reference differs: " + kind)
        for kind in ("source", "delete", "add", "collection"):
            require(object_path(lookup.get("create_expression_" + kind)) == nodes.get(kind), "Expression factory reference differs: " + kind)
        for name in ("set_collection_defaults", "set_expression_source", "set_expression_delete", "set_expression_collection"):
            require(lookup.get(name) is True, "Official property setter did not succeed: " + name)
        require(native_result("resolve").get("success") is True and native_result("analyze").get("success") is True, "Resolve/analyze failed")
        initial = graph("initial_nodes")
        require(initial.get("node_count") == (5 if extended else 4), "Initial graph expression count differs")
        require({node.get("object_path") for node in initial.get("nodes") or []} == set(nodes.values()), "Initial graph expression identities differ")
        collection = native_result("collection_nodes")
        collection_data = collection.get("data", {})
        collection_entries = collection_data.get("nodes", [])
        require(collection.get("success") is True and collection_data.get("collection_parameter_node_count") == 1 and len(collection_entries) == 1, "Collection-expression count differs")
        collection_entry = collection_entries[0] if len(collection_entries) == 1 else {}
        require(collection_entry.get("parameter_name") == "CubelessScalar" and collection_entry.get("collection_parameter_found") is True and collection_entry.get("parameter_id_matches_collection") is True, "Collection expression name/ID verification failed")
        collection_values("collection_values")
        wiring("independent_connection_readback", nodes.get("delete"))
        wiring("independent_deleted_link_readback", None)
        root_wiring("independent_property_readback")
        mutation_stages = ["connect_A", "replay_connect_A", "connect_B", "connect_property", "layout", "delete"]
        if extended:
            mutation_stages += ["cubeless_add", "replay_cubeless_add", "cubeless_set"]
        for name in mutation_stages:
            value = native_result(name)
            native = value.get("data", {}).get("native_outcome", {})
            require(value.get("success") is True and value.get("validation", {}).get("native_postcondition_passed") is True and native.get("postcondition_passed") is True, "Unverified mutation: " + name)
            require(native.get("change_state_known") is True, "Unknown mutation state: " + name)
            require(value.get("compile", {}).get("requested") is False and value.get("save", {}).get("saved") is False, "Unexpected eager compile/save: " + name)
            require(value.get("retry", {}).get("automatic_retry") is False, "Mutation allows automatic retry: " + name)
        require(native_result("replay_connect_A").get("retry", {}).get("replayed") is True, "Duplicate operation was not a journal replay")
        bad = native_result("invalid_pin_rejected")
        require(bad.get("success") is False and bool(bad.get("error_code")), "Missing invalid-pin rejection")
        require(bad.get("data", {}).get("native_outcome", {}).get("changed") is False, "Rejected pin changed graph/has unknown outcome")
        require(graph("before_invalid_pin") == graph("invalid_pin_readback"), "Rejected pin modified independently read graph")
        saved = graph("saved_graph_readback")
        require(saved == report.get("saved_graph"), "Saved graph summary differs from stage result")
        require(saved.get("node_count") == (4 if extended else 3) and isinstance(saved.get("nodes"), list), "Surviving expression count differs")
        require(not any(node.get("object_path") == nodes.get("delete") for node in saved.get("nodes") or []), "Deleted node remains in saved graph")
        survivors = ("source", "add", "collection", "added") if extended else ("source", "add", "collection")
        require({node.get("object_path") for node in saved.get("nodes") or []} == {nodes.get(key) for key in survivors}, "Saved graph survivor identities differ")
        if extended:
            for name in ("cubeless_add", "replay_cubeless_add", "cubeless_set"):
                require(native_result(name).get("data", {}).get("object_path") == nodes.get("added"), "Add/set node identity differs: " + name)
            require(native_result("replay_cubeless_add").get("retry", {}).get("replayed") is True, "Add was not replayed from journal")
            rejected = native_result("invalid_added_property_rejected")
            observation = rejected.get("data", {}).get("native_outcome", {})
            require(rejected.get("success") is False and bool(rejected.get("error_code")) and observation.get("mutation_attempted") is False and observation.get("changed") is False, "Invalid added property did not fail before mutation")
            require(report.get("compile", {}).get("shader_completion_verified") is False and report.get("compile", {}).get("status") == "returned_without_reported_errors", "Shader completion must not be inferred from void recompile return")
    elif phase == "reload":
        require(report.get("compile", {}).get("explicit_calls") == 0 and report.get("save", {}).get("explicit_calls") == 0, "Reload must not compile/save")
        require(report.get("reload", {}).get("status") == "passed", "Cold reload not passed")
        require(lookup.get("reload_clean_material") is False and lookup.get("reload_clean_collection") is False, "Reloaded fixture became dirty")
        require(native_result("resolve_reloaded").get("success") is True, "Reload resolve failed")
        collection_values("reloaded_collection_values")
        root_wiring("reloaded_property_readback")
        require(bool(parent) and parent_bytes is not None, "Reload verification requires actual author report bytes")
        if parent and parent_bytes is not None:
            parent_check = verify(parent, require_phase="author")
            require(parent_check["passed"], "Author report does not independently pass")
            require(report.get("author_report_sha256") == hashlib.sha256(parent_bytes).hexdigest(), "Author report fingerprint mismatch")
            require(editor.get("instance_id") != parent.get("editor", {}).get("instance_id"), "Author process was reused")
            require(report.get("author_editor") == parent.get("editor"), "Author editor summary mismatch")
            for key in ("run_id", "assets", "objects", "fixture"):
                require(report.get(key) == parent.get(key), "Author/reload identity mismatch: " + key)
            require(same_project_path(report.get("project_file"), parent.get("project_file")), "Author/reload project path differs")
            require(graph("reloaded_nodes") == parent.get("saved_graph"), "Reloaded graph differs from saved graph")
            require(report.get("schema") == parent.get("schema"), "Author/reload schema differs")
    if extended:
        property_stages = ["independent_added_property", "independent_rejected_property", "saved_added_property"] if phase == "author" else ["reloaded_added_property"]
        for name in property_stages:
            actual = field(lookup.get(name), "R")
            require(type(actual) in (int, float) and abs(actual - 0.875) < 1e-6, "Independent retained R differs: " + name)
            require(field(lookup.get(name), "Desc") == "Cubeless add-set retained fixture", "Independent retained Desc differs: " + name)
    return {"schema": "cubeless_material_report_verification_v1", "passed": not errors, "errors": errors, "phase": phase,
            "scope": "Independent report consistency/required-stage checks; execution authenticity requires attached live official-MCP evidence", "full_material_migration_complete": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--phase", choices=["author", "reload"])
    parser.add_argument("--parent-report", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    raw_parent = args.parent_report.read_bytes() if args.parent_report else None
    result = verify(json.loads(args.report.read_text(encoding="utf-8-sig")), require_phase=args.phase,
                    parent=json.loads(raw_parent) if raw_parent else None, parent_bytes=raw_parent)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("x", encoding="utf-8") as stream:
            json.dump(result, stream, indent=2)
    print(json.dumps(result))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
