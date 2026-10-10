"""Validate copied fixture assets in a disposable editor without MCP plugins.

Execute through UE -ExecutePythonScript with --project-file --fixture-report
--report. Compiles in memory using the public SkipSave API; never saves assets,
runs PIE, packages, launches a game, or claims cooked/runtime dependency proof.
The JSON report's success field is authoritative, not the editor process code.
"""

import argparse
from collections import deque
import hashlib
import json
import os
from pathlib import Path
import time
import traceback

import unreal


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def absolute(path):
    return Path(unreal.Paths.convert_relative_path_to_full(str(path))).resolve()


def write_report(path, report):
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def package_file(package, project_dir, engine_dir):
    require(".." not in package and "." not in package, "Expected a package name without traversal or object suffix: " + package)
    if package.startswith("/Game/"):
        root, suffix = project_dir / "Content", package[len("/Game/"):]
    elif package.startswith("/Engine/"):
        root, suffix = engine_dir / "Content", package[len("/Engine/"):]
    else:
        raise RuntimeError("Cannot prove the disk file for this mount: " + package)
    base = (root / suffix).resolve()
    require(base.is_relative_to(root.resolve()), "Package escaped content root")
    matches = [base.with_suffix(ext) for ext in (".uasset", ".umap") if base.with_suffix(ext).is_file()]
    require(len(matches) == 1, "Expected one existing package file: " + package)
    return matches[0]


def forbidden_package(package):
    parts = package.strip("/").split("/")
    identifier = (parts[1] if len(parts) > 1 and parts[0] == "Script" else parts[0]).lower()
    # /Game/CubelessMCPTests is an ordinary asset folder, not a plugin mount.
    return ("modelcontextprotocol" in identifier or "toolset" in identifier or
            identifier.startswith("unrealmcp") or identifier.startswith("cubelessmcp"))


def enabled_plugins():
    names = sorted(str(name) for name in unreal.PluginBlueprintLibrary.get_enabled_plugin_names())
    forbidden = [name for name in names if "mcp" in name.lower() or "toolset" in name.lower() or "modelcontextprotocol" in name.lower()]
    return {"names": names, "forbidden_enabled": forbidden, "passed": not forbidden}


