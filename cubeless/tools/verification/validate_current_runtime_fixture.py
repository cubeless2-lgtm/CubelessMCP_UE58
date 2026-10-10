"""Fresh-process no-MCP readback of the current combined runtime fixture.

Never saves packages, creates markers, cooks or launches games. Rendering and
shader completion are separate proof scopes owned by the packaging runner.
"""
import argparse
import copy
import json
import os
from pathlib import Path
import sys
import time
import traceback

sys.path.insert(0, str(Path(__file__).resolve().parent))
import current_runtime_manifest as contract
from current_runtime_manifest import digest, object_path, read_json, require


def saved_map_readback(author, manifest):
    """Map-copy save may retain old author-world name; allow only exact remapping."""
    result = copy.deepcopy(author["map_readback"])
    old = object_path(manifest["source_map"]); new = object_path(manifest["default_map"])
    require(result["world"] in (old, new), "Author world is neither exact source nor saved map")
    result["world"] = new
    for row in result["actors"] + result["cubes"]:
        prefix, separator, suffix = row["path"].partition(":")
        require(separator == ":" and prefix in (old, new) and suffix.startswith("PersistentLevel.") and len(suffix) > len("PersistentLevel."), "Actor path escaped exact persistent-level identity")
        row["path"] = new + ":" + suffix
    result["actors"].sort(key=lambda r: r["path"])
    return result


