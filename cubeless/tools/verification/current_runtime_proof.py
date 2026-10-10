"""Read-only combined proof for the exact current B/C editor and D Shipping cohort.

No UnrealPak, editor, build or game process is launched. Existing direct listings,
process evidence and source/staged bytes are independently checked. This scoped
proof does not declare complete migration, shader completion or rendering quality.
"""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_package_independence as package_check
import current_runtime_manifest as contract
import current_runtime_report as editor_check
import validate_runtime_marker as marker_check
from current_runtime_manifest import read_json, require

DEFAULTS = {
    "author": "current-runtime-fixture-B.json", "cold": "current-runtime-cold-C.json",
    "package": "current-package-static-D.json", "build_log": "build-current-runtime-shipping-D.log",
    "baseline": "current-runtime-marker-baseline-D.json", "launch": "current-runtime-launch-D.json",
    "marker": "current-runtime-marker-verification-D.json", "previous_package": "current-package-static-C.json",
    "previous_build_log": "build-current-runtime-shipping-C.log",
    "packaging_record": "current-runtime-packaging-config-D.json",
}
STAGE = contract.ROOT / "cubeless/build/CurrentRuntimeShipping-20261010-D/Windows"


def sha(path): return package_check.sha256(Path(path))
def artifact(path):
    path = Path(path).resolve()
    return {"path": str(path), "sha256": sha(path), "bytes": path.stat().st_size}


def checked_records(paths):
    require(set(paths) == set(DEFAULTS), "Evidence role coverage differs")
    result = {}
    for role, path in paths.items():
        path = Path(path).resolve()
        require(path.is_relative_to(contract.RECORDS.resolve()) and path.is_file(), "Evidence missing or outside owned records: " + role)
        result[role] = path
    return result


def packaging_config(manifest):
    path = contract.PROJECT.parent / "Config/DefaultGame.ini"
    text = path.read_text(encoding="utf-8-sig")
    parser = configparser.ConfigParser(interpolation=None, strict=True); parser.optionxform = str
    parser.read_string(text); settings = parser["/Script/UnrealEd.ProjectPackagingSettings"]
    require(all(settings.get(k) == v for k, v in {"bUsePakFile": "True", "bUseIoStore": "False", "bUseZenStore": "False", "bCookAll": "False", "bSkipEditorContent": "True", "bGenerateChunks": "False"}.items()), "Require explicit Pak-only map-scoped packaging")
    require(settings.get("+MapsToCook") == '(FilePath="' + manifest["default_map"] + '")', "Packaging map differs")
    require(not any(k.lstrip("+-!").startswith("DirectoriesTo") for k in settings), "Unexpected extra cook/stage directories")
    return dict(artifact(path), text=text, scope="DefaultGame packaging configuration added after B/C; original project descriptor and DefaultEngine SHA still checked by B/C verifier")


def dependency_query_source(package_report, query_options):
    engine = Path(package_report["unrealpak"]["path"]).resolve().parents[2]
    path = engine / "Source/Runtime/AssetRegistry/Private/AssetRegistry.cpp"
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    start = next(i for i, line in enumerate(lines) if line.startswith("bool FAssetRegistryDependencyOptions::GetPackageQuery("))
    end = next(i for i in range(start + 1, len(lines)) if lines[i] == "}")
    snippet = "\n".join(lines[start:end + 1])
    require("if (bIncludeSoftPackageReferences || bIncludeHardPackageReferences)" in snippet and "if (bIncludeGamePackageReferences || bIncludeEditorOnlyPackageReferences)" in snippet and "if (!bIncludeGamePackageReferences) Flags.Excluded |= UE::AssetRegistry::EDependencyProperty::Game;" in snippet and "if (!bIncludeEditorOnlyPackageReferences) Flags.Required |= UE::AssetRegistry::EDependencyProperty::Game;" in snippet, "Installed dependency-query source changed; review semantics before interpreting flags")
    require(all(query_options[k]["include_game_package_references"] is False and query_options[k]["include_editor_only_package_references"] is False for k in ("hard_package", "soft_package")), "Recorded query does not match source-based no-Game-filter interpretation")
    return {"source": artifact(path), "function": "FAssetRegistryDependencyOptions::GetPackageQuery", "line_start": start + 1, "line_end": end + 1, "function_text": snippet, "recorded_options": query_options, "interpretation": "With both Game and EditorOnly selectors false, the Game-property branch does not execute. Each selected Hard/Soft package query therefore has no Game-property filter and includes editor-only as well as game package references. False here does not mean exclude editor-only.", "historical_correction": "material-runtime-proof-B.md item 6 and operations-material-runtime-proof-B-independent.json limitations[1] incorrectly interpreted the false editor-only selector in isolation. Their raw observations and Pak/marker evidence are preserved; see operations-material-runtime-B-query-scope-correction-D.json."}


