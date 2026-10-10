"""Scan a staged Win64 game and directly list every pak, without running the game.

Exit 0 means the requested scan scope passed, not that packaging independence or
the full migration is proven. A static-only success is named static_pass. Optional
runtime corroboration requires a fixture marker and the runner's actual exit code.
All evidence must be written outside the staged directory.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

FORBIDDEN_MODULES = (
    "CubelessMCPUE58Editor", "ModelContextProtocol", "ModelContextProtocolEngine",
    "ModelContextProtocolEditor", "ToolsetRegistry", "PythonScriptPlugin",
    "PythonScriptPluginPreload", "EditorScriptingUtilities", "UnrealMCP", "CubelessControlRigTools",
)
FORBIDDEN_PLUGINS = ("CubelessMCP_UE58", "ModelContextProtocol", "ToolsetRegistry",
                     "PythonScriptPlugin", "EditorScriptingUtilities", "UnrealMCP", "CubelessControlRigTools")
# UE 5.8 PakFileUtilities.cpp:3456/3463. Parse all extensions to avoid hiding files.
_PAK_ENTRY = re.compile(r'"([^"\r\n]+)"\s+offset:\s*\d+,\s*size:\s*\d+\s+bytes', re.I)
_PAK_TOTAL = re.compile(r"\b(\d+) files \(\d+ bytes\), \(\d+ filtered bytes\)\.")
_RUNTIME_ERROR = re.compile(r"Fatal error:|Assertion failed:|Unhandled Exception:|Critical error:|"
                            r"Log(?:Linker|Streaming|Load|UObjectGlobals|PackageName):\s*(?:Error|Fatal):", re.I)


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def forbidden_references(text: str) -> list[str]:
    normalized = text.replace("\\", "/")
    found = []
    for name in FORBIDDEN_MODULES:
        if re.search(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"(?![A-Za-z0-9_])", normalized, re.I):
            found.append(name)
    for name in FORBIDDEN_PLUGINS:
        if re.search(r"/" + re.escape(name) + r"/|\b" + re.escape(name) + r"\.uplugin\b", normalized, re.I):
            found.append(name)
    if re.search(r"/Content/Python/|\.py(?:c|o|d|w)?(?:\s|\"|$)", normalized, re.I):
        found.append("staged_python_code")
    return sorted(set(found))


def _finish(result: dict) -> dict:
    result["static_status"] = "fail" if result["violations"] else ("incomplete" if result["incomplete_reasons"] else "pass")
    if result["static_status"] == "fail" or result["runtime"]["status"] == "failed":
        result["status"] = "fail"
    elif result["static_status"] == "incomplete" or result["runtime"]["status"] == "incomplete":
        result["status"] = "incomplete"
    else:
        result["status"] = "fixture_pass" if result["runtime"]["status"] == "marker_and_exit_observed" else "static_pass"
    result["packaging_independence_proven"] = False
    result["full_migration_complete"] = False
    return result


def _inventory(stage: Path) -> dict[str, tuple[int, int]]:
    return {p.relative_to(stage).as_posix(): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in sorted(stage.rglob("*")) if p.is_file()}


def _descriptor_references(descriptor: dict) -> tuple[str, list[str]]:
    """Do not mistake explicitly disabled/editor-target references for game dependencies.

    This handles the two configurations used by this project. It is not a UBT
    enablement resolver; unsupported conditions are conservatively retained.
    """
    filtered = dict(descriptor)
    inactive = []
    if isinstance(descriptor.get("Plugins"), list):
        active = []
        for entry in descriptor["Plugins"]:
            if isinstance(entry, dict) and (entry.get("Enabled") is False or entry.get("TargetAllowList") == ["Editor"]):
                inactive.append(str(entry.get("Name", "<unnamed>")))
            else:
                active.append(entry)
        filtered["Plugins"] = active
    return json.dumps(filtered, ensure_ascii=False), inactive


def _runtime_evidence(log: Path, marker: str, exit_code: int | None) -> dict:
    result = {"status": "incomplete", "log": str(log.resolve()), "marker": marker, "exit_code": exit_code,
              "reasons": [], "limitation": "Runner-supplied exit code and fixture log only; no process/build binding or game launch by this checker."}
    try:
        raw = log.read_bytes()
    except OSError as error:
        result["reasons"].append(f"Cannot read runtime log: {error}")
        return result
    result["log_sha256"] = hashlib.sha256(raw).hexdigest()
    lines = raw.decode("utf-8-sig", errors="replace").splitlines()
    hits = [i + 1 for i, line in enumerate(lines) if marker.strip() and marker in line
            and not re.search(r"command\s*line|arguments:", line, re.I)]
    errors = [line for line in lines if _RUNTIME_ERROR.search(line)]
    result.update({"marker_lines": hits, "fatal_or_load_error": bool(errors), "error_lines": errors[:100]})
    if not marker.strip():
        result["reasons"].append("A nonempty fixture-specific runtime marker is required")
    if exit_code is None:
        result["reasons"].append("The runner's actual process exit code is required")
    if errors or (exit_code is not None and exit_code != 0) or (marker.strip() and not hits):
        result["status"] = "failed"
    elif not result["reasons"]:
        result["status"] = "marker_and_exit_observed"
    return result


def inspect(stage: Path, unrealpak: Path | None, runtime_log: Path | None, marker: str,
            listing_dir: Path | None = None, *, receipt_paths: list[Path] | None = None,
            runtime_exit_code: int | None = None) -> dict:
    stage = stage.resolve()
    result = {"schema": "cubeless_package_independence_v2", "stage": str(stage),
              "scope": "static_staged_files_receipts_and_direct_pak_listings_with_optional_fixture_log",
              "violations": [], "incomplete_reasons": [], "runtime": {"status": "not_run"},
              "limitations": ["Does not parse serialized UObject references or execute/cook/build a game.",
                              "Clean filenames, receipts and mounted entry names are not a complete dependency proof.",
                              "IoStore content inspection is not implemented."]}
    if runtime_log is not None:
        result["runtime"] = _runtime_evidence(runtime_log, marker, runtime_exit_code)
    elif marker or runtime_exit_code is not None:
        result["runtime"] = {"status": "incomplete", "reasons": ["Runtime evidence requires a log file"]}
    if not stage.is_dir():
        result["incomplete_reasons"].append("Staged directory does not exist")
        return _finish(result)
    try:
        before_inventory = _inventory(stage)
    except OSError as error:
        result["incomplete_reasons"].append(f"Cannot enumerate staged files: {error}")
        return _finish(result)
    if not before_inventory:
        result["incomplete_reasons"].append("Staged directory is empty")
    result["file_count"] = len(before_inventory)
    result["inventory_sha256"] = hashlib.sha256(json.dumps(before_inventory, sort_keys=True).encode()).hexdigest()
    containers, receipt_files = [], set(receipt_paths or [])
    executables = {Path(name).name.casefold() for name in before_inventory if Path(name).suffix.lower() == ".exe"}
    if not executables:
        result["incomplete_reasons"].append("No Win64 executable exists in the staged directory")
    for rel in before_inventory:
        path = stage / rel
        if not path.resolve().is_relative_to(stage) or path.is_symlink():
            result["incomplete_reasons"].append(f"Staged file resolves through an external link: {rel}")
            continue
        forbidden = forbidden_references("/" + rel)
        if forbidden:
            result["violations"].append({"source": rel, "matches": forbidden})
        if path.suffix.lower() in {".pak", ".utoc", ".ucas"}:
            containers.append(rel)
        if path.suffix.lower() in {".target", ".modules", ".uplugin", ".uproject"}:
            receipt_files.add(path)
    result["containers"] = containers
    result["receipts"] = []
    game_receipts = []
    for path in sorted(receipt_files):
        row = {"path": str(path.resolve())}
        result["receipts"].append(row)
        try:
            raw = path.read_bytes()
            text = raw.decode("utf-8-sig")
            descriptor = json.loads(text)
            if not isinstance(descriptor, dict):
                raise ValueError("Descriptor must be a JSON object")
            row["sha256"] = hashlib.sha256(raw).hexdigest()
            scan_text, inactive = _descriptor_references(descriptor) if path.suffix.lower() in {".uproject", ".uplugin"} else (text, [])
            row["inactive_plugin_references"] = inactive
            forbidden = forbidden_references(scan_text)
            if forbidden:
                result["violations"].append({"source": str(path), "matches": forbidden, "location": "descriptor_or_receipt"})
            if path.suffix.lower() == ".target":
                row.update({key: descriptor.get(key) for key in ("TargetName", "TargetType", "Platform", "Configuration")})
                products = descriptor.get("BuildProducts", [])
                if not isinstance(products, list):
                    raise ValueError("BuildProducts must be an array")
                matches = [item.get("Path") for item in products if isinstance(item, dict)
                           and item.get("Type") == "Executable" and isinstance(item.get("Path"), str)
                           and Path(item["Path"].replace("\\", "/")).name.casefold() in executables]
                row["staged_executable_name_matches"] = matches
                if descriptor.get("TargetType") in {"Game", "Client", "Server"} and descriptor.get("Platform") == "Win64" and matches:
                    game_receipts.append(row)
        except (OSError, UnicodeError, ValueError) as error:
            result["incomplete_reasons"].append(f"Invalid/unreadable descriptor {path}: {error}")
    if not game_receipts:
        result["incomplete_reasons"].append("No Win64 Game/Client/Server .target receipt references a staged executable; supply --receipt when UAT keeps receipts outside staging")
    result["receipt_binding_scope"] = "target_type_platform_and_staged_executable_basename; not a cryptographic build-to-receipt binding"
    result["pak_listings"] = []
    paks = [stage / rel for rel in containers if Path(rel).suffix.lower() == ".pak"]
    can_list = bool(unrealpak and unrealpak.is_file() and listing_dir)
    if listing_dir and listing_dir.resolve().is_relative_to(stage):
        result["incomplete_reasons"].append("Listing evidence directory must be outside the staged directory")
        can_list = False
    if paks and not can_list:
        result["incomplete_reasons"].append("Every pak requires a direct UnrealPak listing generated by this verifier")
    if paks and can_list:
        try:
            listing_dir.mkdir(parents=True, exist_ok=True)
            result["unrealpak"] = {"path": str(unrealpak.resolve()), "sha256": sha256(unrealpak)}
        except OSError as error:
            result["incomplete_reasons"].append(f"Cannot prepare direct listing evidence: {error}")
            can_list = False
    if paks and can_list:
        for index, pak in enumerate(paks):
            row = {"pak": str(pak), "status": "incomplete"}
            result["pak_listings"].append(row)
            try:
                before = sha256(pak)
                row["pak_sha256_before"] = before
                listing = listing_dir / (str(index) + "-" + pak.stem + "-" + before[:12] + ".log")
                command = [str(unrealpak.resolve()), str(pak), "-List", "-UTF8Output"]
                row.update({"command": command, "listing": str(listing)})
                with listing.open("xb") as output:
                    try:
                        ran = subprocess.run(command, capture_output=True, timeout=300, check=False)
                        raw = ran.stdout + ran.stderr
                        row["exit_code"] = ran.returncode
                    except subprocess.TimeoutExpired as error:
                        raw = (error.stdout or b"") + (error.stderr or b"")
                        output.write(raw)
                        row.update({"timed_out": True, "listing_sha256": hashlib.sha256(raw).hexdigest()})
                        raise
                    output.write(raw)
                row["listing_sha256"] = hashlib.sha256(raw).hexdigest()
                after = sha256(pak)
                row["pak_sha256_after"] = after
                text = raw.decode("utf-8-sig", errors="replace")
                entries = _PAK_ENTRY.findall(text)
                totals = _PAK_TOTAL.findall(text)
                row.update({"recognized_entries": len(entries), "reported_entry_totals": [int(value) for value in totals]})
                complete = ran.returncode == 0 and before == after and len(totals) == 1 and int(totals[0]) == len(entries)
                if any("<FileNamesNotLoaded" in entry for entry in entries):
                    complete = False
                row["status"] = "pass" if complete else "incomplete"
                if not complete:
                    result["incomplete_reasons"].append(f"Listing failed, pak changed, filenames unavailable, or entry count incomplete: {pak.name}")
                for entry in entries:
                    forbidden = forbidden_references(entry)
                    if forbidden:
                        result["violations"].append({"source": str(listing), "pak": str(pak), "entry": entry, "matches": forbidden})
            except (OSError, subprocess.TimeoutExpired) as error:
                row["error"] = str(error)
                result["incomplete_reasons"].append(f"Could not list {pak}: {error}")
    passed_listings = sum(row["status"] == "pass" for row in result["pak_listings"])
    result["pak_coverage"] = {"expected": len(paks), "direct_listing_records": len(result["pak_listings"]), "complete_listings": passed_listings}
    if passed_listings != len(paks):
        result["incomplete_reasons"].append("Pak listing coverage is incomplete")
    if any(Path(item).suffix.lower() in {".utoc", ".ucas"} for item in containers):
        result["incomplete_reasons"].append("IoStore contents require a separate verified IoStore listing; this checker does not parse them")
    try:
        if _inventory(stage) != before_inventory:
            result["incomplete_reasons"].append("Staged files changed during inspection")
    except OSError as error:
        result["incomplete_reasons"].append(f"Cannot verify final staged inventory: {error}")
    return _finish(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", required=True, type=Path)
    parser.add_argument("--unrealpak", type=Path, help="Installed UnrealPak executable; verifier lists every pak directly")
    parser.add_argument("--receipt", action="append", default=[], type=Path, help="Additional actual UBT .target receipt outside staging")
    parser.add_argument("--runtime-log", type=Path)
    parser.add_argument("--runtime-marker", default="")
    parser.add_argument("--runtime-exit-code", type=int, help="Actual game process exit code; checker never launches a game")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Refusing to overwrite evidence")
    if args.output.resolve().is_relative_to(args.stage.resolve()):
        parser.error("Output evidence must be outside the staged directory")
    result = inspect(args.stage, args.unrealpak, args.runtime_log, args.runtime_marker,
                     args.output.parent / (args.output.stem + "-pak-listings"),
                     receipt_paths=args.receipt, runtime_exit_code=args.runtime_exit_code)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({"status": result["status"], "static_status": result["static_status"],
                      "violations": len(result["violations"]), "runtime": result["runtime"]["status"],
                      "packaging_independence_proven": False}))
    return 0 if result["status"] in {"static_pass", "fixture_pass"} else 1


if __name__ == "__main__":
    raise SystemExit(main())