def check_dependencies(manifest, audit):
    require(audit.get("complete") is True and audit.get("passed_within_declared_scope") is True and audit.get("forbidden_references") == [], "Dependency audit incomplete/forbidden")
    edges = audit["edges"]; hard = {(e["from"], e["to"]) for e in edges if e["category"] == "hard_package"}
    for row in manifest["material_actors"]: require((manifest["default_map"], row["package"]) in hard, "Map does not reference required Material: " + row["package"])
    for row in manifest["blueprint_actors"]: require((manifest["default_map"], row["package"]) in hard, "Map does not reference required Blueprint: " + row["package"])
    for clone in manifest["clone_specs"]: require((clone["target"], clone["collection"]) in hard, "Connected runtime clone lacks MPC hard reference")
    mpc = read_json(manifest["parents"]["mpc"]["path"])["packages"]
    require((mpc["leaf"], mpc["mid"]) in hard and (mpc["mid"], mpc["base"]) in hard, "MPC base chain lacks package references")
    packages = {row["package"] for row in audit["packages"]}
    require(set(manifest["required_cooked_packages"]) <= packages, "One or more planned runtime packages are not reachable in registry")
    require(manifest["excluded_old_map"] not in packages, "New map retains old-map dependency")
    return {"passed": True, "required_package_count": len(manifest["required_cooked_packages"]), "scope": "Saved registry hard edges plus bounded transitive disk graph; cooked Pak membership remains separate", "query_options": audit["query_options"]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("--manifest", required=True, type=Path); parser.add_argument("--fixture-report", required=True, type=Path); parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args(argv); output = args.report.resolve(); require(not output.exists(), "Refusing to overwrite cold evidence")
    import unreal
    import create_current_runtime_fixture as authoring
    from validate_fixture_without_mcp import dependency_audit, enabled_plugins, package_file, write_report
    manifest = read_json(args.manifest); fixture = read_json(args.fixture_report)
    output.parent.mkdir(parents=True, exist_ok=True)
    report = {"schema": "cubeless_current_runtime_cold_v1", "success": False, "stage": "preflight", "process_id": os.getpid(), "project_file": str(contract.PROJECT.resolve()), "manifest": {"path": str(args.manifest.resolve()), "sha256": digest(args.manifest)}, "fixture_report": {"path": str(args.fixture_report.resolve()), "sha256": digest(args.fixture_report)}, "started_ns": time.time_ns(), "save": {"requested": False, "attempted": False}, "compile": [], "shader_completion_verified": False, "automatic_retry": False, "atomic_rollback": False, "runtime_marker_created": False, "packaging": "not_run"}
    def stage(name): report["stage"] = name; write_report(output, report)
    write_report(output, report)
    new_files = {}
    try:
        require(fixture.get("schema") == "cubeless_current_runtime_fixture_v1" and fixture.get("success") is True and fixture.get("stage") == "complete" and fixture.get("preservation", {}).get("passed") is True, "Author fixture did not complete/preserve input bytes")
        require(fixture["manifest"] == report["manifest"] and fixture["project_file"] == report["project_file"] and fixture["process_id"] != os.getpid(), "Cold process/parent identity mismatch")
        require(fixture["save"].get("clone_calls") == fixture["save"].get("map_calls") == 1 and fixture["save"].get("clones_saved") is True and fixture["save"].get("map_saved") is True and fixture["save"].get("old_map_save_requested") is False, "Author save contract differs")
        assets, engine = authoring.preflight(manifest, report)
        require(report["configuration"] == fixture["configuration"], "Project/config changed between author and cold validation")
        new_files = fixture["new_asset_files"]
        require(set(new_files) == {manifest["default_map"]} | {c["target"] for c in manifest["clone_specs"]}, "Unexpected new package set")
        for package, row in new_files.items(): require(package_file(package, contract.PROJECT.parent, engine) == Path(row["path"]).resolve() and digest(row["path"]) == row["sha256"], "New package bytes changed before cold load")
        stage("source_readback"); report["source_readback"] = authoring.validate_source_assets(manifest, assets)
        require(report["source_readback"] == fixture["source_readback"], "Source fixtures differ after cold load")
        report["clones"] = {c["target"]: authoring.clone_snapshot(assets.load_asset(c["target"]), c) for c in manifest["clone_specs"]}
        require(report["clones"] == fixture["clones"], "Runtime clone graph changed after cold load")
        stage("load_new_map"); world = unreal.EditorLoadingAndSavingUtils.load_map(new_files[manifest["default_map"]]["path"])
        require(world is not None and authoring.path(world) == object_path(manifest["default_map"]), "Wrong cold map")
        report["map_readback"] = authoring.inspect_map(world, manifest)
        report["expected_saved_map_readback"] = saved_map_readback(fixture, manifest)
        require(report["map_readback"] == report["expected_saved_map_readback"], "Actor/cube paths, labels, classes or material slots differ after map-copy reload")
        stage("dependency_audit"); dependency_audit([manifest["default_map"]], unreal.AssetRegistryHelpers.get_asset_registry(), contract.PROJECT.parent, engine, report)
        report["required_dependencies"] = check_dependencies(manifest, report["dependency_audit"])
        for row in manifest["files"]:
            if row["asset_type"] == "map": continue
            asset = assets.load_asset(row["package"])
            if not isinstance(asset, unreal.Blueprint): continue
            stage("compile_without_save_" + row["package"].rsplit("/", 1)[1]); observation = {"package": row["package"], "requested": True, "save_requested": False}; report["compile"].append(observation); write_report(output, report)
            returned = bool(unreal.BlueprintEditorLibrary.compile_blueprint(asset)); status = asset.get_editor_property("status"); errors = []
            for graph in unreal.BlueprintEditorLibrary.list_graphs(asset):
                editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
                errors.extend({"node": node.get_path_name(), "error": str(node.get_editor_property("error_msg"))} for node in editor.list_nodes_with_errors())
            observation.update(returned=returned, status=str(status), errors=errors)
            require(returned and not errors and status in (unreal.BlueprintStatus.BS_UP_TO_DATE, unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS), "No-plugin Blueprint compile failed")
        report["plugins_after"] = enabled_plugins(); require(report["plugins_after"]["passed"], "MCP/Toolset enabled during cold validation")
        report["dirty_after_compile"] = [authoring.path(p) for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())]
        report.update(success=True, stage="complete")
    except Exception as error: report.update(error=str(error), traceback=traceback.format_exc())
    finally:
        report["preservation"] = authoring.preservation(manifest)
        report["new_files_unchanged"] = bool(new_files) and all(Path(row["path"]).is_file() and digest(row["path"]) == row["sha256"] for row in new_files.values())
        if not report["preservation"]["passed"] or not report["new_files_unchanged"]: report["success"] = False; report["preservation_error"] = "Original, copied or authored disk bytes changed"
        report["finished_ns"] = time.time_ns(); write_report(output, report)
    return report["success"]


if __name__ == "__main__" and not main(): raise RuntimeError("Current no-MCP cold validation failed; inspect report")