def build_observation(path, manifest, expected_archive):
    text = Path(path).read_text(encoding="utf-8-sig", errors="replace"); lines = text.splitlines()
    commands = [line for line in lines if line.startswith("Parsing command line: BuildCookRun ")]
    require(len(commands) == 1, "Build log lacks one unambiguous BuildCookRun command")
    command = commands[0]
    def option(name):
        found = re.findall(r"(?<!\S)-" + re.escape(name) + r'=("[^"]+"|\S+)', command, re.I)
        require(len(found) == 1, "Missing/duplicate build option: " + name); return found[0].strip('"')
    require(Path(option("project")).resolve() == contract.PROJECT.resolve() and Path(option("archivedirectory")).resolve() == Path(expected_archive).resolve(), "Build project/archive differs")
    require(option("map") == manifest["default_map"] and option("clientconfig").lower() == "shipping" and option("platform").lower() == "win64", "Build map/target/config differs")
    require(all(re.search(r"(?<!\S)-" + name + r"(?=\s|$)", command, re.I) for name in ("build", "cook", "stage", "pak", "package", "archive")), "Build skipped required phases")
    require("BUILD SUCCESSFUL" in lines and any("COOK COMMAND COMPLETED" in line for line in lines) and lines[-1].strip() == "AutomationTool exiting with ExitCode=0 (Success)", "Build/cook completion or actual UAT exit absent")
    times = re.findall(r"^BuildCookRun time: ([0-9.]+) s$", text, re.M)
    require(len(times) == 1 and float(times[0]) > 0, "Build duration missing/ambiguous")
    warnings = []; shader_diagnostics = []; errors = []; mentions = {p: [] for p in manifest["required_cooked_packages"]}
    for number, line in enumerate(lines, 1):
        row = {"line": number, "text": line}
        if re.search(r"\bLog\w+:\s*(?:Error|Fatal):|Fatal error:|Assertion failed:|Unhandled Exception:", line, re.I): errors.append(row)
        if re.search(r"\bWarning:", line, re.I): warnings.append(row)
        if re.search(r"shader|material.*compil|compil.*material", line, re.I) and re.search(r"warning|error|failed|failure", line, re.I): shader_diagnostics.append(row)
        normalized = line.replace("\\", "/")
        for package in mentions:
            if re.search(re.escape(package) + r'(?=[\s.\"\x27,)]|$)', normalized): mentions[package].append(row)
    require(not errors, "Build/cook log reports errors")
    require(not shader_diagnostics, "Cook reports shader/material warning or error; inspect without claiming valid shaders")
    return {"log": artifact(path), "success_markers_verified": True, "build_cook_run_seconds": float(times[0]), "measurement_conditions": "One local Win64 Shipping BuildCookRun, existing DDC/build cache; not a cross-workload performance comparison", "warnings": warnings, "errors": errors, "shader_diagnostics": shader_diagnostics, "target_mentions": mentions, "missing_log_mentions": [p for p, rows in mentions.items() if not rows], "log_only_asset_coverage": "complete" if all(mentions.values()) else "incomplete", "shader_completion_verified": False, "scope": "Actual UAT completion and recorded diagnostics only; exact asset inclusion is checked from Pak entries"}


def expected_entries(manifest):
    return {p: contract.PROJECT.stem + "/" + contract.relative_asset(p, p == manifest["default_map"]) for p in manifest["required_cooked_packages"]}


def parse_listing(text, expected_count):
    entries = package_check._PAK_ENTRY.findall(text); totals = package_check._PAK_TOTAL.findall(text)
    require(len(totals) == 1 and int(totals[0]) == len(entries) == expected_count and entries, "Pak listing entry totals differ")
    normalized = [e.replace("\\", "/") for e in entries]
    require(len(set(normalized)) == len(normalized) and not any(e.startswith("/") or ".." in e.split("/") or "<FileNamesNotLoaded" in e for e in normalized), "Duplicate/unsafe/unavailable Pak entry names")
    require(not any(package_check.forbidden_references(e) for e in normalized), "Forbidden plugin or Python entry in Pak")
    require('mount point "../../../"' in text, "Unexpected Pak mount point")
    return normalized


