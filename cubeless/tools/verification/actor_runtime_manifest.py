"""Plan, validate, or explicitly prepare a fresh no-MCP Actor runtime workspace.

No engine/process calls. The root operator owns --prepare; the default creates
only an immutable manifest. Existing assets, projects and reports are refused.
"""
import argparse
import configparser
import copy
import json
import math
from pathlib import Path
import re
import shutil
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parent))
from current_runtime_manifest import ROOT, RECORDS, SOURCE_PROJECT, checked_parent, digest, object_path, read_json, relative_asset, require
from actor_pipeline_report import verify as verify_actor

PROJECT = ROOT / "cubeless/workspaces/UE58ActorRuntimeProof/CubelessActorRuntimeProof.uproject"
FIELDS = ("name", "class_path", "actor_path", "label", "location", "rotation", "scale")
MARKER_LABEL = "CubelessActorRuntimeMarker"


def parent_contract(author_path, cold_path, marker_path):
    paths = [Path(p).resolve() for p in (author_path, cold_path, marker_path)]
    require(all(p.is_relative_to(RECORDS.resolve()) for p in paths), "Parent evidence outside verification records")
    author, cold, marker = map(read_json, paths)
    fixture_path = Path(author["fixture_report"]).resolve()
    require(fixture_path.is_relative_to(RECORDS.resolve()), "Actor fixture report escaped records")
    fixture_bytes = fixture_path.read_bytes(); fixture = json.loads(fixture_bytes)
    require(verify_actor(author, fixture=fixture, fixture_bytes=fixture_bytes)["passed"], "Actor author independent verification failed")
    require(cold.get("phase") == "reload" and verify_actor(cold, fixture=fixture, fixture_bytes=fixture_bytes, parent=author, parent_bytes=paths[0].read_bytes())["passed"], "Actor cold independent verification failed")
    checked_parent("marker", paths[2])
    return author, cold, marker, fixture, {key: {"path": str(path), "sha256": digest(path)} for key, path in zip(("actor_author", "actor_cold", "marker", "actor_fixture"), paths + [fixture_path])}


def build(author_path, cold_path, marker_path, run_id=None, *, fresh=True):
    run = run_id or uuid.uuid4().hex
    require(re.fullmatch(r"[a-f0-9]{32}", run), "Invalid runtime UUID")
    if fresh: require(not PROJECT.parent.exists(), "Actor runtime workspace already exists; never overwrite")
    author, cold, marker, fixture, parents = parent_contract(author_path, cold_path, marker_path)
    root = "/Game/CubelessMCPTests/ActorRuntime_" + run
    files = []
    for role, package, expected in [("actor_map", author["map_package"], author["asset_file"]["sha256"])] + [(role, marker["assets"][role], marker["asset_files"][role]["sha256"]) for role in ("actor", "save_game")]:
        relative = relative_asset(package, role == "actor_map")
        source = (SOURCE_PROJECT.parent / relative).resolve(); destination = (PROJECT.parent / relative).resolve()
        require(source.is_file() and digest(source) == expected, "Source package missing or changed: " + role)
        if fresh: require(not destination.exists(), "Target package exists")
        files.append({"role": role, "package": package, "relative_path": relative, "source": str(source), "destination": str(destination), "sha256": expected})
    owned = [{key: row[key] for key in FIELDS} for row in author["saved_actors"] if row["name"].startswith("Cubeless_")]
    require(len(owned) == 5, "Expected five retained owned actors")
    require(sorted(row["class_path"] for row in owned) == sorted("/Script/Engine." + c for c in ("StaticMeshActor", "PointLight", "PointLight", "SpotLight", "DirectionalLight")), "Saved native classes differ")
    return {"schema": "cubeless_actor_runtime_manifest_v1", "run_id": run, "project_file": str(PROJECT.resolve()), "source_project": str(SOURCE_PROJECT.resolve()), "parents": parents, "files": files,
            "source_map": author["map_package"], "default_map": root + "/Map_ActorRuntimeProof", "new_asset_root": root, "owned_actors": sorted(owned, key=lambda r:r["name"]),
            "protected_actor": {key: next(r for r in author["saved_actors"] if r["name"] == fixture["protected_actor"]["name"])[key] for key in FIELDS},
            "deleted_actor_name": "Cubeless_CameraActor_" + author["run_id"][:8], "marker_actor_class": object_path(marker["assets"]["actor"]) + "_C", "marker_label": MARKER_LABEL,
            "marker_contract": {key: marker[key] for key in ("run_id", "slot_name", "marker_class_path", "runtime_function_paths")},
            "required_cooked_packages": sorted([root + "/Map_ActorRuntimeProof", marker["assets"]["actor"], marker["assets"]["save_game"]]),
            "excluded_old_maps": [author["map_package"], marker["assets"]["map"]], "copied_plugins": False, "gameplay_cpp_added": False,
            "scope": "Five saved native actors across four distinct classes, deleted Camera remains absent; marker proves execution only, rendering/lighting and complete Actor parity excluded"}


