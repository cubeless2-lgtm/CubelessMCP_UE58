"""Read-only legacy editor registration/route inventory; no imports from source."""
import ast
import hashlib
import json
from pathlib import Path
import re

PROJECT = Path(__file__).resolve().parents[4]
LEGACY = Path("C:/Git/UnrealMCP5_6_1")
PYTHON = LEGACY / "Python/tools/editor_tools.py"
NATIVE = LEGACY / "MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp"


def main():
    source = PYTHON.read_text(encoding="utf-8-sig")
    cpp = NATIVE.read_text(encoding="utf-8-sig")
    catalog_path = PROJECT / "cubeless/records/migration/implementation-2026-10-10/legacy-runtime-registration.eager.json"
    catalog = {row["name"]: row for row in json.loads(catalog_path.read_text(encoding="utf-8"))["catalog_tools"]}
    definitions = list(re.finditer(r"TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::(Handle\w+)\(", cpp))
    methods = {}
    for i, match in enumerate(definitions):
        end = definitions[i + 1].start() if i + 1 < len(definitions) else len(cpp)
        body = cpp[match.start():end]
        methods[match.group(1)] = {"line": cpp[:match.start()].count("\n") + 1,
            "body_sha256": hashlib.sha256(body.encode()).hexdigest(),
            "static_call_references": sorted(set(re.findall(r"\b([A-Z]\w*::\w+)\s*(?:<[^;\n]*?>)?\(", body))),
            "member_call_names": sorted(set(re.findall(r"->(\w+)\s*(?:<[^;\n]*?>)?\(", body)))}
    route_start = cpp.index("FUnrealMCPEditorCommands::HandleCommandInternal")
    route_end = cpp.index("FUnrealMCPEditorCommands::HandleOpenNiagaraPreviewPlayer", route_start)
    route_text = cpp[route_start:route_end]
    routes = {}
    for match in re.finditer(r'CommandType == TEXT\("([^"]+)"\)', route_text):
        next_return = re.search(r'return (Handle\w+)\(Params\)', route_text[match.end():])
        if next_return:
            routes[match.group(1)] = next_return.group(1)
    first = {"get_actors_in_level", "find_actors_by_name", "spawn_actor", "delete_actor", "set_actor_transform", "get_actor_properties"}
    follow = {"spawn_blueprint_actor", "set_actor_property"}
    rows = []
    registrar = next(node for node in ast.parse(source).body if isinstance(node, ast.FunctionDef) and node.name == "register_editor_tools")
    for node in registrar.body:
        if not isinstance(node, ast.FunctionDef) or not node.decorator_list:
            continue
        commands = sorted({call.args[0].value for call in ast.walk(node) if isinstance(call, ast.Call) and isinstance(call.func, ast.Attribute)
                           and call.func.attr == "send_command" and call.args and isinstance(call.args[0], ast.Constant) and isinstance(call.args[0].value, str)})
        args = [argument.arg for argument in node.args.args if argument.arg != "ctx"]
        row = {"name": node.name, "python_line": node.lineno, "python_signature": ast.unparse(node.args), "input_names": args,
               "commands": commands, "runtime_registered": node.name in catalog, "runtime_schema": catalog.get(node.name, {}).get("inputSchema"),
               "native_routes": [{"command": command, "handler": routes.get(command), **methods.get(routes.get(command), {})} for command in commands],
               "status": "planned_first_slice_not_implemented" if node.name in first else "planned_second_actor_slice_not_implemented" if node.name in follow else "incomplete_later_domain_review",
               "ue58_public_api_parity": "source_verified_core_candidates_runtime_pending" if node.name in first else "not_established_by_this_inventory"}
        rows.append(row)
    report = {"schema": "cubeless_editor_actor_source_inventory_v1", "scope": "All decorated functions in editor_tools.py matched to captured SDK registration and native route/handler. Call references are lexical aids, not C++ semantic or UE58 parity proof.",
        "source_read_only": True, "source_hashes": {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in (PYTHON, NATIVE, catalog_path)},
        "public_count": len(rows), "runtime_matched_count": sum(row["runtime_registered"] for row in rows),
        "missing_routes": [row["name"] for row in rows if not row["commands"] or any(not route["handler"] for route in row["native_routes"])],
        "native_only_routes": [{"command": command, "handler": handler, **methods.get(handler, {})} for command, handler in routes.items()
                               if command not in {command for row in rows for command in row["commands"]}], "tools": rows}
    out = Path(__file__).with_name("editor-actor-source-inventory-K.json")
    with out.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    print(json.dumps({"public_count": len(rows), "runtime_matched_count": report["runtime_matched_count"], "missing_routes": report["missing_routes"],
        "native_only_routes": [{key: value for key, value in route.items() if key in ("command", "handler")} for route in report["native_only_routes"]],
        "names": [row["name"] for row in rows]}, indent=2))


if __name__ == "__main__":
    main()
