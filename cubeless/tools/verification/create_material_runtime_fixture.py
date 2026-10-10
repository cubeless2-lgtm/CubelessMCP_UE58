"""Create a Material/MPC runtime clone and a separate proof map, without MCP.

Editor-internal execution only. Root prepares/launches the exact isolated project.
Preserves all six copied assets and all six sources; saves only a new clone/map.
The original runtime-fixture-B report remains a separate marker-only contract.
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

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from validate_fixture_without_mcp import absolute, dependency_audit, digest, enabled_plugins, package_file, require, write_report

ROOT = Path(__file__).resolve().parents[3]
ALLOWED_PROJECT = (ROOT / "cubeless/workspaces/UE58MaterialRuntimeProof/CubelessMaterialRuntimeProof.uproject").resolve()
CUBE_LABEL = "CubelessMaterialRuntimeCube"
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
SOURCE_PATHS = [
    "Source/Editor/MaterialEditor/Public/MaterialEditingLibrary.h",
    "Source/Editor/MaterialEditor/Private/MaterialEditingLibrary.cpp",
    "Source/Editor/UnrealEd/Public/FileHelpers.h",
    "Source/Editor/UnrealEd/Public/Subsystems/EditorActorSubsystem.h",
    "Source/Runtime/Engine/Classes/GameFramework/Actor.h",
    "Source/Runtime/Engine/Classes/Components/PrimitiveComponent.h",
    "Source/Runtime/Engine/Classes/Components/StaticMeshComponent.h",
    "Source/Runtime/Engine/Public/Materials/MaterialParameterCollection.h",
    "Source/Developer/AssetTools/Public/IAssetTools.h",
    "Source/Editor/UnrealEd/Public/Subsystems/EditorAssetSubsystem.h",
]


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def path_of(value):
    return value.get_path_name() if value is not None else None


def preflight(project_file, report):
    project_file = Path(project_file).resolve()
    require(project_file == ALLOWED_PROJECT and project_file.is_file(), "Restricted to the prepared UE58MaterialRuntimeProof project")
    require(absolute(unreal.Paths.get_project_file_path()) == project_file, "Current editor differs from the requested isolated project")
    report["project_file"] = str(project_file)
    report["engine_version"] = unreal.SystemLibrary.get_engine_version()
    require(report["engine_version"].startswith("5.8.3-"), "Only installed UE5.8.3 is covered")
    descriptor = read_json(project_file)
    require(descriptor.get("DisableEnginePluginsByDefault") is True and not descriptor.get("Modules"), "Require disabled engine plugins by default and no gameplay C++ modules")
    for plugin in descriptor.get("Plugins", []):
        if plugin.get("Enabled"):
            require(plugin["Name"] in {"PythonScriptPlugin", "EditorScriptingUtilities"} and plugin.get("TargetAllowList") == ["Editor"], "Only editor-target Python and EditorScriptingUtilities are permitted")
    local = project_file.parent / "Plugins"
    require(not local.exists() or not list(local.rglob("*.uplugin")), "Project must contain no plugins")
    report["project_descriptor_sha256"] = digest(project_file)
    report["plugins_before"] = enabled_plugins()
    require(report["plugins_before"]["passed"], "MCP/Toolset plugins are enabled")
    require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, "PIE/SIE is active")
    dirty = list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    report["initial_dirty_packages"] = [item.get_path_name() for item in dirty]
    require(not dirty, "Dirty packages exist; do not save or discard them")
    engine_dir = absolute(unreal.Paths.engine_dir())
    report["engine_source_sha256"] = {name: digest(engine_dir / name) for name in SOURCE_PATHS}
    return project_file, engine_dir


def load_contracts(preparation_path, author_path, marker_path, report):
    from material_pipeline_report import verify
    preparation_path, author_path, marker_path = [Path(path).resolve() for path in (preparation_path, author_path, marker_path)]
    preparation, author, marker = map(read_json, (preparation_path, author_path, marker_path))
    require(preparation.get("schema") == "cubeless_material_runtime_project_preparation_v1", "Unexpected preparation schema")
    require(Path(preparation["project_file"]).resolve() == ALLOWED_PROJECT and preparation.get("plugins_copied") is False, "Preparation identifies another project or copied plugins")
    require(author_path.name == preparation["material_author_report"] and digest(author_path) == preparation["material_author_sha256"], "Material author does not match preparation bytes")
    require(verify(author, require_phase="author")["passed"], "Material author evidence failed independent verification")
    require(marker_path.name == preparation["runtime_fixture_report"] and marker.get("schema") == "cubeless_runtime_fixture_v1" and marker.get("success") is True, "Marker parent is not the prepared runtime fixture")
    require(marker.get("validation", {}).get("runtime_marker_created_by_this_script") is False, "Marker authoring report must exclude runtime marker creation")
    require(marker.get("slot_name") == "Cubeless_" + marker.get("run_id", ""), "Marker run/slot mismatch")
    require(marker.get("marker_class_path") == marker["assets"]["save_game"] + ".BP_MarkerData_C", "Marker class does not match the SaveGame asset")
    packages = {**marker["assets"], "pipeline": marker["pipeline_blueprint"], **author["assets"]}
    require(set(packages) == {"save_game", "actor", "map", "pipeline", "material", "collection"}, "Expected six exact fixture packages")
    expected_hashes = {kind: marker["asset_files"][kind]["sha256"] for kind in marker["assets"]}
    expected_hashes["pipeline"] = marker["pipeline_asset_sha256_after"]
    files = preparation.get("copied_files", [])
    require(len(files) == 6 and len({row["path"] for row in files}) == 6, "Preparation must contain exactly six distinct files")
    lookup = {row["path"].replace("\\", "/"): row for row in files}
    tracked = {}
    for kind, package in packages.items():
        require(package.startswith("/Game/CubelessMCPTests/") and ".." not in package, "Fixture outside the test mount")
        relative = "Content/" + package[len("/Game/"):] + (".umap" if kind == "map" else ".uasset")
        row = lookup.get(relative)
        require(row is not None, "Preparation is missing the exact package: " + package)
        copied = (ALLOWED_PROJECT.parent / relative).resolve()
        require(copied.is_relative_to((ALLOWED_PROJECT.parent / "Content").resolve()), "Copied asset escaped Content")
        source = Path(row["source"]).resolve()
        require(source != copied and not source.is_relative_to(ALLOWED_PROJECT.parent), "Source must be outside the mutation project")
        require(source.is_file() and copied.is_file(), "Source/copied asset missing: " + kind)
        require(digest(source) == digest(copied) == row["sha256"], "Source/copy hash mismatch: " + kind)
        require(kind not in expected_hashes or row["sha256"] == expected_hashes[kind], "Marker parent hash differs from preparation: " + kind)
        tracked[kind] = {"package": package, "path": str(copied), "source": str(source), "sha256_before": row["sha256"], "source_sha256_before": row["sha256"]}
    report["parents"] = {name: {"path": str(path), "sha256": digest(path)} for name, path in (("preparation", preparation_path), ("material_author", author_path), ("runtime_marker", marker_path))}
    report["assets"] = packages
    report["marker_contract"] = {key: marker[key] for key in ("run_id", "slot_name", "marker_class_path", "runtime_function_paths")}
    report["marker_contract"]["scope"] = "Unmodified runtime-fixture-B reused only for SaveGame marker validation; material/map evidence is this separate report"
    report["asset_files"] = tracked
    return author, marker, tracked


def inspect_material(author):
    eas = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    material = eas.load_asset(author["assets"]["material"])
    collection = eas.load_asset(author["assets"]["collection"])
    require(isinstance(material, unreal.Material) and isinstance(collection, unreal.MaterialParameterCollection), "Material/MPC cannot load without MCP")
    expected = author["fixture"]["nodes"]
    expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
    observed = {item.get_path_name(): item for item in expressions}
    require(set(observed) == {expected[key] for key in ("source", "add", "collection")}, "Saved expression identities differ from author")
    add = observed[expected["add"]]
    names = [str(name) for name in unreal.MaterialEditingLibrary.get_material_expression_input_names(add)]
    sources = list(unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, add))
    require(names == ["A", "B"] and len(sources) == 2, "Add expression input contract differs")
    inputs = {name: path_of(value) for name, value in zip(names, sources)}
    require(inputs == {"A": expected["source"], "B": None}, "Material input wiring differs")
    root = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    require(path_of(root) == expected["add"], "BaseColor root wiring differs")
    collection_node = observed[expected["collection"]]
    require(path_of(collection_node.get_editor_property("collection")) == collection.get_path_name(), "Expression references a different MPC")
    require(str(collection_node.get_editor_property("parameter_name")) == "CubelessScalar", "Collection expression parameter differs")
    scalars, vectors = list(collection.get_editor_property("scalar_parameters")), list(collection.get_editor_property("vector_parameters"))
    require(len(scalars) == len(vectors) == 1, "MPC parameter counts differ")
    scalar = {"name": str(scalars[0].get_editor_property("parameter_name")), "default": float(scalars[0].get_editor_property("default_value"))}
    color = vectors[0].get_editor_property("default_value")
    vector = {"name": str(vectors[0].get_editor_property("parameter_name")), "default": {key: float(getattr(color, key)) for key in ("r", "g", "b", "a")}}
    require(scalar["name"] == "CubelessScalar" and abs(scalar["default"] - 0.25) < 1e-6, "MPC scalar value differs")
    require(vector["name"] == "CubelessVector" and all(abs(vector["default"][key] - value) < 1e-5 for key, value in {"r": 0.1, "g": 0.2, "b": 0.3, "a": 1.0}.items()), "MPC vector value differs")
    return material, {"material": material.get_path_name(), "collection": collection.get_path_name(), "expression_paths": sorted(observed), "add_inputs": inputs, "base_color_source": path_of(root), "scalar": scalar, "vector": vector, "passed": True}


def inspect_map(world, marker, material, expect_cube):
    actors = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor.static_class()))
    observed = [{"path": actor.get_path_name(), "label": actor.get_actor_label(False), "class_path": actor.get_class().get_path_name()} for actor in actors]
    for expected in marker["spawned_actors"]:
        require(sum(row["label"] == expected["label"] and row["class_path"] == expected["class_path"] for row in observed) == 1, "Original fixture actor missing/duplicated: " + expected["label"])
    cubes = [actor for actor in actors if actor.get_actor_label(False) == CUBE_LABEL]
    require(len(cubes) == int(expect_cube), "Unexpected existing/missing proof cube; do not automatically rerun authoring")
    result = {"actors": observed, "existing_marker_and_pipeline_actors_verified": True}
    if expect_cube:
        cube = cubes[0]
        require(cube.get_class().get_path_name() == "/Script/Engine.StaticMeshActor", "Cube must use the native Engine actor")
        component = cube.get_component_by_class(unreal.StaticMeshComponent.static_class())
        # GetStaticMesh is an unreflected C++ method in 5.8.3; StaticMesh is an
        # exposed UPROPERTY(EditAnywhere, BlueprintReadOnly, AllowPrivateAccess).
        require(component is not None and path_of(component.get_editor_property("static_mesh")) == CUBE_PATH, "Cube StaticMesh readback differs")
        require(path_of(component.get_material(0)) == material.get_path_name(), "Cube material slot0 readback differs")
        result["cube"] = {"path": cube.get_path_name(), "label": CUBE_LABEL, "class_path": cube.get_class().get_path_name(), "mesh": path_of(component.get_editor_property("static_mesh")), "material_slot": 0, "material": path_of(component.get_material(0))}
    return result


def inspect_runtime_material(material, collection_path):
    expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
    require(len(expressions) == 3, "Runtime clone must retain three expressions")
    by_class = {}
    for node in expressions:
        by_class.setdefault(node.get_class().get_path_name(), []).append(node)
    expected_classes = {"/Script/Engine.MaterialExpressionConstant", "/Script/Engine.MaterialExpressionAdd", "/Script/Engine.MaterialExpressionCollectionParameter"}
    require(set(by_class) == expected_classes and all(len(value) == 1 for value in by_class.values()), "Runtime clone expression classes differ")
    constant = by_class["/Script/Engine.MaterialExpressionConstant"][0]
    add = by_class["/Script/Engine.MaterialExpressionAdd"][0]
    collection = by_class["/Script/Engine.MaterialExpressionCollectionParameter"][0]
    names = [str(name) for name in unreal.MaterialEditingLibrary.get_material_expression_input_names(add)]
    sources = list(unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, add))
    inputs = dict(zip(names, [path_of(node) for node in sources]))
    require(names == ["A", "B"] and len(sources) == 2 and inputs == {"A": constant.get_path_name(), "B": collection.get_path_name()}, "Runtime clone lacks Constant/MPC Add inputs")
    require(path_of(collection.get_editor_property("collection")) == collection_path and str(collection.get_editor_property("parameter_name")) == "CubelessScalar", "Runtime clone collection reference differs")
    require(path_of(unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)) == add.get_path_name(), "Runtime clone BaseColor differs")
    return {"passed": True, "material": material.get_path_name(), "expression_paths": sorted(node.get_path_name() for node in expressions), "add_inputs": inputs, "collection": collection_path, "mpc_is_connected_to_base_color": True, "scope": "Official API clone adds MPC usage; this is distinct from the unchanged Cubeless E3 graph"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("project-file", "preparation-report", "material-author-report", "marker-fixture-report", "report"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--run-suffix", required=True, help="Fresh explicit attempt identity, e.g. B; never reuse an earlier asset root")
    parser.add_argument("--prior-attempt-report", help="Read-only provenance for an earlier failed attempt; never reused or changed")
    args = parser.parse_args(argv)
    require(re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]{0,31}", args.run_suffix), "Invalid fresh run suffix")
    output = Path(args.report).resolve()
    require(not output.exists(), "Do not overwrite fixture evidence")
    output.parent.mkdir(parents=True, exist_ok=True)
    report = {"schema": "cubeless_material_runtime_fixture_v1", "success": False, "stage": "preflight", "process_id": os.getpid(), "started_ns": time.time_ns(), "script_sha256": digest(__file__), "changes": [], "compile": {"requested": False, "shader_completion_verified": False}, "save": {}, "automatic_retry": False, "atomic_rollback": False, "validation": {"rendering_quality": "not_verified", "packaging": "not_run", "shipping_execution": "not_run", "runtime_marker_created_by_this_script": False}}
    tracked = {}
    write_report(output, report)
    try:
        project, engine_dir = preflight(args.project_file, report)
        author, marker, tracked = load_contracts(args.preparation_report, args.material_author_report, args.marker_fixture_report, report)
        report["run_suffix"] = args.run_suffix
        report["fixture_run_id"] = author["run_id"] + "_" + args.run_suffix
        report["new_operation"] = {"mode": "fresh_owned_asset_root", "resumed_prior_mutation": False, "previous_assets_reused": False}
        if args.prior_attempt_report:
            prior_path = Path(args.prior_attempt_report).resolve()
            prior = read_json(prior_path)
            require(prior.get("schema") == report["schema"] and prior.get("success") is False and Path(prior["project_file"]).resolve() == project, "Prior attempt must be a failed report for this project")
            require(prior["parents"]["material_author"]["sha256"] == digest(args.material_author_report), "Prior attempt has different Material provenance")
            report["prior_attempt"] = {"path": str(prior_path), "sha256": digest(prior_path), "status": "preserved_failed", "failed_stage": prior["stage"]}
        report["stage"] = "material_readback"
        write_report(output, report)
        material, report["material_readback"] = inspect_material(author)
        report["source_assets"] = dict(report["assets"])
        runtime_root = "/Game/CubelessMCPTests/RuntimeMaterial_" + report["fixture_run_id"]
        clone_path = runtime_root + "/M_RuntimeMPC"
        map_path = runtime_root + "/Map_MaterialRuntimeProof"
        if args.prior_attempt_report:
            require(clone_path != prior.get("assets", {}).get("runtime_material") and map_path != prior.get("assets", {}).get("map"), "New attempt must use different assets from prior failure")
        eas = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        require(not eas.does_directory_exist(runtime_root), "Runtime fixture root already exists; no automatic rerun")
        report["assets"].update(runtime_material=clone_path, map=map_path)
        report["stage"] = "duplicate_material_for_runtime_mpc"
        report["changes"].append({"kind": "duplicate_material", "source": material.get_path_name(), "target": clone_path, "attempted": True, "created": None})
        write_report(output, report)
        clone = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset("M_RuntimeMPC", runtime_root, material)
        require(isinstance(clone, unreal.Material), "Material duplicate failed")
        report["changes"][-1]["created"] = clone.get_path_name()
        nodes = list(unreal.MaterialEditingLibrary.get_material_expressions(clone))
        adds = [node for node in nodes if isinstance(node, unreal.MaterialExpressionAdd)]
        collections = [node for node in nodes if isinstance(node, unreal.MaterialExpressionCollectionParameter)]
        require(len(adds) == len(collections) == 1, "Clone selectors are ambiguous")
        report["stage"] = "connect_runtime_mpc"
        report["changes"].append({"kind": "official_connect_collection_to_add_b", "asset": clone.get_path_name(), "attempted": True, "connected": None})
        write_report(output, report)
        report["changes"][-1]["connected"] = bool(unreal.MaterialEditingLibrary.connect_material_expressions(collections[0], "", adds[0], "B"))
        require(report["changes"][-1]["connected"], "Runtime MPC connection failed")
        report["runtime_material_readback"] = inspect_runtime_material(clone, author["objects"]["collection"])
        report["stage"] = "request_runtime_material_recompile"
        report["compile"] = {"requested": True, "attempted": True, "returned": False, "shader_completion_verified": False, "scope": "Public RecompileMaterial returned current resource errors; empty also occurs when no material resource exists"}
        write_report(output, report)
        errors = [str(error) for error in unreal.MaterialEditingLibrary.recompile_material(clone)]
        report["compile"].update(returned=True, reported_errors=errors, status="returned_without_reported_errors" if not errors else "reported_errors")
        require(not errors, "Runtime clone reports Material compiler errors")
        report["stage"] = "save_runtime_material"
        report["save"]["runtime_material"] = {"attempted": True, "saved": None, "package": clone_path}
        write_report(output, report)
        report["save"]["runtime_material"]["saved"] = bool(eas.save_loaded_asset(clone, False))
        require(report["save"]["runtime_material"]["saved"], "Runtime clone save failed")
        report["stage"] = "load_copied_map"
        write_report(output, report)
        world = unreal.EditorLoadingAndSavingUtils.load_map(tracked["map"]["path"])
        require(world is not None and world.get_path_name().split(".", 1)[0] == report["source_assets"]["map"], "Loaded world differs from copied map")
        report["map_before"] = inspect_map(world, marker, clone, False)
        report["stage"] = "spawn_cube"
        report["changes"].append({"kind": "spawn_native_engine_cube", "map": report["assets"]["map"], "attempted": True, "completed": False})
        write_report(output, report)
        mesh = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).load_asset(CUBE_PATH)
        require(isinstance(mesh, unreal.StaticMesh), "Engine BasicShapes Cube missing")
        cube = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.StaticMeshActor.static_class(), unreal.Vector(600, 0, 0), unreal.Rotator(0, 0, 0), False)
        require(cube is not None, "Engine cube spawn failed")
        cube.set_actor_label(CUBE_LABEL)
        component = cube.get_component_by_class(unreal.StaticMeshComponent.static_class())
        require(component is not None and component.set_static_mesh(mesh), "Cube mesh assignment failed")
        component.set_material(0, clone)
        report["map_after"] = inspect_map(world, marker, clone, True)
        report["changes"][-1]["completed"] = True
        report["stage"] = "save_new_runtime_map"
        report["save"]["map"] = {"attempted": True, "saved": None, "package": map_path, "source_map_saved": False}
        write_report(output, report)
        report["save"]["map"]["saved"] = bool(unreal.EditorLoadingAndSavingUtils.save_map(world, map_path))
        require(report["save"]["map"]["saved"], "New runtime map save failed")
        report["map_after"] = inspect_map(world, marker, clone, True)
        report["new_asset_files"] = {}
        for kind, package in (("runtime_material", clone_path), ("map", map_path)):
            file = package_file(package, project.parent, engine_dir)
            report["new_asset_files"][kind] = {"package": package, "path": str(file), "sha256": digest(file), "size": file.stat().st_size}
        report["stage"] = "dependency_audit"
        write_report(output, report)
        dependency_audit(list(report["assets"].values()), unreal.AssetRegistryHelpers.get_asset_registry(), project.parent, engine_dir, report)
        report["plugins_after"] = enabled_plugins()
        require(report["plugins_after"]["passed"], "MCP/Toolset became enabled")
        report["validation"].update(material_mpc_readback="passed", copied_map_assignment="passed", dependency_audit="passed_within_declared_scope", cold_reload="not_run")
        report.update(success=True, stage="complete")
    except Exception as error:
        report.update(error=str(error), traceback=traceback.format_exc(), retry_policy="Inspect actual partial map/assets; no automatic retry and no rollback")
    finally:
        unchanged = len(tracked) == 6
        for kind, item in tracked.items():
            item["sha256_after"] = digest(item["path"]) if Path(item["path"]).is_file() else None
            item["source_sha256_after"] = digest(item["source"]) if Path(item["source"]).is_file() else None
            item["source_unchanged"] = item["source_sha256_after"] == item["source_sha256_before"]
            item["copied_bytes_unchanged"] = item["sha256_after"] == item["sha256_before"]
            unchanged = unchanged and item["source_unchanged"] and item["copied_bytes_unchanged"]
        report["validation"]["six_copies_and_all_sources_unchanged"] = unchanged
        if not unchanged:
            report["success"] = False
            report["error"] = report.get("error", "") + " Preservation checks incomplete or failed."
        report["finished_ns"] = time.time_ns()
        write_report(output, report)
        unreal.log("CUBELESS_MATERIAL_RUNTIME_FIXTURE_REPORT=" + str(output))
    return report["success"]


if __name__ == "__main__" and not main():
    raise RuntimeError("Material runtime fixture creation failed; inspect report")
