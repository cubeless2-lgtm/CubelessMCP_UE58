"""Prepare an immutable copy plan, never copy assets or launch Unreal.

Inputs must be successful independent fixture evidence. Root owns project
creation, file copies, configuration, editor launch, cooking and game execution.
"""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import re
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verification.material_add_set_edges_report import decoded_raw

ROOT = Path(__file__).resolve().parents[3]
SOURCE_PROJECT = ROOT / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
PROJECT = ROOT / "cubeless/workspaces/UE58CurrentRuntimeProof/CubelessCurrentRuntimeProof.uproject"
RECORDS = ROOT / "cubeless/records/verification/implementation-2026-10-10"


def require(condition, message):
    if not condition: raise RuntimeError(message)


def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read_json(path): return json.loads(Path(path).read_text(encoding="utf-8-sig"))
def object_path(package): return package + "." + package.rsplit("/", 1)[1]


def relative_asset(package, is_map=False):
    require(re.fullmatch(r"/Game/CubelessMCPTests/[A-Za-z0-9_/]+", package) is not None, "Package outside owned test mount")
    return "Content/" + package[len("/Game/"):] + (".umap" if is_map else ".uasset")


def checked_parent(role, path):
    path = Path(path).resolve(); require(path.is_relative_to(RECORDS.resolve()), "Parent evidence outside verification records")
    value = read_json(path)
    if role.startswith("material_"):
        from verification.material_pipeline_report import verify
        require(verify(value, require_phase="author")["passed"] and value.get("schema") == "cubeless_material_pipeline_v2", "Material v2 author contract failed")
    elif role == "edges":
        from verification.material_add_set_edges_report import verify
        require(verify(value, phase="author")["passed"], "Edges author contract failed")
    elif role == "mpc":
        from verification.mpc_inheritance_report import verify
        require(value.get("phase") == "author" and verify(value)["status"] == "pass", "MPC author contract failed")
    elif role == "batch":
        from verification.batch_pipeline_report import verify
        require(verify(value, require_phase="author")["passed"], "Batch author contract failed")
    elif role == "custom":
        from verification.custom_material_report import verify
        require(value.get("phase") == "author" and verify(value)["passed"], "Custom author contract failed")
    elif role == "compile":
        from verification.batch_pipeline_report import saved
        require(value.get("status") == "pass" and value.get("automatic_retries") == 0 and value.get("outcome_unknown") is False and value.get("save") is True, "Compile wrapper evidence failed")
        stages = value["stages"]
        require([s["name"] for s in stages] == ["create", "compile_blueprint", "compile_and_validate_blueprint", "compile_and_save_blueprint", "inspect_saved"], "Compile wrapper stages differ")
        require(all(decoded_raw(s["raw"]) == s["result"] and s["result"].get("success") is True for s in stages), "Compile wrapper raw/result mismatch")
        saved(stages[3]["result"])
        inspection = value["saved_inspection"]
        require(inspection == stages[-1]["result"] and inspection["data"]["path"] == value["package"] and inspection["data"]["dirty"] is False and inspection["data"]["parent_class"] == "/Script/Engine.Actor", "Compile wrapper saved asset mismatch")
    elif role == "marker":
        require(value.get("schema") == "cubeless_runtime_fixture_v1" and value.get("success") is True and value.get("stage") == "complete", "Marker author did not finish")
        require(value["validation"]["runtime_marker_created_by_this_script"] is False and value["slot_name"] == "Cubeless_" + value["run_id"], "Marker scope/slot differs")
        require(value["marker_class_path"] == object_path(value["assets"]["save_game"]) + "_C" and value["pipeline_asset_sha256_before"] == value["pipeline_asset_sha256_after"], "Marker class or preserved pipeline mismatch")
        require(all(value["save"][key]["saved"] is True and value["compile"][key]["compiled"] is True and value["compile"][key]["errors"] == [] for key in ("save_game", "actor")), "Marker blueprint compile/save failed")
    else: raise RuntimeError("Unknown parent role: " + role)
    project = value.get("project_file") or value.get("preflight", {}).get("project_file")
    require(Path(project).resolve() == SOURCE_PROJECT.resolve(), "Parent belongs to another source project")
    return value, {"role": role, "path": str(path), "sha256": digest(path)}