def membership(entries, manifest):
    wanted = expected_entries(manifest); observed = set(entries)
    require(set(wanted.values()) <= observed, "One or more of the 22 exact cooked package entries is missing")
    old = contract.PROJECT.stem + "/" + contract.relative_asset(manifest["excluded_old_map"], True)
    require(old not in observed and old.removesuffix(".umap") + ".uexp" not in observed, "Old marker map was cooked")
    return {"required_package_count": len(wanted), "exact_entries": wanted, "excluded_old_map_entry": old, "old_map_absent": True}


def static_evidence(report, manifest):
    require(report.get("schema") == "cubeless_package_independence_v2" and report.get("static_status") == "pass" and report.get("status") == "static_pass" and report.get("violations") == [] and report.get("incomplete_reasons") == [] and report.get("full_migration_complete") is False, "Static package checker did not complete cleanly")
    stage = Path(report["stage"]).resolve(); require(stage == STAGE.resolve(), "Expected exact D staged build")
    inventory = package_check._inventory(stage)
    require(len(inventory) == report["file_count"] and hashlib.sha256(json.dumps(inventory, sort_keys=True).encode()).hexdigest() == report["inventory_sha256"], "Staged inventory changed after direct package inspection")
    require(not any(package_check.forbidden_references("/" + name) for name in inventory), "Forbidden staged file")
    containers = sorted(n for n in inventory if Path(n).suffix.lower() in {".pak", ".utoc", ".ucas"})
    require(containers == sorted(report["containers"]) and containers and all(Path(n).suffix.lower() == ".pak" for n in containers), "IoStore or incomplete container inventory cannot pass Pak proof")
    require(report["pak_coverage"] == {"expected": len(containers), "direct_listing_records": len(containers), "complete_listings": len(containers)}, "Incomplete direct Pak coverage")
    require(len(report["pak_listings"]) == len(containers), "Direct listing count differs")
    paks = []; entries = []
    for row in report["pak_listings"]:
        pak = Path(row["pak"]).resolve(); require(pak.is_relative_to(stage) and pak.relative_to(stage).as_posix() in containers, "Listing names another staged Pak")
        require(row["status"] == "pass" and row["exit_code"] == 0 and row["pak_sha256_before"] == row["pak_sha256_after"] == sha(pak), "Pak bytes/listing command failed or changed")
        listing = Path(row["listing"]); listing = listing if listing.is_absolute() else contract.ROOT / listing
        require(listing.resolve().is_relative_to(contract.RECORDS.resolve()) and sha(listing) == row["listing_sha256"], "Listing bytes/path differ")
        command = row["command"]
        require(len(command) == 4 and Path(command[0]).resolve() == Path(report["unrealpak"]["path"]).resolve() and Path(command[1]).resolve() == pak and command[2:] == ["-List", "-UTF8Output"], "Not an exact direct UnrealPak list command")
        require(sha(command[0]) == report["unrealpak"]["sha256"], "UnrealPak executable identity differs")
        parsed = parse_listing(listing.read_text(encoding="utf-8-sig", errors="replace"), row["recognized_entries"])
        require(row["reported_entry_totals"] == [len(parsed)], "Reported Pak count differs")
        entries.extend(parsed); paks.append({"pak": artifact(pak), "listing": artifact(listing), "entry_count": len(parsed)})
    receipts = []
    for row in report["receipts"]:
        path = Path(row["path"]).resolve(); require(sha(path) == row["sha256"], "Receipt changed")
        value = read_json(path)
        if path.suffix.lower() == ".target":
            require(value.get("TargetName") == contract.PROJECT.stem and value.get("TargetType") == "Game" and value.get("Platform") == "Win64" and value.get("Configuration") == "Shipping", "Receipt is not the exact Shipping Game target")
            require(not package_check.forbidden_references(json.dumps(value)), "Forbidden module in receipt")
            matches = [p["Path"] for p in value["BuildProducts"] if p.get("Type") == "Executable" and Path(p["Path"].replace("\\", "/")).name == contract.PROJECT.stem + "-Win64-Shipping.exe"]
            require(matches and row["staged_executable_name_matches"] == matches, "Receipt does not identify staged Shipping executable")
            receipts.append(artifact(path))
        else:
            filtered, _ = package_check._descriptor_references(value)
            require(not package_check.forbidden_references(filtered), "Forbidden active descriptor reference")
    require(receipts, "No independent Shipping receipt")
    return {"status": "pass", "stage": str(stage), "file_count": len(inventory), "pak_entry_count": len(entries), "paks": paks, "receipts": receipts, "membership": membership(entries, manifest), "receipt_binding_scope": "Exact target/configuration, file hash and staged executable name; not cryptographic receipt-to-build provenance"}


