"""Fresh-process no-MCP Actor map readback and bounded disk dependency audit.

Never saves, launches, cooks, or creates a runtime marker. Compiles only the two
existing marker Blueprints in memory with the public SkipSave wrapper.
"""
import argparse
import os
from pathlib import Path
import sys
import time
import traceback

sys.path.insert(0,str(Path(__file__).resolve().parent))
import actor_runtime_manifest as contract
import create_actor_runtime_fixture as authoring
from actor_runtime_manifest import digest,object_path,read_json,require


def check_dependencies(manifest,audit):
    require(audit.get("complete") is True and audit.get("passed_within_declared_scope") is True and audit.get("forbidden_references")==[],"Dependency graph incomplete/forbidden")
    packages={row["package"] for row in audit["packages"]}; require(set(manifest["required_cooked_packages"])<=packages,"Required runtime packages not reachable")
    require(not(set(manifest["excluded_old_maps"])&packages),"Old map remains a runtime dependency")
    hard={(row["from"],row["to"]) for row in audit["edges"] if row["category"]=="hard_package"}
    actor=next(r["package"] for r in manifest["files"] if r["role"]=="actor");save_game=next(r["package"] for r in manifest["files"] if r["role"]=="save_game")
    require((manifest["default_map"],actor) in hard and (actor,save_game) in hard,"Marker package hard-edge chain missing")
    # AssetRegistry does not promise that every loaded native class's /Script
    # package appears as an outgoing package dependency. Concrete Engine Actor
    # classes and Blueprint parents are checked by independent public readback.
    return {"passed":True,"required_packages":manifest["required_cooked_packages"],"observed_native_script_terminals":audit["native_script_terminals"],"scope":"Bounded disk package references including editor-only, current management refs. Native terminal enumeration is observation, not exhaustive class/module dependency proof; concrete classes/parents come from separate public readbacks. Pak/Shipping remains separate."}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--manifest",required=True,type=Path);parser.add_argument("--fixture-report",required=True,type=Path);parser.add_argument("--report",required=True,type=Path)
    args=parser.parse_args(argv);output=args.report.resolve();require(not output.exists(),"Never overwrite cold evidence")
    import unreal
    from validate_fixture_without_mcp import dependency_audit,enabled_plugins,package_file,write_report
    manifest=read_json(args.manifest);fixture=read_json(args.fixture_report)
    report={"schema":"cubeless_actor_runtime_cold_v1","success":False,"stage":"preflight","project_file":str(contract.PROJECT.resolve()),"process_id":os.getpid(),"started_ns":time.time_ns(),"manifest":{"path":str(args.manifest.resolve()),"sha256":digest(args.manifest)},"fixture_report":{"path":str(args.fixture_report.resolve()),"sha256":digest(args.fixture_report)},"script_sha256":digest(__file__),"save":{"requested":False,"attempted":False},"compile":[],"automatic_retry":False,"atomic_rollback":False,"runtime_marker_created":False,"packaging":"not_run","full_actor_parity":False,"rendering_lighting_verified":False}
    output.parent.mkdir(parents=True,exist_ok=True)
    def stage(name):report["stage"]=name;write_report(output,report)
    write_report(output,report);new_file=None
    try:
        require(fixture.get("schema")=="cubeless_actor_runtime_fixture_v1" and fixture.get("success") is True and fixture.get("stage")=="complete" and fixture.get("preservation",{}).get("passed") is True,"Author failed/preservation unproven")
        require(fixture["manifest"]==report["manifest"] and fixture["project_file"]==report["project_file"] and fixture["process_id"]!=os.getpid(),"Wrong parent or same process")
        require(fixture["save"]=={"map_calls":1,"old_map_save_requested":False,"map_package":manifest["default_map"],"map_saved":True} and fixture["compile"]=={"requested":False},"Author save/compile scope differs")
        assets,actors,engine=authoring.preflight(manifest,report);require(report["configuration"]==fixture["configuration"],"Project config changed")
        new_file=fixture["new_map_file"]
        require(new_file["package"]==manifest["default_map"] and package_file(new_file["package"],contract.PROJECT.parent,engine)==Path(new_file["path"]).resolve() and digest(new_file["path"])==new_file["sha256"],"Saved map identity/hash differs")
        report["marker_blueprints"]=authoring.marker_blueprints(manifest,assets);require(report["marker_blueprints"]==fixture["marker_blueprints"],"Marker class changed")
        stage("load_new_map");world=unreal.EditorLoadingAndSavingUtils.load_map(new_file["path"]);require(world is not None and authoring.path(world)==object_path(manifest["default_map"]),"Wrong cold map")
        report["map_readback"]=authoring.inspect_map(world,manifest,marker_required=True)
        old=fixture["map_readback"]["world"].rsplit(".",1)[0];require(old in (manifest["source_map"],manifest["default_map"]),"Author world outside exact source/new map")
        report["expected_saved_actors"]=contract.remap_rows(fixture["map_readback"]["verified_actors"],old,manifest["default_map"])
        require(report["map_readback"]["verified_actors"]==report["expected_saved_actors"],"Actor identity/transform/marker changed after cold load")
        report["dirty_after_readback"]=authoring.dirty();require(report["dirty_after_readback"]==[],"Cold readback dirtied packages")
        stage("dependency_audit");dependency_audit([manifest["default_map"]],unreal.AssetRegistryHelpers.get_asset_registry(),contract.PROJECT.parent,engine,report)
        report["required_dependencies"]=check_dependencies(manifest,report["dependency_audit"])
        for role in ("actor","save_game"):
            package=report["marker_blueprints"][role]["package"];blueprint=assets.load_asset(package)
            stage("compile_without_save_"+role);row={"package":package,"requested":True,"save_requested":False};report["compile"].append(row);write_report(output,report)
            returned=bool(unreal.BlueprintEditorLibrary.compile_blueprint(blueprint));status=blueprint.get_editor_property("status");errors=[]
            for graph in unreal.BlueprintEditorLibrary.list_graphs(blueprint):
                errors.extend({"node":node.get_path_name(),"error":str(node.get_editor_property("error_msg"))} for node in unreal.BlueprintGraphEditor.get_graph_editor(graph).list_nodes_with_errors())
            row.update(returned=returned,status=str(status),errors=errors)
            require(returned and not errors and status in (unreal.BlueprintStatus.BS_UP_TO_DATE,unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS),"No-MCP marker compile failed")
        report["dirty_after_compile"]=authoring.dirty();report["plugins_after"]=enabled_plugins();require(report["plugins_after"]["passed"],"MCP/Toolset enabled")
        report.update(success=True,stage="complete")
    except Exception as error:report.update(error=str(error),traceback=traceback.format_exc())
    finally:
        report["preservation"]=authoring.preservation(manifest);report["new_map_unchanged"]=bool(new_file) and Path(new_file["path"]).is_file() and digest(new_file["path"])==new_file["sha256"]
        if not report["preservation"]["passed"] or not report["new_map_unchanged"]:report["success"]=False;report["preservation_error"]="Source/copy/new map bytes changed"
        report["finished_ns"]=time.time_ns();write_report(output,report)
    return report["success"]


if __name__=="__main__" and not main():raise RuntimeError("Actor runtime cold verification failed; inspect report")