def build_manifest(parent_paths, run_id=None, *, fresh_destination=True):
    run = run_id or uuid.uuid4().hex
    require(re.fullmatch(r"[0-9a-f]{32}", run) is not None, "Invalid manifest run UUID")
    if fresh_destination: require(not PROJECT.exists() and not (PROJECT.parent / "Content").exists(), "Copy plan requires a fresh destination workspace")
    required = {"marker", "edges", "mpc", "batch", "compile", "custom"}
    require(required <= set(parent_paths) and {k for k in parent_paths if k.startswith("material_")}, "Missing required evidence roles")
    parents = {}; rows = {}; files = {}; materials = []; blueprints = []; clones = []
    root = "/Game/CubelessMCPTests/CurrentRuntime_" + run
    def add(role, kind, package, expected_hash=None, asset_type="asset"):
        relative = relative_asset(package, asset_type == "map")
        source = (SOURCE_PROJECT.parent / relative).resolve(); destination = (PROJECT.parent / relative).resolve()
        require(source.is_file() and (not fresh_destination or not destination.exists()), "Source missing or target exists: " + package)
        sha = digest(source); require(expected_hash is None or sha == expected_hash, "Author/disk hash mismatch: " + package)
        row = {"package": package, "relative_path": relative, "source": str(source), "destination": str(destination), "sha256": sha, "roles": [role + ":" + kind], "asset_type": asset_type}
        if package in files:
            require(files[package]["sha256"] == sha, "Conflicting duplicate source")
            files[package]["roles"].extend(row["roles"])
        else: files[package] = row
        if asset_type == "material": materials.append({"label": "CubelessCurrent_" + role, "package": package, "source_role": role, "kind": "original"})
        if asset_type == "blueprint_actor": blueprints.append({"label": "CubelessCurrent_" + role + "_" + kind, "package": package, "class_path": object_path(package) + "_C"})
    for role, path in parent_paths.items():
        parent, rows[role] = checked_parent(role, path); parents[role] = parent
        if role == "marker":
            for kind, package in parent["assets"].items(): add(role, kind, package, parent["asset_files"][kind]["sha256"], "map" if kind == "map" else "asset")
            add(role, "pipeline", parent["pipeline_blueprint"], parent["pipeline_asset_sha256_after"])
        elif role.startswith("material_"):
            for kind, package in parent["assets"].items(): add(role, kind, package, asset_type="material" if kind == "material" else "asset")
            clones.append({"role": role, "source": parent["assets"]["material"], "target": root + "/M_" + role + "_MPC", "collection": parent["assets"]["collection"], "connection": "collection_to_add_B", "source_node_name": parent["fixture"]["nodes"]["collection"].rsplit(":", 1)[1], "target_node_name": parent["fixture"]["nodes"]["add"].rsplit(":", 1)[1]})
        elif role in {"edges", "custom"}:
            expected = parent.get("asset_file", {}).get("sha256")
            add(role, "material", parent["material_package"], expected, "material")
        elif role == "mpc":
            for kind, package in parent["packages"].items(): add(role, kind, package, parent["asset_files"][kind]["sha256"], "material" if kind == "material" else "asset")
            clones.append({"role": role, "source": parent["packages"]["material"], "target": root + "/M_Inherited_MPC", "collection": parent["packages"]["leaf"], "connection": "root_vector_to_base_color", "source_node_name": parent["nodes"]["RootVector"].rsplit(":", 1)[1]})
        elif role == "batch":
            for kind in ("batch", "standalone", "stop_true", "stop_false"): add(role, kind, parent["assets"][kind], asset_type="blueprint_actor")
            require(not (SOURCE_PROJECT.parent / relative_asset(parent["assets"]["unsupported"])).exists(), "Unsupported-prefix fixture unexpectedly exists")
        elif role == "compile": add(role, "compile", parent["package"], asset_type="blueprint_actor")
    for clone in clones: materials.append({"label": "CubelessCurrent_" + clone["role"] + "_connected_MPC", "package": clone["target"], "source_role": clone["role"], "kind": "official_runtime_clone"})
    marker = parents["marker"]
    return {"schema": "cubeless_current_runtime_manifest_v1", "run_id": run, "project_file": str(PROJECT.resolve()), "source_project": str(SOURCE_PROJECT.resolve()), "parents": rows, "files": list(files.values()), "source_map": marker["assets"]["map"], "default_map": root + "/Map_CurrentRuntimeProof", "new_asset_root": root, "clone_specs": clones, "material_actors": materials, "blueprint_actors": blueprints, "preserved_marker_actors": marker["spawned_actors"], "marker_contract": {k: marker[k] for k in ("run_id", "slot_name", "marker_class_path", "runtime_function_paths")}, "marker_fixture_report": rows["marker"], "required_cooked_packages": sorted({p for p in files if p != marker["assets"]["map"]} | {c["target"] for c in clones} | {root + "/Map_CurrentRuntimeProof"}), "excluded_old_map": marker["assets"]["map"], "copied_plugins": False, "gameplay_cpp_added": False, "asset_copy_performed": False, "engine_execution_performed": False, "scope": "Saved fixtures only; no claims about all legacy features or shader/render completion"}


