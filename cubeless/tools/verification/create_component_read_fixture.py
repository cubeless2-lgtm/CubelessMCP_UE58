"""Source-only UE5.8 fixture author. Root executor alone runs author() in UE.

Public SubobjectData gathering is a possible write and is used only on the new
local fixture Blueprint, never on the inherited child or a user asset. No N
component reader contributes to the expected values in this manifest.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import time
import traceback
import uuid

PROJECT_ROOT = Path(__file__).resolve().parents[3]
ALLOWED_PROJECT = PROJECT_ROOT / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
RECORDS = PROJECT_ROOT / "cubeless/records/verification/implementation-2026-10-10"
CUBE = "/Engine/BasicShapes/Cube.Cube"
TAGS = ["CubelessComponentFixture", "ReadOnlyProof"]
MESH_TRANSFORM = {"location": [12.0, 34.0, 56.0], "rotation": [10.0, 20.0, 30.0], "scale": [1.25, 0.75, 2.0]}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def checked_options(report_path, run_id):
    require(isinstance(run_id, str) and re.fullmatch(r"[0-9a-f]{32}", run_id) is not None, "run_id must be 32 lowercase hex characters")
    output = Path(report_path).resolve()
    require(output.parent == RECORDS.resolve() and output.suffix == ".json", "Report must be a JSON file directly in the fixed verification records directory")
    require(not output.exists(), "Existing report will not be overwritten or treated as authorization to retry")
    return output, "/Game/CubelessMCPTests/Components_" + run_id


def checked_project(path):
    require(Path(path).resolve() == ALLOWED_PROJECT.resolve(), "Only the isolated UE58Verification project is authorized")


def write_report(path, report):
    temp = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    with temp.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False, allow_nan=False)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temp, path)


def content_hashes(project):
    content = Path(project).parent / "Content"
    return {str(path.resolve()): digest(path) for pattern in ("*.uasset", "*.umap") for path in content.rglob(pattern)}


def close_value(actual, expected):
    if isinstance(expected, list):
        return isinstance(actual, list) and len(actual) == len(expected) and all(close_value(a, b) for a, b in zip(actual, expected))
    if isinstance(expected, bool):
        return actual is expected
    if isinstance(expected, (int, float)):
        return not isinstance(actual, bool) and isinstance(actual, (int, float)) and abs(actual - expected) <= 1e-5
    return actual == expected


def author(report_path, run_id, expected_pid):
    """Run once in the exact isolated editor; returns report, never retries."""
    import unreal

    output, root = checked_options(report_path, run_id)
    require(isinstance(expected_pid, int) and expected_pid > 0, "A concrete expected editor PID is required")
    report = {"schema": "cubeless_component_read_fixture_v1", "success": False, "status": "started", "stage": "preflight", "run_id": run_id, "root": root,
              "started_ns": time.time_ns(), "automatic_retries": 0, "atomic_rollback": False, "outcome_unknown": False,
              "actions": [], "blueprints": {}, "asset_files": [], "compile": {"success": False, "explicit_calls": 0, "results": [], "implicit_engine_compilation_possible": True},
              "save": {"saved": False, "explicit_calls": 0}, "source_hashes": {str(Path(__file__).resolve()): digest(__file__)},
              "coverage_limits": ["Local reachable SCS only", "node_guid has no independent public Python accessor in this fixture; native repeat/reload comparison only", "No FString/FText component field fixture or malformed enum descriptor execution", "Subobject gathering is fixture authoring, not a read-only verification API"]}
    before_files = None
    assets = None

    def action(name, fn, mutation=True, summary=None):
        report["stage"] = name
        row = {"name": name, "mutation_intent": mutation, "attempted": True, "returned": False}
        report["actions"].append(row)
        write_report(output, report)  # Failure here prevents the next engine call.
        value = fn()
        row["returned"] = True
        if summary:
            row["result"] = summary(value)
        write_report(output, report)
        return value

    def path(obj):
        return obj.get_path_name() if obj is not None else ""

    def dirty():
        return sorted({path(p) for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())})

    def compile_bp(bp):
        report["compile"]["explicit_calls"] += 1
        result = action("compile_" + bp.get_name(), lambda: json.loads(unreal.CubelessNativeToolset.compile_blueprint_validation(path(bp), False, False)), summary=lambda value: value)
        report["compile"]["results"].append(result)
        require(result.get("success") is True and result.get("compiled") is True and result.get("saved") is False, "Explicit fixture compile failed or unexpectedly reported a save")

    def create_bp(name, parent):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = action("create_" + name, lambda: unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, root, unreal.Blueprint.static_class(), factory), summary=path)
        require(isinstance(bp, unreal.Blueprint) and path(bp) == root + "/" + name + "." + name, "Unexpected created Blueprint identity")
        return bp

    try:
        actual_project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())
        checked_project(actual_project)
        report["project_file"] = str(Path(actual_project).resolve())
        report["engine_version"] = unreal.SystemLibrary.get_engine_version()
        require(report["engine_version"].startswith("5.8.3-"), "Only installed UE5.8.3 is covered")
        report["editor"] = json.loads(unreal.CubelessNativeToolset.get_editor_diagnostics())
        require(report["editor"]["process_id"] == expected_pid and report["editor"]["game_thread"] is True and report["editor"]["pie_or_sie_active"] is False, "Editor PID/thread/PIE guard failed")
        # BlueprintEditorSettings has no generated Python class in this build.
        # Use its real CDO through the installed official property API.
        settings = unreal.load_object(None, "/Script/BlueprintGraph.Default__BlueprintEditorSettings")
        require(settings is not None, "Blueprint editor settings CDO unavailable")
        preference = json.loads(unreal.ToolsetLibrary.get_object_properties(settings, ["SaveOnCompile"]))["SaveOnCompile"]
        report["save_on_compile"] = {"name": preference, "value": 0 if preference == "SoC_Never" else None, "changed_by_fixture": False}
        require(preference == "SoC_Never", "BlueprintFactory implicitly compiles without SkipSave; fixture requires SoC_Never before creation")
        report["dirty_before"] = dirty()
        require(report["dirty_before"] == [], "Existing unsaved packages must be preserved")
        assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        library = unreal.SubobjectDataBlueprintFunctionLibrary
        require(assets is not None and subsystem is not None, "Required public editor subsystems unavailable")
        for name in ("BP_Local", "BP_Empty", "BP_Inherited"):
            require(not assets.does_asset_exist(root + "/" + name), "Fixture asset already exists; no overwrite/retry")
        target_dir = ALLOWED_PROJECT.parent / "Content" / root.removeprefix("/Game/")
        require(not target_dir.exists(), "Fixture disk directory already exists; no overwrite/retry")
        before_files = content_hashes(ALLOWED_PROJECT)
        report["preexisting_asset_file_count"] = len(before_files)
        engine = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.engine_dir())).resolve()
        for relative in ("Source/Editor/SubobjectDataInterface/Public/SubobjectDataSubsystem.h", "Source/Editor/SubobjectDataInterface/Public/SubobjectDataBlueprintFunctionLibrary.h", "Source/Editor/SubobjectDataInterface/Private/SubobjectDataSubsystem.cpp", "Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp", "Source/Runtime/Engine/Classes/Components/SceneComponent.h", "Plugins/Experimental/ToolsetRegistry/Source/ToolsetRegistry/Private/ToolsetRegistry/ToolsetLibrary.cpp"):
            report["source_hashes"][str(engine / relative)] = digest(engine / relative)
        write_report(output, report)

        local = create_bp("BP_Local", unreal.Actor.static_class())

        def gather_local():
            # The only gather target is this newly created native-parent BP.
            require(path(local) == root + "/BP_Local.BP_Local", "Foreign gather target rejected")
            handles = action("gather_local_for_fixture_authoring", lambda: list(subsystem.k2_gather_subobject_data_for_blueprint(local)), summary=lambda value: {"handle_count": len(value), "may_create_editor_templates": True})
            result = {}
            # UE5.8 FactoryCreateInheritedBpSubobject appends each child in its
            # own recursion and again in its parent (Subsystem.cpp:2928-2937).
            # unreal.Array membership uses FProperty::Identical, hence the
            # handle's native DataPtr equality; Python struct == need not do so.
            seen_handles = unreal.Array(unreal.SubobjectDataHandle)
            duplicate_handles = 0
            for handle in handles:
                if handle in seen_handles:
                    duplicate_handles += 1
                    continue
                seen_handles.append(handle)
                data = library.get_data(handle)
                if not library.is_component(data) or library.is_inherited_component(data) or library.is_native_component(data):
                    continue
                name = str(library.get_variable_name(data))
                require(name not in result, "Duplicate fixture component variable name")
                # Deprecated but public direct WeakObjectPtr getter. Unlike
                # GetAssociatedObject it does not call GetObjectForBlueprint.
                obj = library.get_object(data)
                require(isinstance(obj, unreal.ActorComponent), "Expected actual local component template")
                result[name] = (handle, data, obj)
            report["actions"][-1]["result"].update(unique_handle_count=len(seen_handles), duplicate_handle_count=duplicate_handles)
            write_report(output, report)
            return result

        def add_component(parent_name, name, cls):
            rows = gather_local()
            require(parent_name in rows and name not in rows, "Missing parent or duplicate fixture component")
            params = unreal.AddNewSubobjectParams()
            params.set_editor_property("parent_handle", rows[parent_name][0])
            params.set_editor_property("new_class", cls.static_class())
            params.set_editor_property("blueprint_context", local)
            params.set_editor_property("skip_mark_blueprint_modified", False)
            params.set_editor_property("conform_transform_to_parent", False)
            handle, reason = action("add_" + name, lambda: subsystem.add_new_subobject(params), summary=lambda value: {"handle_valid": library.is_handle_valid(value[0]), "failure_reason": str(value[1])})
            require(library.is_handle_valid(handle), "Subobject creation failed: " + str(reason))
            require(action("rename_" + name, lambda: subsystem.rename_subobject(handle, name), summary=bool) is True, "Subobject rename failed")
            require(name in gather_local(), "Requested variable name did not survive rename")

        require(set(gather_local()) == {"DefaultSceneRoot"}, "New actor Blueprint's local root differs from declared fixture")
        add_component("DefaultSceneRoot", "MeshChild", unreal.StaticMeshComponent)
        add_component("MeshChild", "LightChild", unreal.PointLightComponent)
        templates = {name: item[2] for name, item in gather_local().items()}
        require(set(templates) == {"DefaultSceneRoot", "MeshChild", "LightChild"}, "Expected exactly three local SCS components")
        mesh, light = templates["MeshChild"], templates["LightChild"]
        cube = assets.load_asset(CUBE)
        require(isinstance(cube, unreal.StaticMesh), "Engine cube fixture missing")
        require(action("mesh_asset", lambda: mesh.set_static_mesh(cube), summary=bool) is True, "Mesh assignment failed")
        for prop, value in (("relative_location", unreal.Vector(*MESH_TRANSFORM["location"])), ("relative_rotation", unreal.Rotator(pitch=10.0, yaw=20.0, roll=30.0)), ("relative_scale3d", unreal.Vector(*MESH_TRANSFORM["scale"])), ("visible", False), ("component_tags", TAGS), ("mobility", unreal.ComponentMobility.MOVABLE)):
            action("set_mesh_" + prop, lambda prop=prop, value=value: mesh.set_editor_property(prop, value))
        action("set_light_intensity_units", lambda: light.set_intensity_units(unreal.LightUnits.LUMENS))
        action("set_light_intensity", lambda: light.set_intensity(1234.5))
        compile_bp(local)
        empty = create_bp("BP_Empty", unreal.SaveGame.static_class())
        compile_bp(empty)
        inherited = create_bp("BP_Inherited", local.generated_class())
        compile_bp(inherited)
        # Never gather the inherited child: gathering can create overrides.
        rows = gather_local()

        def triple(value, rotation=False):
            return [float(getattr(value, key)) for key in (("pitch", "yaw", "roll") if rotation else ("x", "y", "z"))]

        expected_rows = []
        for name, (_, data, obj) in rows.items():
            parent_data = library.get_data(library.get_parent_handle(data))
            parent_name = str(library.get_variable_name(parent_data)) if library.is_component(parent_data) else ""
            row = {"component_name": name, "variable_name": name, "component_class": obj.get_class().get_name(), "component_class_path": path(obj.get_class()), "template_name": obj.get_name(), "template_path": path(obj), "parent_component_name": parent_name,
                   "relative_transform": {"location": triple(obj.get_editor_property("relative_location")), "rotation": triple(obj.get_editor_property("relative_rotation"), True), "scale": triple(obj.get_editor_property("relative_scale3d"))}}
            require(parent_name == {"DefaultSceneRoot": "", "MeshChild": "DefaultSceneRoot", "LightChild": "MeshChild"}[name], "Fixture parent hierarchy differs")
            require(row["component_class_path"] == "/Script/Engine." + {"DefaultSceneRoot": "SceneComponent", "MeshChild": "StaticMeshComponent", "LightChild": "PointLightComponent"}[name], "Unexpected fixture component class")
            if name == "MeshChild":
                row["static_mesh"] = path(obj.get_editor_property("static_mesh"))
                require(row["static_mesh"] == CUBE and all(close_value(row["relative_transform"][key], value) for key, value in MESH_TRANSFORM.items()), "Fixture mesh transform/asset differs")
            else:
                require(all(close_value(row["relative_transform"][key], value) for key, value in {"location": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1]}.items()), "Default component transform differs from declared fixture")
            expected_rows.append(row)
        expected_rows.sort(key=lambda item: item["variable_name"])
        mesh, light = rows["MeshChild"][2], rows["LightChild"][2]
        specs = [
            ("MeshChild", "RelativeLocation", "FVector", MESH_TRANSFORM["location"], triple(mesh.get_editor_property("relative_location"))),
            ("MeshChild", "RelativeRotation", "FRotator", MESH_TRANSFORM["rotation"], triple(mesh.get_editor_property("relative_rotation"), True)),
            ("MeshChild", "RelativeScale3D", "FVector", MESH_TRANSFORM["scale"], triple(mesh.get_editor_property("relative_scale3d"))),
            ("MeshChild", "StaticMesh", None, CUBE, path(mesh.get_editor_property("static_mesh"))),
            ("MeshChild", "bVisible", "uint8", False, bool(mesh.get_editor_property("visible"))),
            ("MeshChild", "ComponentTags", None, TAGS, [str(item) for item in mesh.get_editor_property("component_tags")]),
            ("MeshChild", "Mobility", "TEnumAsByte<EComponentMobility::Type>", 2, int(mesh.get_editor_property("mobility").value)),
            ("LightChild", "Intensity", "float", 1234.5, float(light.get_editor_property("intensity"))),
            ("LightChild", "IntensityUnits", "ELightUnits", "Lumens", "Lumens" if light.get_editor_property("intensity_units") == unreal.LightUnits.LUMENS else str(light.get_editor_property("intensity_units"))),
            ("LightChild", "AttachSocketName", "FName", "None", str(light.get_attach_socket_name())),
        ]
        properties = []
        for name, prop, cpp_type, expected, actual in specs:
            require(close_value(actual, expected), "Independent fixture getter mismatch: " + name + "." + prop)
            entry = {"id": name + "." + prop, "component_name": name, "property_name": prop, "property_type": cpp_type, "property_encoding": "legacy_ue_export_text_bounded_scalar_array" if prop == "ComponentTags" else "legacy_typed_json", "declared_expected_value": expected, "independent_readback_value": actual}
            if prop == "AttachSocketName":
                entry.update(official_property_baseline=None, official_property_status="not_accessible_by_generic_tool", independent_getter="USceneComponent.GetAttachSocketName")
            else:
                raw = unreal.ToolsetLibrary.get_object_properties(rows[name][2], [prop])
                baseline = json.loads(raw)
                require(prop in baseline, "Official property baseline absent: " + prop)
                entry.update(official_property_baseline=baseline, official_property_baseline_json=raw, official_property_status="observed", independent_getter="UObject.get_editor_property")
            properties.append(entry)
        report["blueprints"] = {
            "local": {"package": root + "/BP_Local", "object_path": path(local), "has_scs": True, "expected_components": expected_rows, "properties": properties, "node_guid_independently_verified": False},
            "empty": {"package": root + "/BP_Empty", "object_path": path(empty), "has_scs": False, "expected_components": [], "properties": [], "expected_parent_class": "/Script/Engine.SaveGame"},
            "inherited": {"package": root + "/BP_Inherited", "object_path": path(inherited), "has_scs": True, "expected_components": [], "properties": [], "parent_blueprint": path(local), "inherited_components_excluded": True, "gather_called": False},
        }
        report["compile"]["success"] = True
        report["independent_before_save"] = {"local_component_count": len(rows), "blueprint_status": {bp.get_name(): str(bp.get_editor_property("status")) for bp in (local, empty, inherited)}}
        report["dirty_before_save"] = dirty()
        expected_packages = {item["package"] for item in report["blueprints"].values()}
        require(set(report["dirty_before_save"]).issubset(expected_packages), "Fixture authoring dirtied a package outside the declared new assets")
        require(not any(target_dir.glob("*.uasset")), "Unexpected implicit asset save occurred before explicit fixture save")
        report["save"].update(explicit_calls=1, packages=sorted(expected_packages))
        saved = action("save_three_new_blueprints_once", lambda: assets.save_loaded_assets([local, empty, inherited], False), summary=bool)
        report["save"]["saved"] = bool(saved)
        require(saved is True, "Explicit fixture save failed")
        report["dirty_after"] = dirty()
        report["independent_after_save"] = {"blueprint_status": {bp.get_name(): str(bp.get_editor_property("status")) for bp in (local, empty, inherited)}}
        require(report["dirty_after"] == [], "Unexpected dirty packages remain after fixture save")
        for item in report["blueprints"].values():
            disk = ALLOWED_PROJECT.parent / "Content" / (item["package"].removeprefix("/Game/") + ".uasset")
            require(disk.is_file(), "Saved fixture file missing")
            report["asset_files"].append({"package": item["package"], "path": str(disk.resolve()), "sha256": digest(disk)})
        report.update(success=True, status="pass", stage="complete")
    except Exception as error:
        report.update(success=False, status="failed", error=str(error), traceback=traceback.format_exc())
        report["outcome_unknown"] = bool(report["actions"] and not report["actions"][-1]["returned"])
    finally:
        if before_files is not None:
            differences = [path for path, old in before_files.items() if not Path(path).is_file() or digest(path) != old]
            report["preservation"] = {"preexisting_files": len(before_files), "changed_or_missing": differences, "passed": not differences}
            if differences:
                report.update(success=False, status="failed", preservation_error="Preexisting isolated asset bytes changed")
        try:
            report["dirty_final"] = dirty()
        except Exception as error:
            report["dirty_final_error"] = str(error)
        report["finished_ns"] = time.time_ns()
        write_report(output, report)
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--expected-pid", type=int, required=True)
    args = parser.parse_args()
    result = author(args.report, args.run_id, args.expected_pid)
    if not result["success"]:
        raise RuntimeError("Component fixture failed; inspect the saved partial report and do not automatically retry")


if __name__ == "__main__":
    main()