def runtime_evidence(manifest, paths):
    baseline = read_json(paths["baseline"]); launch = read_json(paths["launch"]); recorded = read_json(paths["marker"])
    marker_fixture = manifest["marker_fixture_report"]
    require(sha(marker_fixture["path"]) == marker_fixture["sha256"], "Marker author bytes changed")
    require(launch.get("schema") == "cubeless_runtime_launch_v1" and launch.get("success") is True and launch.get("stage") == "exited" and launch.get("automatic_retry") is False and launch.get("exit_code") == 0, "Runtime launch did not exit normally")
    require(Path(launch["baseline"]).resolve() == paths["baseline"] and launch["baseline_sha256"] == sha(paths["baseline"]), "Runtime launch baseline binding differs")
    executable = STAGE / contract.PROJECT.stem / "Binaries/Win64" / (contract.PROJECT.stem + "-Win64-Shipping.exe")
    require(Path(launch["executable"]).resolve() == executable.resolve(), "Game launch used another build")
    require(Path(baseline["runtime_user_dir"]).resolve().is_relative_to((contract.ROOT / "cubeless/workspaces").resolve()), "Runtime UserDir is outside owned workspace")
    require(launch["arguments"] == ["-nullrhi", "-unattended", "-NoSplash", "-NoSound", "-UserDir=" + str(Path(baseline["runtime_user_dir"]).resolve())], "Unexpected launch arguments or map override")
    require(launch["launcher_sha256"] == sha(Path(__file__).with_name("launch_runtime_fixture.py")), "Launcher bytes differ")
    observed = marker_check.verify(marker_fixture["path"], baseline, launch)
    require(observed["success"] is True and recorded.get("success") is True, "Fresh runtime marker revalidation failed")
    for name in ("schema", "checks", "verifier_sha256", "timestamp_tolerance_ns", "header", "marker", "runtime_process"):
        require(recorded[name] == observed[name], "Recorded marker proof disagrees with independent header/process check: " + name)
    console = Path(launch["console_log"])
    require(console.resolve().is_relative_to(contract.RECORDS.resolve()) and not package_check._RUNTIME_ERROR.search(console.read_text(encoding="utf-8-sig", errors="replace")), "Runtime console reports a fatal/load error")
    return {"status": "pass", "pid": launch["pid"], "exit_code": launch["exit_code"], "seconds": (launch["ended_ns"] - launch["started_ns"]) / 1e9, "marker": observed["marker"], "header": observed["header"], "executable": artifact(executable), "console": artifact(console), "scope": "Exact D executable with no map override produced fresh BeginPlay SaveGame under isolated UserDir; nullrhi does not verify rendering"}