def validate(manifest, *, copied=False):
    require(manifest.get("schema") == "cubeless_actor_runtime_manifest_v1", "Wrong manifest schema")
    parents = manifest["parents"]
    for row in parents.values(): require(digest(row["path"]) == row["sha256"], "Parent report bytes changed")
    expected = build(*(parents[k]["path"] for k in ("actor_author", "actor_cold", "marker")), manifest["run_id"], fresh=False)
    require(manifest == expected, "Manifest differs from independently rederived evidence")
    if copied:
        for row in manifest["files"]: require(Path(row["destination"]).is_file() and digest(row["destination"]) == row["sha256"], "Copied asset bytes differ")
    return True


def verify_config(manifest):
    descriptor = read_json(PROJECT)
    require(descriptor.get("DisableEnginePluginsByDefault") is True and not descriptor.get("Modules"), "Require default-disabled plugins and no gameplay modules")
    require({p["Name"] for p in descriptor.get("Plugins", []) if p.get("Enabled")} == {"PythonScriptPlugin", "EditorScriptingUtilities"}, "Only two editor support plugins permitted")
    require(all(not p.get("Enabled") or p.get("TargetAllowList") == ["Editor"] for p in descriptor["Plugins"]), "Support plugins must be Editor-only")
    plugins = PROJECT.parent / "Plugins"
    require(not plugins.exists() or not any(plugins.iterdir()), "Project plugin files forbidden")
    config = configparser.ConfigParser(interpolation=None, strict=False); config.optionxform = str
    config.read(PROJECT.parent / "Config/DefaultEngine.ini", encoding="utf-8-sig")
    maps = config["/Script/EngineSettings.GameMapsSettings"]
    require(maps.get("GameDefaultMap") == maps.get("EditorStartupMap") == manifest["default_map"], "Explicit default map differs")
    game = configparser.ConfigParser(interpolation=None, strict=False); game.optionxform = str
    game.read(PROJECT.parent / "Config/DefaultGame.ini", encoding="utf-8-sig"); package = game["/Script/UnrealEd.ProjectPackagingSettings"]
    require(package.get("bUsePakFile") == "True" and package.get("bUseIoStore") == package.get("bUseZenStore") == "False", "Pak-only proof settings differ")
    require(package.get("+MapsToCook") == '(FilePath="' + manifest["default_map"] + '")', "Cook map differs")
    return {"project_sha256": digest(PROJECT), "default_engine_sha256": digest(PROJECT.parent / "Config/DefaultEngine.ini"), "default_game_sha256": digest(PROJECT.parent / "Config/DefaultGame.ini")}