def validate_manifest(manifest, copied):
    require(manifest.get("schema") == "cubeless_current_runtime_manifest_v1" and Path(manifest["project_file"]).resolve() == PROJECT.resolve() and Path(manifest["source_project"]).resolve() == SOURCE_PROJECT.resolve(), "Wrong manifest/project")
    require(manifest.get("copied_plugins") is False and manifest.get("gameplay_cpp_added") is False, "Plugin/gameplay dependency introduced")
    require(re.fullmatch(r"[0-9a-f]{32}", manifest["run_id"]), "Invalid manifest run")
    root = "/Game/CubelessMCPTests/CurrentRuntime_" + manifest["run_id"]
    require(manifest["new_asset_root"] == root and manifest["default_map"] == root + "/Map_CurrentRuntimeProof", "New package ownership differs")
    for role, parent in manifest["parents"].items():
        require(digest(parent["path"]) == parent["sha256"], "Parent bytes changed")
        checked_parent(role, parent["path"])
    require(len({f["package"] for f in manifest["files"]}) == len(manifest["files"]), "Duplicate manifest packages")
    for row in manifest["files"]:
        relative = relative_asset(row["package"], row["asset_type"] == "map")
        require(row["relative_path"] == relative and Path(row["source"]).resolve() == (SOURCE_PROJECT.parent / relative).resolve() and Path(row["destination"]).resolve() == (PROJECT.parent / relative).resolve(), "Source/copy path escaped expected roots")
        require(digest(row["source"]) == row["sha256"], "Original source changed")
        if copied: require(digest(row["destination"]) == row["sha256"], "Copy hash differs")
    for clone in manifest["clone_specs"]:
        require(clone["target"].startswith(root + "/M_") and clone["connection"] in {"collection_to_add_B", "root_vector_to_base_color"}, "Unknown clone operation")
    expected = build_manifest({role: row["path"] for role, row in manifest["parents"].items()}, manifest["run_id"], fresh_destination=False)
    require(expected == manifest, "Manifest contract does not match independently rederived parent evidence")
    return True


def verify_project_config(manifest):
    descriptor = read_json(PROJECT)
    require(descriptor.get("DisableEnginePluginsByDefault") is True and not descriptor.get("Modules"), "Require default-disabled plugins and no gameplay modules")
    for plugin in descriptor.get("Plugins", []):
        if plugin.get("Enabled"): require(plugin["Name"] in {"PythonScriptPlugin", "EditorScriptingUtilities"} and plugin.get("TargetAllowList") == ["Editor"], "Only editor Python plugins permitted")
    plugins = PROJECT.parent / "Plugins"
    require(not plugins.exists() or not any(plugins.iterdir()), "No plugin files may be copied")
    config = configparser.ConfigParser(interpolation=None, strict=False); config.optionxform = str
    config.read(PROJECT.parent / "Config/DefaultEngine.ini", encoding="utf-8-sig")
    section = config["/Script/EngineSettings.GameMapsSettings"]
    require(section.get("GameDefaultMap") == manifest["default_map"] and section.get("EditorStartupMap") == manifest["default_map"], "Default maps must explicitly identify the new runtime map")
    return {"project_sha256": digest(PROJECT), "default_engine_sha256": digest(PROJECT.parent / "Config/DefaultEngine.ini")}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for role in ("marker", "edges", "mpc", "batch", "compile", "custom"): parser.add_argument("--" + role + "-report", required=True, type=Path)
    parser.add_argument("--material-report", action="append", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path); parser.add_argument("--run-id")
    args = parser.parse_args(argv); require(not args.output.exists(), "Refusing to overwrite manifest")
    paths = {role: getattr(args, role + "_report") for role in ("marker", "edges", "mpc", "batch", "compile", "custom")}
    paths.update({"material_" + str(index): path for index, path in enumerate(args.material_report)})
    manifest = build_manifest(paths, args.run_id); validate_manifest(manifest, copied=False)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as f: json.dump(manifest, f, indent=2)
    print(json.dumps({"output": str(args.output.resolve()), "files": len(manifest["files"]), "default_map": manifest["default_map"], "clone_count": len(manifest["clone_specs"])}))
    return 0


if __name__ == "__main__": raise SystemExit(main())
