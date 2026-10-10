"""Offline proof for Distribution L installed as an Editor-only project plugin.

Reads existing build/Pak/marker evidence and bytes. Never executes a process or
changes assets. D verifier source remains frozen and is reused only read-only.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import current_runtime_proof as prior
from current_runtime_manifest import read_json, require

ROOT = prior.contract.ROOT
PROJECT = ROOT / "cubeless/workspaces/UE58InstalledCurrentRuntimeProof/CubelessCurrentRuntimeProof.uproject"
PLUGIN = PROJECT.parent / "Plugins/CubelessMCP_UE58"
DISTRIBUTION = ROOT / "cubeless/build/Distribution-20261010-L"
STAGE = ROOT / "cubeless/build/InstalledCurrentRuntimeShipping-20261010-L/Windows"
DEFAULTS = {"preparation": "installed-current-runtime-project-preparation-L.json", "build_log": "build-installed-current-runtime-shipping-L.log", "package": "installed-current-package-static-L.json", "baseline": "installed-current-runtime-marker-baseline-L.json", "launch": "installed-current-runtime-launch-L.json", "marker": "installed-current-runtime-marker-verification-L.json", "prior_D": "current-runtime-proof-D.json"}
D_VERIFIER_SHA = "5f433a1a166d7f8f2bfe4241bbe1d5b7956d5bb9ce3091bbe530f5994739f467"
sha = prior.sha
artifact = prior.artifact


def safe_relative(text):
    value = PurePosixPath(text.replace("\\", "/"))
    require(not value.is_absolute() and value.parts and not any(p in {"..", "."} or ":" in p for p in value.parts), "Plugin relative path escapes owned roots")
    return value.as_posix()


def protected_plugin_file(relative):
    value = PurePosixPath(relative)
    return relative.startswith("Source/") or (relative.startswith("Content/Python/") and value.suffix == ".py") or relative == "CubelessMCP_UE58.uplugin"


def generated_plugin_file(relative):
    value = PurePosixPath(relative)
    return relative.startswith(("Binaries/", "Intermediate/")) or (relative.startswith("Content/Python/") and "__pycache__" in value.parts and value.suffix == ".pyc")


def descriptor_check(descriptor, plugin):
    require(descriptor.get("DisableEnginePluginsByDefault") is True and not descriptor.get("Modules"), "Installed proof project must remain content-only/default-disabled")
    expected = {"PythonScriptPlugin", "EditorScriptingUtilities", "CubelessMCP_UE58"}
    rows = descriptor["Plugins"]
    require(len(rows) == len(expected) and {p["Name"] for p in rows} == expected and all(p.get("Enabled") is True and p.get("TargetAllowList") == ["Editor"] for p in rows), "Project plugin enablement is not exact Editor-only L")
    modules = plugin["Modules"]
    require(plugin.get("Installed") is True and len(modules) == 1 and modules[0]["Name"] == "CubelessMCPUE58Editor" and modules[0]["Type"] == "Editor" and modules[0]["TargetAllowList"] == ["Editor"], "Plugin module can enter Game/Runtime target")
    dependencies = plugin["Plugins"]; expected_dependencies = {"ToolsetRegistry", "ModelContextProtocol", "PythonScriptPlugin", "EditorScriptingUtilities"}
    require(len(dependencies) == len(expected_dependencies) and {p["Name"] for p in dependencies} == expected_dependencies and all(p.get("Enabled") is True and p.get("TargetAllowList") == ["Editor"] for p in dependencies), "Plugin dependencies are not exact Editor-only set")


def preparation_evidence(report, manifest, author):
    require(report.get("schema") == "cubeless_installed_current_project_preparation_v1" and report.get("status") == "pass" and report.get("gameplay_cpp_added") is False and report.get("actor_M_included") is False, "Wrong/incomplete preparation scope")
    require(Path(report["source_project"]).resolve() == prior.contract.PROJECT.resolve() and Path(report["project_file"]).resolve() == PROJECT.resolve() and Path(report["plugin_source"]).resolve() == DISTRIBUTION.resolve(), "Preparation refers to another project/distribution")
    require(sha(PROJECT) == report["project_sha256"], "Installed project descriptor changed")
    descriptor_check(read_json(PROJECT), read_json(PLUGIN / "CubelessMCP_UE58.uplugin"))
    require(not (PROJECT.parent / "Source").exists(), "Unexpected gameplay Source folder")
    expected = {str(Path(r["destination"]).resolve()): r["sha256"] for r in manifest["files"]}
    expected.update({str(Path(r["path"]).resolve()): r["sha256"] for r in author["new_asset_files"].values()})
    rows = report["copied_assets"]
    require(len(rows) == len(expected) == 23 and {str(Path(r["source"]).resolve()) for r in rows} == set(expected), "Exactly the original 23 saved assets must be copied")
    copies = []
    for row in rows:
        source = Path(row["source"]).resolve(); relative = source.relative_to(prior.contract.PROJECT.parent)
        target = (PROJECT.parent / relative).resolve()
        require(relative.parts[0] == "Content" and Path(row["destination"]).resolve() == target and target.is_relative_to(PROJECT.parent.resolve()) and sha(source) == sha(target) == row["sha256"] == expected[str(source)], "Copied asset path/hash does not bind original D bytes")
        copies.append({"relative": relative.as_posix(), "sha256": row["sha256"]})
    actual_content = {p.relative_to(PROJECT.parent).as_posix() for p in (PROJECT.parent / "Content").rglob("*") if p.is_file()}
    require(actual_content == {r["relative"] for r in copies}, "Installed content includes omitted/extra files")
    require(set(report["config_files"]) == {"DefaultEngine.ini", "DefaultGame.ini"}, "Config coverage differs")
    configs = {}
    for name, recorded in report["config_files"].items():
        source = prior.contract.PROJECT.parent / "Config" / name; target = PROJECT.parent / "Config" / name
        require(sha(source) == sha(target) == recorded, "Config differs from D map and Pak-only settings")
        configs[name] = artifact(target)
    plugin_rows = {safe_relative(r["relative"]): r["sha256"] for r in report["plugin_files"]}
    require(len(plugin_rows) == len(report["plugin_files"]) == 90, "Distribution L preparation file identity/count differs")
    require(all(protected_plugin_file(p) or generated_plugin_file(p) for p in plugin_rows), "Unclassified plugin payload")
    protected = {p: h for p, h in plugin_rows.items() if protected_plugin_file(p)}
    actual = {p.relative_to(PLUGIN).as_posix() for p in PLUGIN.rglob("*") if p.is_file()}
    require({p for p in actual if protected_plugin_file(p)} == set(protected) and all(protected_plugin_file(p) or generated_plugin_file(p) for p in actual), "Protected files missing/new or unexpected plugin payload")
    require(not any(PurePosixPath(p).name in {"actor.py", "actor_toolset.py"} or PurePosixPath(p).name.startswith("CubelessActor") for p in protected), "Actor M files entered the L cohort")
    for relative, expected_hash in protected.items():
        source = (DISTRIBUTION / relative).resolve(); target = (PLUGIN / relative).resolve()
        require(source.is_relative_to(DISTRIBUTION.resolve()) and target.is_relative_to(PLUGIN.resolve()) and sha(source) == sha(target) == expected_hash, "Protected Distribution source/Python/descriptor changed: " + relative)
    generated = [{"relative": p, "sha256_before": plugin_rows.get(p), "sha256_after": sha(PLUGIN / p)} for p in sorted(actual) if generated_plugin_file(p)]
    removed_generated = [{"relative": p, "sha256_before": plugin_rows[p]} for p in sorted(set(plugin_rows) - actual) if generated_plugin_file(p)]
    return {"status": "pass", "descriptor": artifact(PROJECT), "plugin_descriptor": artifact(PLUGIN / "CubelessMCP_UE58.uplugin"), "configs": configs, "assets": copies, "protected_plugin_files": protected, "protected_count": len(protected), "initial_plugin_file_count": len(plugin_rows), "generated_files": generated, "generated_changes": [r["relative"] for r in generated if r["sha256_before"] != r["sha256_after"]], "removed_generated_files": removed_generated, "scope": "23 exact immutable source/copy assets, exact Editor-only descriptors and config; protected plugin source/Python text unchanged. UAT-generated Binaries/Intermediate/pycache differences including removed initial files are observed, not required unchanged."}


def cleanup_observation(line, following, engine_root):
    prefix = "LogShaderCompilers: Display: Failed to delete old shader autogen file: "
    if not line.startswith(prefix): return False
    name = line[len(prefix):]
    resolved = (Path(engine_root) / "Binaries/Win64" / name).resolve()
    owned = {(PROJECT.parent / "Intermediate/ShaderAutogen" / shader / "AutogenShaderHeaders.ush").resolve() for shader in ("PCD3D_SM5", "PCD3D_SM6")}
    return resolved in owned and following == "LogShaderCompilers: Display: Shader autogen file written: " + name


def build_evidence(path, manifest, engine_root):
    text = Path(path).read_text(encoding="utf-8-sig", errors="replace"); lines = text.splitlines()
    commands = [l for l in lines if l.startswith("Parsing command line: BuildCookRun ")]; require(len(commands) == 1, "Ambiguous build command")
    command = commands[0]
    def option(name):
        found = re.findall(r"(?<!\S)-" + name + r'=("[^"]+"|\S+)', command, re.I)
        require(len(found) == 1, "Missing/duplicate build option: " + name); return found[0].strip('"')
    require(Path(option("project")).resolve() == PROJECT.resolve() and Path(option("archivedirectory")).resolve() == STAGE.parent.resolve() and option("map") == manifest["default_map"] and option("clientconfig") == "Shipping" and option("platform") == "Win64", "Build project/archive/map/target mismatch")
    require(all(re.search(r"(?<!\S)-" + name + r"(?=\s|$)", command, re.I) for name in ("build", "cook", "stage", "pak", "package", "archive")), "Build phases missing")
    require("BUILD SUCCESSFUL" in lines and "COOK COMMAND COMPLETED" in text and lines[-1].strip() == "AutomationTool exiting with ExitCode=0 (Success)", "Build/cook completion or UAT exit absent")
    times = re.findall(r"^BuildCookRun time: ([0-9.]+) s$", text, re.M); require(len(times) == 1 and float(times[0]) > 0, "Missing build duration")
    warnings = []; errors = []; shader = []; editor_compile = []; cleanup = []
    for i, line in enumerate(lines, 1):
        row = {"line": i, "text": line}
        if re.search(r"\bWarning:", line, re.I): warnings.append(row)
        if re.search(r"\bLog\w+:\s*(?:Error|Fatal):|Fatal error:|Assertion failed:|Unhandled Exception:", line, re.I): errors.append(row)
        if re.search(r"shader|material.*compil|compil.*material", line, re.I) and re.search(r"warning|error|failed|failure", line, re.I):
            if cleanup_observation(line, lines[i] if i < len(lines) else "", engine_root): cleanup.append(row)
            else: shader.append(row)
        if re.search(r"(?:Compile|Link).*CubelessMCPUE58Editor", line): editor_compile.append(row)
    require(not errors and not shader and editor_compile, "Build errors/shader diagnostics or expected Editor plugin build evidence absent")
    source = Path(engine_root) / "Source/Runtime/Engine/Private/ShaderCompiler/ShaderGenerationUtil.cpp"
    source_text = source.read_text(encoding="utf-8-sig")
    require("note that it will fail to delete when the file does not exist, which is acceptable" in source_text and 'UE_LOGF(LogShaderCompilers, Display, "Failed to delete old shader autogen file: %ls"' in source_text, "Cleanup source semantics changed")
    return {"log": artifact(path), "build_cook_run_seconds": float(times[0]), "warnings": warnings, "errors": errors, "shader_diagnostics": shader, "shader_autogen_cleanup_observations": cleanup, "cleanup_primary_source": artifact(source), "cleanup_source_lines": [2225, 2232], "cleanup_interpretation": "Exact Display-level deletion observations for this project's SM5/SM6 autogen path, immediately followed by the same-path written message. Source allows DeleteFile failure when absent; the actual deletion failure reason is not inferred. Save/rename/compile failures and Warning/Error remain blocking.", "editor_plugin_build_observations": editor_compile, "scope": "Actual Win64 Shipping BuildCookRun includes Editor plugin compilation for cook, then staged Game proof; not a timing comparison with differently cached D", "shader_completion_verified": False}


def package_evidence(report, manifest):
    require(report.get("schema") == "cubeless_package_independence_v2" and report.get("static_status") == "pass" and report.get("status") == "static_pass" and report.get("violations") == [] and report.get("incomplete_reasons") == [], "Static audit did not pass completely")
    stage = Path(report["stage"]).resolve(); require(stage == STAGE.resolve(), "Wrong installed L stage")
    inventory = prior.package_check._inventory(stage)
    require(len(inventory) == report["file_count"] and hashlib.sha256(json.dumps(inventory, sort_keys=True).encode()).hexdigest() == report["inventory_sha256"], "Staged inventory changed")
    require(not any(prior.package_check.forbidden_references("/" + p) for p in inventory), "Editor/MCP/Python payload staged")
    containers = sorted(p for p in inventory if Path(p).suffix.lower() in {".pak", ".utoc", ".ucas"})
    require(containers and containers == sorted(report["containers"]) and all(Path(p).suffix.lower() == ".pak" for p in containers), "Non-Pak/omitted containers")
    require(report["pak_coverage"] == {"expected": len(containers), "direct_listing_records": len(containers), "complete_listings": len(containers)}, "Incomplete listing coverage")
    require(len(report["pak_listings"]) == len(containers), "Listing count differs")
    paks = []; entries = []; seen = set()
    for row in report["pak_listings"]:
        pak = Path(row["pak"]).resolve(); require(pak.is_relative_to(stage) and pak.relative_to(stage).as_posix() in containers and pak not in seen, "Wrong/duplicate Pak")
        seen.add(pak); require(row["status"] == "pass" and row["exit_code"] == 0 and row["pak_sha256_before"] == row["pak_sha256_after"] == sha(pak), "Pak changed/listing failed")
        listing = Path(row["listing"]); listing = listing if listing.is_absolute() else ROOT / listing
        require(listing.resolve().is_relative_to(prior.contract.RECORDS.resolve()) and sha(listing) == row["listing_sha256"], "Listing hash/path mismatch")
        command = row["command"]; require(len(command) == 4 and Path(command[0]).resolve() == Path(report["unrealpak"]["path"]).resolve() and sha(command[0]) == report["unrealpak"]["sha256"] and Path(command[1]).resolve() == pak and command[2:] == ["-List", "-UTF8Output"], "Direct listing provenance mismatch")
        parsed = prior.parse_listing(listing.read_text(encoding="utf-8-sig", errors="replace"), row["recognized_entries"])
        require(row["reported_entry_totals"] == [len(parsed)], "Entry count mismatch")
        entries.extend(parsed); paks.append({"pak": artifact(pak), "listing": artifact(listing), "entry_count": len(parsed)})
    receipts = []
    for row in report["receipts"]:
        path = Path(row["path"]).resolve(); require(sha(path) == row["sha256"], "Receipt bytes changed")
        value = read_json(path)
        if path.suffix.lower() == ".target":
            require(path.is_relative_to(PROJECT.parent.resolve()) and value.get("TargetName") == PROJECT.stem and value.get("TargetType") == "Game" and value.get("Platform") == "Win64" and value.get("Configuration") == "Shipping" and not prior.package_check.forbidden_references(json.dumps(value)), "Shipping receipt binds wrong/forbidden target")
            matches = [p["Path"] for p in value["BuildProducts"] if p.get("Type") == "Executable" and Path(p["Path"].replace("\\", "/")).name == PROJECT.stem + "-Win64-Shipping.exe"]
            require(matches and row["staged_executable_name_matches"] == matches, "Receipt executable identity differs"); receipts.append(artifact(path))
        else:
            text, _ = prior.package_check._descriptor_references(value); require(not prior.package_check.forbidden_references(text), "Forbidden active staged descriptor")
    require(receipts, "No Game Shipping receipt")
    return {"status": "pass", "stage": str(stage), "file_count": len(inventory), "pak_entry_count": len(entries), "paks": paks, "receipts": receipts, "membership": prior.membership(entries, manifest), "receipt_binding_scope": "Target/configuration/hash and executable basename, not cryptographic build-to-receipt linkage"}


def runtime_evidence(paths, manifest, prior_D):
    baseline = read_json(paths["baseline"]); launch = read_json(paths["launch"]); recorded = read_json(paths["marker"])
    executable = STAGE / PROJECT.stem / "Binaries/Win64" / (PROJECT.stem + "-Win64-Shipping.exe")
    require(launch.get("schema") == "cubeless_runtime_launch_v1" and launch.get("success") is True and launch.get("stage") == "exited" and launch.get("automatic_retry") is False and launch.get("exit_code") == 0 and Path(launch["executable"]).resolve() == executable.resolve(), "Wrong/failed L launch")
    require(Path(launch["baseline"]).resolve() == paths["baseline"] and launch["baseline_sha256"] == sha(paths["baseline"]), "Launch baseline binding differs")
    user_dir = ROOT / "cubeless/workspaces/InstalledCurrentRuntimeUser-L"
    require(Path(baseline["runtime_user_dir"]).resolve() == user_dir.resolve() and launch["arguments"] == ["-nullrhi", "-unattended", "-NoSplash", "-NoSound", "-UserDir=" + str(user_dir.resolve())], "Unexpected user directory or map override")
    require(launch["launcher_sha256"] == sha(Path(__file__).with_name("launch_runtime_fixture.py")), "Launcher changed")
    observed = prior.marker_check.verify(manifest["marker_fixture_report"]["path"], baseline, launch)
    require(observed["success"] is True and recorded.get("success") is True, "Fresh marker revalidation failed")
    for key in ("schema", "checks", "verifier_sha256", "timestamp_tolerance_ns", "header", "marker", "runtime_process"): require(recorded[key] == observed[key], "Recorded marker/header/process mismatch: " + key)
    console = Path(launch["console_log"]); require(console.resolve().is_relative_to(prior.contract.RECORDS.resolve()) and not prior.package_check._RUNTIME_ERROR.search(console.read_text(encoding="utf-8-sig", errors="replace")), "Runtime console error")
    require(sha(executable) == prior_D["runtime"]["executable"]["sha256"] and sha(prior_D["runtime"]["executable"]["path"]) == sha(executable), "Observed D and installed L Game executable hashes differ")
    return {"status": "pass", "pid": launch["pid"], "exit_code": launch["exit_code"], "seconds": (launch["ended_ns"] - launch["started_ns"]) / 1e9, "marker": observed["marker"], "header": observed["header"], "executable": artifact(executable), "console": artifact(console), "same_game_executable_as_D": True, "scope": "Exact installed-L staged Shipping executable, no map override, fresh authored BeginPlay marker; nullrhi rendering unverified"}


def build(paths):
    result = {"schema": "cubeless_installed_current_runtime_proof_v1", "success_within_scope": False, "scope": "Distribution L installed for Editor only, exact D fixture bytes, actual Shipping cook/Pak exclusion and fresh game marker", "full_migration_complete": False, "actor_M_included": False, "shader_completion_verified": False, "rendering_quality": "not_verified", "engine_execution_performed_by_this_verifier": False}
    try:
        require(set(paths) == set(DEFAULTS), "Missing evidence roles")
        paths = {k: Path(p).resolve() for k, p in paths.items()}
        require(all(p.is_relative_to(prior.contract.RECORDS.resolve()) and p.is_file() for p in paths.values()), "Missing/outside evidence")
        require(sha(Path(prior.__file__)) == D_VERIFIER_SHA, "Frozen D verifier source changed")
        prior_D = read_json(paths["prior_D"]); check = prior.verify_summary(prior_D)
        require(check["passed"], "Independent D proof no longer passes")
        author = read_json(prior_D["artifacts"]["author"]["path"]); manifest = read_json(prior_D["manifest"]["path"])
        result["artifacts"] = {k: artifact(p) for k, p in paths.items()}; result["prior_D_independent"] = check
        result["preparation"] = preparation_evidence(read_json(paths["preparation"]), manifest, author)
        package_report = read_json(paths["package"])
        result["build"] = build_evidence(paths["build_log"], manifest, Path(package_report["unrealpak"]["path"]).resolve().parents[2])
        result["package"] = package_evidence(package_report, manifest)
        result["runtime"] = runtime_evidence(paths, manifest, prior_D)
        result["sources"] = [artifact(Path(__file__)), artifact(Path(prior.__file__)), artifact(Path(prior.marker_check.__file__)), artifact(Path(prior.package_check.__file__))]
        result["limitations"] = ["Distribution L only; Actor M implementation and all untested legacy tools/pipelines are excluded.", "Installed source/Python text/descriptor hashes are preserved; Editor DLL/PDB/module files, Intermediate outputs and Python bytecode can be rebuilt by UAT.", "The 23 saved inputs are the exact D cohort. 22 packages including the new map must be cooked; the old source map remains excluded.", "Editor compilation/load during UAT does not prove every MCP tool invocation. Dedicated official-MCP evidence is separate.", "Pak names, receipt and fresh runtime marker jointly support this fixture; dynamic string loads and native binary import analysis remain outside the audit.", "Identical D/L Shipping executable hashes are an observation, not a universal binary reproducibility or full dependency proof.", "Build timings differ in cache state and Editor-plugin build workload; no performance-speedup claim.", "Shader completion, rendered quality, every Blueprint code path and complete migration are not verified."]
        result["success_within_scope"] = True
    except (RuntimeError, ValueError, TypeError, KeyError, IndexError, OSError, AttributeError) as error: result["error"] = str(error)
    return result


def verify(summary):
    try:
        require(summary.get("schema") == "cubeless_installed_current_runtime_proof_v1" and summary.get("success_within_scope") is True and summary.get("full_migration_complete") is False and summary.get("actor_M_included") is False and summary.get("shader_completion_verified") is False, "Incomplete/overstated installed proof")
        for row in summary["artifacts"].values(): require(artifact(row["path"]) == row, "Raw evidence fingerprint changed")
        require(build({k: r["path"] for k, r in summary["artifacts"].items()}) == summary, "Independent installed proof recomputation differs")
        return {"schema": "cubeless_installed_current_runtime_independent_v1", "passed": True, "errors": [], "scope": summary["scope"], "full_migration_complete": False, "actor_M_included": False, "shader_completion_verified": False, "rendering_quality": "not_verified"}
    except (RuntimeError, ValueError, TypeError, KeyError, OSError) as error: return {"schema": "cubeless_installed_current_runtime_independent_v1", "passed": False, "errors": [str(error)]}


def markdown(report):
    if not report["success_within_scope"]: return "# Installed runtime L proof\n\nIncomplete: " + report.get("error", "unknown") + "\n"
    p = report["preparation"]; b = report["build"]; r = report["runtime"]
    return "\n".join(["# Distribution L 설치 상태의 Shipping 독립성 검증", "", "Cubeless 플러그인을 Editor 대상으로 설치한 별도 프로젝트에서 D와 동일한 저장 에셋을 Cook·패키징·실행했다. 이 문서는 L의 지정된 에셋과 실행 범위이며 Actor M이나 전체 이식 완료를 뜻하지 않는다.", "", f"- 저장 에셋 23개와 설정의 원본/복사 SHA256 일치. 초기 플러그인 90개 파일 중 Source·Python 소스·descriptor {p['protected_count']}개는 배포 L과 설치본 모두 보존됐다.", f"- UAT가 바꾼 Binaries·Intermediate·Python bytecode는 별도 기록했다({len(p['generated_changes'])}개). 삭제된 초기 생성물 {len(p['removed_generated_files'])}개도 기록했다. 이 생성물을 소스 불변 검사와 혼동하지 않았다.", f"- Win64 Shipping BuildCookRun {b['build_cook_run_seconds']}초. Editor 플러그인 빌드가 포함되므로 D와 시간 비교를 통한 성능 주장은 하지 않는다.", f"- Pak {report['package']['pak_entry_count']:,}개 항목에서 정확한 필수 22개 패키지와 이전 맵 부재를 확인했다. Shipping receipt·stage·Pak에서 MCP/Toolset/Python 런타임 포함 위반은 없었다.", f"- Shipping PID {r['pid']} 정상 종료, {r['seconds']:.5f}초. 새로운 UserDir의 {r['marker']['size']}바이트 SaveGame을 헤더·정확한 클래스·mtime·실행 파일 SHA로 재검수했다. Game 실행 파일은 D와 SHA256이 같다.", "", f"ShaderCompiler의 정확한 Display 수준 old-autogen 삭제 관측 {len(b['shader_autogen_cleanup_observations'])}행은 같은 경로의 후속 written 로그 및 ShaderGenerationUtil.cpp 소스 근거로 별도 기록했다. 삭제 실패 원인은 추정하지 않으며 Warning/Error·save/rename/compile 실패와 구분한다.", "", "실제 화면 품질·shader 완료·모든 Blueprint 경로·모든 MCP 호출은 검증하지 않았다. nullrhi 실행의 범위와 D에서 보존한 B 레지스트리 누락/C IoStore 검사 미완료 기록을 그대로 유지한다.", ""])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for role, name in DEFAULTS.items(): parser.add_argument("--" + role.replace("_", "-"), type=Path, default=prior.contract.RECORDS / name)
    parser.add_argument("--verify", type=Path); parser.add_argument("--output", required=True, type=Path); parser.add_argument("--markdown", type=Path)
    args = parser.parse_args(); require(not args.output.exists() and (not args.markdown or not args.markdown.exists()), "Refusing evidence overwrite")
    result = verify(read_json(args.verify)) if args.verify else build({k: getattr(args, k) for k in DEFAULTS})
    with args.output.open("x", encoding="utf-8") as f: json.dump(result, f, ensure_ascii=False, indent=2)
    if args.markdown:
        require(not args.verify, "Markdown applies to build mode")
        with args.markdown.open("x", encoding="utf-8") as f: f.write(markdown(result))
    print(json.dumps({"passed": result.get("passed", result.get("success_within_scope")), "error": result.get("error", result.get("errors"))})); return 0 if result.get("passed", result.get("success_within_scope")) else 1


if __name__ == "__main__": raise SystemExit(main())
