"""Read actual 5.6 FastMCP registrations, without a server lifespan or UE connection.

Run with the source venv Python: python.exe -I -B -S this_file.py
The controller only copies bytes and runs guarded children. All audit outputs and
isolated logs stay under the legacy-runtime-registration.* artifact prefix.
No substitute FastMCP, registry, decorators, or tool bodies are used.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import sys
import time

HERE = Path(__file__).resolve().parent
PREFIX = "legacy-runtime-registration"
SOURCE = Path(r"C:\Git\UnrealMCP5_6_1\Python")
ISOLATED = HERE / (PREFIX + ".isolated")
COPY = ISOLATED / "Python"
SITE = SOURCE / ".venv/Lib/site-packages"
INVENTORY = HERE.parent / "planning-2026-10-10/legacy-feature-inventory.json"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, data: object) -> None:
    with path.open("x", encoding="utf-8") as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def inside(path: object, parent: Path) -> bool:
    if not isinstance(path, (str, bytes, os.PathLike)):
        return False
    absolute = os.path.normcase(os.path.abspath(os.fsdecode(path)))
    root = os.path.normcase(str(parent))
    return absolute == root or absolute.startswith(root + os.sep)


def child(mode: str) -> int:
    # -S prevents .pth startup code; this path points to the actual installed SDK.
    sys.dont_write_bytecode = True
    sys.path[:0] = [str(COPY), str(SITE)]
    run_directory = ISOLATED / mode
    os.chdir(run_directory)
    # Record environment variable names only; never disclose host credentials.
    cleared_names = sorted(k for k in os.environ if k.startswith("UNREAL_MCP_"))
    for key in cleared_names:
        del os.environ[key]
    os.environ["UNREAL_MCP_TOOL_EXPOSURE"] = mode
    events: list[dict] = []
    mutation_events: list[dict] = []

    def audit(event: str, args: tuple) -> None:
        # Deny even socket construction: no listen, connect, DNS or UDP activity.
        if event.startswith("socket.") or event in {"subprocess.Popen", "os.system", "os.startfile", "os.startfile/2"} or event.startswith("os.exec") or event.startswith("os.spawn"):
            events.append({"event": event, "blocked": True})
            raise RuntimeError("Registration audit forbids network/process activity: " + event)
        if event == "open":
            path, mode_value, flags = args
            writing = bool(isinstance(mode_value, str) and any(c in mode_value for c in "wax+"))
            writing = writing or bool(isinstance(flags, int) and flags & (os.O_WRONLY | os.O_RDWR | os.O_CREAT | os.O_TRUNC | os.O_APPEND))
            if writing and not inside(path, ISOLATED):
                mutation_events.append({"event": event, "path": os.fsdecode(path), "blocked": True})
                raise RuntimeError("Registration audit forbids writes outside isolation")
        if event in {"os.remove", "os.rmdir", "os.mkdir", "os.chmod", "os.utime", "os.rename", "os.link", "os.symlink"}:
            paths = args[:2] if event in {"os.rename", "os.link", "os.symlink"} else args[:1]
            if any(not inside(path, ISOLATED) for path in paths):
                mutation_events.append({"event": event, "blocked": True})
                raise RuntimeError("Registration audit forbids filesystem mutation outside isolation")

    sys.addaudithook(audit)
    import importlib.metadata
    import inspect
    import traceback

    report: dict = {
        "scope": "actual_sdk_runtime_registration_without_lifespan_or_tool_invocation",
        "mode": mode,
        "python": sys.version,
        "executable": sys.executable,
        "flags": {"isolated": sys.flags.isolated, "no_site": sys.flags.no_site, "dont_write_bytecode": sys.dont_write_bytecode},
        "cleared_environment_variable_names": cleared_names,
        "network_and_process_attempts": events,
        "outside_isolation_mutation_attempts": mutation_events,
        "lifespan_started": False,
        "tool_bodies_invoked": False,
        "transport_started": False,
    }
    started = time.perf_counter()
    try:
        import unreal_mcp_server as server
        import mcp.server.fastmcp.server as sdk_server

        assert inside(server.__file__, COPY)
        assert inside(sdk_server.__file__, SITE)
        report["sdk"] = {
            "mcp_version": importlib.metadata.version("mcp"),
            "pydantic_version": importlib.metadata.version("pydantic"),
            "fastmcp_server_file": sdk_server.__file__,
            "fastmcp_server_sha256": sha(Path(sdk_server.__file__)),
            "list_tools_source": inspect.getsource(sdk_server.FastMCP.list_tools),
            "class_identity_matches_installed_sdk": type(server.mcp) is sdk_server.FastMCP,
        }
        # SDK 1.5.0 list_tools has no await expression. Run its genuine coroutine
        # directly to completion, avoiding asyncio's loopback socketpair setup.
        coroutine = server.mcp.list_tools()
        try:
            yielded = coroutine.send(None)
        except StopIteration as completed:
            public_tools = completed.value
        else:
            coroutine.close()
            raise RuntimeError("SDK list_tools unexpectedly suspended: " + repr(yielded))

        def describe(tool):
            fn = inspect.unwrap(tool.fn)
            return {
                "name": tool.name,
                "description": tool.description,
                "inputSchema": tool.parameters,
                "is_async": tool.is_async,
                "context_kwarg": tool.context_kwarg,
                "function_module": fn.__module__,
                "function_qualname": fn.__qualname__,
                "function_source": str(Path(inspect.getsourcefile(fn)).relative_to(COPY)).replace("\\", "/"),
                "function_first_line": fn.__code__.co_firstlineno,
            }

        report["public_tools"] = [tool.model_dump(mode="json") for tool in sorted(public_tools, key=lambda item: item.name)]
        report["catalog_tools"] = [describe(tool) for _, tool in sorted(server.get_catalog_tools().items())]
        report["registry"] = server.tool_registry.report(include_tools=True)
        report["telemetry"] = server.request_telemetry.report(include_recent=True)
        report["unreal_connection_is_none"] = server._unreal_connection is None
        report["server_exposure_mode"] = server.TOOL_EXPOSURE_MODE
        report["counts"] = {
            "public": len(report["public_tools"]),
            "catalog": len(report["catalog_tools"]),
            "registry": report["registry"]["total_tools"],
        }
        loaded = []
        for name, module in sorted(sys.modules.items()):
            if name in {"unreal_mcp_server", "tools", "services"} or name.startswith(("tools.", "services.")):
                path = getattr(module, "__file__", None)
                if path is not None:
                    if not inside(path, COPY):
                        raise RuntimeError("Legacy module escaped isolated source: " + name)
                    loaded.append({"module": name, "file": str(Path(path).relative_to(COPY)).replace("\\", "/"), "sha256": sha(Path(path))})
        report["loaded_legacy_modules"] = loaded
        report["status"] = "pass" if not events and not mutation_events and server._unreal_connection is None else "fail"
    except BaseException:
        report["status"] = "fail"
        report["error"] = traceback.format_exc()
    report["elapsed_seconds"] = time.perf_counter() - started
    # This one report is inside the isolated output directory allowed by the hook.
    write_json(run_directory / "result.json", report)
    print(json.dumps({key: report.get(key) for key in ("mode", "status", "counts", "elapsed_seconds", "error")}))
    return 0 if report["status"] == "pass" else 1


def controller() -> int:
    import subprocess

    if ISOLATED.exists():
        raise RuntimeError("Refusing to overwrite previous isolation evidence: " + str(ISOLATED))
    COPY.mkdir(parents=True)
    sources = [SOURCE / "unreal_mcp_server.py"]
    for folder in ("tools", "services"):
        sources.extend(sorted((SOURCE / folder).rglob("*.py")))
    manifest = []
    for path in sources:
        relative = path.relative_to(SOURCE)
        destination = COPY / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        data = path.read_bytes()
        destination.write_bytes(data)
        manifest.append({"source": str(path), "relative": str(relative).replace("\\", "/"), "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    write_json(HERE / (PREFIX + ".sources.json"), manifest)
    results = {}
    for mode in ("search", "eager"):
        run_directory = ISOLATED / mode
        run_directory.mkdir()
        command = [str(SOURCE / ".venv/Scripts/python.exe"), "-I", "-B", "-S", str(Path(__file__).resolve()), "--child", mode]
        result = subprocess.run(command, cwd=run_directory, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
        (HERE / (PREFIX + "." + mode + ".stdout.log")).write_text(result.stdout, encoding="utf-8")
        (HERE / (PREFIX + "." + mode + ".stderr.log")).write_text(result.stderr, encoding="utf-8")
        raw = run_directory / "result.json"
        report = json.loads(raw.read_text(encoding="utf-8")) if raw.exists() else {"status": "fail", "error": "No child report"}
        report["process_exit_code"] = result.returncode
        report["command"] = command
        write_json(HERE / (PREFIX + "." + mode + ".json"), report)
        results[mode] = report
        print(json.dumps({"mode": mode, "status": report["status"], "counts": report.get("counts"), "exit_code": result.returncode}))
    unchanged = [entry["relative"] for entry in manifest if sha(Path(entry["source"])) != entry["sha256"]]
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    definitions = inventory["python_tool_definitions"]
    static_names = {entry["name"] for entry in definitions}
    runtime_names = {entry["name"] for report in results.values() for key in ("public_tools", "catalog_tools") for entry in report.get(key, [])}
    source_hash_mismatch = []
    for entry in definitions:
        path = SOURCE.parent / entry["source"]["path"]
        if sha(path) != entry["source"]["sha256"]:
            source_hash_mismatch.append(entry["name"])
    catalogs = [{tool["name"]: tool["inputSchema"] for tool in results[mode].get("catalog_tools", [])} for mode in ("search", "eager")]
    summary = {
        "schema_version": 1,
        "scope": "actual_legacy_registration_inventory_only",
        "source_root": str(SOURCE.parent),
        "source_files_copied": len(manifest),
        "source_files_changed_during_audit": unchanged,
        "planning_inventory_sha256": sha(INVENTORY),
        "planning_inventory_source_hash_mismatches": source_hash_mismatch,
        "static_definition_count": len(definitions),
        "static_unique_names": len(static_names),
        "runtime_union_unique_names": len(runtime_names),
        "static_missing_from_runtime": sorted(static_names - runtime_names),
        "runtime_not_in_static": sorted(runtime_names - static_names),
        "search_eager_catalog_schemas_identical": catalogs[0] == catalogs[1],
        "modes": {mode: {key: report.get(key) for key in ("status", "counts", "elapsed_seconds", "process_exit_code", "network_and_process_attempts", "outside_isolation_mutation_attempts", "unreal_connection_is_none")} for mode, report in results.items()},
        "per_definition": [{"name": entry["name"], "category": entry["category"], "source": entry["source"], "runtime_catalog": entry["name"] in catalogs[0], "search_public": entry["name"] in {t["name"] for t in results["search"].get("public_tools", [])}, "eager_public": entry["name"] in {t["name"] for t in results["eager"].get("public_tools", [])}} for entry in definitions],
        "engine_operations_verified": False,
        "ue58_migration_complete": False,
        "limitations": [
            "Actual installed FastMCP and unchanged isolated legacy modules were imported; no fake decorators or registry were used.",
            "This is execution of the real FastMCP list_tools handler without transport; it is not an MCP wire-protocol or UE endpoint test.",
            "No lifespan, engine connection, domain tool body, or mutation was invoked.",
            "CPython audit hooks denied socket/process events and outside-isolation filesystem writes during import/listing; this is not an OS-wide security sandbox or external process observation.",
            "UNREAL_MCP_* variables were cleared in child processes before setting the requested exposure mode; default registry configuration was measured.",
        ],
    }
    summary["status"] = "pass" if all(r["status"] == "pass" and r["process_exit_code"] == 0 for r in results.values()) and not unchanged and not source_hash_mismatch and static_names == runtime_names and catalogs[0] == catalogs[1] else "fail"
    write_json(HERE / (PREFIX + ".json"), summary)
    print(json.dumps({key: summary[key] for key in ("status", "static_definition_count", "runtime_union_unique_names", "static_missing_from_runtime", "runtime_not_in_static", "search_eager_catalog_schemas_identical")}))
    return 0 if summary["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(child(sys.argv[2]) if len(sys.argv) == 3 and sys.argv[1] == "--child" else controller())
