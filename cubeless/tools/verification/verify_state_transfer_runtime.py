"""Run four direct state-transfer calls on one owned Actor in the no-MCP editor.

One transient-requested Actor is spawned and removed once. This is a world
mutation, not a read-only probe or atomic rollback. No compile/save or retry.
"""
from __future__ import annotations

import argparse
import math
import os
from pathlib import Path
import re
import sys
import time
import traceback

sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_function_timer_runtime import PROJECT, RECORDS, require, digest, write_report
from verify_gated_timer_runtime import configuration_files

CALLER, MEMBER = "CFNL_UpdateCapture", "TimeSinceLastClick"
SLOTS = ("Buttons", "Capture")
# RequestCapture, Buttons.MousePressed, delta, expected timer/valid/mouse/has sample.
CASES = ((False, True, .25, 0.0, False, False, False),
         (True, True, .25, .25, True, True, True),
         (False, True, 1.25, .25, False, False, True),
         (True, False, .5, .75, True, False, True))
# Verified UE5.8 public set_editor_property notify_mode=NEVER avoids editor
# PostEdit reconstruction during transient fixture input preparation. It does
# not bypass property editability, so RequestCapture/MousePressed must be exposed.



def verify(project_file, host_package, host_sha256, component_package, component_sha256,
           report_path, expected_pid=None):
    import unreal
    from validate_fixture_without_mcp import enabled_plugins

    output = Path(report_path).resolve()
    require(output.parent == RECORDS.resolve() and output.suffix == ".json" and not output.exists(),
            "Use a new JSON report in the fixed verification directory")
    report = {"schema": "cubeless_state_transfer_runtime_v1", "success": False, "stage": "preflight",
        "started_ns": time.time_ns(), "process_id": os.getpid(), "expected_process_id": expected_pid,
        "host_package": host_package, "component_package": component_package, "function": CALLER,
        "expected_sha256": {"host": host_sha256, "component": component_sha256},
        "steps": [], "spawn": {"attempted": False, "returned": False, "transient_requested": True},
        "cleanup": {"attempted": False, "returned": False, "success": False},
        "input_updates_attempted": 0, "input_updates_returned": 0,
        "calls_attempted": 0, "calls_returned": 0, "calls_verified": 0,
        "automatic_retries": 0, "atomic_rollback": False, "outcome_unknown": False,
        "compile": {"explicit_calls": 0, "implicit_load_compilation": "not_measured"},
        "save": {"explicit_calls": 0}, "settings_writes": 0, "dirty_flag_clears": 0,
        "scope": "Fresh saved native-parent Actor and component load; two SCS instance identities and four direct state-transfer calls, then owned Actor removal",
        "side_effect_scope": "Spawn runs editor placement/construction/registration; destroy runs lifecycle/editor callbacks. Current disposable map may remain dirty; it is never saved or force-cleared.",
        "invocation_scope": "Direct reflected CFNL_UpdateCapture calls; no game Tick or hardware/player events",
        "input_preparation_scope": "Only transient instance RequestCapture and Buttons.MousePressed via Never notification; not an editor asset setter equivalence test",
        "unverified": ["BeginPlay", "Tick", "hardware input", "real player/controller", "Shipping/packaging",
                       "arbitrary callback side effects", "global editor click/undo state restoration"],
        "source_hashes": {str(Path(__file__).resolve()): digest(__file__),
            **{str(Path(__file__).with_name(n)): digest(Path(__file__).with_name(n)) for n in
               ("verify_function_timer_runtime.py", "verify_gated_timer_runtime.py",
                "create_component_read_fixture.py", "validate_fixture_without_mcp.py")}}}
    actual = world = actors = host_cls = component_cls = host = None
    blueprints, cdos, disks = {}, {}, {}
    before_files = before_status = before_cdos = before_dirty = before_world = before_config = before_selection = None
    world_package = None
    in_flight = False

    def path(obj): return obj.get_path_name() if obj is not None else None

    def stage(name):
        report["stage"] = name
        write_report(output, report)

    def dirty():
        return {"content": sorted(path(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()),
                "maps": sorted(path(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())}

    def files():
        return {key: {"path": str(p), "sha256": digest(p), "bytes": p.stat().st_size,
                      "mtime_ns": p.stat().st_mtime_ns} for key, p in disks.items()}

    def number(value):
        require(type(value) in (float, int) and math.isfinite(value), "Expected finite numeric value")
        return float(value)

    def boolean(obj, name):
        value = obj.get_editor_property(name)
        require(type(value) is bool, name + " must be an actual bool")
        return value

    def vector(obj, name):
        value = obj.get_editor_property(name)
        require(isinstance(value, unreal.Vector), name + " must be a Vector")
        return [number(getattr(value, axis)) for axis in ("x", "y", "z")]

    def component_values(obj):
        owner = obj.get_editor_property("OwnerActor")
        controller = obj.get_editor_property("Controller")
        require(owner is None or isinstance(owner, unreal.Actor), "OwnerActor has wrong type")
        require(controller is None or isinstance(controller, unreal.Controller), "Controller has wrong type")
        return {MEMBER: number(obj.get_editor_property(MEMBER)),
                "SampleValid": boolean(obj, "SampleValid"), "MousePressed": boolean(obj, "MousePressed"),
                "SimulationPosition": vector(obj, "SimulationPosition"),
                "OwnerActor": path(owner), "Controller": path(controller)}

    def host_values(obj):
        return {"RequestCapture": boolean(obj, "RequestCapture"), "SamplePosition": vector(obj, "SamplePosition")}

    def expected_values(timer=0.0, valid=True, mouse=True, position=None, owner=None):
        return {MEMBER: timer, "SampleValid": valid, "MousePressed": mouse,
                "SimulationPosition": [0.0, 0.0, 0.0] if position is None else position,
                "OwnerActor": owner, "Controller": None}

    def matches(actual_values, expected):
        # Vector fixture values are exactly representable; only timer sums use tolerance.
        return (actual_values.keys() == expected.keys()
                and abs(actual_values[MEMBER] - expected[MEMBER]) <= 1e-6
                and all(actual_values[k] == v for k, v in expected.items() if k != MEMBER))

    def cdo_state():
        return {"host_path": path(cdos["host"]), "host_values": host_values(cdos["host"]),
                "host_component_references": {slot: path(cdos["host"].get_editor_property(slot)) for slot in SLOTS},
                "component_path": path(cdos["component"]), "component_values": component_values(cdos["component"])}

    def world_rows():
        require(world is not None, "World has not been validated")
        # EditorActorSubsystem.get_all_level_actors excludes RF_Transient actors.
        rows = {path(a): path(a.get_class()) for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)}
        return dict(sorted(rows.items()))

    def host_paths():
        require(world is not None and host_cls is not None, "Host class/world unavailable")
        return sorted(path(a) for a in unreal.GameplayStatics.get_all_actors_of_class(world, host_cls))

    def component_state():
        attached = {slot: host.get_editor_property(slot) for slot in SLOTS}
        all_matches = list(host.get_components_by_class(component_cls))
        require(len(all_matches) == 2 and attached["Buttons"] != attached["Capture"]
                and all(obj in all_matches for obj in attached.values()),
                "Two SCS properties must identify the two distinct actual component instances")
        states = {}
        for slot, obj in attached.items():
            require(isinstance(obj, unreal.ActorComponent) and obj != cdos["component"]
                    and path(obj.get_class()) == path(component_cls) and obj.get_owner() == host,
                    slot + " class/owner/identity differs")
            states[slot] = {"path": path(obj), "class": path(obj.get_class()), "owner": path(obj.get_owner()),
                            "values": component_values(obj)}
        return attached, states

    write_report(output, report)
    try:
        require(expected_pid is None or type(expected_pid) is int and expected_pid == os.getpid() and expected_pid > 0,
                "Editor PID differs")
        actual = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
        require(Path(project_file).resolve() == PROJECT.resolve() == actual, "Exact no-MCP proof project required")
        require(re.fullmatch(r"/Game/CubelessMCPTests/StateTransfer_[0-9a-f]{32}/BP_StateHost", host_package), "Wrong host ownership")
        require(component_package == host_package.rsplit("/", 1)[0] + "/BP_StateComponent",
                "Component must be in the same owned StateTransfer fixture folder")
        report.update(project_file=str(actual), engine_version=unreal.SystemLibrary.get_engine_version())
        require(report["engine_version"].startswith("5.8.3-"), "Requires verified UE5.8.3")
        notify_never = getattr(getattr(unreal, "PropertyAccessChangeNotifyMode", None), "NEVER", None)
        require(notify_never is not None, "Public Never notification binding required; no fallback")
        report["input_notify_mode"] = str(notify_never)
        for key, package, expected in (("host", host_package, host_sha256), ("component", component_package, component_sha256)):
            require(re.fullmatch(r"[0-9a-fA-F]{64}", expected), "Concrete SHA256 required")
            disk = (actual.parent / "Content" / (package[6:] + ".uasset")).resolve()
            require(disk.is_relative_to((actual.parent / "Content").resolve()) and disk.is_file(), "Asset missing/outside project")
            require(digest(disk) == expected.lower(), "Asset differs from the authorized saved fixture")
            disks[key] = disk
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        require(editor is not None and actors is not None and editor.get_game_world() is None, "Idle editor subsystems required")
        world = editor.get_editor_world()
        require(world is not None, "Editor world required for real SCS construction")
        world_package = path(world.get_outermost())
        report["world"] = {"path": path(world), "package": world_package}
        if world_package.startswith("/Game/"):
            map_file = (actual.parent / "Content" / (world_package[6:] + ".umap")).resolve()
            require(map_file.is_relative_to((actual.parent / "Content").resolve()), "Map escaped project")
            if map_file.is_file(): disks["loaded_map"] = map_file
        before_files, before_world, before_dirty = files(), world_rows(), dirty()
        before_config = configuration_files(actual)
        before_selection = sorted(path(a) for a in actors.get_selected_level_actors())
        report.update(files_before=before_files, actors_before=before_world, dirty_before=before_dirty,
            project_config_before=before_config, selected_before=before_selection,
            saved_editor_config_before=configuration_files(actual, saved=True), plugins_before=enabled_plugins())
        require(report["plugins_before"]["passed"] and not before_dirty["content"] and not before_dirty["maps"],
                "No MCP/Toolsets or dirty assets permitted at start")
        stage("load_saved_blueprints")
        for key, package, expected_parent in (("component", component_package, "/Script/Engine.ActorComponent"),
                                               ("host", host_package, "/Script/Engine.Actor")):
            object_path = package + "." + package.rsplit("/", 1)[1]
            bp = unreal.load_asset(object_path); blueprints[key] = bp
            require(isinstance(bp, unreal.Blueprint) and path(bp) == object_path, "Blueprint identity differs")
            require(path(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)) == expected_parent, "Native parent differs")
            require(bp.get_editor_property("status") in (unreal.BlueprintStatus.BS_UP_TO_DATE,
                unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS), "Saved Blueprint is not compiled; no compile attempted")
            cls = bp.generated_class()
            require(cls is not None and path(cls) == object_path + "_C", "Wrong authoritative generated class")
            cdos[key] = unreal.get_default_object(cls)
            if key == "host": host_cls = cls
            else: component_cls = cls
        require(isinstance(cdos["host"], unreal.Actor) and isinstance(cdos["component"], unreal.ActorComponent), "CDO types differ")
        before_cdos = cdo_state(); before_status = {key: str(bp.get_editor_property("status")) for key, bp in blueprints.items()}
        report.update(cdos_before=before_cdos, blueprint_status_before=before_status, host_class=path(host_cls), component_class=path(component_cls))
        require(matches(before_cdos["component_values"], expected_values())
                and before_cdos["host_values"] == {"RequestCapture": False, "SamplePosition": [10.0, 20.0, 30.0]}
                and host_paths() == [], "Expected exact component/host CDO defaults and no existing host instance")
        require(files() == before_files and dirty() == before_dirty and world_rows() == before_world, "Loading changed baseline")
        stage("spawn_host")
        report["spawn"]["attempted"] = True; in_flight = True; write_report(output, report)
        host = actors.spawn_actor_from_class(host_cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), True)
        in_flight = False; report["spawn"]["returned"] = True
        require(isinstance(host, unreal.Actor) and path(host.get_class()) == path(host_cls) and host != cdos["host"], "Wrong spawned host")
        host_path = path(host); report["spawn"]["actor_path"] = host_path
        require(host.get_world() == world and host_path not in before_world, "Host world/identity differs")
        require(host_paths() == [host_path] and world_rows() == dict(sorted({**before_world, host_path: path(host_cls)}.items())),
                "Spawn produced unexpected world actors")
        instances, initial = component_state()
        require(all(matches(row["values"], expected_values()) for row in initial.values())
                and host_values(host) == before_cdos["host_values"] and cdo_state() == before_cdos,
                "Spawned defaults/CDOs differ")
        report.update(components_initial=initial, dirty_after_spawn=dirty(), actor_paths_after_spawn=host_paths())
        for index, (request, mouse, delta, expected_timer, expected_valid, expected_mouse, has_sample) in enumerate(CASES, 1):
            expected = expected_values(expected_timer, expected_valid, expected_mouse,
                [10.0, 20.0, 30.0] if has_sample else None, host_path if has_sample else None)
            row = {"index": index, "delta": delta, "request_capture": request, "buttons_mouse_pressed": mouse,
                   "expected_capture": expected, "host_before": host_values(host), "before": component_state()[1],
                   "input_updates": [], "attempted": False, "returned": False, "verified": False}
            report["steps"].append(row)
            for obj, name, value in ((host, "RequestCapture", request), (instances["Buttons"], "MousePressed", mouse)):
                change = {"object": path(obj), "name": name, "requested": value, "notify_mode": str(notify_never),
                          "attempted": False, "returned": False}
                row["input_updates"].append(change); stage("prepare_input_" + str(index) + "_" + name)
                change["attempted"] = True; report["input_updates_attempted"] += 1; in_flight = True
                write_report(output, report)
                obj.set_editor_property(name, value, notify_mode=notify_never)
                change["returned"] = True; report["input_updates_returned"] += 1
                observed = boolean(obj, name); change["observed"] = observed
                require(observed is value, "Fixture input setter readback differs")
            prepared_instances, prepared = component_state()
            row.update(prepared=prepared, host_prepared=host_values(host), prepared_cdos=cdo_state())
            in_flight = False  # Do not clear uncertainty before both instances and CDOs were observed.
            expected_buttons = dict(row["before"]["Buttons"]["values"], MousePressed=mouse)
            require(prepared_instances == instances and prepared["Capture"] == row["before"]["Capture"]
                    and prepared["Buttons"]["values"] == expected_buttons
                    and row["host_prepared"] == {"RequestCapture": request, "SamplePosition": [10.0, 20.0, 30.0]}
                    and row["prepared_cdos"] == before_cdos, "Input preparation changed other state/identity/CDOs")
            stage("call_state_transfer_" + str(index))
            row["attempted"] = True; report["calls_attempted"] += 1; in_flight = True; write_report(output, report)
            result = host.call_method(CALLER, (delta,), {})
            row["returned"] = True; report["calls_returned"] += 1
            observed_instances, after = component_state()
            row.update(return_type=type(result).__name__, after=after, host_after=host_values(host),
                       cdo_after=cdo_state(), dirty_after=dirty())
            in_flight = False
            row["verified"] = (result is None and observed_instances == instances
                and matches(after["Capture"]["values"], expected) and after["Buttons"] == prepared["Buttons"]
                and row["host_after"] == row["host_prepared"] and row["cdo_after"] == before_cdos)
            write_report(output, report); require(row["verified"], "State transfer/reset/retained fields or preservation differ")
            report["calls_verified"] += 1
        report.update(success=True, stage="calls_complete",
                      accumulated_seconds=report["steps"][-1]["after"]["Capture"]["values"][MEMBER])
    except Exception as error:
        report.update(error=str(error), traceback=traceback.format_exc(),
            outcome_unknown=in_flight or report["stage"] in ("load_saved_blueprints", "spawn_host"))
    finally:
        # Cleanup only the exact returned Actor, once. Never guess a lost spawn's target.
        if host is not None:
            try:
                if unreal.SystemLibrary.is_valid(host):
                    require(world is not None and host.get_world() == world and path(host.get_class()) == path(host_cls)
                            and path(host) not in (before_world or {}), "Cleanup target ownership differs")
                    report["cleanup"]["actor_path"] = path(host)
                    report["cleanup"]["attempted"] = True; write_report(output, report)
                    removed = actors.destroy_actor(host)
                    report["cleanup"].update(returned=True, return_value=removed, success=removed is True)
                    if removed is not True: report["outcome_unknown"] = True
                else:
                    report["cleanup"].update(already_invalid=True, success=False)
            except Exception as error:
                report["cleanup"]["error"] = str(error); report["outcome_unknown"] = True
        final = {}
        for key, observe in (("files", files), ("dirty", dirty), ("actors", world_rows), ("host_instances", host_paths),
            ("cdos", cdo_state), ("plugins", enabled_plugins),
            ("blueprint_status", lambda: {k: str(bp.get_editor_property("status")) for k, bp in blueprints.items()}),
            ("project_config", lambda: configuration_files(actual)),
            ("saved_editor_config", lambda: configuration_files(actual, saved=True)),
            ("selected", lambda: sorted(path(a) for a in actors.get_selected_level_actors()))):
            try: final[key] = observe()
            except Exception as error: final[key] = {"error": str(error)}
        report["final_observations"] = final
        mutations_attempted = report["spawn"]["attempted"] or report["calls_attempted"] or report["input_updates_attempted"]
        if mutations_attempted and any(isinstance(final[k], dict) and "error" in final[k] for k in ("actors", "host_instances", "cdos", "dirty", "files", "blueprint_status")):
            report["outcome_unknown"] = True
        assets_preserved = before_files is not None and final["files"] == before_files and before_cdos is not None and final["cdos"] == before_cdos and final["blueprint_status"] == before_status
        world_restored = before_world is not None and final["actors"] == before_world and final["host_instances"] == []
        final_dirty = final["dirty"]
        dirty_scope = (before_dirty is not None and isinstance(final_dirty, dict) and final_dirty.get("content") == before_dirty["content"]
                       and isinstance(final_dirty.get("maps"), list) and set(final_dirty["maps"]).issubset(set(before_dirty["maps"]) | {world_package}))
        config_preserved = before_config is not None and final["project_config"] == before_config
        report["preservation"] = {"assets_and_cdos": bool(assets_preserved), "world_actor_identities_restored": bool(world_restored),
            "dirty_within_disposable_world_only": bool(dirty_scope), "project_config": bool(config_preserved),
            "selection_unchanged": before_selection is not None and final["selected"] == before_selection,
            "dirty_unchanged": before_dirty is not None and final_dirty == before_dirty,
            "scope": "Two BP files/CDOs/status and optional loaded map file; world actor path/class set; no claim of global undo/click state restoration"}
        report["saved_editor_config_changed"] = final["saved_editor_config"] != report.get("saved_editor_config_before")
        report["success"] = bool(report["success"] and report["calls_verified"] == len(CASES) and report["cleanup"]["success"]
            and assets_preserved and world_restored and dirty_scope and config_preserved and report["preservation"]["selection_unchanged"]
            and final["plugins"].get("passed") is True and not report["outcome_unknown"])
        if report["stage"] == "calls_complete": report["stage"] = "complete" if report["success"] else "cleanup_or_preservation"
        report["finished_ns"] = time.time_ns(); write_report(output, report)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-file", required=True); parser.add_argument("--host-package", required=True)
    parser.add_argument("--host-sha256", required=True); parser.add_argument("--component-package", required=True)
    parser.add_argument("--component-sha256", required=True); parser.add_argument("--report", required=True)
    parser.add_argument("--expected-pid", type=int)
    args = parser.parse_args(argv)
    return verify(args.project_file, args.host_package, args.host_sha256, args.component_package,
                  args.component_sha256, args.report, args.expected_pid)["success"]


if __name__ == "__main__" and not main():
    raise RuntimeError("State transfer runtime proof failed; inspect the report and do not automatically retry")
