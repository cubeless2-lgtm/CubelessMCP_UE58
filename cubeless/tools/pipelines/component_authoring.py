"""Port the legacy TestBP/CubeVisual asset-authoring flow to official UE58 MCP.

Source: Python/scripts/blueprints/test_create_and_spawn_cube_blueprint.py.
Preserves create -> add StaticMeshComponent -> assign Engine Cube -> compile,
and stops on the first failure. An optional root-prepared map enables the source's
final Blueprint spawn. Explicit save/readback and a separate cold-read phase are
added. This client starts no server/editor and never retries a mutation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from verification.actor_pipeline_report import fixture_contract as actor_fixture_contract, actor_map, mutation as actor_mutation

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
RECORDS = ROOT / "cubeless/records/verification"
SETTINGS = "/Script/BlueprintGraph.Default__BlueprintEditorSettings"
MESH = "/Engine/BasicShapes/Cube.Cube"
LEGACY = {
    "id": "script:Python/scripts/blueprints/test_create_and_spawn_cube_blueprint.py",
    "source_path": "Python/scripts/blueprints/test_create_and_spawn_cube_blueprint.py",
    "source_sha256": "aeac8725e0e4b7a15bf18da44185104c7649bf6048a6e124aa0780dac0f055b4",
    "preserved": ["Actor Blueprint", "CubeVisual StaticMeshComponent", "identity relative transform", MESH,
                  "create/add/set/compile ordering", "stop on first failed command"],
    "adaptations": ["official MCP client replaces TCP", "fresh UUID-owned package replaces reusable TestBP",
                    "P uses the restored public set_static_mesh_properties mesh contract; O used a generic typed setter",
                    "explicit DefaultSceneRoot attachment preserves the fresh root; legacy omitted parent used SCS.AddNode",
                    "defer add compile until one final explicit compile/save", "structured failure report and actual readback",
                    "explicit disk save and optional new-process reload were absent from the source"],
    "conditional_stage": "final spawn_blueprint_actor requires --actor-fixture-report; otherwise pending",
    "incomplete": ["reuse-existing-Blueprint behavior and broader static-mesh-property tool parity"],
}


def require(value, message):
    if not value:
        raise AssertionError(message)


def fingerprint(path):
    if not path.exists():
        return None
    stat = path.stat()
    return {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "bytes": stat.st_size, "mtime_ns": stat.st_mtime_ns}


def success(value):
    require(isinstance(value, dict) and value.get("success") is True, "Operation failed: " + str(value))
    require(value.get("error_code", "") == "" and value.get("error", "") == "", "Success contradicts error fields")


def component_success(value, mutation=False):
    success(value)
    native = value["data"]["native_outcome"]
    require(native.get("change_state_known") is True and native.get("postcondition_passed") is True,
            "Native component postcondition or change state is uncertain")
    require(value["save"].get("saved") is False and value["save"].get("requested") is False,
            "Component operation unexpectedly requested/reported saving")
    if not mutation:
        require(native.get("changed") is False, "Read changed its observed state")
    require(value["compile"].get("requested") is False, "Unexpected intermediate compile")


def close_vector(actual, expected):
    return (type(actual) is list and len(actual) == 3 and all(type(a) in (int, float)
            and math.isfinite(a) and math.isclose(a, e, abs_tol=0.0001) for a, e in zip(actual, expected)))


def field(value, name):
    matches = [v for k, v in value.items() if canonical(k) == canonical(name)]
    require(len(matches) == 1, "Missing/ambiguous property: " + name)
    return matches[0]


def listing(value, package):
    component_success(value)
    rows = value["data"]["components"]
    require(len(rows) == 2 and {r["component_name"] for r in rows} == {"DefaultSceneRoot", "CubeVisual"},
            "Expected exactly the preserved root and CubeVisual")
    expected = {"DefaultSceneRoot": ("SceneComponent", ""), "CubeVisual": ("StaticMeshComponent", "DefaultSceneRoot")}
    view = []
    for row in rows:
        name = row["component_name"]; kind, parent = expected[name]
        require(row["component_class_path"] == "/Script/Engine." + kind and row["parent_component_name"] == parent,
                "Component class/parent differs: " + name)
        require(row["template_path"].startswith(package + ".") and ":" in row["template_path"], "Template escaped owned asset")
        transform = row["relative_transform"]
        require(close_vector(transform["location"], [0, 0, 0]) and close_vector(transform["rotation"], [0, 0, 0])
                and close_vector(transform["scale"], [1, 1, 1]), "Legacy relative transform differs")
        require(isinstance(row["node_guid"], str) and row["node_guid"], "Missing persistent node identity")
        if name == "CubeVisual":
            require(row.get("static_mesh") == MESH, "Component list mesh differs")
        view.append({k: row[k] for k in ("component_name", "component_class_path", "parent_component_name", "node_guid", "relative_transform")})
    return sorted(view, key=lambda r: r["component_name"]), next(r for r in rows if r["component_name"] == "CubeVisual")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--expected-project", type=Path, required=True)
    parser.add_argument("--expected-pid", type=int, required=True)
    parser.add_argument("--phase", choices=["author", "reload"], required=True)
    parser.add_argument("--parent-report", type=Path)
    parser.add_argument("--actor-fixture-report", type=Path, help="Optional fresh root-prepared Actor map fixture for the final Blueprint spawn; reload reuses the author fixture")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args(argv)
    require(args.expected_project.resolve() == PROJECT.resolve(), "Only the exact isolated UE58Verification project is allowed")
    output = args.output.resolve()
    require(output.is_relative_to(RECORDS.resolve()) and not output.exists(), "Use a fresh verification report under cubeless/records/verification")
    require((args.phase == "reload") == bool(args.parent_report), "Only reload accepts/requires an author report")
    parent_bytes = args.parent_report.read_bytes() if args.parent_report else None
    parent = json.loads(parent_bytes) if parent_bytes else None
    if parent:
        require(parent.get("schema") == "cubeless_component_authoring_v1" and parent.get("phase") == "author"
                and parent.get("status") == "pass" and parent.get("outcome_unknown") is False
                and Path(parent["project_file"]).resolve() == PROJECT.resolve(), "Parent is not a passing owned author result")
        require(parent["save"].get("saved") is True and parent["compile"].get("compiled") is True, "Parent compile/save incomplete")
    fixture_path = Path(parent["actor_fixture_report"]) if parent and parent.get("actor_fixture_report") else args.actor_fixture_report
    if parent and args.actor_fixture_report:
        require(fixture_path.resolve() == args.actor_fixture_report.resolve(), "Reload fixture differs from author")
    fixture_bytes = fixture_path.read_bytes() if fixture_path else None
    fixture = json.loads(fixture_bytes) if fixture_bytes else None
    level, map_disk = actor_fixture_contract(fixture) if fixture else (None, None)
    if fixture:
        require(map_disk.is_file(), "Prepared map is missing")
        if parent:
            require(hashlib.sha256(fixture_bytes).hexdigest() == parent["actor_fixture_sha256"]
                    and fingerprint(map_disk) == parent["map_file"], "Author map fixture/bytes differ")
        else:
            require(fingerprint(map_disk)["sha256"] == fixture["asset_file"]["sha256"], "Prepared map changed before authoring")
    run = parent["run_id"] if parent else uuid.uuid4().hex
    require(re.fullmatch(r"[a-f0-9]{32}", run), "Invalid ownership ID")
    folder = "/Game/CubelessMCPTests/ComponentAuthoring_" + run
    package = folder + "/BP_CubeComponent"
    actor_name = "TestBPInstance_" + run[:12]
    disk = PROJECT.parent / "Content" / (package[6:] + ".uasset")
    if parent:
        require(parent["package"] == package and fingerprint(disk) == parent["asset_file"], "Parent asset path or saved bytes differ")
    else:
        require(not disk.parent.exists(), "New UUID folder already exists; inspect rather than retry")
    config = PROJECT.parent / "Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini"
    report = {"schema": "cubeless_component_authoring_v1", "status": "running", "phase": args.phase,
        "run_id": run, "package": package, "project_file": str(PROJECT.resolve()), "endpoint": args.endpoint,
        "legacy": parent["legacy"] if parent else LEGACY, "full_legacy_pipeline_complete": False, "stages": [], "changes": [],
        "compile": {"status": "not_requested"}, "save": {"status": "not_requested", "saved": False},
        "reload": "not_run", "spawn_blueprint_actor": "pending", "packaging": "not_run",
        "automatic_retries": 0, "atomic_rollback": False, "outcome_unknown": False,
        "config_before": fingerprint(config), "asset_before": fingerprint(disk), "started_ns": time.time_ns(),
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    if parent:
        report.update(parent_report=str(args.parent_report.resolve()), parent_sha256=hashlib.sha256(parent_bytes).hexdigest())
    if fixture:
        report.update(actor_fixture_report=str(fixture_path.resolve()), actor_fixture_sha256=hashlib.sha256(fixture_bytes).hexdigest(),
                      map_package=level, map_before=fingerprint(map_disk), actor_name=actor_name, map_save={"status": "not_requested"})
    schemas = {}; client = OfficialMCPClient(args.endpoint, args.timeout)

    def persist():
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = output.with_suffix(output.suffix + ".partial")
        temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        temporary.replace(output)

    def invoke(label, suffix, tool, arguments=None, mutation=False, check=None):
        report["stage"] = label
        descriptor = schemas[suffix]
        choices = [t for t in descriptor["schema"]["tools"] if t["name"].rsplit(".", 1)[-1] == tool]
        require(len(choices) == 1, "Missing unique registered tool: " + tool)
        inputs = choices[0].get("inputSchema", {}); mapped = {}
        for key, value in (arguments or {}).items():
            keys = [k for k in inputs.get("properties", {}) if canonical(k) == canonical(key)]
            require(len(keys) == 1, "Missing/ambiguous registered input: " + key)
            mapped[keys[0]] = value
        require(all(k in mapped for k in inputs.get("required", [])), "Unexpected required input; no fabricated default")
        row = {"name": label, "toolset": descriptor["name"], "tool": tool, "arguments": mapped,
               "mutation": mutation, "state": "submitted", "started_ns": time.time_ns()}
        if tool == "compile_and_save_blueprint":
            report["compile"] = {"requested": True, "attempted": None, "status": "submitted"}
            report["save"] = {"requested": True, "attempted": None, "saved": None, "status": "submitted"}
        report["stages"].append(row); persist()
        try:
            row["raw"] = client.call_toolset(descriptor["name"], tool, mapped, mutation=mutation)
            if suffix.startswith("Cubeless"):
                value = operation_result(row["raw"])
            else:
                value = tool_payload(row["raw"])
                # Official FString property containers are JSON; level paths
                # and scalar strings remain ordinary strings.
                if isinstance(value, str) and value.lstrip().startswith(("{", "[")):
                    value = json.loads(value)
            row.update(result=value, state="responded")
            if isinstance(value, dict) and "changes" in value:
                report["changes"].append({"stage": label, "operation_id": value.get("operation_id"), "observations": value["changes"]})
                if any(c.get("change_state_known") is False for c in value["changes"]): report["outcome_unknown"] = True
                if mutation:
                    row["compile"] = value.get("compile"); row["save"] = value.get("save")
                if tool == "compile_and_save_blueprint":
                    report["compile"] = value.get("compile", {"status": "unknown"})
                    report["save"] = value.get("save", {"status": "unknown"})
            if check: check(value)
            row["passed"] = True
            return value
        except Exception as error:
            row.update(error=str(error), passed=False)
            if row["state"] == "submitted" and (mutation or suffix == "CubelessComponentTools"):
                report["outcome_unknown"] = True
            raise
        finally:
            row["elapsed_seconds"] = (time.time_ns() - row["started_ns"]) / 1e9; persist()

    def diag(label, clean=True, allowed_dirty=None):
        value = invoke(label, "CubelessNativeToolset", "GetEditorDiagnostics", check=success)
        require(Path(value["project_file"]).resolve() == PROJECT.resolve() and value["process_id"] == args.expected_pid
                and value["engine_version"].startswith("5.8.3-") and value["game_thread"] is True
                and value["pie_or_sie_active"] is False, "Wrong engine/editor/world execution state")
        allowed = set(allowed_dirty) if allowed_dirty is not None else ({package} if not clean else set())
        require(set(value["dirty_packages"]) <= allowed, "Unexpected dirty packages")
        report[label] = value
        return value

    def readback(prefix):
        listed = invoke(prefix + "_components", "CubelessComponentTools", "list_blueprint_components", {"blueprint_name": package})
        view, cube = listing(listed, package)
        for prop, expected in (("StaticMesh", MESH), ("RelativeLocation", [0, 0, 0])):
            value = invoke(prefix + "_" + prop, "CubelessComponentTools", "get_component_property",
                {"blueprint_name": package, "component_name": "CubeVisual", "property_name": prop}, check=component_success)
            actual = value["data"]["property_value"]
            require(close_vector(actual, expected) if isinstance(expected, list) else actual == expected, "Property readback differs: " + prop)
        actual = invoke(prefix + "_official", "ObjectTools", "get_properties",
            {"instance": {"refPath": cube["template_path"]}, "properties": ["StaticMesh", "RelativeLocation"]})
        require(field(actual, "StaticMesh") == {"refPath": MESH}, "Independent mesh reference differs")
        location = field(actual, "RelativeLocation")
        require(close_vector([field(location, k) for k in ("x", "y", "z")], [0, 0, 0]), "Independent relative location differs")
        return view

    def actor_readback(prefix, baseline=None):
        value = invoke(prefix + "_actors", "CubelessActorTools", "get_actors_in_level", {"expected_level_path": level}, check=success)
        observed = actor_map(value["data"]["actors"])
        expected_class = package + ".BP_CubeComponent_C"
        require(actor_name in observed, "Spawned actor object name missing")
        actor = observed[actor_name]
        require(actor["name"] == actor_name and actor["class_path"] == expected_class
                and actor["level_path"] == level and actor["actor_path"] == fixture["map_object"] + ":PersistentLevel." + actor_name,
                "Spawned actor class/path differs")
        require(close_vector(actor["location"], [0, 0, 100]) and close_vector(actor["rotation"], [0, 0, 0])
                and close_vector(actor["scale"], [1, 1, 1]), "Spawned actor legacy transform differs")
        if baseline is not None:
            require(set(observed) == set(baseline) | {actor_name}
                    and all(observed[n] == row for n, row in baseline.items()), "Spawn/save changed another actor")
        if parent:
            require(actor == parent["spawned_actor"], "Cold spawned actor differs")
            protected = fixture["protected_actor"]["name"]
            require(observed.get(protected) == parent["saved_actor_baseline"][protected], "Cold protected WorldSettings differs")
        return actor, observed

    try:
        persist(); report["initialize"] = client.initialize()
        catalog = tool_payload(client.call("list_toolsets"))
        require(isinstance(catalog, str), "Unexpected Registry catalog")
        names = re.findall(r"^- ([\w.]+):", catalog, re.MULTILINE)
        needed = ["CubelessNativeToolset", "CubelessBlueprintTools", "CubelessComponentTools", "AssetTools", "ObjectTools"]
        if fixture: needed += ["CubelessActorTools", "SceneTools"]
        for suffix in needed:
            matches = [n for n in names if n.endswith("." + suffix)]
            require(len(matches) == 1, "Need exactly one registered Toolset: " + suffix)
            schemas[suffix] = {"name": matches[0], "schema": tool_payload(client.call("describe_toolset", {"toolset_name": matches[0]}))}
        required = {"CubelessComponentTools": ["list_blueprint_components", "get_component_property"]}
        if not parent:
            required["CubelessComponentTools"] += ["add_component_to_blueprint", "set_static_mesh_properties"]
            required["CubelessBlueprintTools"] = ["create_blueprint", "compile_and_save_blueprint"]
            if fixture: required["CubelessActorTools"] = ["spawn_blueprint_actor", "get_actors_in_level"]
        for suffix, tools in required.items():
            actual = [t["name"].rsplit(".", 1)[-1] for t in schemas[suffix]["schema"]["tools"]]
            require(all(actual.count(name) == 1 for name in tools), "Required pipeline tools absent; no asset creation attempted")
        report["schemas"] = schemas; report["editor"] = diag("preflight")
        setting = invoke("save_policy", "ObjectTools", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
        report["save_policy_before"] = field(setting, "SaveOnCompile")
        require(report["save_policy_before"] == "SoC_Never", "Requires SaveOnCompile=Never; this pipeline never changes settings")
        baseline_actors = None
        if fixture:
            invoke("load_owned_map", "SceneTools", "load_level", {"level_path": level})
            if not parent:
                initial = invoke("actor_baseline", "CubelessActorTools", "get_actors_in_level", {"expected_level_path": level}, check=success)
                baseline_actors = actor_map(initial["data"]["actors"])
                require(actor_name not in baseline_actors and not any(n.startswith("TestBPInstance_") for n in baseline_actors), "Map already has a pipeline instance")
                protected = fixture["protected_actor"]
                observed_protected = baseline_actors.get(protected["name"], {})
                require(all(observed_protected.get(key) == value for key, value in protected.items()), "Protected prepared actor changed")
                report["saved_actor_baseline"] = baseline_actors
        if parent:
            require(report["editor"]["instance_id"] != parent["editor"]["instance_id"]
                    and report["editor"]["process_id"] != parent["editor"]["process_id"], "Cold read requires a new editor process")
            report["components"] = readback("cold")
            require(report["components"] == parent["components"], "Cold component identity/hierarchy/transform differs")
            report["reload"] = "representative_saved_component_read_passed; full_negative_suite_not_repeated"
        else:
            require(invoke("asset_absent", "AssetTools", "exists", {"path": package}) is False, "Owned asset already exists")
            created = invoke("create", "CubelessBlueprintTools", "create_blueprint",
                {"name": "BP_CubeComponent", "parent_class": "Actor", "package_path": folder, "operation_id": run + "_create"}, True, success)
            require(created["save"]["saved"] is False and not disk.exists(), "Creation unexpectedly saved")
            report["implicit_factory_compile"] = created["compile"].get("implicit_factory_compile")
            baseline = invoke("root_before_add", "CubelessComponentTools", "list_blueprint_components", {"blueprint_name": package}, check=component_success)
            roots = baseline["data"]["components"]
            require(len(roots) == 1 and roots[0]["component_name"] == "DefaultSceneRoot"
                    and roots[0]["component_class_path"] == "/Script/Engine.SceneComponent", "Expected fresh Actor DefaultSceneRoot")
            invoke("add_component", "CubelessComponentTools", "add_component_to_blueprint",
                {"blueprint_name": package, "component_type": "StaticMeshComponent", "component_name": "CubeVisual",
                 "location_json": "[0,0,0]", "rotation_json": "[0,0,0]", "scale_json": "[1,1,1]",
                 "parent_component": "DefaultSceneRoot", "compile_blueprint": False,
                 "operation_id": run + "_add"}, True, lambda v: component_success(v, True))
            invoke("set_mesh", "CubelessComponentTools", "set_static_mesh_properties",
                {"blueprint_name": package, "component_name": "CubeVisual",
                 "operation_id": run + "_mesh"}, True, lambda v: component_success(v, True))
            report["before_save_components"] = readback("authored")
            require(next(r for r in report["before_save_components"] if r["component_name"] == "DefaultSceneRoot")["node_guid"] == roots[0]["node_guid"], "Original root was replaced")
            require(not disk.exists() and invoke("dirty_before_save", "AssetTools", "is_dirty", {"asset_path": package}) is True,
                    "Expected unsaved dirty authored asset")
            diag("before_save", clean=False)
            saved = invoke("compile_save", "CubelessBlueprintTools", "compile_and_save_blueprint",
                {"blueprint_name": package, "save": True, "operation_id": run + "_save"}, True, success)
            report["compile"] = saved["compile"]; report["save"] = saved["save"]
            require(saved["compile"].get("compiled") is True and saved["compile"].get("error_count") == 0
                    and saved["data"].get("validation_pass") is True and saved["save"].get("saved") is True
                    and saved["data"].get("package_dirty_after") is False, "Compile/save validation failed")
            report["components"] = readback("saved")
            require(report["components"] == report["before_save_components"], "Compile/save changed authored component state")
        if fixture:
            if not parent:
                invoke("spawn_blueprint_actor", "CubelessActorTools", "spawn_blueprint_actor",
                    {"blueprint_name": package, "actor_name": actor_name, "location_json": "[0,0,100]",
                     "rotation_json": "[0,0,0]", "scale_json": "[1,1,1]", "expected_level_path": level,
                     "operation_id": run + "_spawn"}, True, actor_mutation)
                report["spawned_actor"], _ = actor_readback("spawned", baseline_actors)
                diag("before_map_save", allowed_dirty={level})
                report["map_save"] = {"requested": True, "attempted": True, "saved": None, "status": "submitted"}; persist()
                saved_map = invoke("save_map_once", "AssetTools", "save_assets", {"asset_paths": [level]}, True)
                report["map_save"].update(saved=saved_map is True, status="saved" if saved_map is True else "failed")
                require(saved_map is True, "Map save failed")
            report["spawned_actor"], report["saved_actors"] = actor_readback("saved" if not parent else "cold", baseline_actors)
            require(invoke("map_clean", "AssetTools", "is_dirty", {"asset_path": level}) is False, "Owned map is dirty")
            report["map_file"] = fingerprint(map_disk)
            require(report["map_file"] is not None, "Saved map missing")
            if parent: require(report["map_file"] == parent["map_file"], "Cold map bytes changed")
            report["spawn_blueprint_actor"] = "passed_saved_instance" if not parent else "passed_cold_instance"
        require(invoke("asset_clean", "AssetTools", "is_dirty", {"asset_path": package}) is False, "Saved/read asset is dirty")
        diag("final_editor")
        report["asset_file"] = fingerprint(disk)
        require(report["asset_file"] is not None, "Saved asset file is missing")
        if parent: require(report["asset_file"] == parent["asset_file"], "Cold read changed saved asset bytes/mtime")
        require(not report["outcome_unknown"], "Operation outcome remains uncertain")
        report.update(status="pass", stage="complete", selected_legacy_flow_completed=bool(fixture))
    except Exception as error:
        report.update(status="failed", error=str(error), failed_stage=report.get("stage", "discovery"),
                      do_not_retry=True, retry_policy="Inspect recorded changes and dirty/save state; no automatic retry or rollback")
        if report["stages"]: report["stages"][-1]["pipeline_check_failed_after_response"] = True
    finally:
        stage_before_final_policy = report.get("stage")
        if report.get("editor") and report.get("save_policy_before") == "SoC_Never":
            try:
                final_policy = invoke("final_save_policy", "ObjectTools", "get_properties",
                    {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
                report["save_policy_after"] = field(final_policy, "SaveOnCompile")
                report["save_policy_unchanged"] = report["save_policy_after"] == report["save_policy_before"]
                require(report["save_policy_unchanged"], "SaveOnCompile changed during this run")
            except Exception as error:
                report["final_save_policy_error"] = str(error)
                if report.get("status") != "failed":
                    report.update(error=str(error), failed_stage="final_save_policy")
                report["status"] = "failed"
            finally:
                report["stage"] = stage_before_final_policy
        report["asset_after"] = fingerprint(disk); report["config_after"] = fingerprint(config)
        if fixture: report["map_after"] = fingerprint(map_disk)
        report["config_changed"] = report["config_after"] != report["config_before"]
        report["config_observation_scope"] = "Whole ini changes are observed only: official map loading may update recent-map/editor bookkeeping. SaveOnCompile is checked separately."
        report.update(finished_ns=time.time_ns(), http_observations=client.observations,
                      mutation_calls=sum(row["mutation"] for row in report["stages"]), tool_calls=len(report["stages"]))
        persist(); client.close()
    print(json.dumps({k: report.get(k) for k in ("status", "phase", "package", "failed_stage", "error", "outcome_unknown", "tool_calls")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
