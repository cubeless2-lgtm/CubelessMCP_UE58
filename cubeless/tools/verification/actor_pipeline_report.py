"""Independent raw-evidence verifier for the bounded Actor HTTP smoke."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from verification.material_add_set_edges_report import decoded_raw

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
CLASSES = ("StaticMeshActor", "PointLight", "SpotLight", "DirectionalLight", "CameraActor")
FIELDS = ("name", "class", "class_path", "actor_path", "level_path", "location", "rotation", "scale", "label")


def require(condition, message):
    if not condition: raise AssertionError(message)


def sha(data): return hashlib.sha256(data).hexdigest()


def fixture_contract(fixture):
    require(fixture.get("schema") == "cubeless_actor_map_fixture_v1" and fixture.get("success") is True and fixture.get("stage") == "complete", "Map fixture failed")
    run = fixture["run_id"]; require(re.fullmatch(r"[a-f0-9]{32}", run), "Invalid map run ID")
    package = "/Game/CubelessMCPTests/Actors_" + run + "/Map_Actors"
    disk = PROJECT.parent / "Content" / (package[len("/Game/"):] + ".umap")
    require(Path(fixture["project_file"]).resolve() == PROJECT.resolve() and fixture["map_package"] == package and fixture["map_object"] == package + ".Map_Actors", "Fixture project/map differs")
    require(fixture["save"] == {"requested": True, "attempted": True, "saved": True} and fixture["dirty_before"] == fixture["dirty_after"] == [], "Fixture was not cleanly saved")
    require(fixture["compile"] == {"requested": False} and fixture["automatic_retry"] is False and fixture["atomic_rollback"] is False and fixture["packaging"] == "not_run", "Fixture scope differs")
    protected = fixture["protected_actor"]
    require(protected in fixture["baseline_actors"] and protected["class_path"] == "/Script/Engine.WorldSettings", "Invalid protected actor")
    require(all(row["actor_path"] == fixture["map_object"] + ":PersistentLevel." + row["name"] for row in fixture["baseline_actors"]), "Baseline actor escaped map")
    require(len({r["name"] for r in fixture["baseline_actors"]}) == len(fixture["baseline_actors"]), "Duplicate baseline names")
    require(Path(fixture["asset_file"]["path"]).resolve() == disk.resolve() and re.fullmatch(r"[a-f0-9]{64}", fixture["asset_file"]["sha256"]), "Wrong fixture file evidence")
    return package, disk


def plan(fixture, phase):
    """Explicit sequence of named tools. Shared sequencing is not verification."""
    package, _ = fixture_contract(fixture); run = fixture["run_id"]; short = run[:8]
    names = {kind: "Cubeless_" + kind + "_" + short for kind in CLASSES}
    alias = "Cubeless_Alias_" + short; label = "DisplayOnly_" + short
    rows = []
    def add(stage_name, toolset, tool, args=None, mutation=False, check="success", **expected):
        rows.append({"name": stage_name, "toolset_suffix": toolset, "tool": tool, "arguments": args or {}, "mutation": mutation, "check": check, "expected": expected})
    def ours(stage_name, tool, args=None, mutation=False, check="success", **expected):
        add(stage_name, "CubelessActorTools", tool, args, mutation, check, **expected)
    guard = {"expected_level_path": package}
    add("preflight", "CubelessNativeToolset", "GetEditorDiagnostics", check="editor")
    add("load_map", "SceneTools", "load_level", {"level_path": package}, check="void")
    add("current_map", "SceneTools", "get_current_level", check="map")
    if phase == "author":
        ours("baseline", "get_actors_in_level", guard, check="baseline")
        for kind in CLASSES:
            # Defaults are intentionally omitted, including expected_level_path
            # on one call; preflight pins the isolated map and core rechecks it.
            args = {"name": names[kind], "type": kind.upper(), "operation_id": run + "_" + kind}
            if kind != CLASSES[0]: args.update(guard)
            ours("spawn_" + kind, "spawn_actor", args, True, "spawn", name=names[kind], kind=kind)
            ours("inspect_" + kind, "get_actor_properties", {"name": names[kind], **guard}, check="inspect", name=names[kind])
        ours("create_alias", "create_actor", {"name": alias, "type": "PointLight", "operation_id": run + "_alias", **guard}, True, "spawn", name=alias, kind="PointLight", operation="create_actor")
        ours("inspect_alias", "get_actor_properties", {"name": alias, **guard}, check="inspect", name=alias)
        target_path = fixture["map_object"] + ":PersistentLevel." + names[CLASSES[0]]
        add("set_display_label", "ActorTools", "set_label", {"actor": {"refPath": target_path}, "label": label}, True, "label_set", name=names[CLASSES[0]], label=label)
        add("get_display_label", "ActorTools", "get_label", {"actor": {"refPath": target_path}}, check="label", label=label)
        ours("label_keeps_object_name", "get_actor_properties", {"name": names[CLASSES[0]], **guard}, check="inspect", name=names[CLASSES[0]])
        ours("find_label_only", "find_actors_by_name", {"pattern": label, **guard}, check="find", names=[])
        ours("find_object_name", "find_actors_by_name", {"pattern": names[CLASSES[0]].lower(), **guard}, check="find", names=[names[CLASSES[0]]])
        ours("find_empty", "find_actors_by_name", {"pattern": "", **guard}, check="list")
        transform = {"name": alias, "location_json": "[40,50,60]", "operation_id": run + "_transform", **guard}
        ours("partial_transform", "set_actor_transform", transform, True, "transform", name=alias)
        ours("inspect_transform", "get_actor_properties", {"name": alias, **guard}, check="inspect", name=alias)
        ours("noop_transform", "set_actor_transform", {"name": alias, "operation_id": run + "_noop", **guard}, True, "noop", name=alias)
        ours("inspect_noop", "get_actor_properties", {"name": alias, **guard}, check="inspect", name=alias)
        ours("replay_transform", "set_actor_transform", transform, True, "replay", name=alias)
        ours("replay_readback", "get_actors_in_level", guard, check="list")
        negative = [
            ("conflict", "set_actor_transform", {**transform, "location_json": "[70,80,90]"}, "operation_id_conflict"),
            ("duplicate", "spawn_actor", {"name": alias, "type": "CameraActor", "operation_id": run + "_duplicate", **guard}, "actor_name_conflict"),
            ("missing", "delete_actor", {"name": "Cubeless_Missing_" + short, "operation_id": run + "_missing", **guard}, "actor_not_found"),
            ("bad_vector", "set_actor_transform", {"name": alias, "location_json": "[true,0,0]", "operation_id": run + "_bad_vector", **guard}, "invalid_actor_vector"),
            ("wrong_level", "set_actor_transform", {"name": alias, "location_json": "[1,2,3]", "operation_id": run + "_wrong_level", "expected_level_path": package + "_Wrong"}, "editor_level_mismatch"),
            ("protected", "delete_actor", {"name": fixture["protected_actor"]["name"], "operation_id": run + "_protected", **guard}, "actor_target_unsupported")]
        for key, tool, args, error in negative:
            ours("negative_" + key, tool, args, True, "reject", error=error)
            ours("after_" + key, "get_actors_in_level", guard, check="list")
        victim = names["CameraActor"]
        ours("delete_camera", "delete_actor", {"name": victim, "operation_id": run + "_delete", **guard}, True, "delete", name=victim)
        ours("after_delete", "get_actors_in_level", guard, check="list")
        ours("deleted_missing", "get_actor_properties", {"name": victim, **guard}, check="reject", error="actor_not_found")
        ours("after_missing_read", "get_actors_in_level", guard, check="list")
        add("before_save_editor", "CubelessNativeToolset", "GetEditorDiagnostics", check="dirty_editor")
        add("save_map_once", "AssetTools", "save_assets", {"asset_paths": [package]}, True, "true")
    else:
        ours("reload_actors", "get_actors_in_level", guard, check="list")
        for name in [names[kind] for kind in CLASSES if kind != "CameraActor"] + [alias]:
            ours("reload_" + name, "get_actor_properties", {"name": name, **guard}, check="inspect", name=name)
        ours("deleted_still_missing", "get_actor_properties", {"name": names["CameraActor"], **guard}, check="reject", error="actor_not_found")
    ours("saved_actors", "get_actors_in_level", guard, check="list")
    add("map_clean", "AssetTools", "is_dirty", {"asset_path": package}, check="false")
    add("final_editor", "CubelessNativeToolset", "GetEditorDiagnostics", check="editor")
    return rows


def projected(actor):
    require(isinstance(actor, dict) and all(k in actor for k in FIELDS), "Incomplete actor observation")
    return {key: copy.deepcopy(actor[key]) for key in FIELDS}


def actor_map(rows):
    require(isinstance(rows, list), "Missing actor array")
    result = {row["name"]: projected(row) for row in rows}
    require(len(result) == len(rows), "Ambiguous actor names")
    return result


def persisted_actor_map(rows, fixture):
    """System/transient baseline actors can legitimately differ across processes."""
    values = actor_map(rows); protected = fixture["protected_actor"]["name"]
    return {name: row for name, row in values.items() if name.startswith("Cubeless_") or name == protected}


def close_vector(value, expected):
    return isinstance(value, list) and len(value) == 3 and all(type(a) in (int, float) and math.isfinite(a) and abs(a-b) < .01 for a, b in zip(value, expected))


def mutation(value):
    require(value.get("success") is True, "Actor mutation failed")
    data = value["data"]; native = data["native_outcome"]
    require(native.get("change_state_known") is True and native.get("postcondition_passed") is True and value["validation"].get("native_postcondition_passed") is True, "Mutation was not independently observed")
    require(data.get("contract_version") == "mutation_result_v1" and data.get("verified_success") is True and data.get("postcondition_verified") is True and data["delta"].get("failed_count") == 0, "Legacy mutation contract missing")
    require(value["compile"]["requested"] is False and value["save"]["requested"] is False and value["save"]["saved"] is False and value["retry"]["automatic_retry"] is False, "Unexpected compile/save/retry")
    return data


def decode_call(raw, toolset_suffix):
    if toolset_suffix.startswith("Cubeless"):
        return decoded_raw(raw)
    # Official FString tools return ordinary strings (map paths/labels), not
    # the native Cubeless helper's JSON-encoded FString envelope.
    require(not raw.get("isError"), "Raw official tool error")
    if raw.get("structuredContent") is not None:
        value = raw["structuredContent"]
    else:
        texts = [r.get("text", "") for r in raw.get("content", []) if r.get("type") == "text"]
        require(len(texts) == 1, "No unique official scalar payload")
        try: value = json.loads(texts[0])
        except ValueError: value = texts[0]
    return value["returnValue"] if isinstance(value, dict) and "returnValue" in value else value


def verify(report, *, fixture=None, fixture_bytes=None, parent=None, parent_bytes=None, check_files=True):
    errors = []
    try:
        require(fixture is not None and fixture_bytes is not None and json.loads(fixture_bytes) == fixture, "Missing exact fixture evidence")
        package, disk = fixture_contract(fixture)
        require(report.get("schema") == "cubeless_actor_pipeline_v1" and report.get("status") == "pass" and report.get("phase") in {"author", "reload"}, "Wrong/failed Actor report")
        require(report["fixture_sha256"] == sha(fixture_bytes) and report["run_id"] == fixture["run_id"] and report["map_package"] == package and Path(report["project_file"]).resolve() == PROJECT.resolve(), "Report ownership differs")
        require(report["outcome_unknown"] is False and report["automatic_retries"] == 0 and report["atomic_rollback"] is False and report["packaging"] == "not_run" and report["full_legacy_actor_parity"] is False, "Overstated scope/uncertain outcome")
        expected = plan(fixture, report["phase"]); stages = report["stages"]; calls = report["tool_calls"]
        require([r["name"] for r in stages] == [s["name"] for s in expected] and all(s.get("passed") is True for s in stages), "Missing/reordered/failed Actor stage")
        require(len(calls) == len(stages) and [s["call_index"] for s in stages] == list(range(len(calls))), "Call/stage binding differs")
        model = {}; editor_id = None
        if report["phase"] == "reload":
            require(parent is not None and parent_bytes is not None and json.loads(parent_bytes) == parent and parent.get("phase") == "author" and verify(parent, fixture=fixture, fixture_bytes=fixture_bytes, check_files=check_files)["passed"], "Invalid author parent")
            require(report["author_sha256"] == sha(parent_bytes) and report["asset_file"] == parent["asset_file"], "Author hash/asset bytes differ")
            model = persisted_actor_map(parent["saved_actors"], fixture)
        else:
            require(report["initial_map_sha256"] == fixture["asset_file"]["sha256"], "Initial map did not match fixture")
        for spec, stage, call in zip(expected, stages, calls):
            require(call["toolset"].endswith("." + spec["toolset_suffix"]) and call["tool"] == spec["tool"] and call["arguments"] == spec["arguments"] and call["mutation"] is spec["mutation"], "Call target/arguments differ: " + spec["name"])
            value = decode_call(call["raw"], spec["toolset_suffix"])
            require(value == call["decoded"] == stage["result"], "Raw/cached/stage result differs")
            kind = spec["check"]; e = spec["expected"]
            if kind in {"editor", "dirty_editor"}:
                require(value["success"] is True and Path(value["project_file"]).resolve() == PROJECT.resolve() and value["game_thread"] is True and value["pie_or_sie_active"] is False and value["engine_version"].startswith("5.8.3-"), "Wrong editor")
                require(bool(value["instance_id"]), "Missing editor instance")
                if editor_id is None: editor_id = value["instance_id"]
                require(value["instance_id"] == editor_id and (value["dirty_packages"] == [] if kind == "editor" else set(value["dirty_packages"]) == {package}), "Editor changed or unrelated dirty package")
                if parent: require(editor_id != parent["editor"]["instance_id"], "Reload used the author process")
            elif kind == "void": require(value is None or value == {}, "Unexpected void result")
            elif kind == "map": require(value == package, "Wrong current map")
            elif kind == "true": require(value is True, "Explicit save failed")
            elif kind == "false": require(value is False, "Map is still dirty")
            elif kind == "label": require(value == e["label"], "Official label readback differs")
            elif kind == "label_set": require(value is True, "Label write failed"); model[e["name"]]["label"] = e["label"]
            elif kind == "reject":
                require(value.get("success") is False and value.get("error_code") == e["error"] and bool(value.get("error")) and value.get("changes") == [] and value["retry"]["automatic_retry"] is False, "Expected no-change rejection")
            elif kind == "baseline":
                require(value.get("success") is True and value["data"]["persistent_level_path"] == package, "Wrong baseline level")
                model = actor_map(value["data"]["actors"])
                protected = fixture["protected_actor"]
                require(protected["name"] in model and all(model[protected["name"]][k] == protected[k] for k in ("name", "class_path", "actor_path")), "Protected persistent identity differs from fixture")
                require(not any(name.startswith("Cubeless_") for name in model), "Fixture contains an earlier owned Actor attempt")
            elif kind in {"list", "find", "inspect"}:
                require(value.get("success") is True and value["data"]["persistent_level_path"] == package, "Wrong read scope")
                if kind == "inspect": require(projected(value["data"]) == model[e["name"]], "Exact actor readback differs")
                else:
                    expected_model = model if kind == "list" else {name: model[name] for name in e["names"]}
                    rows = value["data"]["actors"]
                    actual_model = persisted_actor_map(rows, fixture) if report["phase"] == "reload" and kind == "list" else actor_map(rows)
                    require(actual_model == expected_model and value["data"]["actor_count"] == len(rows), "Actor set/state differs")
            elif kind in {"spawn", "transform", "noop", "replay", "delete"}:
                data = mutation(value)
                require(data["persistent_level_path"] == package, "Mutation left fixture map")
                if kind != "replay": require(actor_map(data["before"]["actors"]) == model, "Mutation before-state differs")
                if kind == "spawn":
                    row = projected(data); name = e["name"]
                    require(row["name"] == name and row["class"] == e["kind"] and row["class_path"] == "/Script/Engine." + e["kind"] and row["actor_path"] == fixture["map_object"] + ":PersistentLevel." + name and row["level_path"] == package, "Spawn identity/class differs")
                    require(close_vector(row["location"], [0,0,0]) and close_vector(row["rotation"], [0,0,0]) and close_vector(row["scale"], [1,1,1]) and name not in model and data["delta"]["created_count"] == 1 and data["operation"] == e.get("operation", "spawn_actor"), "Spawn defaults/alias/postcondition differ")
                    model[name] = row
                elif kind == "transform": model[e["name"]]["location"] = [40,50,60]; require(data["delta"]["updated_count"] == 1, "Transform not observed")
                elif kind == "noop": require(data["native_outcome"].get("skipped_noop") is True and data["native_outcome"]["mutation_attempted"] is False and value["changes"] == [] and data["delta"]["skipped_count"] == 1, "No-op performed a mutation")
                elif kind == "replay": require(value["retry"]["replayed"] is True, "Repeated request was reexecuted")
                elif kind == "delete":
                    require(projected(data["deleted_actor"]) == model[e["name"]] and data["deleted"] is True and data["delta"]["removed_count"] == 1, "Delete target/postcondition differs"); del model[e["name"]]
                require(actor_map(data["after"]["actors"]) == model, "Mutation changed unexpected actor state")
        saved_stage = next(row for row in stages if row["name"] == "saved_actors")
        saved_model = persisted_actor_map(report["saved_actors"], fixture) if report["phase"] == "reload" else actor_map(report["saved_actors"])
        require(report["editor"] == stages[0]["result"] and report["saved_actors"] == saved_stage["result"]["data"]["actors"] and saved_model == model, "Summary is not backed by readback")
        require(report["save_calls"] == (1 if report["phase"] == "author" else 0), "Save count differs")
        require(Path(report["asset_file"]["path"]).resolve() == disk.resolve() and re.fullmatch(r"[a-f0-9]{64}", report["asset_file"]["sha256"]), "Invalid saved map evidence")
        if check_files: require(disk.is_file() and sha(disk.read_bytes()) == report["asset_file"]["sha256"], "Saved map bytes changed")
    except (AssertionError, AttributeError, ValueError, TypeError, KeyError, IndexError, OSError) as error:
        errors.append(str(error))
    return {"schema": "cubeless_actor_pipeline_verification_v1", "passed": not errors, "errors": errors, "phase": report.get("phase"), "full_legacy_actor_parity": False, "packaging": "not_run"}


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument("report", type=Path); p.add_argument("--fixture-report", type=Path, required=True); p.add_argument("--parent-report", type=Path); p.add_argument("--output", required=True, type=Path)
    a = p.parse_args(); raw = a.report.read_bytes(); fixture = a.fixture_report.read_bytes(); parent = a.parent_report.read_bytes() if a.parent_report else None
    result = verify(json.loads(raw), fixture=json.loads(fixture), fixture_bytes=fixture, parent=json.loads(parent) if parent else None, parent_bytes=parent)
    result["report_sha256"] = sha(raw)
    with a.output.open("x", encoding="utf-8") as f: json.dump(result, f, indent=2)
    print(json.dumps(result)); return 0 if result["passed"] else 1


if __name__ == "__main__": raise SystemExit(main())