def build(paths):
    result = {"schema": "cubeless_current_runtime_proof_v1", "success_within_scope": False, "scope": "Exact saved fixture cohort B/C editor readback plus D Pak-only Shipping inclusion and fresh BeginPlay marker", "full_migration_complete": False, "shader_completion_verified": False, "rendering_quality": "not_verified", "automatic_retry": False, "engine_execution_performed_by_this_verifier": False, "checks": []}
    try:
        paths = checked_records(paths); result["artifacts"] = {role: artifact(path) for role, path in paths.items()}
        author_bytes = paths["author"].read_bytes(); author = json.loads(author_bytes); cold = read_json(paths["cold"])
        manifest = read_json(author["manifest"]["path"])
        result["manifest"] = artifact(author["manifest"]["path"])
        result["editor_paired"] = editor_check.verify(cold, author, author_bytes)
        require(result["editor_paired"]["passed"], "B/C independent editor pair failed")
        result["author_standalone"] = editor_check.verify(author)
        require(result["author_standalone"]["passed"] is False and result["author_standalone"]["errors"] == ["Connected runtime clone lacks MPC hard reference"], "Expected original B incomplete registry observation changed")
        result["packaging_configuration"] = packaging_config(manifest)
        preparation = read_json(paths["packaging_record"])
        require(preparation.get("status") == "pass" and preparation.get("asset_mutations") is False and Path(preparation["added_file"]).resolve() == Path(result["packaging_configuration"]["path"]).resolve() and preparation["sha256"] == result["packaging_configuration"]["sha256"] and preparation["settings"] == result["packaging_configuration"]["text"] and preparation["unchanged_author_configuration"] == author["configuration"] and preparation["prior_iostore_status"] == "incomplete_unsupported_contents", "Packaging preparation is not bound to unchanged B/C configuration")
        result["build"] = build_observation(paths["build_log"], manifest, STAGE.parent)
        package_report = read_json(paths["package"])
        result["package"] = static_evidence(package_report, manifest)
        result["dependency_query_source"] = dependency_query_source(package_report, cold["dependency_audit"]["query_options"])
        result["runtime"] = runtime_evidence(manifest, paths)
        previous = read_json(paths["previous_package"])
        require(previous.get("status") == previous.get("static_status") == "incomplete" and previous.get("violations") == [] and previous.get("incomplete_reasons") == ["IoStore contents require a separate verified IoStore listing; this checker does not parse them"] and any(Path(p).suffix == ".utoc" for p in previous["containers"]), "C inspection history changed or has unreported violations")
        previous_archive = contract.ROOT / "cubeless/build/CurrentRuntimeShipping-20261010-C"
        result["prior_C"] = {"build": build_observation(paths["previous_build_log"], manifest, previous_archive), "package_status": "incomplete", "reason": previous["incomplete_reasons"][0], "interpretation": "Build succeeded; IoStore package contents were not inspected. Zero reported violations did not close the proof. C evidence was preserved and D used explicitly configured Pak-only containers."}
        result["source_files"] = [artifact(Path(__file__).with_name(n)) for n in ("current_runtime_proof.py", "current_runtime_report.py", "current_runtime_manifest.py", "create_current_runtime_fixture.py", "validate_current_runtime_fixture.py", "check_package_independence.py", "validate_runtime_marker.py", "launch_runtime_fixture.py")]
        result["limitations"] = ["Applies to the exact 19 copied source packages, 3 official-API connected Material clones and 1 new map. The old copied source map is excluded from the 22 required Pak packages.", "Original Material graphs and official-API MPC-connected clones are distinct evidence; these fixture connections are not extra migrated Cubeless tools.", "New Actor tools, untested legacy tools/pipelines and complete migration are outside this proof.", "Reported compile/cook success and nullrhi execution do not prove per-material shader completion or rendered quality.", "Runtime proves fresh authored BeginPlay SaveGame behavior, not execution of every Blueprint path or shader.", "Registry includes editor-only references and available management references. Dynamic string loads and native binary import analysis are not covered.", "IoStore C is retained as incomplete inspection; D does not retroactively validate C.", "The UAT content-project temporary Core module is generated build infrastructure, not user-authored gameplay C++."]
        result["success_within_scope"] = True
    except (RuntimeError, ValueError, TypeError, KeyError, IndexError, OSError, AttributeError) as error:
        result["error"] = str(error)
    return result


def verify_summary(summary):
    """Recompute proof from bound raw evidence and compare substantive content."""
    try:
        require(summary.get("schema") == "cubeless_current_runtime_proof_v1" and summary.get("success_within_scope") is True and summary.get("full_migration_complete") is False and summary.get("shader_completion_verified") is False and summary.get("rendering_quality") == "not_verified", "Incomplete/overstated summary")
        paths = {role: row["path"] for role, row in summary["artifacts"].items()}
        for row in summary["artifacts"].values(): require(artifact(row["path"]) == row, "Bound raw evidence changed")
        rebuilt = build(paths); require(rebuilt == summary, "Combined proof does not match independent recomputation")
        return {"schema": "cubeless_current_runtime_proof_independent_v1", "passed": True, "errors": [], "scope": summary["scope"], "shader_completion_verified": False, "rendering_quality": "not_verified", "full_migration_complete": False}
    except (RuntimeError, ValueError, TypeError, KeyError, OSError) as error:
        return {"schema": "cubeless_current_runtime_proof_independent_v1", "passed": False, "errors": [str(error)]}


