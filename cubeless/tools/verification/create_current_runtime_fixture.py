"""Author only new map/MPC clones in the exact prepared no-MCP runtime project."""
import argparse
import json
import os
from pathlib import Path
import sys
import time
import traceback

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import current_runtime_manifest as contract
from current_runtime_manifest import digest, object_path, read_json, require
from validate_fixture_without_mcp import absolute, dependency_audit, enabled_plugins, package_file, write_report

CUBE = "/Engine/BasicShapes/Cube.Cube"


def path(value): return value.get_path_name() if value is not None else None
def field(value, key): return value.get_editor_property(key)


def preflight(manifest, report):
    require(absolute(unreal.Paths.get_project_file_path()) == contract.PROJECT.resolve(), "Editor is not exact UE58CurrentRuntimeProof")
    contract.validate_manifest(manifest, copied=True)
    report["configuration"] = contract.verify_project_config(manifest)
    report["engine_version"] = unreal.SystemLibrary.get_engine_version()
    require(report["engine_version"].startswith("5.8.3-"), "Only UE58.3 is covered")
    require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, "PIE/SIE active")
    report["plugins_before"] = enabled_plugins(); require(report["plugins_before"]["passed"], "MCP/Toolset enabled")
    report["dirty_before"] = [path(p) for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())]
    require(report["dirty_before"] == [], "Existing unsaved changes must be preserved")
    engine = absolute(unreal.Paths.engine_dir())
    report["engine_api_sources"] = {name: digest(engine / name) for name in ["Source/Editor/UnrealEd/Public/FileHelpers.h", "Source/Editor/UnrealEd/Public/Subsystems/EditorAssetSubsystem.h", "Source/Editor/UnrealEd/Public/Subsystems/EditorActorSubsystem.h", "Source/Editor/MaterialEditor/Public/MaterialEditingLibrary.h", "Source/Runtime/Engine/Public/Materials/MaterialExpressionCustom.h"]}
    return unreal.get_editor_subsystem(unreal.EditorAssetSubsystem), engine


def material_snapshot(material):
    require(isinstance(material, unreal.Material), "Expected native Material")
    nodes = []
    for expression in unreal.MaterialEditingLibrary.get_material_expressions(material):
        names = [str(n) for n in unreal.MaterialEditingLibrary.get_material_expression_input_names(expression)]
        sources = list(unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, expression))
        require(len(names) == len(sources), "Expression input parallel arrays differ")
        row = {"path": path(expression), "name": expression.get_name(), "class": path(expression.get_class()), "x": int(field(expression, "material_expression_editor_x")), "y": int(field(expression, "material_expression_editor_y")), "desc": str(field(expression, "desc")), "inputs": dict(zip(names, map(path, sources)))}
        if isinstance(expression, unreal.MaterialExpressionConstant): row["r"] = float(field(expression, "r"))
        if isinstance(expression, unreal.MaterialExpressionCollectionParameter): row.update(collection=path(field(expression, "collection")), parameter_name=str(field(expression, "parameter_name")))
        if isinstance(expression, unreal.MaterialExpressionCustom):
            row["custom"] = {"code": str(field(expression, "code")), "description": str(field(expression, "description")), "output_type": str(field(expression, "output_type")), "float3": field(expression, "output_type") == unreal.CustomMaterialOutputType.CMOT_FLOAT3, "input_names": [str(field(i, "input_name")) for i in field(expression, "inputs")], "defines": [{"name": str(field(d, "define_name")), "value": str(field(d, "define_value"))} for d in field(expression, "additional_defines")], "includes": list(field(expression, "include_file_paths"))}
        nodes.append(row)
    return {"material": path(material), "nodes": sorted(nodes, key=lambda r: r["path"]), "base_color": path(unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR))}


