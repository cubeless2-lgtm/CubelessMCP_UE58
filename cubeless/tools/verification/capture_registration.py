"""Read actual Cubeless Toolset schemas and bind them to an isolated plugin snapshot."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload

TOOLSETS = [
    "CubelessMCPUE58Editor.CubelessNativeToolset",
    "cubeless_mcp_ue58.registration.toolsets.CubelessPythonDiagnostics",
    "cubeless_mcp_ue58.registration.toolsets.CubelessBlueprintTools",
    "cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools",
    "cubeless_mcp_ue58.registration.material_toolset.CubelessMaterialTools",
    "cubeless_mcp_ue58.registration.batch_toolset.CubelessBatchTools",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--expected-project", required=True, type=Path)
    parser.add_argument("--expected-count", required=True, type=int)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Refusing to overwrite evidence")
    project = args.expected_project.resolve()
    client = OfficialMCPClient(args.endpoint)
    report = {"schema": "cubeless_registration_snapshot_v2", "status": "fail", "endpoint": args.endpoint,
              "project_file": str(project), "started_ns": time.time_ns(), "raw": {}, "descriptions": {}}
    try:
        report["initialize"] = client.initialize()
        report["raw"]["toolsets"] = client.call("list_toolsets")
        report["toolsets"] = tool_payload(report["raw"]["toolsets"])
        assert isinstance(report["toolsets"], str), "Unexpected official list_toolsets format"
        discovered = [line[2:].split(":", 1)[0] for line in report["toolsets"].splitlines()
                      if line.startswith(("- CubelessMCPUE58Editor.", "- cubeless_mcp_ue58."))]
        assert len(discovered) == len(set(discovered)), "Duplicate Cubeless Toolset identity"
        assert set(TOOLSETS) <= set(discovered), "A previously registered core Toolset is missing"
        report["discovered_cubeless_toolsets"] = discovered
        raw = client.call_toolset(TOOLSETS[0], "GetEditorDiagnostics")
        report["raw"]["diagnostics"] = raw
        diagnostics = tool_payload(raw)
        if isinstance(diagnostics, str):
            diagnostics = json.loads(diagnostics)
        report["diagnostics"] = diagnostics
        assert diagnostics["success"] is True
        assert Path(diagnostics["project_file"]).resolve() == project, "Wrong editor project"
        assert diagnostics["engine_version"].startswith("5.8.3-"), "Wrong engine version"
        for name in discovered:
            raw = client.call("describe_toolset", {"toolset_name": name})
            report["raw"][name] = raw
            description = tool_payload(raw)
            if isinstance(description, str):
                description = json.loads(description)
            assert description.get("name") == name, "Toolset name mismatch"
            report["descriptions"][name] = description
        names = [tool["name"] for desc in report["descriptions"].values() for tool in desc["tools"]]
        report["counts"] = {"toolsets": len(report["descriptions"]), "tools": len(names)}
        assert len(names) == len(set(names)) == args.expected_count, "Tool count or uniqueness mismatch"
        plugin = project.parent / "Plugins/CubelessMCP_UE58"
        report["plugin_files"] = []
        for file in sorted(plugin.rglob("*")):
            relative = file.relative_to(plugin)
            if not file.is_file() or "Intermediate" in relative.parts or "__pycache__" in relative.parts:
                continue
            if file.suffix.lower() not in {".h", ".cpp", ".inc", ".cs", ".py", ".uplugin", ".dll", ".modules"}:
                continue
            report["plugin_files"].append({"path": relative.as_posix(), "sha256": hashlib.sha256(file.read_bytes()).hexdigest()})
        assert any(row["path"].endswith(".dll") for row in report["plugin_files"]), "No plugin DLL fingerprint"
        report["status"] = "pass"
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["calls"] = client.observations
        client.close()
        report["finished_ns"] = time.time_ns()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
    print(json.dumps({key: report.get(key) for key in ("status", "counts", "error")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
