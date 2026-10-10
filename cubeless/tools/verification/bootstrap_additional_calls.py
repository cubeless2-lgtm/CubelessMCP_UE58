"""External official-MCP checks for three initial Cubeless tools.

Reads an installed Engine SkeletalCube and compiles an existing isolated author
fixture twice, once with node refresh. Never saves, launches/stops an editor,
retries a call, creates assets, or claims complete legacy skeletal/runtime parity.
Only main() opens a connection; imports and --help are offline.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import time
import traceback
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import MCPError, OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from verification.pipeline_report import verify


MESH = "/Engine/EngineMeshes/SkeletalCube.SkeletalCube"


def ensure(condition, message):
    if not condition:
        raise AssertionError(message)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require_success(value):
    ensure(isinstance(value, dict) and value.get("success") is True, "Tool did not succeed: " + str(value))


def check_sections(value):
    require_success(value)
    ensure(value.get("skeletal_mesh_path") == MESH and value.get("lod_index") == 0, "Skeletal result identity/LOD mismatch")
    ensure(value.get("mutation_performed") is False and value.get("saved") is False, "Skeletal read reported mutation/save")
    ensure(value.get("package_dirty") is False, "Installed engine fixture became dirty")
    rows = value.get("sections")
    ensure(isinstance(rows, list) and len(rows) > 0, "Skeletal imported LOD contains no sections")
    ensure([row.get("section_index") for row in rows] == list(range(len(rows))), "Section indices are not contiguous")
    for row in rows:
        for key in ("section_index", "material_index", "original_data_section_index", "chunked_parent_section_index"):
            ensure(type(row.get(key)) is int, "Missing/noninteger skeletal section field: " + key)
        ensure(row["material_index"] >= 0 and type(row.get("enabled")) is bool, "Invalid material index/enabled state")
        if "source_enabled" in row:
            ensure(type(row["source_enabled"]) is bool, "source_enabled must be boolean when source data is present")


def check_compile(value, refreshed, expected_id, asset_path):
    require_success(value)
    ensure(value.get("operation_id") == expected_id, "Mutation request identity changed")
    compile_info, save_info, native = value["compile"], value["save"], value["data"]
    ensure(compile_info.get("requested") is True and compile_info.get("attempted") is True and compile_info.get("compiled") is True, "Compile did not execute successfully")
    ensure(compile_info.get("error_count") == 0 and compile_info.get("status") in {"up_to_date", "up_to_date_with_warnings"}, "Compiler status/errors contradict success")
    ensure(compile_info.get("refreshed_nodes") is refreshed and native.get("refreshed_nodes") is refreshed, "Refresh request/observation mismatch")
    ensure(save_info.get("requested") is False and save_info.get("attempted") is False and save_info.get("saved") is False, "No-save compile unexpectedly saved")
    ensure(native.get("saved") is False and native.get("requested_save") is False and native.get("mutation_performed") is True, "Native change/save evidence mismatch")
    ensure(native.get("validation_pass") is True and native.get("compile_errors") == [], "Native diagnostics did not pass")
    ensure(isinstance(native.get("diagnostics"), list) and isinstance(native.get("compile_warnings"), list), "Missing native compiler diagnostics")
    for phase in ("before", "after"):
        snapshot = native.get(phase, {})
        ensure(snapshot.get("path") == asset_path and snapshot.get("parent_class") == "/Script/Engine.Actor", "Blueprint snapshot identity/parent mismatch")
    ensure(native["after"].get("generated_class"), "Compile has no generated class")
    ensure(value["retry"].get("automatic_retry") is False and value["retry"].get("replayed") is False, "Compile was retried/replayed instead of executed")
    expected_kind = "refresh_and_compile" if refreshed else "compile"
    ensure(any(change.get("kind") == expected_kind and change.get("attempted") is True and change.get("mutation_observed") is True for change in value["changes"]), "Missing attempted/observed compile change")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--expected-project", type=Path, required=True)
    parser.add_argument("--author-report", type=Path, required=True)
    parser.add_argument("--engine-dir", type=Path, required=True, help="Installed UE5.8 Engine directory, containing Content and Source")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args(argv)
    ensure(not args.output.exists(), "Refusing to overwrite evidence; select a new report path")
    project = args.expected_project.resolve()
    engine = args.engine_dir.resolve()
    author = json.loads(args.author_report.read_text(encoding="utf-8-sig"))
    ensure(verify(author, require_phase="author")["passed"], "Author report did not pass its independent contract validator")
    ensure(Path(author["project_file"]).resolve() == project, "Author report belongs to another project")
    expected_project = (Path(__file__).resolve().parents[2] / "workspaces/UE58Verification/CubelessMCPVerification.uproject").resolve()
    ensure(project == expected_project and project.is_file(), "Only the root-owned isolated UE58Verification project is supported")
    asset_path = author["asset_path"]
    ensure(re.fullmatch(r"/Game/CubelessMCPTests/Run_[a-f0-9]{32}/BP_MutationContract", asset_path), "Unexpected author fixture path")
    bp_file = project.parent / "Content" / (asset_path[len("/Game/"):] + ".uasset")
    mesh_file = engine / "Content/EngineMeshes/SkeletalCube.uasset"
    ensure(bp_file.is_file() and mesh_file.is_file(), "Author Blueprint or installed mesh file is missing")
    run_id = uuid.uuid4().hex
    tracked = {"blueprint": {"path": str(bp_file), "sha256_before": sha256(bp_file)},
               "skeletal_mesh": {"path": str(mesh_file), "sha256_before": sha256(mesh_file)}}
    report = {"schema": "cubeless_bootstrap_additional_calls_v1", "status": "running", "success": False,
              "run_id": run_id, "project_file": str(project), "endpoint": args.endpoint, "asset_path": asset_path,
              "skeletal_mesh": MESH, "author_report_sha256": sha256(args.author_report), "script_sha256": sha256(__file__),
              "started_ns": time.time_ns(), "stages": [], "automatic_retries": 0, "atomic_rollback": False,
              "outcome_unknown": False, "save_requested": False, "files": tracked,
              "validation_scope": {"skeletal": "actual reflected USkeletalMesh load/imported LOD section contract, repeat consistency and invalid-input rejection; no independent imported-model golden or full legacy parity",
                                   "blueprint": "two wrapper calls, native compiler diagnostics, before/after state and graph topology, disk hash unchanged; in-memory compile/refresh changes are expected",
                                   "disk_reload_after_calls": "not_run", "packaging": "not_run", "runtime_behavior": "not_run"}}
    client = OfficialMCPClient(args.endpoint, args.timeout)
    schemas = {}
    def persist():
        args.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.output.with_suffix(args.output.suffix + ".partial")
        temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        temporary.replace(args.output)
    def invoke(toolset, tool, arguments=None, mutation=False):
        selected = [item for item in schemas[toolset]["tools"] if item["name"].rsplit(".", 1)[-1] == tool]
        ensure(len(selected) == 1, "Tool absent/ambiguous in actual registered schema: " + tool)
        schema = selected[0]["inputSchema"]
        supplied = {}
        for key, value in (arguments or {}).items():
            candidates = [name for name in schema.get("properties", {}) if canonical(name) == canonical(key)]
            ensure(len(candidates) == 1, "Argument absent/ambiguous in actual schema: " + key)
            supplied[candidates[0]] = value
        ensure(set(schema.get("required", [])) <= set(supplied), "A required registered input is missing: " + tool)
        try:
            return operation_result(client.call_toolset(toolset, tool, supplied, mutation=mutation))
        except MCPError as error:
            error.outcome_unknown = error.outcome_unknown or mutation
            raise
    def step(name, action, check=require_success, *, mutation=False, request=None):
        entry = {"name": name, "passed": False, "state": "started", "mutation": mutation, "request": request}
        report["stages"].append(entry)
        persist()  # Preserve a request-attempt checkpoint before network dispatch.
        start = time.perf_counter()
        try:
            entry["result"] = action()
            check(entry["result"])
            entry.update(passed=True, state="complete")
            return entry["result"]
        except Exception as error:
            entry.update(error=str(error), state="failed")
            if isinstance(error, MCPError):
                entry["mcp_error"] = error.detail
                report["outcome_unknown"] = report["outcome_unknown"] or error.outcome_unknown
            elif mutation and "result" not in entry:
                report["outcome_unknown"] = True
            raise
        finally:
            entry["elapsed_seconds"] = round(time.perf_counter() - start, 6)
            persist()
    persist()
    try:
        report["initialize"] = client.initialize()
        report["top_level_tools"] = client.list_tools()
        catalog = tool_payload(client.call("list_toolsets"))
        ensure(isinstance(catalog, str), "Expected official registry text catalog")
        names = re.findall(r"^- ([\w.]+):", catalog, re.MULTILINE)
        def discover(suffix):
            matches = [name for name in names if name.endswith("." + suffix)]
            ensure(len(matches) == 1, "Expected one registered toolset: " + suffix)
            name = matches[0]
            schemas[name] = tool_payload(client.call("describe_toolset", {"toolset_name": name}))
            ensure(isinstance(schemas[name], dict) and schemas[name].get("tools"), "Registered schema is empty")
            return name
        native = discover("CubelessNativeToolset")
        blueprint = discover("CubelessBlueprintTools")
        graph = discover("CubelessBlueprintGraphTools")
        report["discovered_schemas"] = schemas
        report["registered_schema_sha256"] = hashlib.sha256(json.dumps(schemas, sort_keys=True).encode()).hexdigest()
        def guard(value, allow_target_dirty=False):
            require_success(value)
            ensure(Path(value["project_file"]).resolve() == project and str(value["engine_version"]).startswith("5.8.3-"), "Wrong project/engine on endpoint")
            ensure(value["pie_or_sie_active"] is False and value["game_thread"] is True, "Requires the editor game thread outside PIE/SIE")
            dirty = set(value.get("dirty_packages", []))
            ensure(dirty <= ({asset_path} if allow_target_dirty else set()), "Unexpected dirty packages; no save/discard will be performed")
        report["editor"] = step("preflight", lambda: invoke(native, "GetEditorDiagnostics"), guard)
        deployed = project.parent / "Plugins/CubelessMCP_UE58"
        inputs = [Path(__file__), Path(__file__).parents[1] / "official_mcp_client.py",
                  Path(__file__).parents[1] / "pipelines/blueprint_mutation_smoke.py",
                  deployed / "Content/Python/cubeless_mcp_ue58/registration/toolsets.py",
                  deployed / "Content/Python/cubeless_mcp_ue58/operations/blueprint.py",
                  deployed / "Content/Python/cubeless_mcp_ue58/operations/state.py",
                  deployed / "Binaries/Win64/UnrealEditor-CubelessMCPUE58Editor.dll",
                  engine / "Source/Editor/PhysicsAssetEditor/Private/PhysicsAssetEditorSharedData.cpp"]
        report["loaded_installation_file_hashes"] = {str(path): sha256(path) for path in inputs}
        mesh = step("skeletal_sections", lambda: invoke(native, "GetSkeletalMeshSections", {"skeletal_mesh_path": MESH, "lod_index": 0}), check_sections)
        def repeat_check(value):
            check_sections(value)
            ensure(value["sections"] == mesh["sections"], "Repeated read-only section content differs")
        step("skeletal_repeat_read_consistency", lambda: invoke(native, "GetSkeletalMeshSections", {"skeletal_mesh_path": MESH, "lod_index": 0}), repeat_check)
        def rejected(stage):
            def check(value):
                ensure(value.get("success") is False and value.get("stage") == stage and value.get("error"), "Expected read-only input rejection")
                ensure(value.get("mutation_performed") is False and value.get("saved") is False, "Rejected read reported mutation/save")
            return check
        step("negative_lod_rejected", lambda: invoke(native, "GetSkeletalMeshSections", {"skeletal_mesh_path": MESH, "lod_index": -1}), rejected("preflight"))
        step("wrong_asset_type_rejected", lambda: invoke(native, "GetSkeletalMeshSections", {"skeletal_mesh_path": "/Engine/BasicShapes/Cube.Cube", "lod_index": 0}), rejected("load"))
        before = step("blueprint_before", lambda: invoke(blueprint, "inspect_blueprint", {"blueprint_name": asset_path}))
        ensure(before["data"].get("parent_class") == "/Script/Engine.Actor" and before["data"].get("dirty") is False, "Author fixture must be a clean Actor Blueprint")
        graph_before = step("graph_before", lambda: invoke(graph, "inspect_graph", {"blueprint_name": asset_path, "graph_name": "EventGraph"}))
        ensure(any(node["node_id"] == author["fixture"]["remaining_node_id"] for node in graph_before["data"]["nodes"]), "Expected author-D surviving Branch is missing")
        for tool, refreshed in (("compile_blueprint", False), ("compile_and_validate_blueprint", True)):
            step("guard_" + tool, lambda: invoke(native, "GetEditorDiagnostics"), lambda value: guard(value, True))
            operation_id = run_id + "_" + tool
            arguments = {"blueprint_name": asset_path, "operation_id": operation_id}
            if refreshed:
                arguments.update(save=False, refresh_nodes=True)
            step(tool, lambda: invoke(blueprint, tool, arguments, True),
                 lambda value: check_compile(value, refreshed, operation_id, asset_path), mutation=True,
                 request={"toolset": blueprint, "tool": tool, "arguments": arguments})
        after = step("blueprint_after", lambda: invoke(blueprint, "inspect_blueprint", {"blueprint_name": asset_path}))
        ensure(after["data"]["generated_class"] == before["data"]["generated_class"], "Generated class identity changed")
        def graph_check(value):
            require_success(value)
            ensure(value["data"] == graph_before["data"], "Compile/refresh changed this fixture's graph topology, pins, or positions")
        step("graph_after", lambda: invoke(graph, "inspect_graph", {"blueprint_name": asset_path, "graph_name": "EventGraph"}), graph_check)
        report["editor_after"] = step("editor_after", lambda: invoke(native, "GetEditorDiagnostics"), lambda value: guard(value, True))
        report.update(status="pass", success=True)
    except Exception as error:
        report.update(status="fail", success=False, error=str(error), traceback=traceback.format_exc())
    finally:
        for item in tracked.values():
            path = Path(item["path"])
            item["sha256_after"] = sha256(path) if path.is_file() else None
            item["disk_bytes_unchanged"] = item["sha256_before"] == item["sha256_after"]
        if not all(item["disk_bytes_unchanged"] for item in tracked.values()):
            report.update(status="fail", success=False)
            report["disk_hash_error"] = "A tracked asset file changed despite no save being requested"
        report["calls"] = client.observations
        report["finished_ns"] = time.time_ns()
        client.close()
        persist()
    print(json.dumps({"status": report["status"], "output": str(args.output), "outcome_unknown": report["outcome_unknown"]}))
    return 0 if report["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