def validate_source_assets(manifest, assets):
    """Independent public API readbacks; original package bytes are separately bound."""
    observations = {}
    for role, entry in manifest["parents"].items():
        parent = read_json(entry["path"])
        if role.startswith("material_"):
            value = material_snapshot(assets.load_asset(parent["assets"]["material"])); expected = parent["saved_graph"]["nodes"]
            require({n["path"] for n in value["nodes"]} == {n["object_path"] for n in expected}, "Material node identity changed")
            for node in value["nodes"]:
                old = next(n for n in expected if n["object_path"] == node["path"])
                require(node["class"] == old["class_path"] and (node["x"], node["y"]) == (old["x"], old["y"]), "Material class/position differs")
                require(node["inputs"] == {p["name"]: p.get("connected_object_path") if p["connected"] else None for p in old["inputs"]}, "Material source links differ")
            require(value["base_color"] == parent["fixture"]["nodes"]["add"], "Material BaseColor differs")
            added = next(n for n in value["nodes"] if n["path"] == parent["fixture"]["nodes"]["added"])
            require(abs(added["r"] - .875) < 1e-6, "Saved Add/Set retained value differs")
            observations[role] = value
        elif role == "edges":
            value = material_snapshot(assets.load_asset(parent["material_package"]))
            require({n["path"] for n in value["nodes"]} == set(parent["nodes"].values()), "Edges node set differs")
            from material_add_set_edges_report import INITIAL_DESC, FINAL_DESC
            for kind, expected_r, position, desc in [("default", .125, (0, 0), INITIAL_DESC), ("position", .875, (-700, 550), FINAL_DESC)]:
                node = next(n for n in value["nodes"] if n["path"] == parent["nodes"][kind])
                require(abs(node["r"] - expected_r) < 1e-6 and (node["x"], node["y"]) == position and node["desc"] == desc, "Edges number/text/position differs")
            observations[role] = value
        elif role == "custom":
            from custom_material_report import DEFAULT_CODE, ALIAS_CODE, DESCRIPTION
            value = material_snapshot(assets.load_asset(parent["material_package"]))
            require({n["path"] for n in value["nodes"]} == set(parent["nodes"].values()) and value["base_color"] == parent["nodes"]["alias"], "Custom graph/root differs")
            for kind in ("default", "alias"):
                node = next(n for n in value["nodes"] if n["path"] == parent["nodes"][kind]); c = node["custom"]
                require(c["code"] == (DEFAULT_CODE if kind == "default" else ALIAS_CODE) and c["float3"] is True and c["includes"] == [], "Custom code/output/include differs")
                require(c["description"] == node["desc"] == ("" if kind == "default" else DESCRIPTION), "Custom description differs")
                require(c["input_names"] == ([] if kind == "default" else ["Signal", "Bias"]) and node["inputs"] == ({} if kind == "default" else {"Signal": parent["nodes"]["default"], "Bias": parent["nodes"]["default"]}), "Custom independent input wiring differs")
                require(c["defines"] == ([] if kind == "default" else [{"name": "CUBE_SCALE", "value": "0.5"}, {"name": "CUBE_FLAG", "value": "1"}, {"name": "CUBE_DEFAULT", "value": "1"}]), "Custom defines differ")
            observations[role] = value
        elif role == "mpc":
            value = material_snapshot(assets.load_asset(parent["packages"]["material"]))
            require({n["path"] for n in value["nodes"]} == set(parent["nodes"].values()), "Inherited MPC expression set differs")
            for n in value["nodes"]: require(n["collection"] == parent["objects"]["leaf"] and n["parameter_name"] in {"RootScalar", "RootVector"}, "MPC expression reference differs")
            observations[role] = value
            local_expected = {"base": ({"RootScalar": .25}, {"RootVector": (.1, .2, .3, 1)}, None), "mid": ({"MidScalar": .5}, {}, parent["objects"]["base"]), "leaf": ({}, {"LeafVector": (.4, .5, .6, 1)}, parent["objects"]["mid"])}
            collections = {}
            for kind, (scalars, vectors, base) in local_expected.items():
                collection = assets.load_asset(parent["packages"][kind]); require(isinstance(collection, unreal.MaterialParameterCollection), "MPC failed no-plugin load")
                actual_s = {str(field(p, "parameter_name")): float(field(p, "default_value")) for p in field(collection, "scalar_parameters")}
                actual_v = {str(field(p, "parameter_name")): [float(getattr(field(p, "default_value"), c)) for c in ("r", "g", "b", "a")] for p in field(collection, "vector_parameters")}
                require(path(field(collection, "base")) == base and actual_s.keys() == scalars.keys() and actual_v.keys() == vectors.keys(), "MPC base/local parameter sets differ")
                require(all(abs(actual_s[k] - v) < 1e-5 for k, v in scalars.items()) and all(all(abs(x - y) < 1e-5 for x, y in zip(actual_v[k], v)) for k, v in vectors.items()), "MPC local defaults differ")
                collections[kind] = {"collection": path(collection), "base": base, "scalars": actual_s, "vectors": actual_v}
            observations["mpc_local_chain"] = collections
    for row in manifest["blueprint_actors"]:
        blueprint = assets.load_asset(row["package"])
        require(isinstance(blueprint, unreal.Blueprint) and path(blueprint.generated_class()) == row["class_path"] and path(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)) == "/Script/Engine.Actor", "Runtime Blueprint generated class/parent differs")
    return observations


