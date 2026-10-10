"""Editor-internal timer execution proof for one owned, hash-pinned test Blueprint.

Root runs this only in the disposable no-MCP UE58RuntimeProof editor. No explicit
compile/save, world, actor spawn, registration, global GC, or process control.
The report is authoritative; a successful editor exit alone is not proof.
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
from create_component_read_fixture import digest, write_report

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "cubeless/workspaces/UE58RuntimeProof/CubelessRuntimeProof.uproject"
RECORDS = ROOT / "cubeless/records/verification/implementation-2026-10-10"
FUNCTION = "CFNL_AdvanceMouseTime"
MEMBER = "TimeSinceLastClick"
PARENT = "/Script/Engine.ActorComponent"
_KEEP_ALIVE = []  # Keep the sole probe alive until the disposable process ends.


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def verify(project_file, package, expected_sha256, report_path, expected_pid=None):
    """Single execution. expected_pid=None is for a fresh externally tracked PID."""
    import unreal
    from validate_fixture_without_mcp import enabled_plugins

    output = Path(report_path).resolve()
    require(output.parent == RECORDS.resolve() and output.suffix == ".json", "Use a new JSON report in the fixed verification directory")
    require(not output.exists(), "Existing evidence will not be overwritten or resumed")
    report = {
        "schema": "cubeless_function_timer_runtime_v1", "success": False,
        "stage": "preflight", "started_ns": time.time_ns(), "process_id": os.getpid(),
        "expected_process_id": expected_pid, "package": package, "function": FUNCTION, "member": MEMBER,
        "expected_asset_sha256": expected_sha256, "calls_attempted": 0, "calls_returned": 0,
        "calls_verified": 0, "steps": [], "automatic_retries": 0, "atomic_rollback": False,
        "outcome_unknown": False, "compile": {"explicit_calls": 0, "implicit_load_compilation": "not_measured"},
        "save": {"explicit_calls": 0}, "lifetime_policy": "probe reference retained; disposable process exit owns final cleanup",
        "scope": "Direct execution of three timer calls on one transient-package, ownerless ActorComponent instance",
        "unverified": ["press/reset/release", "hardware input", "caller gating", "Tick integration", "automatic EndPlay", "cooked/Shipping execution", "packaging", "arbitrary callback side effects"],
        "source_hashes": {str(Path(__file__).resolve()): digest(__file__),
                          str(Path(__file__).with_name("create_component_read_fixture.py")): digest(Path(__file__).with_name("create_component_read_fixture.py")),
                          str(Path(__file__).with_name("validate_fixture_without_mcp.py")): digest(Path(__file__).with_name("validate_fixture_without_mcp.py"))},
    }
    bp = cls = cdo = probe = None
    disk = None
    before_disk = None
    before_cdo = None
    before_status = None
    before_dirty = None
    in_flight = False

    def stage(name):
        report["stage"] = name
        write_report(output, report)

    def path(value):
        return value.get_path_name() if value is not None else ""

    def dirty():
        return sorted({path(p) for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
                       + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())})

    def value(obj):
        result = obj.get_editor_property(MEMBER)
        require(type(result) in (int, float) and math.isfinite(result), "Timer member is not a finite numeric property")
        return float(result)

    write_report(output, report)
    try:
        require(expected_pid is None or type(expected_pid) is int and expected_pid > 0 and expected_pid == os.getpid(), "Editor PID differs")
        actual = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
        require(Path(project_file).resolve() == PROJECT.resolve() == actual, "Only the exact isolated UE58RuntimeProof project is permitted")
        report["project_file"] = str(actual)
        report["engine_version"] = unreal.SystemLibrary.get_engine_version()
        require(str(report["engine_version"]).startswith("5.8.3"), "This verifier has only been designed for UE 5.8.3")
        require(isinstance(package, str) and len(package) < 512 and re.fullmatch(r"/Game/CubelessMCPTests/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)+", package), "Use an exact owned test package below /Game/CubelessMCPTests")
        require(isinstance(expected_sha256, str) and re.fullmatch(r"[0-9a-fA-F]{64}", expected_sha256), "A concrete expected asset SHA256 is required")
        disk = (actual.parent / "Content" / (package[6:] + ".uasset")).resolve()
        require(disk.is_relative_to((actual.parent / "Content").resolve()) and disk.is_file(), "Test asset file is missing/outside this project")
        before_disk = digest(disk)
        report["asset_file"] = {"path": str(disk), "sha256_before": before_disk}
        require(before_disk == expected_sha256.lower(), "Target asset bytes do not match the authorized saved fixture")
        report["plugins_before"] = enabled_plugins()
        require(report["plugins_before"]["passed"], "MCP/Toolset plugins must not be enabled in this proof process")
        require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, "PIE/SIE game world must not be active")
        before_dirty = dirty()
        report["dirty_before"] = before_dirty
        require(not before_dirty, "The disposable proof editor must start with no dirty assets")
        object_path = package + "." + package.rsplit("/", 1)[1]
        stage("load_blueprint")
        bp = unreal.load_asset(object_path)
        require(isinstance(bp, unreal.Blueprint) and path(bp) == object_path, "Wrong Blueprint identity")
        parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)
        require(path(parent) == PARENT, "Requires an exact native Engine.ActorComponent parent")
        before_status = bp.get_editor_property("status")
        require(before_status in (unreal.BlueprintStatus.BS_UP_TO_DATE, unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS), "Saved Blueprint is not compiled/up to date; verifier will not compile it")
        cls = bp.generated_class()
        require(cls is not None and path(cls) == object_path + "_C", "Missing or unexpected authoritative generated class")
        cdo = unreal.get_default_object(cls)
        require(cdo is not None and isinstance(cdo, unreal.ActorComponent), "Wrong generated CDO type")
        before_cdo = value(cdo)
        report["blueprint"] = {"object_path": object_path, "parent_class": path(parent), "generated_class": path(cls), "status_before": str(before_status), "cdo_path": path(cdo), "cdo_value_before": before_cdo}
        require(before_cdo == 0.0, "Expected saved fixture CDO timer default 0")
        report["dirty_after_load"] = dirty()
        require(report["dirty_after_load"] == before_dirty and digest(disk) == before_disk, "Asset load/CDO access changed dirty state or bytes")
        stage("create_probe")
        probe = unreal.new_object(cls, base_type=unreal.ActorComponent)
        _KEEP_ALIVE.append(probe)
        require(isinstance(probe, unreal.ActorComponent) and path(probe.get_class()) == path(cls), "Wrong probe type")
        require(probe != cdo and path(probe.get_outer()) == "/Engine/Transient" and path(probe.get_outermost()) == "/Engine/Transient", "Probe must be a separate transient-package instance")
        require(probe.get_owner() is None, "Probe unexpectedly has an Actor owner")
        initial = value(probe)
        report["probe"] = {"path": path(probe), "class": path(probe.get_class()), "outer": path(probe.get_outer()), "owner": path(probe.get_owner()), "initial_value": initial,
                           "creation_api": "unreal.new_object(cls, base_type=unreal.ActorComponent)", "construction_flags_from_source": "RF_Transactional; transient package outer (RF_Transient flag is not asserted)", "explicit_registration_calls": 0}
        require(initial == 0.0, "Fresh probe does not inherit the expected 0 timer default")
        for index, (delta, expected) in enumerate(((0.25, 0.25), (0.5, 0.75), (1.25, 2.0)), 1):
            row = {"index": index, "delta": delta, "expected_after": expected, "before": value(probe), "attempted": False, "returned": False, "verified": False}
            report["steps"].append(row)
            stage("call_timer_" + str(index))  # Durable intent precedes the call.
            in_flight = True
            row["attempted"] = True
            report["calls_attempted"] += 1
            write_report(output, report)
            returned = probe.call_method(FUNCTION, (delta,), {})
            in_flight = False
            row["returned"] = True
            row["return_type"] = type(returned).__name__
            report["calls_returned"] += 1
            row["after"] = value(probe)
            row["cdo_after"] = value(cdo)
            row["verified"] = returned is None and abs(row["after"] - expected) <= 1e-6 and row["cdo_after"] == before_cdo
            write_report(output, report)
            require(row["verified"], "Timer accumulation or CDO preservation failed")
            report["calls_verified"] += 1
        report["accumulated_seconds"] = value(probe)
        report["success"] = True
        report["stage"] = "complete"
    except Exception as error:
        report.update(error=str(error), traceback=traceback.format_exc(),
                      outcome_unknown=in_flight or report["stage"] in ("load_blueprint", "create_probe"))
    finally:
        observations = {}
        for label, read in (("dirty_final", dirty), ("plugins_after", enabled_plugins),
                            ("asset_sha256_after", lambda: digest(disk) if disk is not None and disk.is_file() else None),
                            ("cdo_value_after", lambda: value(cdo) if cdo is not None else None),
                            ("probe_value_after", lambda: value(probe) if probe is not None else None),
                            ("blueprint_status_after", lambda: str(bp.get_editor_property("status")) if bp is not None else None)):
            try:
                observations[label] = read()
            except Exception as error:
                observations[label] = {"error": str(error)}
        report["final_observations"] = observations
        if report["calls_attempted"] and (type(observations["probe_value_after"]) not in (int, float)
                                          or type(observations["cdo_value_after"]) not in (int, float)):
            report["outcome_unknown"] = True
        preserved = before_disk is not None and observations["asset_sha256_after"] == before_disk \
            and before_dirty is not None and observations["dirty_final"] == before_dirty \
            and before_cdo is not None and observations["cdo_value_after"] == before_cdo \
            and before_status is not None and observations["blueprint_status_after"] == str(before_status) \
            and isinstance(observations["plugins_after"], dict) and observations["plugins_after"].get("passed") is True
        report["preservation"] = {"passed": bool(preserved), "scope": "target file hash, generated CDO timer, Blueprint status, all observed dirty packages, enabled plugins"}
        report["success"] = report["success"] and preserved and report["calls_verified"] == 3
        if not report["success"] and report["stage"] == "complete":
            report["stage"] = "final_preservation"
        report["finished_ns"] = time.time_ns()
        write_report(output, report)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-file", required=True)
    parser.add_argument("--package", required=True)
    parser.add_argument("--expected-asset-sha256", required=True)
    parser.add_argument("--report", required=True)
    parser.add_argument("--expected-pid", type=int)
    args = parser.parse_args(argv)
    return verify(args.project_file, args.package, args.expected_asset_sha256, args.report, args.expected_pid)["success"]


if __name__ == "__main__" and not main():
    raise RuntimeError("Timer runtime proof failed; inspect the persisted report and do not automatically retry")