def dependency_audit(roots, registry, project_dir, engine_dir, report):
    profile_fields = {
        "hard_package": "include_hard_package_references",
        "soft_package": "include_soft_package_references",
        "hard_management": "include_hard_management_references",
        "soft_management": "include_soft_management_references",
    }
    options = {}
    option_values = {}
    all_fields = tuple(profile_fields.values()) + (
        "include_game_package_references", "include_editor_only_package_references", "include_searchable_names")
    for profile, selected in profile_fields.items():
        option = unreal.AssetRegistryDependencyOptions()
        # UE58 GetPackageQuery independently activates for Game/EditorOnly flags.
        # Those must also be false for management-only queries. Selecting only a
        # hard/soft package flag includes both Game and EditorOnly without a filter.
        for field in all_fields:
            option.set_editor_property(field, False)
        option.set_editor_property(selected, True)
        option_values[profile] = {field: bool(option.get_editor_property(field)) for field in all_fields}
        require(sum(option_values[profile].values()) == 1 and option_values[profile][selected],
                "Dependency category options did not retain the requested isolation")
        options[profile] = option
    audit = {"scope": "AssetRegistry disk package references and currently available management references",
             "root_packages": roots, "max_packages": 2048, "max_depth": 64, "max_edges": 16384,
             "packages": [], "edges": [], "native_script_terminals": [], "forbidden_references": [],
             "query_none_observations": [], "complete": False, "query_options": option_values,
             "limitations": ["Native /Script packages are terminals; native module binary dependencies are not traversed.",
                            "Searchable-name references and dynamic string-based loads are outside this audit.",
                            "AssetManager management rules are not generated or refreshed; management queries describe only the current registry.",
                            "Editor-only package references are included. This is not a cooked package or Shipping executable audit.",
                            "None from a category query can mean no registry entry/dependencies; these observations are retained."]}
    report["dependency_audit"] = audit
    pending = deque((name, 0) for name in roots)
    visited = set()
    while pending:
        package, depth = pending.popleft()
        if package in visited:
            continue
        require(len(visited) < audit["max_packages"] and depth <= audit["max_depth"], "Dependency traversal bound exceeded")
        visited.add(package)
        if forbidden_package(package):
            audit["forbidden_references"].append(package)
            raise RuntimeError("MCP/Toolset package dependency: " + package)
        if package.startswith("/Script/"):
            audit["native_script_terminals"].append(package)
            continue
        file = package_file(package, project_dir, engine_dir)
        registry.scan_files_synchronous([str(file)], False)
        assets = registry.get_assets_by_package_name(unreal.Name(package), True, False)
        require(assets is not None and len(assets) > 0, "No on-disk AssetRegistry record: " + package)
        audit["packages"].append({"package": package, "depth": depth, "file": str(file), "sha256": digest(file), "registry_asset_count": len(assets)})
        for profile, option in options.items():
            found = registry.get_dependencies(unreal.Name(package), option)
            if found is None:
                audit["query_none_observations"].append({"package": package, "category": profile})
                continue
            for dependency in sorted(set(str(name) for name in found)):
                require(len(audit["edges"]) < audit["max_edges"], "Dependency edge bound exceeded")
                audit["edges"].append({"from": package, "to": dependency, "category": profile})
                pending.append((dependency, depth + 1))
    audit["visited_count"] = len(visited)
    audit["complete"] = True
    audit["passed_within_declared_scope"] = not audit["forbidden_references"]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-file", required=True)
    parser.add_argument("--fixture-report", required=True)
    parser.add_argument("--report", required=True)
    args = parser.parse_args(argv)
    output = Path(args.report).resolve()
    require(not output.exists(), "Refusing to overwrite an existing verification report")
    output.parent.mkdir(parents=True, exist_ok=True)
    report = {"schema": "cubeless_mcp_free_editor_fixture_v1", "success": False, "stage": "preflight",
              "process_id": os.getpid(), "started_ns": time.time_ns(), "script_sha256": digest(__file__),
              "changes": [], "compile": {}, "save": {"requested": False, "attempted": False},
              "automatic_retry": False, "atomic_rollback": False,
              "validation": {"packaging": "not_run", "shipping_execution": "not_run", "runtime_marker": "not_run"}}
    tracked = {}
    write_report(output, report)
    try:
        project_file = Path(args.project_file).resolve()
        allowed_project = (Path(__file__).resolve().parents[2] / "workspaces/UE58RuntimeProof/CubelessRuntimeProof.uproject").resolve()
        require(project_file == allowed_project, "This validator is restricted to the isolated UE58RuntimeProof project")
        require(absolute(unreal.Paths.get_project_file_path()) == project_file, "Current editor project differs from --project-file")
        report["project_file"] = str(project_file)
        project_dir = project_file.parent
        engine_dir = absolute(unreal.Paths.engine_dir())
        report["engine_version"] = unreal.SystemLibrary.get_engine_version()
        require(report["engine_version"].startswith("5.8.3-"), "Only UE 5.8.3 is validated by this script")
        descriptor = json.loads(project_file.read_text(encoding="utf-8-sig"))
        require(descriptor.get("DisableEnginePluginsByDefault") is True, "Engine plugins must be disabled by default")
        require(not descriptor.get("Modules"), "The isolated proof project must not add gameplay C++ modules")
        for item in descriptor.get("Plugins", []):
            if item.get("Enabled", False):
                require(item["Name"] in {"PythonScriptPlugin", "EditorScriptingUtilities"} and
                        item.get("TargetAllowList") == ["Editor"], "Only editor-target Python/EditorScriptingUtilities may be explicitly enabled")
        local_plugins = list((project_dir / "Plugins").rglob("*.uplugin")) if (project_dir / "Plugins").exists() else []
        require(not local_plugins, "The proof project must contain no project plugins")
        report["project_descriptor_sha256"] = digest(project_file)
        report["plugins_before"] = enabled_plugins()
        require(report["plugins_before"]["passed"], "MCP/Toolset plugin unexpectedly enabled")
        require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, "PIE/SIE is active")
        dirty = list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
        require(not dirty, "Unsaved packages exist in the disposable editor")
        fixture_path = Path(args.fixture_report).resolve()
        fixture = json.loads(fixture_path.read_text(encoding="utf-8-sig"))
        require(fixture.get("success") is True and fixture.get("schema") == "cubeless_runtime_fixture_v1", "Fixture authoring did not succeed")
        require(fixture.get("process_id") != os.getpid() and Path(fixture["project_file"]).resolve() != project_file, "Proof must use a different process and project from authoring")
        report["fixture_report"] = str(fixture_path)
        report["fixture_report_sha256"] = digest(fixture_path)
        report["run_id"] = fixture["run_id"]
        assets = dict(fixture["assets"])
        assets["pipeline"] = fixture["pipeline_blueprint"]
        require(set(assets) == {"map", "actor", "save_game", "pipeline"}, "Expected exactly four fixture packages")
        for kind, package in assets.items():
            require(package.startswith("/Game/CubelessMCPTests/"), "Fixture outside the isolated test folder")
            file = package_file(package, project_dir, engine_dir)
            expected_hash = fixture["pipeline_asset_sha256_after"] if kind == "pipeline" else fixture["asset_files"][kind]["sha256"]
            tracked[kind] = {"path": str(file), "package": package, "sha256_before": digest(file), "expected_author_sha256": expected_hash}
            require(tracked[kind]["sha256_before"] == expected_hash, "Copied fixture bytes differ from author evidence: " + kind)
        report["asset_files"] = tracked
        sources = ["Source/Runtime/Engine/Public/PluginBlueprintLibrary.h",
                   "Source/Runtime/AssetRegistry/Public/AssetRegistry/IAssetRegistry.h",
                   "Source/Runtime/AssetRegistry/Private/AssetRegistry.cpp",
                   "Source/Editor/UnrealEd/Public/FileHelpers.h",
                   "Source/Editor/UnrealEd/Private/FileHelpers.cpp",
                   "Source/Runtime/Engine/Classes/Kismet/GameplayStatics.h",
                   "Source/Runtime/Engine/Private/GameplayStatics.cpp",
                   "Source/Editor/BlueprintEditorLibrary/Private/BlueprintEditorLibrary.cpp"]
        report["engine_source_sha256"] = {path: digest(engine_dir / path) for path in sources}
        report["stage"] = "dependency_audit"
        write_report(output, report)
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        dependency_audit(list(assets.values()), registry, project_dir, engine_dir, report)
        report["stage"] = "cold_map_load"
        report["changes"].append({"kind": "load_level_in_editor", "package": assets["map"], "attempted": True})
        write_report(output, report)
        # LoadMap takes a package filename and returns the editor world directly;
        # neither this call nor actor enumeration needs a LevelEditor UI subsystem.
        world = unreal.EditorLoadingAndSavingUtils.load_map(tracked["map"]["path"])
        require(world is not None, "Map could not load without MCP")
        require(world and world.get_path_name().split(".", 1)[0] == assets["map"], "Loaded world differs from fixture map")
        report["map_load_api"] = "EditorLoadingAndSavingUtils.load_map(package_filename)"
        report["actor_enumeration_api"] = "GameplayStatics.get_all_actors_of_class(loaded_world, Actor)"
        observed = [{"path": actor.get_path_name(), "class_path": actor.get_class().get_path_name(), "label": actor.get_actor_label(False)}
                    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor.static_class())]
        report["level_actors"] = observed
        for expected in fixture["spawned_actors"]:
            require(sum(item["class_path"] == expected["class_path"] and item["label"] == expected["label"] for item in observed) == 1,
                    "Fixture actor class/label is missing or duplicated: " + expected["label"])
        eas = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        for kind in ("save_game", "pipeline", "actor"):
            report["stage"] = "compile_" + kind
            blueprint = eas.load_asset(assets[kind])
            require(isinstance(blueprint, unreal.Blueprint), "Fixture Blueprint could not load: " + kind)
            parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
            expected_parent = unreal.SaveGame.static_class() if kind == "save_game" else unreal.Actor.static_class()
            require(parent and parent.get_path_name() == expected_parent.get_path_name(), "Unexpected fixture Blueprint parent: " + kind)
            item = {"attempted": True, "compiled": None, "status": "unknown_until_response", "parent": parent.get_path_name(), "save_attempted": False}
            report["compile"][kind] = item
            report["changes"].append({"kind": "compile_blueprint_in_memory", "package": assets[kind], "attempted": True})
            write_report(output, report)
            returned = bool(unreal.BlueprintEditorLibrary.compile_blueprint(blueprint))
            status = blueprint.get_editor_property("status")
            errors, warnings = [], []
            for graph in unreal.BlueprintEditorLibrary.list_graphs(blueprint):
                editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
                for collection, nodes in ((errors, editor.list_nodes_with_errors()), (warnings, editor.list_nodes_with_warnings())):
                    collection.extend({"node": node.get_path_name(), "message": node.get_editor_property("error_msg")} for node in nodes)
            item.update(returned=returned, status=str(status), errors=errors, warnings=warnings,
                        diagnostics_scope="K2 node compiler messages plus Blueprint compile bool/status; not FCompilerResultsLog parity")
            item["compiled"] = returned and status in (unreal.BlueprintStatus.BS_UP_TO_DATE, unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS) and not errors
            generated = blueprint.generated_class()
            item["generated_class"] = generated.get_path_name() if generated else None
            require(item["compiled"] and generated, "Blueprint failed to compile without MCP: " + kind)
        report["plugins_after"] = enabled_plugins()
        require(report["plugins_after"]["passed"], "MCP/Toolset plugin became enabled during validation")
        report["dirty_packages_after_compile"] = [p.get_path_name() for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())]
        report["validation"].update({"mcp_free_editor_load": "passed", "blueprint_compile_without_save": "passed", "map_actor_instances": "passed"})
        report["success"] = True
        report["stage"] = "complete"
    except Exception as error:
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
    finally:
        unchanged = True
        for item in tracked.values():
            file = Path(item["path"])
            item["sha256_after"] = digest(file) if file.is_file() else None
            item["disk_bytes_unchanged"] = item["sha256_after"] == item["sha256_before"]
            unchanged = unchanged and item["disk_bytes_unchanged"]
        report["asset_files"] = tracked
        report["validation"]["tracked_disk_bytes_unchanged"] = unchanged if len(tracked) == 4 else "not_fully_observed"
        if not unchanged:
            report["success"] = False
            report["error"] = report.get("error", "") + " Tracked asset disk bytes changed unexpectedly."
        report["finished_ns"] = time.time_ns()
        write_report(output, report)
        unreal.log("CUBELESS_MCP_FREE_FIXTURE_REPORT=" + str(output))
    return report["success"]


if __name__ == "__main__":
    if not main():
        raise RuntimeError("MCP-free editor fixture validation failed; inspect JSON report")