def inspect_map(world, manifest):
    actors = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor.static_class()))
    rows = [{"path": path(a), "label": a.get_actor_label(False), "class_path": path(a.get_class())} for a in actors]
    known = manifest["preserved_marker_actors"] + manifest["blueprint_actors"]
    for expected in known: require(sum(row["label"] == expected["label"] and row["class_path"] == expected["class_path"] for row in rows) == 1, "Missing/duplicate runtime Blueprint actor: " + expected["label"])
    cubes = []
    for expected in manifest["material_actors"]:
        found = [a for a in actors if a.get_actor_label(False) == expected["label"]]
        require(len(found) == 1 and path(found[0].get_class()) == "/Script/Engine.StaticMeshActor", "Wrong proof cube identity")
        component = found[0].get_component_by_class(unreal.StaticMeshComponent.static_class())
        require(component is not None and path(field(component, "static_mesh")) == CUBE and path(component.get_material(0)) == object_path(expected["package"]), "Cube mesh/slot/material differs")
        cubes.append({"path": path(found[0]), "label": expected["label"], "class_path": "/Script/Engine.StaticMeshActor", "mesh": CUBE, "material": path(component.get_material(0)), "slot": 0})
    return {"world": path(world), "actors": sorted(rows, key=lambda r: r["path"]), "cubes": sorted(cubes, key=lambda r: r["label"])}


def clone_snapshot(material, spec):
    value = material_snapshot(material); byname = {n["name"]: n for n in value["nodes"]}
    source = byname[spec["source_node_name"]]
    require(source["collection"] == object_path(spec["collection"]), "Clone has wrong MPC reference")
    if spec["connection"] == "collection_to_add_B":
        target = byname[spec["target_node_name"]]
        require(target["inputs"].get("B") == source["path"] and value["base_color"] == target["path"], "Clone MPC is not connected to BaseColor through Add")
    else: require(source["parameter_name"] == "RootVector" and value["base_color"] == source["path"], "Inherited RootVector is not connected to BaseColor")
    return value