def markdown(report):
    if not report["success_within_scope"]: return "# Current runtime proof D\n\nIncomplete: " + report.get("error", "Unknown error") + "\n"
    text = "\n".join(["# 현재 저장 에셋의 MCP 없는 Shipping 검증 D", "", "UE 5.8.3의 지정된 저장 에셋 묶음에 대한 검증이다. 전체 이식 또는 화면 품질 완료를 뜻하지 않는다.", "", "- B 생성과 C 새 프로세스: 원본 19개·복사 19개·새 4개 파일 해시, Material/Custom/MPC 속성과 연결, 맵의 8개 큐브와 Blueprint 8개 컴파일을 독립 확인했다.", "- B 직후 새 clone의 레지스트리 연결 누락은 단독 실패로 보존했다. C의 새 프로세스 디스크 조회로 같은 저장 파일의 연결을 확인해 결합했다.", f"- D Win64 Shipping BuildCookRun {report['build']['build_cook_run_seconds']}초. 로컬 캐시를 사용하는 단일 측정이며 비교 성능 수치가 아니다.", f"- Pak 항목 {report['package']['pak_entry_count']:,}개에서 필수 22개 패키지를 정확히 확인했다. 이전 맵은 없고 MCP/Python/Toolset 관련 staged·receipt·Pak 위반은 없었다.", f"- D 실행 PID {report['runtime']['pid']}, 정상 종료, {report['runtime']['seconds']:.3f}초. 새 UserDir에 생성된 SaveGame 헤더·클래스·시간·실행 파일 해시를 다시 검수했다.", f"- Cook 로그의 일반 Warning 관측 {len(report['build']['warnings'])}행, shader/material 진단 {len(report['build']['shader_diagnostics'])}행이다. 개별 에셋 로그 누락은 {report['build']['log_only_asset_coverage']}로 유지하고 포함 여부는 Pak에서 확인했다.", "", "C는 빌드 성공 후 IoStore 내용 검사 미지원으로 검사 미완료였다. C 기록을 보존하고 DefaultGame.ini에 Pak-only와 대상 맵을 명시한 별도 D를 검증했다. B/C의 기존 descriptor·DefaultEngine 해시는 그대로다.", "", "Custom HLSL의 BaseColor 연결 및 MPC 사용 연결은 저장·재로드·Cook 대상으로 포함됐다. shader 완료 시점, 실제 화면 품질, 모든 Blueprint 경로, 새 Actor 도구 및 나머지 legacy 기능은 검증하지 않았다. 원본 그래프와 공개 API로 만든 연결 clone은 구분한다.", ""])

    source = report["dependency_query_source"]
    return text + f"\nAssetRegistry 조회 해석: 설치 소스 `{source['source']['path']}` {source['line_start']}–{source['line_end']}행, SHA256 `{source['source']['sha256']}`. Game/EditorOnly selector가 둘 다 false여서 Game 속성 필터가 없으므로 editor-only 참조도 포함한다. 과거 B의 반대 해석은 `operations-material-runtime-B-query-scope-correction-D.json/md`로 정정했으며 원본 기록은 보존했다.\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for role, name in DEFAULTS.items(): parser.add_argument("--" + role.replace("_", "-"), type=Path, default=contract.RECORDS / name)
    parser.add_argument("--verify", type=Path); parser.add_argument("--output", required=True, type=Path); parser.add_argument("--markdown", type=Path)
    args = parser.parse_args(); require(not args.output.exists() and (not args.markdown or not args.markdown.exists()), "Refusing to overwrite proof evidence")
    report = verify_summary(read_json(args.verify)) if args.verify else build({role: getattr(args, role) for role in DEFAULTS})
    with args.output.open("x", encoding="utf-8") as stream: json.dump(report, stream, ensure_ascii=False, indent=2)
    if args.markdown:
        require(not args.verify, "Markdown applies to proof generation")
        with args.markdown.open("x", encoding="utf-8") as stream: stream.write(markdown(report))
    print(json.dumps({"passed": report.get("success_within_scope", report.get("passed")), "error": report.get("error", report.get("errors")), "output": str(args.output.resolve())}, ensure_ascii=False))
    return 0 if report.get("success_within_scope", report.get("passed")) else 1


if __name__ == "__main__": raise SystemExit(main())
