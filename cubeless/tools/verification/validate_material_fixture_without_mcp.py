"""Cold-load a Material runtime fixture without MCP, or inspect supplied cook logs.

Editor mode compiles in memory and never saves assets or creates a runtime marker.
--cook-log mode runs in ordinary Python and never imports unreal or starts UE.
Neither mode claims rendered image quality or packaged-game execution.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import time
import traceback


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_report(path, report):
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def expected_saved_cube(fixture):
    """SaveMap can save a copy while the authoring world retains its old name."""
    expected = dict(fixture["map_after"]["cube"])
    source_map = fixture["source_assets"]["map"]
    saved_map = fixture["assets"]["map"]
    source_object = source_map + "." + source_map.rsplit("/", 1)[1]
    saved_object = saved_map + "." + saved_map.rsplit("/", 1)[1]
    pieces = expected.get("path", "").split(":", 1)
    require(len(pieces) == 2 and pieces[0] in (source_object, saved_object), "Author cube does not belong to the exact source/saved map")
    require(pieces[1].startswith("PersistentLevel.") and len(pieces[1]) > len("PersistentLevel."), "Author cube lacks an exact persistent-level actor suffix")
    expected["path"] = saved_object + ":" + pieces[1]
    return expected


def audit_cook(log_paths, fixture):
    """Record full-path cook mentions and diagnostics, without inventing shader proof."""
    require(fixture.get("schema") == "cubeless_material_runtime_fixture_v1" and fixture.get("success") is True, "Require successful Material runtime fixture authoring")
    targets = {key: fixture["assets"][key] for key in ("map", "runtime_material", "collection")}
    result = {"mode": "offline_cook_log_audit", "logs": [], "target_asset_mentions": {key: [] for key in targets}, "shader_diagnostics": [], "cook_success_markers": [], "fatal_or_error_lines": [], "shader_completion_verified": False,
              "scope": "Supplied cook log text, exact long package/full Content path mentions, reported shader diagnostics; pair with cook exit/receipt/Pak audit",
              "limits": ["Absence of logged errors alone does not prove shader completion.", "A log mention is not independently proof that the asset exists in a staged package.", "Rendering quality and packaged execution are not tested."]}
    for log_path in log_paths:
        path = Path(log_path).resolve()
        require(path.is_file() and path.stat().st_size > 0, "Cook log is missing or empty")
        raw = path.read_bytes()
        text = raw.decode("utf-8-sig", errors="replace")
        result["logs"].append({"path": str(path), "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw)})
        for line_number, line in enumerate(text.splitlines(), 1):
            normalized = line.replace("\\", "/")
            observation = {"file": str(path), "line": line_number, "text": line[:4000]}
            if re.search(r"COOK COMMAND COMPLETED|CookResults:\s*Success|CookResults:\s*Succeeded|Cook completed successfully", line, re.I):
                result["cook_success_markers"].append(observation)
            if re.search(r"\bFatal error\b|\bError:|BUILD FAILED|Cook failed|Error_UnknownCookFailure", line, re.I):
                result["fatal_or_error_lines"].append(observation)
            if re.search(r"shader|material.*compil|compil.*material", line, re.I) and re.search(r"warning|error|failed|failure", line, re.I):
                result["shader_diagnostics"].append(observation)
            for kind, package in targets.items():
                content_path = "/Content/" + package[len("/Game/"):]
                if package in normalized or content_path in normalized:
                    result["target_asset_mentions"][kind].append(observation)
    result["missing_asset_mentions"] = [key for key, value in result["target_asset_mentions"].items() if not value]
    result["passed_within_declared_scope"] = bool(result["cook_success_markers"]) and not result["missing_asset_mentions"] and not result["fatal_or_error_lines"] and not result["shader_diagnostics"]
    return result


def validate_editor(args, report, output):
    import unreal
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import create_material_runtime_fixture as authoring
    from validate_fixture_without_mcp import dependency_audit, enabled_plugins, package_file
    tracked = {}
    try:
        project, engine_dir = authoring.preflight(args.project_file, report)
        fixture_path = Path(args.fixture_report).resolve()
        fixture = read_json(fixture_path)
        require(fixture.get("schema") == "cubeless_material_runtime_fixture_v1" and fixture.get("success") is True, "Material runtime authoring failed")
        require(Path(fixture["project_file"]).resolve() == project and fixture.get("process_id") != os.getpid(), "Require same isolated project and a new process after authoring")
        require(fixture.get("validation", {}).get("six_copies_and_all_sources_unchanged") is True, "Author did not prove input preservation")
        require(fixture.get("compile", {}).get("shader_completion_verified") is False, "Unexpected expanded compile proof scope")
        require(set(fixture.get("new_asset_files", {})) == {"map", "runtime_material"}, "Expected exactly two newly saved packages")
        for kind in ("map", "runtime_material"):
            require(fixture.get("save", {}).get(kind, {}).get("saved") is True, "A new package was not saved")
        report["fixture_report"] = {"path": str(fixture_path), "sha256": digest(fixture_path)}
        parents = fixture["parents"]
        for parent in parents.values():
            require(digest(parent["path"]) == parent["sha256"], "Fixture parent bytes changed")
        original_author, marker, source_tracked = authoring.load_contracts(parents["preparation"]["path"], parents["material_author"]["path"], parents["runtime_marker"]["path"], report)
        report["source_assets"] = dict(report["assets"])
        report["assets"] = fixture["assets"]
        tracked.update(source_tracked)
        for kind, row in fixture["new_asset_files"].items():
            actual = package_file(row["package"], project.parent, engine_dir)
            require(actual == Path(row["path"]).resolve() and digest(actual) == row["sha256"], "New package differs from authoring bytes: " + kind)
            tracked["new_" + kind] = {"path": str(actual), "package": row["package"], "sha256_before": row["sha256"]}
        report["tracked_files"] = tracked
        report["stage"] = "material_and_mpc_readback"
        write_report(output, report)
        _, report["original_material_readback"] = authoring.inspect_material(original_author)
        eas = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        material = eas.load_asset(fixture["assets"]["runtime_material"])
        require(isinstance(material, unreal.Material), "Runtime clone could not load without MCP")
        report["runtime_material_readback"] = authoring.inspect_runtime_material(material, original_author["objects"]["collection"])
        require(report["runtime_material_readback"] == fixture["runtime_material_readback"], "Runtime clone readback changed after cold reload")
        report["stage"] = "cold_load_runtime_map"
        write_report(output, report)
        world = unreal.EditorLoadingAndSavingUtils.load_map(tracked["new_map"]["path"])
        require(world is not None and world.get_path_name().split(".", 1)[0] == fixture["assets"]["map"], "Cold-loaded map differs from fixture")
        report["map_readback"] = authoring.inspect_map(world, marker, material, True)
        report["expected_saved_cube"] = expected_saved_cube(fixture)
        require(report["map_readback"]["cube"] == report["expected_saved_cube"], "Cube identity/slot differs after reload")
        report["stage"] = "dependency_audit"
        write_report(output, report)
        dependency_audit(list(fixture["assets"].values()), unreal.AssetRegistryHelpers.get_asset_registry(), project.parent, engine_dir, report)
        edges = report["dependency_audit"]["edges"]
        require(any(edge["from"] == fixture["assets"]["map"] and edge["to"] == fixture["assets"]["runtime_material"] and edge["category"] == "hard_package" for edge in edges), "Map lacks an on-disk hard dependency on runtime Material")
        require(any(edge["from"] == fixture["assets"]["runtime_material"] and edge["to"] == fixture["assets"]["collection"] and edge["category"] == "hard_package" for edge in edges), "Runtime Material lacks an on-disk hard dependency on MPC")
        for kind in ("save_game", "actor", "pipeline"):
            report["stage"] = "compile_" + kind
            write_report(output, report)
            blueprint = eas.load_asset(fixture["assets"][kind])
            require(isinstance(blueprint, unreal.Blueprint), "Original Blueprint cannot load: " + kind)
            returned = bool(unreal.BlueprintEditorLibrary.compile_blueprint(blueprint))
            status = blueprint.get_editor_property("status")
            errors = []
            for graph in unreal.BlueprintEditorLibrary.list_graphs(blueprint):
                editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
                errors += [{"node": node.get_path_name(), "error": str(node.get_editor_property("error_msg"))} for node in editor.list_nodes_with_errors()]
            report["compile"][kind] = {"returned": returned, "status": str(status), "reported_node_errors": errors, "save_requested": False}
            require(returned and not errors and status in (unreal.BlueprintStatus.BS_UP_TO_DATE, unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS), "Copied Blueprint compile failed: " + kind)
        report["stage"] = "request_material_recompile"
        report["compile"]["runtime_material"] = {"requested": True, "returned": False, "shader_completion_verified": False, "save_requested": False}
        write_report(output, report)
        errors = [str(error) for error in unreal.MaterialEditingLibrary.recompile_material(material)]
        report["compile"]["runtime_material"].update(returned=True, reported_errors=errors, status="returned_without_reported_errors" if not errors else "reported_errors", scope="Current resource reported errors only; API also returns empty if no resource exists; no IsCompilationFinished proof")
        require(not errors, "Runtime Material reports compiler errors without MCP")
        report["plugins_after"] = enabled_plugins()
        require(report["plugins_after"]["passed"], "MCP/Toolset plugin enabled during validation")
        report["dirty_packages_after_compile"] = [item.get_path_name() for item in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())]
        report["validation"].update(mcp_free_editor_load="passed", map_material_mpc_readback="passed", hard_dependency_map_material_mpc="passed", blueprint_compile_without_save="passed", material_reported_errors="none_at_return")
        report.update(success=True, stage="complete")
    finally:
        unchanged = len(tracked) == 8
        for item in tracked.values():
            item["sha256_after"] = digest(item["path"]) if Path(item["path"]).is_file() else None
            item["disk_bytes_unchanged"] = item["sha256_after"] == item["sha256_before"]
            unchanged = unchanged and item["disk_bytes_unchanged"]
            if "source" in item:
                item["source_sha256_after"] = digest(item["source"]) if Path(item["source"]).is_file() else None
                item["source_unchanged"] = item["source_sha256_after"] == item["source_sha256_before"]
                unchanged = unchanged and item["source_unchanged"]
        report["tracked_files"] = tracked
        report["validation"]["eight_copies_and_six_sources_unchanged"] = unchanged
        if not unchanged:
            report["success"] = False
            report["error"] = report.get("error", "") + " Disk preservation checks incomplete or failed."


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-file")
    parser.add_argument("--fixture-report", required=True)
    parser.add_argument("--cook-log", action="append", default=[])
    parser.add_argument("--report", required=True)
    args = parser.parse_args(argv)
    require(bool(args.project_file) != bool(args.cook_log), "Choose editor --project-file or offline --cook-log mode")
    output = Path(args.report).resolve()
    require(not output.exists(), "Refusing to overwrite verification evidence")
    output.parent.mkdir(parents=True, exist_ok=True)
    report = {"schema": "cubeless_material_without_mcp_verification_v1", "success": False, "stage": "preflight", "process_id": os.getpid(), "started_ns": time.time_ns(), "script_sha256": digest(__file__), "compile": {}, "save": {"requested": False, "attempted": False}, "shader_completion_verified": False, "automatic_retry": False, "atomic_rollback": False, "validation": {"rendering_quality": "not_verified", "packaging": "not_run", "shipping_execution": "not_run", "runtime_marker_created_by_this_script": False}}
    write_report(output, report)
    try:
        if args.cook_log:
            report["fixture_report"] = {"path": str(Path(args.fixture_report).resolve()), "sha256": digest(args.fixture_report)}
            report["cook_log_audit"] = audit_cook(args.cook_log, read_json(args.fixture_report))
            report.update(success=report["cook_log_audit"]["passed_within_declared_scope"], stage="complete" if report["cook_log_audit"]["passed_within_declared_scope"] else "incomplete_or_reported_diagnostics")
        else:
            validate_editor(args, report, output)
    except Exception as error:
        report.update(success=False, error=str(error), traceback=traceback.format_exc())
    finally:
        report["finished_ns"] = time.time_ns()
        write_report(output, report)
        print("CUBELESS_MATERIAL_WITHOUT_MCP_REPORT=" + str(output))
    return report["success"]


if __name__ == "__main__" and not main():
    raise RuntimeError("Material no-MCP verification did not pass; inspect report")
