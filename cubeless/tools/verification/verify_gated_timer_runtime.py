"""Run six gated timer cases on one owned transient component in a no-MCP editor.

Root owns the disposable editor and copied fixture. No compile, save, world,
registration, process control or automatic retry is performed by this script.
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
from verify_function_timer_runtime import PROJECT, RECORDS, PARENT, require, digest, write_report

FUNCTION = "CFNL_GatedAdvance"
MEMBER = "TimeSinceLastClick"
GATES = ("FieldResourcesReady", "PawnInside", "VisibilityInputAllowed")
DEFAULTS = {**dict.fromkeys(GATES, False), "LastDelta": 0.0, MEMBER: 0.0}
CASES = (((False, True, True), .25, 0.0), ((True, False, True), .5, 0.0),
         ((True, True, False), 1.25, 0.0), ((True, True, True), .25, .25),
         ((True, True, True), .5, .75), ((False, True, True), 1.25, .75))
_KEEP_ALIVE = []


def configuration_files(project, saved=False):
    """Project configuration is protected; editor bookkeeping is observed separately."""
    folder = project.parent / ("Saved/Config/WindowsEditor" if saved else "Config")
    files = sorted(folder.rglob("*.ini")) if folder.is_dir() else []
    if not saved:
        files.insert(0, project)
    return {str(p.relative_to(project.parent)): digest(p) for p in files}


def verify(project_file, package, expected_sha256, report_path, expected_pid=None):
    import unreal
    from validate_fixture_without_mcp import enabled_plugins

    output = Path(report_path).resolve()
    require(output.parent == RECORDS.resolve() and output.suffix == ".json" and not output.exists(),
            "Use a new JSON report in the fixed verification directory")
    report = {"schema": "cubeless_gated_timer_runtime_v1", "success": False, "stage": "preflight",
        "started_ns": time.time_ns(), "process_id": os.getpid(), "expected_process_id": expected_pid,
        "package": package, "function": FUNCTION, "expected_asset_sha256": expected_sha256,
        "expected_cdo": DEFAULTS, "steps": [], "gate_updates_attempted": 0,
        "calls_attempted": 0, "calls_returned": 0, "calls_verified": 0,
        "automatic_retries": 0, "atomic_rollback": False, "outcome_unknown": False,
        "compile": {"explicit_calls": 0, "implicit_load_compilation": "not_measured"},
        "save": {"explicit_calls": 0}, "settings_writes": 0,
        "settings_scope": "Project descriptor/Config ini bytes protected; Saved editor ini observed only; no unverified global settings getter",
        "scope": "Own gated caller on an ownerless transient ActorComponent: always-path LastDelta plus three-condition timer gate",
        "lifetime_policy": "Probe reference retained until the disposable process exits",
        "unverified": ["inherited Tick", "external component call", "original host", "press/release reset",
                       "hardware input", "automatic EndPlay", "Shipping/packaging", "arbitrary callback side effects"],
        "source_hashes": {str(Path(__file__)): digest(__file__),
            **{str(Path(__file__).with_name(name)): digest(Path(__file__).with_name(name)) for name in
               ("verify_function_timer_runtime.py", "create_component_read_fixture.py", "validate_fixture_without_mcp.py")}}}
    bp = cls = cdo = probe = disk = actual = None
    before_disk = before_status = before_dirty = before_config = before_cdo = None
    in_flight = False

    def stage(name):
        report["stage"] = name
        write_report(output, report)

    def path(value):
        return value.get_path_name() if value is not None else ""

    def dirty():
        return sorted({path(p) for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
                       + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())})

    def values(obj):
        result = {name: obj.get_editor_property(name) for name in DEFAULTS}
        require(all(type(result[name]) is bool for name in GATES), "Gate property is not an actual bool")
        require(all(type(result[name]) in (int, float) and math.isfinite(result[name])
                    for name in ("LastDelta", MEMBER)), "Timer/delta property is not a finite number")
        return result

    def file_state():
        return {"sha256": digest(disk), "bytes": disk.stat().st_size, "mtime_ns": disk.stat().st_mtime_ns} if disk is not None and disk.is_file() else None

    write_report(output, report)
    try:
        require(expected_pid is None or type(expected_pid) is int and expected_pid > 0 and expected_pid == os.getpid(), "Editor PID differs")
        actual = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
        require(Path(project_file).resolve() == PROJECT.resolve() == actual, "Only the exact no-MCP proof project is permitted")
        report["project_file"] = str(actual)
        report["engine_version"] = unreal.SystemLibrary.get_engine_version()
        require(str(report["engine_version"]).startswith("5.8.3-"), "Requires the verified UE5.8.3 engine")
        require(isinstance(package, str) and re.fullmatch(r"/Game/CubelessMCPTests/GatedTimer_[0-9a-f]{32}/BP_GatedTimer", package), "Wrong owned W fixture path")
        require(isinstance(expected_sha256, str) and re.fullmatch(r"[0-9a-fA-F]{64}", expected_sha256), "Expected asset SHA required")
        disk = (actual.parent / "Content" / (package[6:] + ".uasset")).resolve()
        require(disk.is_relative_to((actual.parent / "Content").resolve()) and disk.is_file(), "Asset is missing/outside the proof project")
        before_disk = file_state(); report["asset_file_before"] = before_disk
        require(before_disk["sha256"] == expected_sha256.lower(), "Asset differs from the authorized saved fixture")
        before_config = configuration_files(actual)
        report.update(project_config_before=before_config, saved_editor_config_before=configuration_files(actual, saved=True))
        report["plugins_before"] = enabled_plugins()
        require(report["plugins_before"]["passed"], "MCP/Toolsets must not be enabled")
        require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, "PIE/SIE must not be active")
        before_dirty = dirty(); report["dirty_before"] = before_dirty
        require(not before_dirty, "Start with no dirty assets")
        object_path = package + ".BP_GatedTimer"
        stage("load_blueprint")
        bp = unreal.load_asset(object_path)
        require(isinstance(bp, unreal.Blueprint) and path(bp) == object_path, "Wrong Blueprint identity")
        require(path(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)) == PARENT, "Exact native ActorComponent parent required")
        before_status = bp.get_editor_property("status")
        require(before_status in (unreal.BlueprintStatus.BS_UP_TO_DATE, unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS), "Saved Blueprint not compiled; verifier will not compile it")
        cls = bp.generated_class()
        require(cls is not None and path(cls) == object_path + "_C", "Wrong authoritative generated class")
        cdo = unreal.get_default_object(cls)
        require(isinstance(cdo, unreal.ActorComponent), "Wrong CDO type")
        before_cdo = values(cdo)
        require(before_cdo == DEFAULTS, "Saved CDO defaults differ")
        report["blueprint"] = {"object_path": object_path, "parent_class": PARENT, "generated_class": path(cls),
            "status_before": str(before_status), "cdo_path": path(cdo), "cdo_values_before": before_cdo}
        require(dirty() == before_dirty and file_state() == before_disk, "Loading changed dirty state or file")
        stage("create_probe")
        probe = unreal.new_object(cls, base_type=unreal.ActorComponent); _KEEP_ALIVE.append(probe)
        require(isinstance(probe, unreal.ActorComponent) and path(probe.get_class()) == path(cls) and probe != cdo,
                "Wrong separate probe instance")
        require(path(probe.get_outer()) == path(probe.get_outermost()) == "/Engine/Transient" and probe.get_owner() is None,
                "Probe must be ownerless in the transient package")
        require(values(probe) == DEFAULTS, "Fresh probe defaults differ")
        report["probe"] = {"path": path(probe), "class": path(cls), "outer": path(probe.get_outer()), "owner": "",
            "explicit_registration_calls": 0, "construction_flags_from_source": "RF_Transactional; RF_Transient flag not asserted"}
        for index, (gates, delta, expected_timer) in enumerate(CASES, 1):
            requested = dict(zip(GATES, gates))
            row = {"index": index, "gates": requested, "delta": delta, "expected_timer": expected_timer,
                   "before": values(probe), "gate_updates": [], "attempted": False, "returned": False, "verified": False}
            report["steps"].append(row)
            for name, value in requested.items():
                change = {"name": name, "requested": value, "attempted": False, "returned": False}
                row["gate_updates"].append(change); stage("set_gate_" + str(index) + "_" + name)
                in_flight = True; change["attempted"] = True; report["gate_updates_attempted"] += 1
                write_report(output, report)
                probe.set_editor_property(name, value)
                in_flight = False; change["returned"] = True
            row["prepared"] = values(probe)
            require(all(row["prepared"][name] is value for name, value in requested.items()), "Transient gate setter readback differs")
            stage("call_gated_timer_" + str(index))
            in_flight = True; row["attempted"] = True; report["calls_attempted"] += 1
            write_report(output, report)
            returned = probe.call_method(FUNCTION, (delta,), {})
            in_flight = False; row["returned"] = True; report["calls_returned"] += 1
            row.update(return_type=type(returned).__name__, after=values(probe), cdo_after=values(cdo))
            row["verified"] = (returned is None and abs(row["after"]["LastDelta"] - delta) <= 1e-6
                and abs(row["after"][MEMBER] - expected_timer) <= 1e-6 and row["cdo_after"] == before_cdo
                and all(row["after"][name] is value for name, value in requested.items()))
            write_report(output, report)
            require(row["verified"], "Always-path delta, conditional accumulation, gate or CDO preservation failed")
            report["calls_verified"] += 1
        report.update(success=True, stage="complete", accumulated_seconds=values(probe)[MEMBER])
    except Exception as error:
        report.update(error=str(error), traceback=traceback.format_exc(),
                      outcome_unknown=in_flight or report["stage"] in ("load_blueprint", "create_probe"))
    finally:
        observed = {}
        for label, read in (("dirty_final", dirty), ("plugins_after", enabled_plugins), ("asset_file_after", file_state),
            ("cdo_values_after", lambda: values(cdo) if cdo is not None else None),
            ("probe_values_after", lambda: values(probe) if probe is not None else None),
            ("blueprint_status_after", lambda: str(bp.get_editor_property("status")) if bp is not None else None),
            ("project_config_after", lambda: configuration_files(actual) if actual is not None else None),
            ("saved_editor_config_after", lambda: configuration_files(actual, saved=True) if actual is not None else None)):
            try: observed[label] = read()
            except Exception as error: observed[label] = {"error": str(error)}
        report["final_observations"] = observed
        report["saved_editor_config_changed"] = observed["saved_editor_config_after"] != report.get("saved_editor_config_before")
        if (report["gate_updates_attempted"] or report["calls_attempted"]) and any(
                not isinstance(observed[key], dict) or set(observed[key]) != set(DEFAULTS)
                for key in ("probe_values_after", "cdo_values_after")):
            report["outcome_unknown"] = True
        preserved = (before_disk is not None and observed["asset_file_after"] == before_disk
            and before_dirty is not None and observed["dirty_final"] == before_dirty
            and before_cdo is not None and observed["cdo_values_after"] == before_cdo
            and before_status is not None and observed["blueprint_status_after"] == str(before_status)
            and before_config is not None and observed["project_config_after"] == before_config
            and isinstance(observed["plugins_after"], dict) and observed["plugins_after"].get("passed") is True)
        report["preservation"] = {"passed": bool(preserved), "scope": "Target SHA/mtime, CDO fields/status, dirty packages, enabled plugins and project configuration; Saved editor bookkeeping observed separately"}
        report["success"] = bool(report["success"] and preserved and report["calls_verified"] == len(CASES) and not report["outcome_unknown"])
        if not report["success"] and report["stage"] == "complete": report["stage"] = "final_preservation"
        report["finished_ns"] = time.time_ns(); write_report(output, report)
    return report


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--project-file", required=True); p.add_argument("--package", required=True)
    p.add_argument("--expected-asset-sha256", required=True); p.add_argument("--report", required=True)
    p.add_argument("--expected-pid", type=int)
    a = p.parse_args(argv)
    return verify(a.project_file, a.package, a.expected_asset_sha256, a.report, a.expected_pid)["success"]


if __name__ == "__main__" and not main():
    raise RuntimeError("Gated timer runtime proof failed; inspect the report and do not automatically retry")
