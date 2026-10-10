"""Independent offline checks for combined no-MCP editor author/cold reports.

This does not replace Shipping build, exact Pak membership or fresh-game marker
verification, and never treats those unexecuted scopes as passed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import current_runtime_manifest as contract
from current_runtime_manifest import digest, object_path, read_json, require
from validate_current_runtime_fixture import check_dependencies, saved_map_readback


def source_evidence(report, manifest):
    observed = report["source_readback"]
    expected_roles = {r for r in manifest["parents"] if r.startswith("material_") or r in {"edges", "custom", "mpc"}} | {"mpc_local_chain"}
    require(set(observed) == expected_roles, "Source readback role coverage differs")
    for role, parent_row in manifest["parents"].items():
        if role not in observed: continue
        parent = read_json(parent_row["path"]); value = observed[role]; nodes = value["nodes"]
        material = parent["assets"]["material"] if role.startswith("material_") else parent["packages"]["material"] if role == "mpc" else parent["material_package"]
        require(value["material"] == object_path(material) and len({n["path"] for n in nodes}) == len(nodes), "Source Material identity/unique nodes differ")
        if role.startswith("material_"):
            original = parent["saved_graph"]["nodes"]
            require({n["path"] for n in nodes} == {n["object_path"] for n in original} and value["base_color"] == parent["fixture"]["nodes"]["add"], "Source Material graph/root differs")
            for node in nodes:
                old = next(n for n in original if n["object_path"] == node["path"])
                require(node["class"] == old["class_path"] and (node["x"], node["y"]) == (old["x"], old["y"]) and node["inputs"] == {p["name"]: p.get("connected_object_path") if p["connected"] else None for p in old["inputs"]}, "Source graph properties/connections differ")
            require(abs(next(n for n in nodes if n["path"] == parent["fixture"]["nodes"]["added"])["r"] - .875) < 1e-6, "Retained Add/Set value differs")
        elif role == "edges":
            from material_add_set_edges_report import INITIAL_DESC, FINAL_DESC
            require({n["path"] for n in nodes} == set(parent["nodes"].values()), "Edges node identity differs")
            for kind, number, point, description in [("default", .125, (0, 0), INITIAL_DESC), ("position", .875, (-700, 550), FINAL_DESC)]:
                node = next(n for n in nodes if n["path"] == parent["nodes"][kind])
                require(node["class"] == "/Script/Engine.MaterialExpressionConstant" and abs(node["r"] - number) < 1e-6 and (node["x"], node["y"]) == point and node["desc"] == description, "Edge fixture properties differ")
        elif role == "custom":
            from custom_material_report import DEFAULT_CODE, ALIAS_CODE, DESCRIPTION
            require({n["path"] for n in nodes} == set(parent["nodes"].values()) and value["base_color"] == parent["nodes"]["alias"], "Custom is disconnected/wrong graph")
            for kind in ("default", "alias"):
                node = next(n for n in nodes if n["path"] == parent["nodes"][kind]); custom = node["custom"]
                require(node["class"] == "/Script/Engine.MaterialExpressionCustom" and custom["code"] == (DEFAULT_CODE if kind == "default" else ALIAS_CODE) and custom["float3"] is True and custom["includes"] == [], "Custom code/output/includes differ")
                require(custom["description"] == node["desc"] == ("" if kind == "default" else DESCRIPTION), "Custom caption differs")
                require(custom["input_names"] == ([] if kind == "default" else ["Signal", "Bias"]) and node["inputs"] == ({} if kind == "default" else {"Signal": parent["nodes"]["default"], "Bias": parent["nodes"]["default"]}), "Custom input wiring differs")
                require(custom["defines"] == ([] if kind == "default" else [{"name": "CUBE_SCALE", "value": "0.5"}, {"name": "CUBE_FLAG", "value": "1"}, {"name": "CUBE_DEFAULT", "value": "1"}]), "Custom define readback differs")
        else:
            require({n["path"] for n in nodes} == set(parent["nodes"].values()) and all(n["collection"] == parent["objects"]["leaf"] for n in nodes) and {n["parameter_name"] for n in nodes} == {"RootScalar", "RootVector"}, "MPC graph references differ")
    parent = read_json(manifest["parents"]["mpc"]["path"])
    chain = observed["mpc_local_chain"]
    expected = {"base": ({"RootScalar": .25}, {"RootVector": [.1, .2, .3, 1]}, None), "mid": ({"MidScalar": .5}, {}, parent["objects"]["base"]), "leaf": ({}, {"LeafVector": [.4, .5, .6, 1]}, parent["objects"]["mid"])}
    require(set(chain) == set(expected), "MPC chain coverage differs")
    for kind, (scalars, vectors, base) in expected.items():
        row = chain[kind]
        require(row["collection"] == parent["objects"][kind] and row["base"] == base and row["scalars"].keys() == scalars.keys() and row["vectors"].keys() == vectors.keys(), "MPC base/name identity differs")
        require(all(abs(row["scalars"][k] - v) < 1e-5 for k, v in scalars.items()) and all(len(row["vectors"][k]) == 4 and all(abs(a-b) < 1e-5 for a,b in zip(row["vectors"][k],v)) for k,v in vectors.items()), "MPC default values differ")


def map_evidence(value, manifest):
    require(value["world"] in {object_path(manifest["source_map"]), object_path(manifest["default_map"])}, "Unexpected map identity")
    actors = value["actors"]; cubes = value["cubes"]
    require(len({r["path"] for r in actors}) == len(actors), "Duplicate actor identity")
    for actor in actors:
        prefix, separator, suffix = actor["path"].partition(":")
        require(prefix == value["world"] and separator == ":" and suffix.startswith("PersistentLevel.") and len(suffix) > len("PersistentLevel."), "Actor is outside the exact map persistent level")
    expected = manifest["preserved_marker_actors"] + manifest["blueprint_actors"]
    for row in expected: require(sum(a["label"] == row["label"] and a["class_path"] == row["class_path"] for a in actors) == 1, "Required Blueprint/marker actor missing")
    for row in manifest["preserved_marker_actors"]:
        require(next(a["path"] for a in actors if a["label"] == row["label"]) == value["world"] + ":" + row["path"].split(":", 1)[1], "Original marker actor identity changed")
    require({r["label"] for r in cubes} == {r["label"] for r in manifest["material_actors"]} and len(cubes) == len(manifest["material_actors"]), "Cube coverage differs")
    for row in manifest["material_actors"]:
        cube = next(c for c in cubes if c["label"] == row["label"])
        require(cube["class_path"] == "/Script/Engine.StaticMeshActor" and cube["mesh"] == "/Engine/BasicShapes/Cube.Cube" and cube["material"] == object_path(row["package"]) and cube["slot"] == 0, "Cube class/mesh/material/slot differs")
        require(sum(a["path"] == cube["path"] and a["label"] == cube["label"] and a["class_path"] == cube["class_path"] for a in actors) == 1, "Cube not bound to actor readback")


def independent_dependencies(audit, manifest, new_files):
    """Recompute recorded reachability; do not accept claimed coverage by itself."""
    check_dependencies(manifest, audit)
    require(audit["root_packages"] == [manifest["default_map"]], "Audit starts from another map")
    profiles = {"hard_package": "include_hard_package_references", "soft_package": "include_soft_package_references", "hard_management": "include_hard_management_references", "soft_management": "include_soft_management_references"}
    fields = set(profiles.values()) | {"include_game_package_references", "include_editor_only_package_references", "include_searchable_names"}
    require(set(audit["query_options"]) == set(profiles), "Dependency categories differ")
    for name, selected in profiles.items():
        require(audit["query_options"][name] == {field: field == selected for field in fields}, "Dependency query options are not isolated")
    packages = {row["package"]: row for row in audit["packages"]}
    require(len(packages) == len(audit["packages"]), "Duplicate dependency package")
    adjacency = {}
    for edge in audit["edges"]:
        require(edge["category"] in profiles and edge["from"] in packages, "Unknown dependency category/source")
        adjacency.setdefault(edge["from"], set()).add(edge["to"])
    pending = [manifest["default_map"]]; visited = set()
    while pending:
        package = pending.pop()
        if package in visited: continue
        parts = package.strip("/").split("/"); identifier = (parts[1] if len(parts) > 1 and parts[0].lower() == "script" else parts[0]).lower()
        require(not ("modelcontextprotocol" in identifier or "toolset" in identifier or identifier.startswith(("unrealmcp", "cubelessmcp"))), "Forbidden dependency hidden in graph")
        visited.add(package); pending.extend(adjacency.get(package, ()))
    require(set(packages) <= visited and set(manifest["required_cooked_packages"]) <= visited, "Disconnected package rows cannot prove reachability")
    expected = {row["package"]: {"path": row["destination"], "sha256": row["sha256"]} for row in manifest["files"]}
    expected.update(new_files)
    for package in manifest["required_cooked_packages"]:
        row = packages[package]; bound = expected[package]
        require(row["registry_asset_count"] > 0 and row["sha256"] == bound["sha256"] and Path(row["file"]).resolve() == Path(bound["path"]).resolve(), "Dependency record has wrong disk identity")


def author_changes(report, manifest):
    changes = report["changes"]
    require(len(changes) == len(manifest["clone_specs"]) + len(manifest["blueprint_actors"]) + len(manifest["material_actors"]) + 1 and all(c["attempted"] is True and c["completed"] is True for c in changes), "Author mutation log is incomplete")
    for spec in manifest["clone_specs"]:
        require(sum(c["kind"] == "duplicate_and_connect_material" and c["source"] == spec["source"] and c["target"] == spec["target"] for c in changes if c["kind"] == "duplicate_and_connect_material") == 1, "Clone mutation log differs")
    for kind, rows in (("spawn_blueprint_actor", manifest["blueprint_actors"]), ("spawn_material_cube", manifest["material_actors"])):
        for spec in rows:
            matched = [c for c in changes if c["kind"] == kind and c["requested_label"] == spec["label"]]
            require(len(matched) == 1, "Spawn intent missing/duplicated"); change = matched[0]
            actor = next(a for a in report["map_readback"]["actors"] if a["label"] == spec["label"])
            require(change["stage"] == "complete" and change["label"] == actor["label"] and change["class_path"] == actor["class_path"] and change["returned_actor"].split(":", 1)[1] == actor["path"].split(":", 1)[1], "Spawn outcome differs from independent actor readback")
            if kind == "spawn_material_cube": require(change["mesh_assigned"] is True and change["material"] == object_path(spec["package"]) and change["slot"] == 0, "Cube component completion differs")
            else: require(change["package"] == spec["package"], "Blueprint spawn package differs")
    require(sum(c["kind"] == "save_map_copy" and c["source"] == manifest["source_map"] and c["target"] == manifest["default_map"] for c in changes if c["kind"] == "save_map_copy") == 1, "Map-copy save intent missing")


def verify(report, author=None, author_bytes=None, *, _author_dependency_reconciliation=None):
    errors = []
    try:
        schema = report.get("schema"); require(schema in {"cubeless_current_runtime_fixture_v1", "cubeless_current_runtime_cold_v1"}, "Wrong report schema")
        cold = schema.endswith("cold_v1")
        require(report.get("success") is True and report.get("stage") == "complete" and report.get("automatic_retry") is False and report.get("atomic_rollback") is False and report.get("runtime_marker_created") is False and report.get("shader_completion_verified") is False and report.get("packaging") == "not_run", "Failed/incomplete/overstated editor report")
        require(Path(report["project_file"]).resolve() == contract.PROJECT.resolve() and type(report["process_id"]) is int and report["process_id"] > 0 and report["engine_version"].startswith("5.8.3-"), "Wrong project/process/engine")
        manifest_row = report["manifest"]; require(digest(manifest_row["path"]) == manifest_row["sha256"], "Manifest bytes changed")
        manifest = read_json(manifest_row["path"]); contract.validate_manifest(manifest, True)
        require(report["configuration"] == contract.verify_project_config(manifest) and report["dirty_before"] == [], "Configuration/initial dirty state differs")
        for label in ("plugins_before", "plugins_after"):
            plugins = report[label]; require(plugins.get("passed") is True and plugins.get("forbidden_enabled") == [] and not any(any(s in n.lower() for s in ("mcp", "toolset", "modelcontextprotocol")) for n in plugins["names"]), "MCP/Toolset loaded")
        expected_files = {row["package"] for row in manifest["files"]}
        preservation = report["preservation"]
        require(preservation.get("passed") is True and len(preservation["files"]) == len(expected_files) and {r["package"] for r in preservation["files"]} == expected_files and all(r["source_unchanged"] is True and r["copy_unchanged"] is True for r in preservation["files"]), "Incomplete input preservation")
        source_evidence(report, manifest); map_evidence(report["map_readback"], manifest)
        require(set(report["clones"]) == {c["target"] for c in manifest["clone_specs"]}, "Clone coverage differs")
        for spec in manifest["clone_specs"]:
            value = report["clones"][spec["target"]]; require(value["material"] == object_path(spec["target"]), "Clone identity differs")
            nodes = {n["name"]: n for n in value["nodes"]}; source = nodes[spec["source_node_name"]]
            require(source["collection"] == object_path(spec["collection"]), "Clone MPC reference differs")
            if spec["connection"] == "collection_to_add_B":
                target = nodes[spec["target_node_name"]]; require(target["inputs"]["B"] == source["path"] and value["base_color"] == target["path"], "Clone MPC disconnected")
            else: require(value["base_color"] == source["path"] and source["parameter_name"] == "RootVector", "Inherited MPC disconnected")
        if cold:
            require(author is not None and author_bytes is not None and json.loads(author_bytes) == author and author.get("schema") == "cubeless_current_runtime_fixture_v1" and verify(author, _author_dependency_reconciliation=report)["passed"], "Cold phase needs independently passing bound author evidence and fresh dependency reconciliation")
            require(report["fixture_report"]["sha256"] == hashlib.sha256(author_bytes).hexdigest() and digest(report["fixture_report"]["path"]) == hashlib.sha256(author_bytes).hexdigest() and report["process_id"] != author["process_id"], "Cold phase is same process or wrong author")
            require(report["manifest"] == author["manifest"] and report["source_readback"] == author["source_readback"] and report["clones"] == author["clones"] and report["map_readback"] == saved_map_readback(author, manifest), "Cold asset/map identity differs")
            require(report["save"] == {"requested": False, "attempted": False} and report.get("new_files_unchanged") is True, "Cold validation saved/changed new packages")
            expected_compiled = {r["package"] for r in manifest["files"] if r["asset_type"] == "blueprint_actor" or any(role in {"marker:save_game", "marker:actor", "marker:pipeline"} for role in r["roles"])}
            require({r["package"] for r in report["compile"]} == expected_compiled and len(report["compile"]) == len(expected_compiled), "Cold Blueprint compile coverage differs")
            require(all(r["requested"] is True and r["returned"] is True and r["save_requested"] is False and r["errors"] == [] and r["status"] in {"<BlueprintStatus.BS_UP_TO_DATE: 3>", "<BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS: 5>"} for r in report["compile"]), "Cold Blueprint compile failed")
            new_files = author["new_asset_files"]
        else:
            author_changes(report, manifest)
            save = report["save"]; require(save["clone_calls"] == save["map_calls"] == 1 and save["clones_saved"] is True and save["map_saved"] is True and save["map_package"] == manifest["default_map"] and save["old_map_save_requested"] is False and save["clone_packages"] == [s["target"] for s in manifest["clone_specs"]], "Author save count/target differs")
            require(len(report["compile"]) == len(manifest["clone_specs"]) and {r["material"] for r in report["compile"]} == {object_path(c["target"]) for c in manifest["clone_specs"]} and all(r["requested"] is True and r["returned"] is True and r["shader_completion_verified"] is False and r["reported_errors"] == [] and r["status"] == "returned_without_reported_errors" for r in report["compile"]), "Clone recompile observations differ")
            new_files = report["new_asset_files"]
        require(set(new_files) == {manifest["default_map"]} | {c["target"] for c in manifest["clone_specs"]}, "New file set differs")
        for package, row in new_files.items():
            expected = contract.PROJECT.parent / contract.relative_asset(package, package == manifest["default_map"])
            require(Path(row["path"]).resolve() == expected.resolve() and digest(expected) == row["sha256"], "New package bytes differ")
        dependency_source = report
        if _author_dependency_reconciliation is not None:
            require(not cold, "Only author dependency observations can be reconciled")
            dependency_source = _author_dependency_reconciliation
            require(dependency_source.get("schema") == "cubeless_current_runtime_cold_v1" and dependency_source.get("success") is True and dependency_source["process_id"] != report["process_id"] and dependency_source["manifest"] == report["manifest"] and dependency_source["source_readback"] == report["source_readback"] and dependency_source["clones"] == report["clones"] and dependency_source["map_readback"] == saved_map_readback(report, manifest), "Fresh dependency reconciliation is not bound to the same saved assets")
        independent_dependencies(dependency_source["dependency_audit"], manifest, new_files)
    except (RuntimeError, AssertionError, AttributeError, ValueError, TypeError, KeyError, IndexError, OSError, StopIteration) as error: errors.append(str(error))
    return {"schema": "cubeless_current_runtime_editor_independent_v1", "passed": not errors, "errors": errors, "scope": "Specified editor author/cold evidence plus actual source/copy/new-file SHA and recorded public API observations", "dependency_evidence": "paired_fresh_process_disk_registry" if report.get("schema") == "cubeless_current_runtime_cold_v1" or _author_dependency_reconciliation is not None else "author_observation_only", "shipping_execution": "not_verified_by_this_report", "pak_membership": "not_verified_by_this_report", "shader_completion_verified": False, "rendering_quality": "not_verified"}


def main():
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("report", type=Path); parser.add_argument("--author-report", type=Path); parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(); raw = args.report.read_bytes(); author_bytes = args.author_report.read_bytes() if args.author_report else None
    result = verify(json.loads(raw), json.loads(author_bytes) if author_bytes else None, author_bytes); result["report_sha256"] = hashlib.sha256(raw).hexdigest()
    with args.output.open("x", encoding="utf-8") as stream: json.dump(result, stream, indent=2)
    print(json.dumps(result)); return 0 if result["passed"] else 1


if __name__ == "__main__": raise SystemExit(main())
