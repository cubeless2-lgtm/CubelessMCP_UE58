"""Root-run editor fixture preparation, never imported as a public MCP tool.

Run through the editor's -ExecutePythonScript only in the isolated verification
project. Creates/saves one fresh native empty map; never compiles or edits BP.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import time
import traceback
import uuid

import unreal

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"


def require(condition, message):
    if not condition: raise RuntimeError(message)


def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def object_path(value): return value.get_path_name() if value else None
def dirty(): return [p.get_path_name() for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-file", required=True, type=Path); parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args(argv); output = args.report.resolve()
    require(args.project_file.resolve() == PROJECT.resolve(), "Only exact UE58Verification is permitted")
    require(not output.exists(), "Refusing to overwrite fixture evidence")
    run = uuid.uuid4().hex; package = "/Game/CubelessMCPTests/Actors_" + run + "/Map_Actors"
    disk = PROJECT.parent / "Content" / (package[len("/Game/"):] + ".umap")
    report = {"schema": "cubeless_actor_map_fixture_v1", "success": False, "stage": "preflight", "run_id": run,
        "project_file": str(PROJECT.resolve()), "process_id": os.getpid(), "map_package": package,
        "map_object": package + ".Map_Actors", "started_ns": time.time_ns(), "changes": [],
        "save": {"requested": False, "attempted": False, "saved": False}, "compile": {"requested": False},
        "automatic_retry": False, "atomic_rollback": False, "packaging": "not_run"}
    def persist():
        output.parent.mkdir(parents=True, exist_ok=True); tmp = output.with_suffix(output.suffix + ".partial")
        tmp.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"); tmp.replace(output)
    persist()
    try:
        actual = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
        require(actual == PROJECT.resolve(), "Wrong running project")
        report["engine_version"] = unreal.SystemLibrary.get_engine_version(); require(report["engine_version"].startswith("5.8.3-"), "Only UE58.3 is covered")
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        require(editor is not None and editor.get_game_world() is None, "PIE/SIE or missing editor subsystem")
        report["dirty_before"] = dirty(); require(not report["dirty_before"], "Preserve existing unsaved work")
        require(not disk.exists() and not unreal.EditorAssetLibrary.does_asset_exist(package), "Fresh fixture already exists")
        engine = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.engine_dir())).resolve()
        report["source_hashes"] = {"fixture_script": digest(__file__), "FileHelpers.h": digest(engine / "Source/Editor/UnrealEd/Public/FileHelpers.h"), "FileHelpers.cpp": digest(engine / "Source/Editor/UnrealEd/Private/FileHelpers.cpp")}
        report["stage"] = "new_blank_map"; change = {"kind": "create_map", "package": package, "attempted": True, "created_world": None}; report["changes"].append(change); persist()
        world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        require(world is not None, "NewBlankMap returned no world")
        change["created_world"] = object_path(world)
        report["stage"] = "save_fresh_map"; report["save"].update(requested=True, attempted=True); persist()
        report["save"]["saved"] = bool(unreal.EditorLoadingAndSavingUtils.save_map(world, package))
        require(report["save"]["saved"] and disk.is_file(), "Initial map save failed")
        report["stage"] = "load_saved_fixture"; persist()
        world = unreal.EditorLoadingAndSavingUtils.load_map(str(disk))
        require(world is not None, "Could not load the just-saved isolated map")
        require(object_path(world) == report["map_object"], "Saved new map did not acquire its exact object identity")
        actors = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor.static_class()))
        report["baseline_actors"] = sorted([{"name": a.get_name(), "class_path": object_path(a.get_class()), "actor_path": object_path(a)} for a in actors], key=lambda r: r["actor_path"])
        protected = [a for a in report["baseline_actors"] if a["class_path"] == "/Script/Engine.WorldSettings"]
        require(len(protected) == 1, "Need one known protected WorldSettings actor")
        report["protected_actor"] = protected[0]
        report["dirty_after"] = dirty(); require(not report["dirty_after"], "Fresh fixture remains dirty")
        report["asset_file"] = {"path": str(disk.resolve()), "sha256": digest(disk)}
        report.update(success=True, stage="complete")
    except Exception as error:
        report.update(error=str(error), traceback=traceback.format_exc())
    finally:
        report["finished_ns"] = time.time_ns(); persist()
    return report["success"]


if __name__ == "__main__" and not main(): raise RuntimeError("Actor map fixture failed; inspect partial evidence, do not automatically retry")
