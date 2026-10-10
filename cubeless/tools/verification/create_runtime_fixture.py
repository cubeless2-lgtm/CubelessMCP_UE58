"""Editor-internal authoring of an isolated, MCP-independent Shipping proof map.

Run only in a disposable editor using -ExecutePythonScript. This script authors
assets; it never runs PIE, creates a save-game marker, packages, or starts a game.
The resulting Actor uses only Engine SaveGame functions and QuitGame at runtime.
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


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def pin_identity(pin):
    # Reflected FBlueprintGraphPin wrappers do not have stable Python identity.
    # Compare the owning node object path plus pin name/direction instead.
    return (pin.get_owning_node().get_path_name(), str(pin.get_pin_name()),
            str(pin.get_pin_direction()))


def absolute(path):
    return Path(unreal.Paths.convert_relative_path_to_full(str(path))).resolve()


def write_report(path, report):
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def package_file(project_dir, package, suffix):
    require(package.startswith("/Game/"), "Only /Game assets are permitted")
    path = (project_dir / "Content" / (package[len("/Game/"):] + suffix)).resolve()
    require(path.is_relative_to((project_dir / "Content").resolve()), "Asset path escapes project Content")
    return path


def compile_asset(blueprint):
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    status = blueprint.get_editor_property("status")
    accepted = status in (unreal.BlueprintStatus.BS_UP_TO_DATE, unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS)
    diagnostics = []
    for graph in unreal.BlueprintEditorLibrary.list_graphs(blueprint):
        graph_editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
        for node in graph_editor.list_nodes_with_errors():
            diagnostics.append({"node": node.get_path_name(), "message": node.get_editor_property("error_msg")})
    return {"attempted": True, "compiled": accepted and not diagnostics, "status": str(status), "errors": diagnostics}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-file", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--asset-root", required=True)
    parser.add_argument("--pipeline-blueprint", required=True)
    parser.add_argument("--report", required=True)
    args = parser.parse_args(argv)
    output = Path(args.report).resolve()
    require(not output.exists(), "Refusing to overwrite an existing fixture report")
    output.parent.mkdir(parents=True, exist_ok=True)
    report = {"schema": "cubeless_runtime_fixture_v1", "success": False, "stage": "preflight",
              "started_ns": time.time_ns(), "run_id": args.run_id, "asset_root": args.asset_root,
              "changes": [], "compile": {}, "save": {}, "validation": {"shipping_execution": "not_run"},
              "atomic_rollback": False, "automatic_retry": False, "process_id": os.getpid(),
              "script_sha256": digest(__file__), "script_path": str(Path(__file__).resolve())}
    write_report(output, report)
    try:
        expected_project = Path(args.project_file).resolve()
        actual_project = absolute(unreal.Paths.get_project_file_path())
        report["project_file"] = str(actual_project)
        report["engine_version"] = unreal.SystemLibrary.get_engine_version()
        require(expected_project == actual_project and expected_project.is_file(), "Current editor project does not match --project-file exactly")
        require(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", args.run_id), "Invalid run-id")
        expected_root = "/Game/CubelessMCPTests/RuntimeProof/" + args.run_id
        require(args.asset_root == expected_root, "asset-root must exactly equal /Game/CubelessMCPTests/RuntimeProof/<run-id>")
        input_package = args.pipeline_blueprint.split(".", 1)[0]
        require(input_package.startswith("/Game/CubelessMCPTests/") and ".." not in input_package, "pipeline-blueprint must be an isolated Cubeless test asset")
        editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        require(editor_world.get_game_world() is None, "PIE/SIE is active")
        dirty_content = list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
        dirty_maps = list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
        report["initial_dirty_packages"] = [p.get_path_name() for p in dirty_content + dirty_maps]
        require(not dirty_content and not dirty_maps, "Unsaved content/maps exist; fixture creation will not save or discard them")
        eas = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        require(not eas.does_directory_exist(args.asset_root), "Run root already exists; use a new run-id after inspection")
        pipeline_bp = eas.load_asset(args.pipeline_blueprint)
        require(isinstance(pipeline_bp, unreal.Blueprint), "pipeline-blueprint is not a Blueprint")
        pipeline_class = pipeline_bp.generated_class()
        require(pipeline_class and unreal.MathLibrary.class_is_child_of(pipeline_class, unreal.Actor.static_class()), "pipeline-blueprint must have a compiled Actor generated class")
        project_dir = actual_project.parent
        pipeline_file = package_file(project_dir, input_package, ".uasset")
        require(pipeline_file.is_file(), "Pipeline Blueprint must already be saved")
        report["pipeline_blueprint"] = input_package
        report["pipeline_asset_sha256_before"] = digest(pipeline_file)
        source_paths = {
            "gameplay_save_functions": "Source/Runtime/Engine/Private/GameplayStatics.cpp",
            "runtime_save_path": "Source/Runtime/Engine/Public/SaveGameSystem.h",
            "graph_authoring_api": "Source/Editor/BlueprintEditorLibrary/Private/BlueprintEditorLibrary/BlueprintGraphEditor.h",
        }
        engine_dir = absolute(unreal.Paths.engine_dir())
        report["engine_source_sha256"] = {name: digest(engine_dir / relative) for name, relative in source_paths.items()}
        report["assets"] = {"save_game": args.asset_root + "/BP_MarkerData", "actor": args.asset_root + "/BP_RuntimeProof",
                            "map": args.asset_root + "/Map_RuntimeProof"}
        slot = "Cubeless_" + args.run_id
        report["slot_name"] = slot

        def create_bp(kind, parent):
            path = report["assets"][kind]
            report["stage"] = "create_" + kind
            change = {"kind": "create_blueprint", "asset_path": path, "attempted": True, "created": None}
            report["changes"].append(change)
            write_report(output, report)
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit("/", 1)[1], args.asset_root, unreal.Blueprint.static_class(), factory)
            change["created"] = bp is not None
            require(isinstance(bp, unreal.Blueprint), "Blueprint factory failed for " + path)
            return bp

        def compile_save(kind, bp):
            report["stage"] = "compile_" + kind
            report["compile"][kind] = {"attempted": True, "compiled": None, "status": "unknown_until_response"}
            write_report(output, report)
            report["compile"][kind] = compile_asset(bp)
            require(report["compile"][kind]["compiled"], "Blueprint compile failed for " + kind)
            report["stage"] = "save_" + kind
            report["save"][kind] = {"attempted": True, "saved": None}
            write_report(output, report)
            report["save"][kind]["saved"] = bool(eas.save_loaded_asset(bp, False))
            require(report["save"][kind]["saved"], "Blueprint save failed for " + kind)

        save_bp = create_bp("save_game", unreal.SaveGame.static_class())
        compile_save("save_game", save_bp)
        save_class_path = save_bp.generated_class().get_path_name()
        report["marker_class_path"] = save_class_path
        actor_bp = create_bp("actor", unreal.Actor.static_class())
        graph = unreal.BlueprintEditorLibrary.find_event_graph(actor_bp)
        require(graph is not None, "Actor Blueprint has no EventGraph")
        graph_editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
        report["stage"] = "author_runtime_graph"
        report["changes"].append({"kind": "author_begin_play_save_marker_graph", "asset_path": actor_bp.get_path_name(), "attempted": True})
        write_report(output, report)
        event = graph_editor.find_event_node(unreal.Name("ReceiveBeginPlay"))
        if event is None:
            event = unreal.BlueprintEditorLibrary.add_event_override(actor_bp, unreal.Name("ReceiveBeginPlay"), unreal.IntPoint(0, 0))
        require(event is not None, "Could not resolve/add ReceiveBeginPlay")
        paths = ["/Script/Engine.GameplayStatics.CreateSaveGameObject", "/Script/Engine.GameplayStatics.SaveGameToSlot",
                 "/Script/Engine.KismetSystemLibrary.QuitGame"]
        create_node, save_node, quit_node = [graph_editor.add_call_function_node(path) for path in paths]
        require(all((create_node, save_node, quit_node)), "Could not create the runtime Engine function nodes")
        for index, node in enumerate((event, create_node, save_node, quit_node)):
            node.set_node_pos(unreal.IntPoint(index * 320, 0))
        require(create_node.find_input_pin("SaveGameClass").set_pin_value(save_class_path), "Could not set SaveGameClass")
        require(save_node.find_input_pin("SlotName").set_pin_value(slot), "Could not set SlotName")
        require(save_node.find_input_pin("UserIndex").set_pin_value("0"), "Could not set UserIndex")
        edges = [(event.find_then_pin(), create_node.find_execute_pin()),
                 (create_node.find_then_pin(), save_node.find_execute_pin()),
                 (create_node.find_output_pin("ReturnValue"), save_node.find_input_pin("SaveGameObject")),
                 (save_node.find_then_pin(), quit_node.find_execute_pin())]
        report["runtime_function_paths"] = paths
        report["graph_connections"] = []
        for source, target in edges:
            require(source.is_valid() and target.is_valid(), "A required graph pin was not resolved")
            require(source.try_create_connection(target), "Graph schema rejected a required connection")
            forward = any(pin_identity(pin) == pin_identity(target) for pin in source.list_connected_pins())
            backward = any(pin_identity(pin) == pin_identity(source) for pin in target.list_connected_pins())
            report["graph_connections"].append({"source_node": source.get_owning_node().get_path_name(),
                "source_pin": str(source.get_pin_name()), "target_node": target.get_owning_node().get_path_name(),
                "target_pin": str(target.get_pin_name()), "forward_verified": forward, "reverse_verified": backward})
            require(forward and backward, "Connection readback failed")
        report["validation"]["marker_pin_readback"] = {
            "save_game_class": create_node.find_input_pin("SaveGameClass").get_pin_value(),
            "slot_name": save_node.find_input_pin("SlotName").get_pin_value(),
            "user_index": save_node.find_input_pin("UserIndex").get_pin_value()}
        require(report["validation"]["marker_pin_readback"]["slot_name"] == slot, "Slot name readback mismatch")
        compile_save("actor", actor_bp)

        report["stage"] = "create_map"
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        map_path = report["assets"]["map"]
        report["changes"].append({"kind": "create_map", "asset_path": map_path, "attempted": True})
        report["save"]["initial_map"] = {"attempted": True, "saved": None}
        write_report(output, report)
        report["save"]["initial_map"]["saved"] = bool(les.new_level(map_path, False))
        require(report["save"]["initial_map"]["saved"], "Could not create/save isolated map")
        report["stage"] = "spawn_fixture_actors"
        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        report["spawned_actors"] = []
        for label, actor_class, x in (("CubelessAuthoredPipelineActor", pipeline_class, 0),
                                      ("CubelessShippingRuntimeProof", actor_bp.generated_class(), 300)):
            change = {"kind": "spawn_actor", "class_path": actor_class.get_path_name(), "attempted": True, "spawned": None}
            report["changes"].append(change)
            write_report(output, report)
            actor = actor_subsystem.spawn_actor_from_class(actor_class, unreal.Vector(x, 0, 0), unreal.Rotator(0, 0, 0), False)
            change["spawned"] = actor is not None
            require(actor is not None, "Could not spawn " + label)
            actor.set_actor_label(label)
            report["spawned_actors"].append({"label": label, "path": actor.get_path_name(), "class_path": actor.get_class().get_path_name()})
        report["stage"] = "save_map"
        report["save"]["map"] = {"attempted": True, "saved": None}
        write_report(output, report)
        report["save"]["map"]["saved"] = bool(les.save_current_level())
        require(report["save"]["map"]["saved"], "Could not save the populated map")
        report["stage"] = "saved_file_readback"
        report["asset_files"] = {}
        for kind, package in report["assets"].items():
            file = package_file(project_dir, package, ".umap" if kind == "map" else ".uasset")
            require(file.is_file(), "Expected saved asset file is missing: " + str(file))
            report["asset_files"][kind] = {"path": str(file), "sha256": digest(file), "size": file.stat().st_size}
        report["pipeline_asset_sha256_after"] = digest(pipeline_file)
        require(report["pipeline_asset_sha256_after"] == report["pipeline_asset_sha256_before"], "Existing pipeline Blueprint disk bytes changed")
        report["validation"].update({"authoring_readback": "passed", "disk_reload": "not_run", "dependency_audit": "not_run",
                                      "runtime_marker_created_by_this_script": False})
        report["stage"] = "complete"
        report["success"] = True
    except Exception as error:
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        report["retry_policy"] = "Inspect partial assets and this report; never automatically rerun a mutation. No rollback performed."
    finally:
        report["finished_ns"] = time.time_ns()
        write_report(output, report)
        unreal.log("CUBELESS_RUNTIME_FIXTURE_REPORT=" + str(output))
    return 0 if report["success"] else 1


if __name__ == "__main__":
    # Let the official startup Python executor finish and close its own editor.
    # A Python exception records failure in both the file and the editor log.
    code = main()
    if code:
        raise RuntimeError("Runtime fixture authoring failed; inspect its report")
