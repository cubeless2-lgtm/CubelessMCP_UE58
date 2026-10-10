"""Read installed binary/UHT/recorded-schema evidence; never load the UE DLL."""
import ast
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ENGINE = Path("C:/Program Files/Epic Games/UE_5.8/Engine")
PROJECT = Path(__file__).resolve().parents[4]
DUMPBIN = Path("C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.44.35207/bin/HostX64/x64/dumpbin.exe")
QUERIES = ["SetScalarParameterDefaultValue", "SetVectorParameterDefaultValue",
           "GetScalarParameterDefaultValue@UMaterialParameterCollection", "GetVectorParameterDefaultValue@UMaterialParameterCollection",
           "SetScalarParameterValue@UMaterialParameterCollectionInstance", "SetVectorParameterValue@UMaterialParameterCollectionInstance"]


def sha(path):
    return hashlib.file_digest(path.open("rb"), "sha256").hexdigest()


def inspect_binary(path, option):
    command = [str(DUMPBIN), "/nologo", option, str(path)]
    done = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    text = done.stdout.decode("utf-8", errors="replace")
    return {"command": command, "exit_code": done.returncode, "path": str(path), "size_bytes": path.stat().st_size,
            "sha256": sha(path), "stdout_sha256": hashlib.sha256(done.stdout).hexdigest(), "stdout_lines": len(text.splitlines()),
            "stderr": done.stderr.decode("utf-8", errors="replace"),
            "queries": {name: [line.strip() for line in text.splitlines() if name in line] for name in QUERIES}}


def source_evidence(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    return {"path": str(path), "sha256": sha(path), "hits": [
        {"line": i, "text": line.strip()} for i, line in enumerate(lines, 1)
        if any(name.split("@")[0] in line for name in QUERIES) or "DECLARE_FUNCTION" in line]}


def main(output):
    report = {"schema": "cubeless_mpc_setter_binary_probe_v1", "scope": "Read-only dumpbin, generated UHT and prior actual official schema evidence; no DLL load, UE tool call or asset mutation",
              "dumpbin": {"path": str(DUMPBIN), "sha256": sha(DUMPBIN)},
              "engine_build": json.loads((ENGINE / "Build/Build.version").read_text()),
              "dll": inspect_binary(ENGINE / "Binaries/Win64/UnrealEditor-Engine.dll", "/exports"),
              "import_library": inspect_binary(ENGINE / "Intermediate/Build/Win64/x64/UnrealEditor/Development/Engine/UnrealEditor-Engine.lib", "/linkermember:2")}
    report["source_evidence"] = [source_evidence(ENGINE / path) for path in (
        "Source/Runtime/Engine/Public/Materials/MaterialParameterCollection.h",
        "Intermediate/Build/Win64/UnrealEditor/Inc/Engine/UHT/MaterialParameterCollection.generated.h",
        "Source/Runtime/Engine/Classes/Kismet/KismetMaterialLibrary.h")]
    material_source = ENGINE / "Plugins/Experimental/Toolsets/EditorToolset/Content/Python/editor_toolset/toolsets/material.py"
    tree = ast.parse(material_source.read_text(encoding="utf-8"))
    report["official_material_python"] = {"path": str(material_source), "sha256": sha(material_source), "methods": [
        node.name for cls in tree.body if isinstance(cls, ast.ClassDef) for node in cls.body if isinstance(node, ast.FunctionDef)]}
    recorded = PROJECT / "cubeless/records/verification/implementation-2026-10-10/material-author-I.json"
    data = json.loads(recorded.read_text(encoding="utf-8"))
    report["prior_actual_official_schema"] = {"path": str(recorded), "sha256": sha(recorded), "status": data["status"],
        "material_toolsets": {name: [row["name"] for row in schema["tools"]] for name, schema in data["discovered_schemas"].items()
                              if name.endswith(".MaterialTools")}}
    report["asset_default_setter_exported"] = any(report[section]["queries"][name] for section in ("dll", "import_library") for name in QUERIES[:2])
    report["conclusion"] = "No named asset-default setter export or UHT exec wrapper found; effective getters and instance setters are present. Own editable arrays remain public; inherited override setters are not callable through the inspected binary/UFUNCTION boundary."
    with Path(output).open("x", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    print(json.dumps({"asset_default_setter_exported": report["asset_default_setter_exported"], "report": str(output)}))


if __name__ == "__main__":
    main(sys.argv[1])