def actor_rows(rows, manifest, package, *, marker_required):
    require(package in (manifest["source_map"], manifest["default_map"]), "Unowned map")
    require(len({row["name"] for row in rows}) == len(rows), "Duplicate actor names")
    lookup = {row["name"]: row for row in rows}
    require(manifest["deleted_actor_name"] not in lookup, "Deleted Camera reappeared")
    require({name for name in lookup if name.startswith("Cubeless_")} == {r["name"] for r in manifest["owned_actors"]}, "Owned actor set differs")
    observed = []
    for expected in manifest["owned_actors"] + [manifest["protected_actor"]]:
        row = lookup.get(expected["name"]); require(row is not None, "Missing actor: " + expected["name"])
        for field in ("name", "class_path", "label"): require(row[field] == expected[field], "Actor identity/label differs: " + field)
        require(row["actor_path"] == object_path(package) + ":PersistentLevel." + expected["name"], "Actor left exact persistent level")
        for field in ("location", "rotation", "scale"):
            require(isinstance(row[field], list) and len(row[field]) == 3 and all(type(a) in (int,float) and math.isfinite(a) and abs(a-b) < .01 for a,b in zip(row[field], expected[field])), "Actor transform differs: " + field)
        observed.append({k: copy.deepcopy(row[k]) for k in FIELDS})
    markers = [row for row in rows if row["class_path"] == manifest["marker_actor_class"] or row["label"] == MARKER_LABEL]
    require(len(markers) == int(marker_required), "Missing/duplicate/unexpected marker actor")
    if marker_required:
        marker = markers[0]
        require(marker["class_path"] == manifest["marker_actor_class"] and marker["label"] == MARKER_LABEL, "Marker identity differs")
        require(marker["actor_path"] == object_path(package) + ":PersistentLevel." + marker["name"], "Marker path escaped map")
        for field, expected in (("location", [500,0,0]), ("rotation", [0,0,0]), ("scale", [1,1,1])):
            require(isinstance(marker[field], list) and len(marker[field]) == 3 and all(type(a) in (int,float) and math.isfinite(a) and abs(a-b) < .01 for a,b in zip(marker[field],expected)), "Marker spawn transform differs")
        observed.append({k: copy.deepcopy(marker[k]) for k in FIELDS})
    return sorted(observed, key=lambda r:r["name"])


def remap_rows(rows, old_package, new_package):
    result = copy.deepcopy(rows)
    for row in result:
        require(row["actor_path"] == object_path(old_package) + ":PersistentLevel." + row["name"], "Unexpected actor map/object identity")
        row["actor_path"] = object_path(new_package) + ":PersistentLevel." + row["name"]
    return result


def prepare(manifest, output):
    validate(manifest); output = Path(output).resolve(); require(not output.exists(), "Preparation evidence exists")
    require(not PROJECT.parent.exists(), "Never overwrite an existing runtime workspace")
    report = {"schema": "cubeless_actor_runtime_preparation_v1", "success": False, "project_file": str(PROJECT.resolve()), "copied_files": [], "plugins_copied": False, "gameplay_cpp_added": False}
    try:
        PROJECT.parent.mkdir(parents=True); (PROJECT.parent / "Config").mkdir()
        PROJECT.write_text(json.dumps({"FileVersion":3,"EngineAssociation":"5.8","DisableEnginePluginsByDefault":True,"Plugins":[{"Name":name,"Enabled":True,"TargetAllowList":["Editor"]} for name in ("PythonScriptPlugin","EditorScriptingUtilities")]},indent=2)+"\n",encoding="utf-8")
        (PROJECT.parent/"Config/DefaultEngine.ini").write_text("[/Script/EngineSettings.GameMapsSettings]\nGameDefaultMap="+manifest["default_map"]+"\nEditorStartupMap="+manifest["default_map"]+"\n",encoding="utf-8")
        (PROJECT.parent/"Config/DefaultGame.ini").write_text('[/Script/UnrealEd.ProjectPackagingSettings]\nbUsePakFile=True\nbUseIoStore=False\nbUseZenStore=False\nbCookAll=False\n+MapsToCook=(FilePath="'+manifest["default_map"]+'")\n',encoding="utf-8")
        for row in manifest["files"]:
            target=Path(row["destination"]); require(not target.exists(), "Target file exists"); target.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(row["source"],target)
            report["copied_files"].append({**row,"copied_sha256":digest(target)}); require(digest(target)==row["sha256"],"Copy hash differs")
        validate(manifest,copied=True); report["configuration"]=verify_config(manifest); report["success"]=True
    except Exception as error: report["error"]=str(error)
    with output.open("x",encoding="utf-8") as stream: json.dump(report,stream,indent=2)
    return report["success"]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--actor-author",type=Path); parser.add_argument("--actor-cold",type=Path); parser.add_argument("--marker",type=Path)
    parser.add_argument("--output",required=True,type=Path); parser.add_argument("--prepare",type=Path,help="Existing manifest; root operator only")
    args=parser.parse_args()
    if args.prepare: return 0 if prepare(read_json(args.prepare),args.output) else 1
    require(all((args.actor_author,args.actor_cold,args.marker)),"All parent paths required")
    value=build(args.actor_author,args.actor_cold,args.marker)
    with args.output.open("x",encoding="utf-8") as stream: json.dump(value,stream,indent=2)
    print(json.dumps({"manifest":str(args.output),"default_map":value["default_map"],"files":len(value["files"])})); return 0


if __name__ == "__main__": raise SystemExit(main())