def preservation(manifest):
    result = [{"package": row["package"], "source_unchanged": Path(row["source"]).is_file() and digest(row["source"]) == row["sha256"], "copy_unchanged": Path(row["destination"]).is_file() and digest(row["destination"]) == row["sha256"]} for row in manifest["files"]]
    return {"files": result, "passed": all(r["source_unchanged"] and r["copy_unchanged"] for r in result)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("--manifest", required=True, type=Path); parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args(argv); output = args.report.resolve(); require(not output.exists(), "Refusing to overwrite author evidence")
    output.parent.mkdir(parents=True, exist_ok=True); manifest = read_json(args.manifest)
    report = {"schema": "cubeless_current_runtime_fixture_v1", "success": False, "stage": "preflight", "process_id": os.getpid(), "project_file": str(contract.PROJECT.resolve()), "manifest": {"path": str(args.manifest.resolve()), "sha256": digest(args.manifest)}, "started_ns": time.time_ns(), "changes": [], "save": {"clone_calls": 0, "map_calls": 0}, "compile": [], "shader_completion_verified": False, "automatic_retry": False, "atomic_rollback": False, "runtime_marker_created": False, "packaging": "not_run"}
    def stage(name): report["stage"] = name; write_report(output, report)
    write_report(output, report)
    try:
        assets, engine = preflight(manifest, report)
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        require(actors is not None, "Editor actor subsystem unavailable")
        require(not assets.does_directory_exist(manifest["new_asset_root"]), "New fixture root already exists; inspect prior attempt, never rerun")
        stage("source_readback"); report["source_readback"] = validate_source_assets(manifest, assets)
        clones = []; report["clones"] = {}
        for spec in manifest["clone_specs"]:
            stage("duplicate_" + spec["role"]); change = {"kind": "duplicate_and_connect_material", "source": spec["source"], "target": spec["target"], "attempted": True, "completed": False}; report["changes"].append(change); write_report(output, report)
            material = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(spec["target"].rsplit("/", 1)[1], manifest["new_asset_root"], assets.load_asset(spec["source"]))
            require(isinstance(material, unreal.Material) and path(material) == object_path(spec["target"]), "Material duplicate failed")
            expressions = {n.get_name(): n for n in unreal.MaterialEditingLibrary.get_material_expressions(material)}; source = expressions[spec["source_node_name"]]
            if spec["connection"] == "collection_to_add_B": require(unreal.MaterialEditingLibrary.connect_material_expressions(source, "", expressions[spec["target_node_name"]], "B"), "MPC Add.B connection failed")
            else: require(unreal.MaterialEditingLibrary.connect_material_property(source, "", unreal.MaterialProperty.MP_BASE_COLOR), "MPC BaseColor connection failed")
            report["clones"][spec["target"]] = clone_snapshot(material, spec); change["completed"] = True; clones.append(material)
            stage("recompile_" + spec["role"]); observation = {"material": path(material), "requested": True, "returned": False, "shader_completion_verified": False}; report["compile"].append(observation); write_report(output, report)
            errors = [str(e) for e in unreal.MaterialEditingLibrary.recompile_material(material)]; observation.update(returned=True, reported_errors=errors, status="returned_without_reported_errors" if not errors else "reported_errors"); require(not errors, "Clone reported compiler errors")
        stage("save_new_clones"); report["save"].update(clone_calls=1, clone_packages=[s["target"] for s in manifest["clone_specs"]], clones_saved=None); write_report(output, report)
        report["save"]["clones_saved"] = bool(assets.save_loaded_assets(clones, False)); require(report["save"]["clones_saved"], "Clone save failed")
        stage("load_copied_marker_map"); world = unreal.EditorLoadingAndSavingUtils.load_map(str(contract.PROJECT.parent / contract.relative_asset(manifest["source_map"], True))); require(world is not None and path(world).split(".", 1)[0] == manifest["source_map"], "Wrong marker map loaded")
        for index, row in enumerate(manifest["blueprint_actors"]):
            stage("spawn_" + row["label"])
            change = {"kind": "spawn_blueprint_actor", "package": row["package"], "requested_label": row["label"], "attempted": True, "completed": False, "stage": "spawn_pending"}; report["changes"].append(change); write_report(output, report)
            actor = actors.spawn_actor_from_class(assets.load_asset(row["package"]).generated_class(), unreal.Vector(index * 200, 400, 0), unreal.Rotator(0, 0, 0), False)
            change.update(returned_actor=path(actor), stage="spawn_returned"); write_report(output, report); require(actor is not None, "Blueprint spawn failed")
            change.update(class_path=path(actor.get_class()), stage="label_pending"); write_report(output, report); actor.set_actor_label(row["label"])
            change.update(label=actor.get_actor_label(False), completed=True, stage="complete"); write_report(output, report)
        mesh = assets.load_asset(CUBE); require(isinstance(mesh, unreal.StaticMesh), "Engine cube unavailable")
        for index, row in enumerate(manifest["material_actors"]):
            stage("spawn_" + row["label"])
            change = {"kind": "spawn_material_cube", "material": row["package"], "requested_label": row["label"], "attempted": True, "completed": False, "stage": "spawn_pending"}; report["changes"].append(change); write_report(output, report)
            actor = actors.spawn_actor_from_class(unreal.StaticMeshActor.static_class(), unreal.Vector(index * 200, 0, 100), unreal.Rotator(0, 0, 0), False)
            change.update(returned_actor=path(actor), stage="spawn_returned"); write_report(output, report); require(actor is not None, "Cube spawn failed")
            change.update(class_path=path(actor.get_class()), stage="label_pending"); write_report(output, report); actor.set_actor_label(row["label"])
            change.update(label=actor.get_actor_label(False), stage="mesh_pending"); write_report(output, report)
            component = actor.get_component_by_class(unreal.StaticMeshComponent.static_class()); change["component"] = path(component); write_report(output, report)
            require(component is not None and component.set_static_mesh(mesh), "Cube mesh assignment failed")
            change.update(mesh_assigned=True, stage="material_pending"); write_report(output, report); component.set_material(0, assets.load_asset(row["package"]))
            change.update(material=path(component.get_material(0)), slot=0, completed=True, stage="complete"); write_report(output, report)
        report["map_readback"] = inspect_map(world, manifest)
        stage("save_new_map"); report["save"].update(map_calls=1, map_saved=None, map_package=manifest["default_map"], old_map_save_requested=False)
        change = {"kind": "save_map_copy", "source": manifest["source_map"], "target": manifest["default_map"], "attempted": True, "completed": False}; report["changes"].append(change); write_report(output, report)
        report["save"]["map_saved"] = bool(unreal.EditorLoadingAndSavingUtils.save_map(world, manifest["default_map"])); require(report["save"]["map_saved"], "New map save failed")
        change["completed"] = True; write_report(output, report)
        report["map_readback"] = inspect_map(world, manifest)
        report["new_asset_files"] = {p: {"path": str(package_file(p, contract.PROJECT.parent, engine)), "sha256": digest(package_file(p, contract.PROJECT.parent, engine))} for p in [manifest["default_map"]] + [c["target"] for c in manifest["clone_specs"]]}
        stage("dependency_audit"); dependency_audit([manifest["default_map"]], unreal.AssetRegistryHelpers.get_asset_registry(), contract.PROJECT.parent, engine, report)
        report["plugins_after"] = enabled_plugins(); require(report["plugins_after"]["passed"], "MCP/Toolset enabled after authoring")
        report.update(success=True, stage="complete")
    except Exception as error: report.update(error=str(error), traceback=traceback.format_exc())
    finally:
        report["preservation"] = preservation(manifest)
        if not report["preservation"]["passed"]: report["success"] = False; report["preservation_error"] = "Original/copied bytes changed"
        report["finished_ns"] = time.time_ns(); write_report(output, report)
    return report["success"]


if __name__ == "__main__" and not main(): raise RuntimeError("Current runtime fixture authoring failed; inspect partial report, no automatic retry")
