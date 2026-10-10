"""Author a new Actor proof map in the exact prepared no-MCP workspace.

Root runs this in UE58; it never launches processes. Only a marker Actor is
added to the copied source world, then SaveMap writes one NEW map package.
The saved Cubeless actors and source packages are never modified on disk.
"""
import argparse
import os
from pathlib import Path
import sys
import time
import traceback

sys.path.insert(0, str(Path(__file__).resolve().parent))
import actor_runtime_manifest as contract
from actor_runtime_manifest import digest, object_path, read_json, require


def path(value): return value.get_path_name() if value is not None else None


def preflight(manifest, report):
    import unreal
    from validate_fixture_without_mcp import absolute, enabled_plugins
    require(absolute(unreal.Paths.get_project_file_path()) == contract.PROJECT.resolve(), "Not exact UE58ActorRuntimeProof project")
    contract.validate(manifest, copied=True)
    report["configuration"] = contract.verify_config(manifest)
    report["engine_version"] = unreal.SystemLibrary.get_engine_version(); require(report["engine_version"].startswith("5.8.3-"), "Only UE58.3 supported")
    require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, "PIE/SIE active")
    report["plugins_before"] = enabled_plugins(); require(report["plugins_before"]["passed"], "MCP/Toolset enabled")
    report["dirty_before"] = dirty(); require(report["dirty_before"] == [], "Unsaved packages exist; no save/discard permitted")
    engine = absolute(unreal.Paths.engine_dir())
    report["engine_api_sources"] = {name: digest(engine/name) for name in ["Source/Editor/UnrealEd/Public/FileHelpers.h", "Source/Editor/UnrealEd/Public/Subsystems/EditorActorSubsystem.h", "Source/Editor/UnrealEd/Public/Subsystems/EditorAssetSubsystem.h", "Source/Runtime/Engine/Classes/GameFramework/Actor.h", "Source/Runtime/AssetRegistry/Private/AssetRegistry.cpp"]}
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem); actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(assets is not None and actors is not None, "Editor subsystems unavailable")
    return assets, actors, engine


def dirty():
    import unreal
    return [path(p) for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())]


def inspect_map(world, manifest, *, marker_required):
    import unreal
    world_path = path(world); package = world_path.rsplit(".",1)[0]
    require(world_path == object_path(package), "Unexpected World object identity")
    rows=[]
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor.static_class()):
        location=actor.get_actor_location(); rotation=actor.get_actor_rotation(); scale=actor.get_actor_scale3d()
        rows.append({"name":actor.get_name(),"class_path":path(actor.get_class()),"actor_path":path(actor),"label":actor.get_actor_label(False),"location":[float(location.x),float(location.y),float(location.z)],"rotation":[float(rotation.pitch),float(rotation.yaw),float(rotation.roll)],"scale":[float(scale.x),float(scale.y),float(scale.z)]})
    return {"world":world_path,"actors":sorted(rows,key=lambda r:r["name"]),"verified_actors":contract.actor_rows(rows,manifest,package,marker_required=marker_required),"deleted_camera_absent":True,"rendering_lighting_verified":False}


def marker_blueprints(manifest, assets):
    import unreal
    result={}
    for role,parent in (("actor","/Script/Engine.Actor"),("save_game","/Script/Engine.SaveGame")):
        row=next(r for r in manifest["files"] if r["role"]==role); value=assets.load_asset(row["package"])
        require(isinstance(value,unreal.Blueprint),"Marker Blueprint not loadable")
        actual_parent=path(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(value)); generated=path(value.generated_class())
        require(actual_parent==parent and generated==object_path(row["package"])+"_C","Marker Blueprint class/parent differs")
        result[role]={"package":row["package"],"generated_class":generated,"parent":actual_parent}
    return result


