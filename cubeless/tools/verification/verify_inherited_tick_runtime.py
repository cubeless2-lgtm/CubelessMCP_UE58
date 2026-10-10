"""Invoke inherited ReceiveTick six times on one owned host in a no-MCP editor.

One transient-requested Actor is spawned and removed once. Gates change only on
that instance. Direct reflected event invocation is not scheduled game-world
Tick/BeginPlay. No compile/save, forced dirty clear or uncertain retry.
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
from verify_gated_timer_runtime import configuration_files, GATES, CASES

COMPONENT_PACKAGE = "/Game/CubelessMCPTests/FunctionTimer_ac30351e04c74596887ffe9b9146f4dd/BP_FunctionTimer"
CALLER, MEMBER, SLOT = "ReceiveTick", "TimeSinceLastClick", "MouseButtons"
PARENT_MARKER, CHILD_MARKER = "ParentDelta", "ChildDelta"
# Installed UE5.8.3: Actor.h:2179 declares BlueprintImplementableEvent ReceiveTick.
# PyWrapperObject.cpp:1051-1109 resolves FindFunctionByName without requiring
# Python glue/BlueprintCallable; PyUtil.cpp:606-648 guards the game thread and
# invokes ProcessEvent. This bypasses AActor::Tick and its scheduling conditions.
# PropertyAccessUtil.h:50-57 exposes the BlueprintType notification enum;
# PyGenUtil.cpp:2892/3160 strips its E prefix and uppercases the Never entry.
# PyWrapperObject.cpp:852-879 accepts notify_mode. Never avoids ActorEditor.cpp
# PostEditChangeProperty reconstructing SCS objects during runtime-input setup.


def verify(project_file, parent_package, parent_sha256, host_package, host_sha256,
           component_package, component_sha256, report_path, expected_pid=None):
    import unreal
    from validate_fixture_without_mcp import enabled_plugins

    output = Path(report_path).resolve()
    require(output.parent == RECORDS.resolve() and output.suffix == ".json" and not output.exists(),
            "Use a new JSON report in the fixed verification directory")
    report = {"schema": "cubeless_inherited_tick_runtime_v1", "success": False, "stage": "preflight",
        "started_ns": time.time_ns(), "process_id": os.getpid(), "expected_process_id": expected_pid,
        "parent_package": parent_package, "host_package": host_package, "component_package": component_package, "function": CALLER,
        "expected_sha256": {"parent": parent_sha256, "host": host_sha256, "component": component_sha256},
        "steps": [], "spawn": {"attempted": False, "returned": False, "transient_requested": True},
        "cleanup": {"attempted": False, "returned": False, "success": False},
        "gate_updates_attempted": 0, "gate_updates_returned": 0,
        "calls_attempted": 0, "calls_returned": 0, "calls_verified": 0,
        "automatic_retries": 0, "atomic_rollback": False, "outcome_unknown": False,
        "compile": {"explicit_calls": 0, "implicit_load_compilation": "not_measured"},
        "save": {"explicit_calls": 0}, "settings_writes": 0, "dirty_flag_clears": 0,
        "scope": "Fresh parent/child/component load; direct reflected ReceiveTick calls verify inherited parent marker, child marker and gated component timer, then owned Actor removal",
        "invocation_scope": "FindFunctionByName/ProcessEvent on the instance; bypasses Actor::Tick scheduler/BeginPlay and does not prove automatic world ticking",
        "gate_preparation_scope": "Owned transient-requested Actor runtime inputs only, with explicit Never property notification; no editor PostEdit/construction rerun is requested by gate writes. Not an editor asset setter equivalence test.",
        "side_effect_scope": "Spawn runs editor placement/construction/registration; destroy runs lifecycle/editor callbacks. Current disposable map may remain dirty; it is never saved or force-cleared.",
        "unverified": ["BeginPlay", "automatic game-world Tick", "hardware input", "press/release reset", "Shipping/packaging",
                       "arbitrary callback side effects", "global editor click/undo state restoration"],
        "source_hashes": {str(Path(__file__).resolve()): digest(__file__),
            **{str(Path(__file__).with_name(n)): digest(Path(__file__).with_name(n)) for n in
               ("verify_function_timer_runtime.py", "verify_gated_timer_runtime.py",
                "create_component_read_fixture.py", "validate_fixture_without_mcp.py",
                "verify_component_host_runtime.py")}}}
    actual = world = actors = parent_cls = host_cls = component_cls = host = component = None
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

    def timer(obj):
        value = obj.get_editor_property(MEMBER)
        require(type(value) in (float, int) and math.isfinite(value), "Timer is not a finite numeric value")
        return float(value)

    def actor_values(obj, child=False):
        keys = (*GATES, PARENT_MARKER, *((CHILD_MARKER,) if child else ()))
        values = {key: obj.get_editor_property(key) for key in keys}
        require(all(type(values[k]) is bool for k in GATES), "Inherited gates must be actual bools")
        require(all(type(values[k]) in (int, float) and math.isfinite(values[k])
                    for k in keys if k not in GATES), "Parent/child marker must be finite numeric values")
        return values

    def cdo_state():
        return {"parent_path": path(cdos["parent"]), "parent_values": actor_values(cdos["parent"]),
                "host_path": path(cdos["host"]), "host_values": actor_values(cdos["host"], child=True),
                "host_component_reference": path(cdos["host"].get_editor_property(SLOT)),
                "component_path": path(cdos["component"]), "component_timer": timer(cdos["component"])}

    def world_rows():
        require(world is not None, "World has not been validated")
        # EditorActorSubsystem.get_all_level_actors excludes RF_Transient actors.
        rows = {path(a): path(a.get_class()) for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)}
        return dict(sorted(rows.items()))

    def host_paths():
        require(world is not None and host_cls is not None, "Host class/world unavailable")
        return sorted(path(a) for a in unreal.GameplayStatics.get_all_actors_of_class(world, host_cls))

    def component_state():
        attached = host.get_editor_property(SLOT)
        matches = list(host.get_components_by_class(component_cls))
        require(len(matches) == 1 and attached == matches[0] and isinstance(attached, unreal.ActorComponent),
                "MouseButtons property and actual owned component list differ")
        require(path(attached.get_class()) == path(component_cls) and attached.get_owner() == host,
                "Component class/owner differs")
        return attached, {"path": path(attached), "class": path(attached.get_class()), "owner": path(attached.get_owner()),
                          "timer": timer(attached), "matching_component_count": len(matches)}

    write_report(output, report)
    try:
        require(expected_pid is None or type(expected_pid) is int and expected_pid == os.getpid() and expected_pid > 0,
                "Editor PID differs")
        actual = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
        require(Path(project_file).resolve() == PROJECT.resolve() == actual, "Exact no-MCP proof project required")
        require(re.fullmatch(r"/Game/CubelessMCPTests/InheritedTick_[0-9a-f]{32}/BP_TickHost", host_package), "Wrong host ownership")
        require(parent_package == host_package.rsplit("/", 1)[0] + "/BP_TickParent", "Parent must be in the same owned fixture folder")
        require(component_package == COMPONENT_PACKAGE, "Only the approved V component fixture is permitted")
        report.update(project_file=str(actual), engine_version=unreal.SystemLibrary.get_engine_version())
        require(report["engine_version"].startswith("5.8.3-"), "Requires verified UE5.8.3")
        notify_never = getattr(getattr(unreal, "PropertyAccessChangeNotifyMode", None), "NEVER", None)
        require(notify_never is not None, "Public PropertyAccessChangeNotifyMode.NEVER binding required; no fallback")
        report["gate_notify_mode"] = str(notify_never)
        for key, package, expected in (("parent", parent_package, parent_sha256), ("host", host_package, host_sha256),
                                      ("component", component_package, component_sha256)):
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
                ("parent", parent_package, "/Script/Engine.Actor"),
                ("host", host_package, parent_package + ".BP_TickParent_C")):
            object_path = package + "." + package.rsplit("/", 1)[1]
            bp = unreal.load_asset(object_path); blueprints[key] = bp
            require(isinstance(bp, unreal.Blueprint) and path(bp) == object_path, "Blueprint identity differs")
            require(path(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)) == expected_parent, "Exact direct parent differs")
            require(bp.get_editor_property("status") in (unreal.BlueprintStatus.BS_UP_TO_DATE,
                unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS), "Saved Blueprint is not compiled; no compile attempted")
            cls = bp.generated_class()
            require(cls is not None and path(cls) == object_path + "_C", "Wrong authoritative generated class")
            cdos[key] = unreal.get_default_object(cls)
            if key == "host": host_cls = cls
            elif key == "parent": parent_cls = cls
            else: component_cls = cls
        require(isinstance(cdos["parent"], unreal.Actor) and isinstance(cdos["host"], unreal.Actor)
                and isinstance(cdos["component"], unreal.ActorComponent), "CDO types differ")
        before_cdos = cdo_state(); before_status = {key: str(bp.get_editor_property("status")) for key, bp in blueprints.items()}
        report.update(cdos_before=before_cdos, blueprint_status_before=before_status, parent_class=path(parent_cls), host_class=path(host_cls), component_class=path(component_cls))
        parent_defaults = {**dict.fromkeys(GATES, False), PARENT_MARKER: 0.0}
        child_defaults = {**parent_defaults, CHILD_MARKER: 0.0}
        require(before_cdos["parent_values"] == parent_defaults and before_cdos["host_values"] == child_defaults
                and before_cdos["component_timer"] == 0.0 and host_paths() == [], "Expected False/0 CDOs and no existing host instance")
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
        component, initial = component_state()
        require(initial["timer"] == 0.0 and actor_values(host, child=True) == child_defaults
                and cdo_state() == before_cdos, "Spawned defaults/CDOs differ")
        report.update(component_initial=initial, dirty_after_spawn=dirty(), actor_paths_after_spawn=host_paths())
        for index, (gates, delta, expected) in enumerate(CASES, 1):
            requested = dict(zip(GATES, gates))
            row = {"index": index, "gates": requested, "delta": delta, "expected_timer": expected,
                   "host_before": actor_values(host, child=True), "before": component_state()[1],
                   "gate_updates": [], "attempted": False, "returned": False, "verified": False}
            report["steps"].append(row)
            for name, value in requested.items():
                change = {"name": name, "requested": value, "notify_mode": str(notify_never),
                          "attempted": False, "returned": False}
                row["gate_updates"].append(change); stage("set_gate_" + str(index) + "_" + name)
                change["attempted"] = True; report["gate_updates_attempted"] += 1; in_flight = True
                write_report(output, report)
                host.set_editor_property(name, value, notify_mode=notify_never)
                change["returned"] = True; report["gate_updates_returned"] += 1
                actual_gate = host.get_editor_property(name)
                change["observed"] = actual_gate
                require(type(actual_gate) is bool and actual_gate is value, "Inherited gate setter readback differs")
            row["prepared"] = actor_values(host, child=True)
            prepared_component, prepared = component_state()
            row["prepared_component"] = prepared
            row["prepared_cdos"] = cdo_state()
            in_flight = False  # Preserve uncertainty until all affected instance values were observed.
            require(all(row["prepared"][k] is value for k, value in requested.items())
                    and all(row["prepared"][k] == row["host_before"][k] for k in (PARENT_MARKER, CHILD_MARKER))
                    and prepared_component == component and prepared == row["before"]
                    and row["prepared_cdos"] == before_cdos, "Gate preparation changed timer/markers/CDOs unexpectedly")
            stage("invoke_receive_tick_" + str(index))
            row["attempted"] = True; report["calls_attempted"] += 1; in_flight = True; write_report(output, report)
            result = host.call_method(CALLER, (delta,), {})
            row["returned"] = True; report["calls_returned"] += 1
            observed_component, after = component_state()
            row.update(return_type=type(result).__name__, host_after=actor_values(host, child=True),
                       after=after, cdo_after=cdo_state(), dirty_after=dirty())
            in_flight = False  # Returned call alone does not establish its actual state.
            row["verified"] = (result is None and observed_component == component
                and abs(after["timer"] - expected) <= 1e-6 and row["cdo_after"] == before_cdos
                and all(abs(row["host_after"][k] - delta) <= 1e-6 for k in (PARENT_MARKER, CHILD_MARKER))
                and all(row["host_after"][k] is value for k, value in requested.items()))
            write_report(output, report); require(row["verified"], "Parent/child event markers or gated component timer differ")
            report["calls_verified"] += 1
        report.update(success=True, stage="calls_complete", accumulated_seconds=report["steps"][-1]["after"]["timer"])
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
        mutations_attempted = report["spawn"]["attempted"] or report["calls_attempted"]
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
            "scope": "Three BP files/CDOs/status and optional loaded map file; world actor path/class set; no claim of global undo/click state restoration"}
        report["saved_editor_config_changed"] = final["saved_editor_config"] != report.get("saved_editor_config_before")
        report["success"] = bool(report["success"] and report["calls_verified"] == len(CASES) and report["cleanup"]["success"]
            and assets_preserved and world_restored and dirty_scope and config_preserved and report["preservation"]["selection_unchanged"]
            and final["plugins"].get("passed") is True and not report["outcome_unknown"])
        if report["stage"] == "calls_complete": report["stage"] = "complete" if report["success"] else "cleanup_or_preservation"
        report["finished_ns"] = time.time_ns(); write_report(output, report)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-file", required=True)
    parser.add_argument("--parent-package", required=True); parser.add_argument("--parent-sha256", required=True)
    parser.add_argument("--host-package", required=True)
    parser.add_argument("--host-sha256", required=True); parser.add_argument("--component-package", required=True)
    parser.add_argument("--component-sha256", required=True); parser.add_argument("--report", required=True)
    parser.add_argument("--expected-pid", type=int)
    args = parser.parse_args(argv)
    return verify(args.project_file, args.parent_package, args.parent_sha256, args.host_package, args.host_sha256,
                  args.component_package, args.component_sha256, args.report, args.expected_pid)["success"]


if __name__ == "__main__" and not main():
    raise RuntimeError("Inherited Tick runtime proof failed; inspect the report and do not automatically retry")
