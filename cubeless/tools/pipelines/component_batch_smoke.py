"""Small isolated component cohorts; select shape bounds, physics defaults or material overrides."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from pipelines.component_authoring import PROJECT, RECORDS, SETTINGS, fingerprint, require, success, component_success, close_vector, field

TOOLSETS = {"native": "CubelessMCPUE58Editor.CubelessNativeToolset",
    "bp": "cubeless_mcp_ue58.registration.toolsets.CubelessBlueprintTools",
    "component": "cubeless_mcp_ue58.registration.component_toolset.CubelessComponentTools",
    "batch": "cubeless_mcp_ue58.registration.batch_toolset.CubelessBatchTools",
    "assets": "editor_toolset.toolsets.asset.AssetTools", "objects": "editor_toolset.toolsets.object.ObjectTools"}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", required=True); p.add_argument("--expected-project", required=True, type=Path)
    p.add_argument("--expected-pid", required=True, type=int); p.add_argument("--output", required=True, type=Path)
    mode = p.add_mutually_exclusive_group()
    mode.add_argument("--shape-bounds", action="store_true", help="P: three shape templates, one bounds rejection and one compile/save")
    mode.add_argument("--physics-defaults", action="store_true", help="Q: stored BodyInstance defaults, one invalid mass and one compile/save; no simulation")
    mode.add_argument("--material-overrides", action="store_true", help="R: material-only and mesh/material slot 0 assignment, one invalid reference and one compile/save")
    a = p.parse_args(argv); out = a.output.resolve()
    require(a.expected_project.resolve() == PROJECT.resolve(), "Exact isolated project required")
    require(out.is_relative_to(RECORDS.resolve()) and not out.exists(), "Use a fresh verification report")
    run = uuid.uuid4().hex; cohort = "MaterialOverrides" if a.material_overrides else ("PhysicsDefaults" if a.physics_defaults else ("ShapeBounds" if a.shape_bounds else "ComponentBatch"))
    folder = "/Game/CubelessMCPTests/" + cohort + "_" + run; asset_name = "BP_" + cohort; package = folder + "/" + asset_name
    disk = PROJECT.parent / "Content" / (package[6:] + ".uasset"); require(not disk.parent.exists(), "Owned UUID folder already exists")
    config = PROJECT.parent / "Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini"
    report = {"schema": "cubeless_component_material_overrides_v1" if a.material_overrides else ("cubeless_physics_defaults_smoke_v1" if a.physics_defaults else ("cubeless_shape_bounds_smoke_v1" if a.shape_bounds else "cubeless_component_batch_smoke_v1")), "status": "running", "run_id": run, "package": package,
        "project_file": str(PROJECT.resolve()), "stages": [], "changes": [], "started_ns": time.time_ns(),
        "automatic_retries": 0, "outcome_unknown": False, "atomic_rollback": False, "cold_reload": "not_run",
        "packaging": "not_run", "full_legacy_batch_parity": False, "config_before": fingerprint(config),
        "scope": ("R: hidden legacy material slot 0 contract on local StaticMesh templates; no rendering, shader completion or full legacy pipeline parity" if a.material_overrides else ("Q: extracted stored physics defaults only; no simulation, collision, graph or full legacy pipeline parity" if a.physics_defaults else ("P: local Sphere/Box/Capsule sizes; no collision profile, simulation, spawn or full legacy pipeline parity" if a.shape_bounds else "One default-compile add, one type rejection, five component/compile-save batch actions and saved readback"))),
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    c = OfficialMCPClient(a.endpoint); schemas = {}
    def persist():
        out.parent.mkdir(parents=True, exist_ok=True); temp = out.with_suffix(out.suffix + ".partial")
        temp.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"); temp.replace(out)
    def call(label, group, tool, arguments=None, mutation=False, expect_success=True):
        report["stage"] = label; choices = [t for t in schemas[group]["tools"] if t["name"].rsplit(".", 1)[-1] == tool]
        require(len(choices) == 1, "Missing unique tool " + tool); schema = choices[0]["inputSchema"]; mapped = {}
        for key, value in (arguments or {}).items():
            keys = [k for k in schema.get("properties", {}) if canonical(k) == canonical(key)]
            require(len(keys) == 1, "Missing input " + key); mapped[keys[0]] = value
        require(all(k in mapped for k in schema.get("required", [])), "Unexpected required argument")
        row = {"name": label, "toolset": TOOLSETS[group], "tool": tool, "arguments": mapped, "mutation": mutation,
               "state": "submitted", "started_ns": time.time_ns()}; report["stages"].append(row); persist()
        try:
            row["raw"] = c.call_toolset(TOOLSETS[group], tool, mapped, mutation=mutation)
            value = operation_result(row["raw"]); row.update(result=value, state="responded")
            if isinstance(value, dict) and "changes" in value:
                report["changes"].append({"stage": label, "observations": value["changes"]})
                row.update(compile=value.get("compile"), save=value.get("save"))
                if any(x.get("change_state_known") is False for x in value["changes"]): report["outcome_unknown"] = True
                if value.get("data", {}).get("batch", {}).get("mutation_state_unknown"): report["outcome_unknown"] = True
            if group in {"native", "bp", "component", "batch"} and expect_success: success(value)
            return value
        except Exception as error:
            row["error"] = str(error)
            if row["state"] == "submitted" and mutation: report["outcome_unknown"] = True
            raise
        finally: row["elapsed_seconds"] = (time.time_ns() - row["started_ns"]) / 1e9; persist()
    def prop(label, component, name):
        return call(label, "component", "get_component_property", {"blueprint_name": package, "component_name": component, "property_name": name})["data"]["property_value"]
    def compiled(value, saved):
        success(value); require(value["compile"].get("compiled") is True and value["compile"].get("error_count") == 0, "Compile failed")
        require(value["save"].get("saved") is saved and value["save"].get("requested") is saved, "Wrong save state")
        native = value["data"].get("compile", value["data"])
        require(native.get("validation_pass") is True and native.get("compiled") is True, "Native compiler validation failed")
    try:
        persist(); c.initialize(); schemas.update({k: tool_payload(c.call("describe_toolset", {"toolset_name": v})) for k, v in TOOLSETS.items()}); report["schemas"] = schemas
        required_component = ["add_component_to_blueprint", "set_component_property", "list_blueprint_components", "get_component_property"]
        if a.physics_defaults: required_component.append("set_physics_properties")
        if a.material_overrides: required_component.append("set_static_mesh_properties")
        for group, names in {"component": required_component, "batch": ["execute_batch"]}.items():
            actual = [t["name"].rsplit(".", 1)[-1] for t in schemas[group]["tools"]]; require(all(actual.count(n) == 1 for n in names), "O tools not registered")
        editor = call("preflight", "native", "GetEditorDiagnostics")
        require(Path(editor["project_file"]).resolve() == PROJECT.resolve() and editor["process_id"] == a.expected_pid and editor["game_thread"] is True
                and editor["engine_version"].startswith("5.8.3-") and not editor["pie_or_sie_active"] and editor["dirty_packages"] == [], "Wrong/dirty editor")
        report["editor"] = editor
        setting = call("policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
        report["save_policy_before"] = setting.get("SaveOnCompile")
        require(report["save_policy_before"] == "SoC_Never", "Requires Never; settings are not changed")
        require(call("absent", "assets", "exists", {"path": package}) is False, "Asset exists")
        created = call("create", "bp", "create_blueprint", {"name": asset_name, "parent_class": "Actor", "package_path": folder, "operation_id": run + "_create"}, True)
        require(created["save"]["saved"] is False and not disk.exists(), "Creation saved unexpectedly")
        report["implicit_factory_compile"] = created["compile"].get("implicit_factory_compile")
        if a.material_overrides:
            plane = "/Engine/BasicShapes/Plane.Plane"; cube = "/Engine/BasicShapes/Cube.Cube"
            base = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"
            instance = "/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"
            added = call("add_plane_component", "component", "add_component_to_blueprint", {"blueprint_name": package,
                "component_name": "Surface", "component_type": "StaticMeshComponent", "parent_component": "DefaultSceneRoot",
                "component_properties_json": json.dumps({"StaticMesh": plane}), "compile_blueprint": False, "operation_id": run + "_add"}, True)
            component_success(added, mutation=True); require(not disk.exists(), "Add saved unexpectedly")
            def material_rows(label):
                listed = call(label, "component", "list_blueprint_components", {"blueprint_name": package}); component_success(listed)
                rows = listed["data"]["components"]
                require(len(rows) == 2 and {r["component_name"] for r in rows} == {"DefaultSceneRoot", "Surface"}, "Wrong material component set")
                for row in rows:
                    name = row["component_name"]
                    require(row["component_class_path"] == "/Script/Engine." + ("SceneComponent" if name == "DefaultSceneRoot" else "StaticMeshComponent")
                            and row["parent_component_name"] == ("" if name == "DefaultSceneRoot" else "DefaultSceneRoot"), "Wrong material class/parent")
                    require(row["node_guid"] and row["template_path"].startswith(package + "."), "Material identity escaped owned asset")
                return rows
            initial_rows = material_rows("initial_material_components")
            template = next(r["template_path"] for r in initial_rows if r["component_name"] == "Surface")
            def materials(label, expected_mesh, expected_material):
                actual = call(label, "objects", "get_properties", {"instance": {"refPath": template}, "properties": ["StaticMesh", "OverrideMaterials"]})
                observed = {"static_mesh": field(actual, "StaticMesh"), "override_materials": field(actual, "OverrideMaterials")}
                require(observed["static_mesh"] == {"refPath": expected_mesh}, "Independent mesh reference differs")
                require(observed["override_materials"] == [{"refPath": expected_material}], "Independent slot 0 override differs")
                return observed
            only = call("material_only", "component", "set_static_mesh_properties", {"blueprint_name": package,
                "component_name": "Surface", "static_mesh": "", "material": base, "operation_id": run + "_material"}, True)
            component_success(only, mutation=True)
            first = materials("official_material_only", plane, base); require(not disk.exists(), "Material-only saved unexpectedly")
            commands = [{"type": "CubelessComponentTools.set_static_mesh_properties", "params": {"blueprint_name": package,
                "component_name": "Surface", "static_mesh": cube, "material": instance}},
                {"type": "CubelessBlueprintTools.compile_and_save_blueprint", "params": {"blueprint_name": package, "save": True}}]
            result = call("mesh_material_batch", "batch", "execute_batch", {"commands_json": json.dumps(commands), "operation_id": run + "_both"}, True)
            summary = result["data"]["batch"]
            require(summary["succeeded"] == summary["executed"] == 2 and summary["failed"] == 0 and summary["not_run"] == []
                    and not summary["mutation_state_unknown"], "Mesh/material batch incomplete")
            children = [row["response"] for row in summary["results"]]; require(len(children) == 2, "Missing material child results")
            component_success(children[0], mutation=True); compiled(children[1], True)
            require(len(result["compile"]["entries"]) == len(result["save"]["entries"]) == 1, "Expected one material compile/save")
            rows = material_rows("saved_material_components")
            expected_rows = json.loads(json.dumps(initial_rows))
            surface = next(r for r in expected_rows if r["component_name"] == "Surface")
            require(surface.get("static_mesh") == plane, "Initial component mesh differs"); surface["static_mesh"] = cube
            require(rows == expected_rows, "Material assignment or compilation changed component hierarchy/identity")
            template = next(r["template_path"] for r in rows if r["component_name"] == "Surface")
            observed = materials("official_mesh_material", cube, instance)
            saved = fingerprint(disk); require(saved is not None, "Material component asset not saved")
            missing = folder + "/M_Missing.M_Missing"
            bad = call("reject_missing_material_before_mesh_write", "component", "set_static_mesh_properties", {"blueprint_name": package,
                "component_name": "Surface", "static_mesh": plane, "material": missing, "operation_id": run + "_missing"}, True, False)
            require(bad.get("success") is False and bad.get("stage") == "preflight" and bad.get("error_code") == "component_material_reference_invalid"
                    and bad.get("changes") == [], "Wrong missing material rejection")
            require(bad["data"]["native_outcome"]["mutation_attempted"] is False and not bad["compile"]["attempted"] and not bad["save"]["saved"], "Rejected material performed work")
            require(materials("official_unchanged_after_rejection", cube, instance) == observed, "Rejected combined assignment changed state")
            require(call("material_asset_clean", "assets", "is_dirty", {"asset_path": package}) is False and fingerprint(disk) == saved, "Material failure changed disk/dirty state")
            final = call("final_editor", "native", "GetEditorDiagnostics")
            require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == [], "Editor changed/dirty")
            require(not report["outcome_unknown"], "Uncertain material result")
            report.update(status="pass", components=rows, material_only=first, final_material=observed, asset_file=saved,
                          explicit_full_compile_requests=1, explicit_save_requests=1, shader_completion_verified=False,
                          rendering_verified=False, full_legacy_command_parity=False,
                          representative_rejection={"error_code": bad["error_code"], "stage": bad["stage"], "requested_mesh": plane, "requested_material": missing})
        elif a.physics_defaults:
            commands = [{"type": "CubelessComponentTools.add_component_to_blueprint", "params": {"blueprint_name": package,
                "component_name": name, "component_type": kind, "parent_component": "DefaultSceneRoot",
                "component_properties_json": json.dumps(values), "compile_blueprint": False}} for name, kind, values in [
                    ("Sphere", "SphereComponent", {"SphereRadius": 30}),
                    ("Mesh", "StaticMeshComponent", {"StaticMesh": "/Engine/BasicShapes/Cube.Cube"})]]
            commands += [{"type": "CubelessComponentTools.set_physics_properties", "params": {"blueprint_name": package, "component_name": "Sphere"}},
                {"type": "CubelessComponentTools.set_physics_properties", "params": {"blueprint_name": package, "component_name": "Mesh",
                    "simulate_physics": False, "gravity_enabled": False, "mass": 7.5, "linear_damping": 0.2, "angular_damping": 0.3}},
                {"type": "CubelessBlueprintTools.compile_and_save_blueprint", "params": {"blueprint_name": package, "save": True}}]
            result = call("physics_batch", "batch", "execute_batch", {"commands_json": json.dumps(commands), "operation_id": run + "_physics"}, True)
            summary = result["data"]["batch"]
            require(summary["succeeded"] == summary["executed"] == 5 and summary["failed"] == 0 and summary["not_run"] == []
                    and not summary["mutation_state_unknown"], "Physics default batch incomplete")
            children = [row["response"] for row in summary["results"]]; require(len(children) == 5, "Missing physics child results")
            for child in children[:4]: component_success(child, mutation=True)
            compiled(children[-1], True)
            require(len(result["compile"]["entries"]) == len(result["save"]["entries"]) == 1, "Expected one physics compile/save")
            listed = call("saved_physics_components", "component", "list_blueprint_components", {"blueprint_name": package})
            component_success(listed); rows = listed["data"]["components"]
            require(len(rows) == 3 and {r["component_name"] for r in rows} == {"DefaultSceneRoot", "Sphere", "Mesh"}, "Wrong physics component set")
            templates = {}
            for row in rows:
                name = row["component_name"]; kind = {"DefaultSceneRoot": "SceneComponent", "Sphere": "SphereComponent", "Mesh": "StaticMeshComponent"}[name]
                require(row["component_class_path"] == "/Script/Engine." + kind
                        and row["parent_component_name"] == ("" if name == "DefaultSceneRoot" else "DefaultSceneRoot"), "Wrong physics component class/parent")
                require(row["node_guid"] and row["template_path"].startswith(package + "."), "Physics identity escaped owned asset")
                templates[name] = row["template_path"]
            expected = {"Sphere": {"bSimulatePhysics": True, "bEnableGravity": True, "bOverrideMass": True, "massInKgOverride": 1.0,
                                   "linearDamping": 0.01, "angularDamping": 0.0, "physicsBlendWeight": 1.0},
                        "Mesh": {"bSimulatePhysics": False, "bEnableGravity": False, "bOverrideMass": True, "massInKgOverride": 7.5,
                                 "linearDamping": 0.2, "angularDamping": 0.3, "physicsBlendWeight": 0.0}}
            def body(label, name):
                actual = call(label, "objects", "get_properties", {"instance": {"refPath": templates[name]}, "properties": ["BodyInstance"]})
                value = field(actual, "BodyInstance"); observed = {}
                for key, wanted in expected[name].items():
                    got = field(value, key); observed[key] = got
                    if type(wanted) is bool: require(got is wanted, "Stored body bool differs: " + name + "." + key)
                    else: require(type(got) in (int, float) and math.isfinite(got) and math.isclose(got, wanted, rel_tol=1e-6, abs_tol=1e-6), "Stored body number differs: " + name + "." + key)
                return observed
            observed = {name: body("official_body_" + name, name) for name in expected}
            saved = fingerprint(disk); require(saved is not None, "Physics defaults asset not saved")
            bad = call("reject_negative_mass", "component", "set_physics_properties", {"blueprint_name": package,
                "component_name": "Mesh", "mass": -1, "operation_id": run + "_bad_mass"}, True, False)
            require(bad.get("success") is False and bad.get("stage") == "preflight" and bad.get("error_code") == "component_physics_value_invalid"
                    and bad.get("changes") == [], "Wrong Python mass rejection")
            require(not bad["compile"]["attempted"] and not bad["save"]["saved"] and bad.get("data", {}).get("native_dispatched", False) is False, "Invalid mass dispatched work")
            require(body("body_unchanged_after_rejection", "Mesh") == observed["Mesh"], "Invalid mass changed stored body defaults")
            require(call("physics_asset_clean", "assets", "is_dirty", {"asset_path": package}) is False and fingerprint(disk) == saved, "Invalid mass changed disk/dirty state")
            final = call("final_editor", "native", "GetEditorDiagnostics")
            require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == [], "Editor changed/dirty")
            require(not report["outcome_unknown"], "Uncertain physics result")
            report.update(status="pass", components=rows, body_defaults=observed, body_expected=expected, asset_file=saved,
                          explicit_full_compile_requests=1, explicit_save_requests=1, physics_simulation_verified=False,
                          legacy_physics_pipeline_complete=False, representative_rejection={"error_code": bad["error_code"], "requested_mass": -1, "stage": bad["stage"]})
        elif a.shape_bounds:
            sizes = {"Sphere": {"SphereRadius": 100.0}, "Box": {"BoxExtent": [20.0, 30.0, 40.0]},
                     "Capsule": {"CapsuleRadius": 30.0, "CapsuleHalfHeight": 60.0}}
            commands = [{"type": "CubelessComponentTools.add_component_to_blueprint", "params": {"blueprint_name": package,
                "component_name": name, "component_type": name + "Component", "parent_component": "DefaultSceneRoot",
                "component_properties_json": json.dumps(values), "compile_blueprint": False}} for name, values in sizes.items()]
            commands += [{"type": "CubelessComponentTools.set_component_property", "params": {"blueprint_name": package,
                "component_name": "Sphere", "property_name": "SphereRadius", "property_value_json": "64"}},
                {"type": "CubelessBlueprintTools.compile_and_save_blueprint", "params": {"blueprint_name": package, "save": True}}]
            result = call("shape_batch", "batch", "execute_batch", {"commands_json": json.dumps(commands), "operation_id": run + "_shapes"}, True)
            summary = result["data"]["batch"]
            require(summary["succeeded"] == summary["executed"] == 5 and summary["failed"] == 0 and summary["not_run"] == []
                    and not summary["mutation_state_unknown"], "Shape batch incomplete")
            children = [row["response"] for row in summary["results"]]; require(len(children) == 5, "Missing shape child results")
            for child in children[:4]: component_success(child, mutation=True)
            compiled(children[-1], True)
            require(len(result["compile"]["entries"]) == len(result["save"]["entries"]) == 1, "Expected one shape compile/save")
            sizes["Sphere"]["SphereRadius"] = 64.0
            listed = call("saved_shape_components", "component", "list_blueprint_components", {"blueprint_name": package})
            component_success(listed); rows = listed["data"]["components"]
            require(len(rows) == 4 and {r["component_name"] for r in rows} == {"DefaultSceneRoot", *sizes}, "Wrong shape set")
            for row in rows:
                name = row["component_name"]; expected_class = "SceneComponent" if name == "DefaultSceneRoot" else name + "Component"
                require(row["component_class_path"] == "/Script/Engine." + expected_class
                        and row["parent_component_name"] == ("" if name == "DefaultSceneRoot" else "DefaultSceneRoot"), "Wrong shape hierarchy/class")
                require(row["node_guid"] and row["template_path"].startswith(package + "."), "Shape identity escaped owned asset")
                if name not in sizes: continue
                actual = call("official_" + name, "objects", "get_properties", {"instance": {"refPath": row["template_path"]}, "properties": list(sizes[name])})
                for key, expected in sizes[name].items():
                    value = field(actual, key)
                    if isinstance(expected, list):
                        require(close_vector([field(value, axis) for axis in ("x", "y", "z")], expected), "Independent box extent differs")
                    else: require(type(value) in (int, float) and value == expected, "Independent shape size differs: " + key)
            saved = fingerprint(disk); require(saved is not None, "Shape asset not saved")
            bad = call("reject_capsule_radius_above_halfheight", "component", "set_component_property", {"blueprint_name": package,
                "component_name": "Capsule", "property_name": "CapsuleRadius", "property_value_json": "61", "operation_id": run + "_bad"}, True, False)
            require(bad.get("success") is False and bad.get("stage") == "preflight"
                    and bad.get("error_code") == "component_shape_size_invalid" and bad.get("changes") == [], "Wrong capsule preflight rejection")
            require(bad["data"]["native_outcome"]["mutation_attempted"] is False and not bad["compile"]["attempted"] and not bad["save"]["saved"], "Rejected capsule performed work")
            report["representative_rejection"] = {"error_code": bad["error_code"], "error": bad["error"], "property": "CapsuleRadius", "requested": 61, "companion_halfheight": 60}
            require(prop("capsule_radius_unchanged", "Capsule", "CapsuleRadius") == 30
                    and prop("capsule_halfheight_unchanged", "Capsule", "CapsuleHalfHeight") == 60, "Rejected capsule altered sizes")
            require(call("shape_asset_clean", "assets", "is_dirty", {"asset_path": package}) is False and fingerprint(disk) == saved, "Shape failure changed disk/dirty state")
            final = call("final_editor", "native", "GetEditorDiagnostics")
            require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == [], "Editor changed/dirty")
            require(not report["outcome_unknown"], "Uncertain shape result")
            report.update(status="pass", components=rows, shape_values=sizes, asset_file=saved, explicit_full_compile_requests=1,
                          explicit_save_requests=1, physics_simulation_verified=False, legacy_multi_component_pipeline_complete=False)
        else:
            added = call("standalone_default_compile_add", "component", "add_component_to_blueprint", {"blueprint_name": package,
                "component_type": "SceneComponent", "component_name": "RootSupport", "parent_component": "DefaultSceneRoot",
                "component_properties_json": '{"bVisible":false}', "operation_id": run + "_root"}, True)
            compiled(added, False); require(not disk.exists() and prop("initial_visibility", "RootSupport", "bVisible") is False, "Initial property/save differs")
            before = call("before_rejection", "component", "list_blueprint_components", {"blueprint_name": package})["data"]["components"]
            bad = call("reject_string_bool", "component", "set_component_property", {"blueprint_name": package, "component_name": "RootSupport",
                "property_name": "bVisible", "property_value_json": '"true"', "operation_id": run + "_bad"}, True, False)
            require(bad.get("success") is False and bad.get("error_code") == "component_property_value_unsupported" and bad.get("changes") == [], "Wrong pre-mutation rejection")
            require(bad["data"]["native_outcome"]["mutation_attempted"] is False and not bad["compile"]["attempted"] and not bad["save"]["saved"], "Rejection performed work")
            require(prop("visibility_unchanged", "RootSupport", "bVisible") is False and call("tree_unchanged", "component", "list_blueprint_components", {"blueprint_name": package})["data"]["components"] == before, "Rejected setter changed state")
            commands = [{"type": "CubelessComponentTools.add_component_to_blueprint", "params": {"blueprint_name": package, "component_name": name, "component_type": kind,
                "parent_component": "RootSupport", "compile_blueprint": False}} for name, kind in [("Camera", "CameraComponent"), ("SpringArm", "SpringArmComponent")]]
            commands += [{"type": "CubelessComponentTools.set_component_property", "params": {"blueprint_name": package, "component_name": name,
                "property_name": key, "property_value_json": json.dumps(value)}} for name, key, value in [("RootSupport", "bVisible", True), ("Camera", "AttachSocketName", "Grip")]]
            commands += [{"type": "CubelessBlueprintTools.compile_and_save_blueprint", "params": {"blueprint_name": package, "save": True}}]
            batch = call("batch", "batch", "execute_batch", {"commands_json": json.dumps(commands), "operation_id": run + "_batch"}, True); summary = batch["data"]["batch"]
            require(summary["succeeded"] == summary["executed"] == 5 and summary["failed"] == 0 and summary["not_run"] == [] and not summary["mutation_state_unknown"], "Batch incomplete")
            children = [r["response"] for r in summary["results"]]; require(len(children) == 5, "Missing child results")
            for child in children[:4]:
                success(child); require(child["compile"]["requested"] is False and child["save"]["requested"] is False and not child["save"]["saved"], "Intermediate compile/save")
            compiled(children[4], True); require(len(batch["compile"]["entries"]) == len(batch["save"]["entries"]) == 1, "Expected one batch compile/save")
            rows = call("saved_components", "component", "list_blueprint_components", {"blueprint_name": package})["data"]["components"]
            expected = {"DefaultSceneRoot": ("SceneComponent", ""), "RootSupport": ("SceneComponent", "DefaultSceneRoot"), "Camera": ("CameraComponent", "RootSupport"), "SpringArm": ("SpringArmComponent", "RootSupport")}
            require(len(rows) == 4 and {r["component_name"] for r in rows} == set(expected), "Wrong component set")
            require(all((r["component_class"], r["parent_component_name"]) == expected[r["component_name"]] for r in rows), "Wrong hierarchy/class")
            require(prop("final_visibility", "RootSupport", "bVisible") is True and prop("socket_name", "Camera", "AttachSocketName") == "Grip", "Saved property differs")
            require(call("asset_clean", "assets", "is_dirty", {"asset_path": package}) is False and disk.is_file(), "Save/clean check failed")
            final = call("final_editor", "native", "GetEditorDiagnostics"); require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == [], "Editor changed/dirty")
            require(not report["outcome_unknown"], "Uncertain result"); report.update(status="pass", components=rows, asset_file=fingerprint(disk), explicit_full_compile_requests=2, explicit_save_requests=1)
    except Exception as error: report.update(status="failed", error=str(error), failed_stage=report.get("stage", "discovery"), do_not_retry=True)
    finally:
        original_stage = report.get("stage")
        if report.get("editor") and report.get("save_policy_before") == "SoC_Never":
            try:
                policy = call("final_save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
                report["save_policy_after"] = policy.get("SaveOnCompile"); report["save_policy_unchanged"] = report["save_policy_after"] == report["save_policy_before"]
                require(report["save_policy_unchanged"], "SaveOnCompile changed during this run")
            except Exception as error:
                report["final_save_policy_error"] = str(error)
                if report.get("status") != "failed": report.update(error=str(error), failed_stage="final_save_policy")
                report["status"] = "failed"
            finally: report["stage"] = original_stage
        report.update(asset_after=fingerprint(disk), config_after=fingerprint(config), finished_ns=time.time_ns(), http_observations=c.observations)
        report["config_changed"] = report["config_after"] != report["config_before"]
        report["config_observation_scope"] = "Whole ini hash changes are observations only; SaveOnCompile is checked separately."
        persist(); c.close()
    print(json.dumps({k: report.get(k) for k in ("status", "package", "failed_stage", "error", "outcome_unknown")})); return 0 if report["status"] == "pass" else 1


if __name__ == "__main__": raise SystemExit(main())
