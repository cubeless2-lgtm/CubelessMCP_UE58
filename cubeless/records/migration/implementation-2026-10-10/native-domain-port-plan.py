"""Read-only source measurements for the domain-port decision; not a compiler."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
OLD = Path(r"C:\Git\UnrealMCP5_6_1")
BASE = OLD / "MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP"
ENGINE = Path(r"C:\Program Files\Epic Games\UE_5.8\Engine")
inventory = json.loads((HERE.parent / "planning-2026-10-10/legacy-feature-inventory.json").read_text(encoding="utf-8"))
runtime = json.loads((HERE / "legacy-runtime-registration.eager.json").read_text(encoding="utf-8"))
schemas = {item["name"]: item for item in runtime["catalog_tools"]}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def measure(paths):
    result = []
    for path in sorted(paths):
        body = path.read_text(encoding="utf-8-sig")
        result.append({
            "path": path.relative_to(OLD).as_posix(),
            "sha256": digest(path),
            "line_count": len(body.splitlines()),
            "bytes": path.stat().st_size,
            "includes": re.findall(r'^#include\s+["<]([^">]+)[">]', body, re.M),
            "common_utils_calls": sorted(set(re.findall(r'FUnrealMCPCommonUtils::(\w+)', body))),
            "legacy_class_references": sorted(set(re.findall(r'\b[UFA]UnrealMCP\w+', body))),
            "risk_markers": [{"line": line, "text": text.strip()} for line, text in enumerate(body.splitlines(), 1) if re.search(r'UnrealMCPBridge|FUnrealMCPModule|MCPGameProject|\bUE_VERSION_|/Private/|\bFinishCompilation\(|\bFinishAllCompilation\(|const_cast<|/Game/UltraDynamicSky|NiagaraPreviewPlayerWindow', text)],
        })
    return result


domain_paths = {
    "material": [BASE / "Private/Commands/UnrealMCPMaterialCommands.cpp", BASE / "Public/Commands/UnrealMCPMaterialCommands.h"],
    "editor": [BASE / "Private/Commands/UnrealMCPEditorCommands.cpp", BASE / "Public/Commands/UnrealMCPEditorCommands.h"],
    "blueprint_core": [BASE / "Private/Commands/UnrealMCPBlueprintCommands.cpp", BASE / "Private/Commands/UnrealMCPBlueprintCommands_Physics.cpp", BASE / "Public/Commands/UnrealMCPBlueprintCommands.h"],
    "blueprint_nodes": [*BASE.glob("Private/Commands/UnrealMCPBlueprintNodeCommands*"), BASE / "Public/Commands/UnrealMCPBlueprintNodeCommands.h"],
    "common_utils": [BASE / "Private/Commands/UnrealMCPCommonUtils.cpp", BASE / "Public/Commands/UnrealMCPCommonUtils.h"],
    "batch_bridge_container": [BASE / "Private/UnrealMCPBridge.cpp", OLD / "Python/tools/batch_tools.py"],
}
domains = {name: measure(paths) for name, paths in domain_paths.items()}
engine_paths = subprocess.run(["rg", "--files", str(ENGINE / "Source"), str(ENGINE / "Plugins"), "-g", "*.h"], capture_output=True, text=True, check=True).stdout.splitlines()
engine_normalized = [path.replace("\\", "/") for path in engine_paths]
material = domains["material"][0]
include_resolution = []
for include in material["includes"]:
    matches = [path for path in engine_normalized if path.endswith("/" + include)]
    local = [str(path) for path in [BASE / "Public" / include, BASE / "Private" / include] if path.exists()]
    include_resolution.append({"include": include, "legacy_local": local, "ue58_engine_matches": matches, "has_public_or_classes_match": any("/Public/" in path or "/Classes/" in path for path in matches), "private_or_internal_only": bool(matches) and not any("/Public/" in path or "/Classes/" in path for path in matches)})

material_path = BASE / "Private/Commands/UnrealMCPMaterialCommands.cpp"
body = material_path.read_text(encoding="utf-8-sig")
handlers = list(re.finditer(r'TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::(Handle\w+)\(', body))
handler_info = {}
for index, match in enumerate(handlers):
    section = body[match.start(): handlers[index + 1].start() if index + 1 < len(handlers) else len(body)]
    handler_info[match.group(1)] = {
        "line": body[:match.start()].count("\n") + 1,
        "span_lines": section.count("\n"),
        "engine_static_calls_direct": sorted(set(re.findall(r'\b((?:U|F)[A-Za-z0-9_]+::[A-Za-z0-9_]+)\(', section)) - {"FString::Printf"}),
        "member_calls_direct": sorted(set(re.findall(r'->([A-Za-z0-9_]+)\(', section))),
        "helper_calls_candidates": sorted(set(re.findall(r'(?<![:>\w])([A-Z][A-Za-z0-9_]+)\(', section))),
        "result_keys": sorted(set(re.findall(r'->Set\w+Field\(TEXT\("([^"]+)"\)', section))),
        "blocking_finish_compilation_calls": section.count("->FinishCompilation()"),
    }
routes = dict(re.findall(r'CommandType == TEXT\("([^"]+)"\)\)\s*\{\s*return (Handle\w+)\(Params\)', body))
priority = ["resolve_material_graph", "list_material_nodes", "analyze_material_graph", "list_material_collection_parameter_nodes", "get_material_parameter_collection_values", "mirror_material_parameter_collection", "replace_material_collection_references", "replace_material_collection_parameters", "replace_material_texture_references", "add_material_node", "set_material_node_property", "connect_material_nodes", "connect_material_property", "delete_material_node", "layout_material_nodes", "set_material_parameter_collection_values", "add_custom_material_node", "expand_material_function_calls", "compile_and_save_material"]
tools = []
for item in inventory["python_tool_definitions"]:
    if item["category"] != "material":
        continue
    name = item["name"]
    handler = routes.get(name)
    tools.append({
        "name": name,
        "initial_candidate_rank": priority.index(name) + 1 if name in priority else None,
        "python_source": item["source"],
        "python_signature": item["signature_arguments"],
        "legacy_runtime_input_schema": schemas[name]["inputSchema"],
        "legacy_runtime_description": schemas[name]["description"],
        "literal_commands": item["literal_send_command_calls"],
        "cpp_command": name if handler else None,
        "cpp_handler": handler,
        "cpp_source": material["path"],
        "cpp_handler_scan": handler_info.get(handler),
        "port_status": "planned_not_copied_not_compiled_not_registered",
        "engine_api_analysis": "direct calls and candidate helper calls only; see narrative for manually checked transitive engine paths",
    })
result = {
    "schema_version": 1,
    "scope": "read_only_native_domain_port_planning",
    "recommended_domain": "material",
    "transport_bridge_reuse": False,
    "source_edited": False,
    "domains": domains,
    "domain_totals": {name: {"files": len(files), "lines": sum(f["line_count"] for f in files), "bytes": sum(f["bytes"] for f in files)} for name, files in domains.items()},
    "material_include_resolution": include_resolution,
    "material_tools": tools,
    "candidate_count": len(priority),
    "deferred_material_tools": ["refresh_material_cached_expression_data", "set_material_parameter_collection_sync"],
    "notes": ["This scanner is not a C++ preprocessor, compiler, whole-program call graph, or functionality test.", "Original schema is preserved as evidence; target wrappers need typed named methods and explicit validation.", "Editor-only module reuse can avoid transport dependencies; arbitrary created object classes still require runtime dependency validation."],
}
output = HERE / "native-domain-port-plan.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, ensure_ascii=False, indent=2)
    stream.write("\n")
print(json.dumps({"totals": result["domain_totals"], "material_tools": len(tools), "candidate_count": len(priority), "unresolved_material_headers": [r["include"] for r in include_resolution if not r["legacy_local"] and not r["ue58_engine_matches"]], "private_only_material_headers": [r["include"] for r in include_resolution if r["private_or_internal_only"]]}, indent=2))