def preservation(manifest):
    rows=[{"package":r["package"],"source_unchanged":Path(r["source"]).is_file() and digest(r["source"])==r["sha256"],"copy_unchanged":Path(r["destination"]).is_file() and digest(r["destination"])==r["sha256"]} for r in manifest["files"]]
    return {"files":rows,"passed":all(r["source_unchanged"] and r["copy_unchanged"] for r in rows)}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument("--manifest",required=True,type=Path);parser.add_argument("--report",required=True,type=Path)
    args=parser.parse_args(argv);output=args.report.resolve();require(not output.exists(),"Never overwrite evidence")
    import unreal
    from validate_fixture_without_mcp import enabled_plugins,package_file,write_report
    manifest=read_json(args.manifest);report={"schema":"cubeless_actor_runtime_fixture_v1","success":False,"stage":"preflight","project_file":str(contract.PROJECT.resolve()),"process_id":os.getpid(),"started_ns":time.time_ns(),"manifest":{"path":str(args.manifest.resolve()),"sha256":digest(args.manifest)},"script_sha256":digest(__file__),"changes":[],"save":{"map_calls":0,"old_map_save_requested":False},"compile":{"requested":False},"automatic_retry":False,"atomic_rollback":False,"runtime_marker_created":False,"packaging":"not_run","full_actor_parity":False,"rendering_lighting_verified":False}
    output.parent.mkdir(parents=True,exist_ok=True)
    def stage(name):report["stage"]=name;write_report(output,report)
    write_report(output,report)
    try:
        assets,actors,engine=preflight(manifest,report)
        require(not assets.does_directory_exist(manifest["new_asset_root"]),"New runtime root already exists; inspect earlier attempt")
        stage("marker_blueprints_readback");report["marker_blueprints"]=marker_blueprints(manifest,assets)
        stage("load_copied_source_map")
        source=next(row for row in manifest["files"] if row["role"]=="actor_map")
        world=unreal.EditorLoadingAndSavingUtils.load_map(source["destination"])
        require(world is not None and path(world)==object_path(manifest["source_map"]),"Wrong source World")
        report["source_map_readback"]=inspect_map(world,manifest,marker_required=False)
        require(dirty()==[],"Source load made packages dirty; inspect before mutation")
        marker_class=unreal.load_class(None,manifest["marker_actor_class"]);require(marker_class is not None,"Marker class unavailable")
        stage("spawn_marker_actor")
        change={"kind":"spawn_marker_actor","class_path":manifest["marker_actor_class"],"attempted":True,"completed":False,"stage":"spawn_pending"};report["changes"].append(change);write_report(output,report)
        actor=actors.spawn_actor_from_class(marker_class,unreal.Vector(500,0,0),unreal.Rotator(0,0,0),False)
        change.update(returned_actor=path(actor),stage="spawn_returned");write_report(output,report);require(actor is not None,"Marker spawn failed")
        change["stage"]="label_pending";write_report(output,report);actor.set_actor_label(contract.MARKER_LABEL)
        change.update(label=actor.get_actor_label(False),completed=True,stage="complete");write_report(output,report)
        report["map_readback"]=inspect_map(world,manifest,marker_required=True)
        report["dirty_before_save"]=dirty();require(set(report["dirty_before_save"])=={manifest["source_map"]},"Unexpected package dirty before new-map save")
        stage("save_new_map_once");report["save"].update(map_calls=1,map_package=manifest["default_map"],map_saved=None)
        change={"kind":"save_map_copy","source":manifest["source_map"],"target":manifest["default_map"],"attempted":True,"completed":False};report["changes"].append(change);write_report(output,report)
        report["save"]["map_saved"]=bool(unreal.EditorLoadingAndSavingUtils.save_map(world,manifest["default_map"]));require(report["save"]["map_saved"],"Save new map failed")
        change["completed"]=True;write_report(output,report)
        report["map_readback"]=inspect_map(world,manifest,marker_required=True)
        file=package_file(manifest["default_map"],contract.PROJECT.parent,engine);report["new_map_file"]={"package":manifest["default_map"],"path":str(file),"sha256":digest(file)}
        report["dirty_after"]=dirty();report["plugins_after"]=enabled_plugins();require(report["plugins_after"]["passed"],"MCP/Toolset enabled")
        report["dependency_audit"]={"status":"pending_cold_process","reason":"Fresh saved-map registry graph will be evaluated independently after process restart"}
        report.update(success=True,stage="complete")
    except Exception as error:report.update(error=str(error),traceback=traceback.format_exc())
    finally:
        report["preservation"]=preservation(manifest)
        if not report["preservation"]["passed"]:report["success"]=False;report["preservation_error"]="Source or copied files changed"
        report["finished_ns"]=time.time_ns();write_report(output,report)
    return report["success"]


if __name__=="__main__" and not main():raise RuntimeError("Actor runtime author failed; inspect partial report, never retry automatically")
