"""Official-MCP base/mid/leaf MPC read fixture; no engine launch or automatic retry.

Author creates fresh test assets and saves once. Reload is read-only in a new
editor instance. Private asset override maps, rendering, depth/cycle corruption
and packaging are explicitly outside this fixture's execution coverage.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import MCPError, OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result, require_success
from pipelines.material_graph_smoke import reference, ref_path, field, require

PROJECT_ROOT = Path(__file__).resolve().parents[3]
ALLOWED_PROJECT = PROJECT_ROOT / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
SCHEMA = "cubeless_mpc_inheritance_fixture_v1"


def fixture_asset_path(package):
    require(bool(re.fullmatch(r"/Game/CubelessMCPTests/MPCInheritance_[a-f0-9]{32}/(?:MPC_Base|MPC_Mid|MPC_Leaf|M_Inheritance)", package)), "Package is outside the exact fixture namespace")
    content = (ALLOWED_PROJECT.parent / "Content").resolve()
    path = (content / (package[len("/Game/"):] + ".uasset")).resolve()
    require(path.is_relative_to(content), "Fixture escaped isolated Content")
    return path


def validate_values(value, queried, paths, names, *, defaults=True):
    require_success(value)
    data = value["data"]
    require(data.get("collection_semantics_version") == "ue58_effective_collection_v1", "New collection semantics missing")
    expected_chain = {"base": ["base"], "mid": ["mid", "base"], "leaf": ["leaf", "mid", "base"]}[queried]
    require([x["collection"] for x in data["base_chain"]] == [paths[x] for x in expected_chain], "Wrong exact collection base chain")
    all_rows = data["scalars"] + data["vectors"]
    require({row["name"] for row in all_rows} == set(names) and len(all_rows) == len(names), "Missing, duplicate or unexpected effective parameter")
    require(data["include_runtime"] is False and data["has_runtime_instance"] is False, "This fixture requested no runtime instance")
    expected = {"RootScalar": ("base", 0.25), "RootVector": ("base", {"r": 0.1, "g": 0.2, "b": 0.3, "a": 1.0}),
                "MidScalar": ("mid", 0.5), "LeafVector": ("leaf", {"r": 0.4, "g": 0.5, "b": 0.6, "a": 1.0})}
    for row in all_rows:
        owner, value = expected[row["name"]]
        require(row["declared_in"] == paths[owner] and row["inherited"] is (owner != queried), "Wrong declaration origin/inherited flag")
        require(bool(re.fullmatch(r"[0-9a-fA-F-]{36}", row["id"])) and int(row["id"].replace("-", ""), 16) != 0, "Invalid parameter GUID")
        require(row["runtime_resolved"] is False and "runtime_value" not in row, "Unexpected runtime result")
        if not defaults:
            require("asset_default" not in row, "include_asset_defaults=False was ignored")
        elif isinstance(value, dict):
            require(all(abs(row["asset_default"][key] - number) < 1e-5 for key, number in value.items()), "Wrong inherited vector default")
        else:
            require(abs(row["asset_default"] - value) < 1e-6, "Wrong inherited scalar default")
    return data


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--expected-project", type=Path, required=True)
    parser.add_argument("--phase", choices=["author", "reload"], required=True)
    parser.add_argument("--parent-report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=180)
    args = parser.parse_args(argv)
    require(args.expected_project.resolve() == ALLOWED_PROJECT.resolve(), "Fixture is restricted to this exact project")
    output = args.output.resolve()
    require(not output.exists(), "Prior evidence must not be overwritten")
    parent = None
    if args.phase == "reload":
        require(args.parent_report is not None, "Reload requires an author report")
        parent = json.loads(args.parent_report.read_text(encoding="utf-8"))
        require(parent.get("schema") == SCHEMA and parent.get("phase") == "author" and parent.get("status") == "pass", "Parent author failed")
        require(parent.get("save") == {"attempts": 1, "saved": True}, "Parent lacks exactly one successful save")
        require(parent.get("outcome_unknown") is False and parent.get("automatic_retries") == 0, "Uncertain author outcome")
        require(Path(parent["project_file"]).resolve() == ALLOWED_PROJECT.resolve(), "Parent project mismatch")
        require(all(stage.get("passed") is True for stage in parent["stages"]), "Parent contains a failed stage")
    run_id = parent["run_id"] if parent else uuid.uuid4().hex
    require(bool(re.fullmatch(r"[a-f0-9]{32}", run_id)), "Invalid run ID")
    root = "/Game/CubelessMCPTests/MPCInheritance_" + run_id
    packages = {"base": root + "/MPC_Base", "mid": root + "/MPC_Mid", "leaf": root + "/MPC_Leaf", "material": root + "/M_Inheritance"}
    objects = {key: path + "." + path.rsplit("/", 1)[1] for key, path in packages.items()}
    report = {"schema": SCHEMA, "phase": args.phase, "status": "running", "run_id": run_id,
              "project_file": str(ALLOWED_PROJECT.resolve()), "packages": packages, "objects": objects,
              "stages": [], "started_ns": time.time_ns(), "automatic_retries": 0, "outcome_unknown": False,
              "atomic_rollback": False, "save": {"attempts": 0, "saved": False}, "reload": "not_run",
              "coverage": {"base_free_and_inherited_names_defaults_ids": "pending", "direct_graph_metadata": "pending",
                           "asset_base_override_values": "not_run_private_maps_not_publicly_writable",
                           "runtime_overrides": "not_run", "depth_cycle_ambiguous_corruption": "not_run",
                           "nested_functions_and_collection_transform": "not_run", "shader_render": "not_run", "packaging": "not_run"}}
    if parent:
        require(parent["packages"] == packages and parent["objects"] == objects, "Parent asset ownership differs")
        report["parent_report_sha256"] = hashlib.sha256(args.parent_report.read_bytes()).hexdigest()
        report["nodes"] = parent["nodes"]
    def disk_manifest():
        result = {}
        for key, package in packages.items():
            path = fixture_asset_path(package)
            require(path.is_file(), "Saved fixture package missing: " + str(path))
            result[key] = {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        return result
    if parent:
        require(disk_manifest() == parent.get("asset_files"), "Fixture bytes changed since author report")
    client = OfficialMCPClient(args.endpoint, args.timeout)
    schemas = {}

    def persist():
        output.parent.mkdir(parents=True, exist_ok=True)
        temp = output.with_name(output.name + ".partial")
        with temp.open("w", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, output)

    def invoke(toolset, tool, arguments=None, mutation=False):
        rows = [r for r in schemas[toolset]["tools"] if r["name"].rsplit(".", 1)[-1] == tool]
        require(len(rows) == 1, "Tool missing/ambiguous in live schema: " + tool)
        schema = rows[0]["inputSchema"]
        mapped = {}
        for key, value in (arguments or {}).items():
            candidates = [k for k in schema.get("properties", {}) if canonical(k) == canonical(key)]
            require(len(candidates) == 1, "Argument missing/ambiguous in live schema: " + key)
            mapped[candidates[0]] = value
        require(set(schema.get("required", [])) <= set(mapped), "Required live schema arguments missing")
        try:
            return operation_result(client.call_toolset(toolset, tool, mapped, mutation=mutation))
        except Exception as error:
            if mutation:
                report["outcome_unknown"] = True
            raise

    def step(name, action, check=lambda value: None, mutation=False):
        stage = {"name": name, "mutation": mutation, "passed": False, "started_ns": time.time_ns()}
        report["stages"].append(stage)
        persist()
        try:
            value = action()
            stage["result"] = value
            check(value)
            stage["passed"] = True
            return value
        except Exception as error:
            stage["error"] = str(error)
            if isinstance(error, MCPError):
                stage["detail"] = error.detail
                report["outcome_unknown"] |= error.outcome_unknown
            raise
        finally:
            stage["elapsed_seconds"] = (time.time_ns() - stage["started_ns"]) / 1e9
            persist()

    true = lambda value: require(value is True, "Official API did not confirm success")
    try:
        persist()
        report["initialize"] = client.initialize()
        catalog = tool_payload(client.call("list_toolsets"))
        require(isinstance(catalog, str), "Invalid official catalog")
        names = re.findall(r"^- ([\w.]+):", catalog, flags=re.MULTILINE)
        def discover(suffix):
            matches = [name for name in names if name.endswith("." + suffix)]
            require(len(matches) == 1, "Expected one registered " + suffix)
            name = matches[0]
            schemas[name] = tool_payload(client.call("describe_toolset", {"toolset_name": name}))
            return name
        native, ours, material, obj, asset = [discover(name) for name in ("CubelessNativeToolset", "CubelessMaterialTools", "MaterialTools", "ObjectTools", "AssetTools")]
        report["live_schemas"] = schemas
        def preflight(value):
            require_success(value)
            require(Path(value["project_file"]).resolve() == ALLOWED_PROJECT.resolve(), "Endpoint is another project")
            require(value["engine_version"].startswith("5.8.3") and value["game_thread"] is True and value["pie_or_sie_active"] is False, "Require UE58.3 idle editor")
            require(not value["dirty_packages"], "User/previous dirty packages must be preserved")
        report["editor"] = step("preflight", lambda: invoke(native, "GetEditorDiagnostics"), preflight)
        if parent:
            require(report["editor"]["instance_id"] != parent["editor"]["instance_id"], "Cold reload requires a different editor instance")
        def values(key, names, defaults=True):
            return step("effective_" + key + "_" + str(len(report["stages"])), lambda: invoke(ours, "get_material_parameter_collection_values",
                        {"collection_path": packages[key], "parameter_names": [], "include_asset_defaults": defaults, "include_runtime": False}),
                        lambda value: validate_values(value, key, objects, names, defaults=defaults))
        def set_properties(path, properties, label):
            return step(label, lambda: invoke(obj, "set_properties", {"instance": reference(path), "values": json.dumps(properties)}, True), true, True)
        if args.phase == "author":
            for key, package in packages.items():
                step("absent_" + key, lambda package=package: invoke(asset, "exists", {"path": package}), lambda value: require(value is False, "Existing fixture path; do not overwrite"))
            for key, package in packages.items():
                tool = "create_material" if key == "material" else "create_parameter_collection"
                step("create_" + key, lambda package=package, tool=tool: invoke(material, tool, {"folder_path": root, "asset_name": package.rsplit("/", 1)[1]}, True),
                     lambda value, key=key: require(ref_path(value) == objects[key], "Unexpected created asset"), True)
            set_properties(objects["base"], {"ScalarParameters": [{"ParameterName": "RootScalar", "DefaultValue": 0.25}],
                "VectorParameters": [{"ParameterName": "RootVector", "DefaultValue": {"R": 0.1, "G": 0.2, "B": 0.3, "A": 1.0}}]}, "base_parameters")
            set_properties(objects["mid"], {"Base": reference(objects["base"]), "ScalarParameters": [{"ParameterName": "MidScalar", "DefaultValue": 0.5}]}, "mid_base_and_parameter")
            set_properties(objects["leaf"], {"Base": reference(objects["mid"]), "VectorParameters": [{"ParameterName": "LeafVector", "DefaultValue": {"R": 0.4, "G": 0.5, "B": 0.6, "A": 1.0}}]}, "leaf_base_and_parameter")
            report["nodes"] = {}
            for name in ("RootScalar", "RootVector"):
                created = step("node_" + name, lambda: invoke(material, "add_expression", {"material_or_function": reference(objects["material"]),
                    "expression_class": reference("/Script/Engine.MaterialExpressionCollectionParameter")}, True), ref_path, True)
                report["nodes"][name] = ref_path(created)
                set_properties(ref_path(created), {"Collection": reference(objects["leaf"]), "ParameterName": name}, "node_reference_" + name)
        # Official independent raw properties prove the leaf stores only its own
        # vector; successful effective root-parameter reads must traverse bases.
        own = step("independent_leaf_properties", lambda: invoke(obj, "get_properties", {"instance": reference(objects["leaf"]), "properties": ["Base", "ScalarParameters", "VectorParameters"]}))
        require(ref_path(field(own, "Base")) == objects["mid"] and field(own, "ScalarParameters") == [], "Leaf base/local scalar arrays differ")
        require(len(field(own, "VectorParameters")) == 1 and field(field(own, "VectorParameters")[0], "ParameterName") == "LeafVector", "Leaf local vector differs")
        values("base", ["RootScalar", "RootVector"])
        values("mid", ["RootScalar", "RootVector", "MidScalar"])
        leaf = values("leaf", ["RootScalar", "RootVector", "MidScalar", "LeafVector"])
        report["effective_leaf_manifest"] = {key: leaf["data"][key] for key in ("collection", "scalars", "vectors", "base_chain")}
        if parent:
            require(report["effective_leaf_manifest"] == parent.get("effective_leaf_manifest"), "Cold effective names/GUIDs/defaults differ from author")
        values("leaf", ["RootScalar", "RootVector", "MidScalar", "LeafVector"], defaults=False)
        step("filter_inherited_scalar", lambda: invoke(ours, "get_material_parameter_collection_values", {"collection_path": packages["leaf"], "parameter_names": ["RootScalar"], "include_runtime": False}),
             lambda value: validate_values(value, "leaf", objects, ["RootScalar"]))
        def node_check(value):
            require_success(value)
            data = value["data"]
            require(data["collection_parameter_node_count"] == 2 and data["missing_collection_parameter_count"] == data["mismatched_id_count"] == 0, "Inherited nodes incorrectly reported missing/mismatched")
            by_name = {row["name"]: row for row in leaf["data"]["scalars"] + leaf["data"]["vectors"]}
            require({r["parameter_name"] for r in data["nodes"]} == {"RootScalar", "RootVector"}, "Unexpected expression names")
            for row in data["nodes"]:
                require(row["object_path"] == report["nodes"][row["parameter_name"]] and row["declared_in"] == objects["base"] and row["inherited"] is True, "Inherited node identity/owner differs")
                require(row["parameter_id_matches_collection"] is True and row["collection_parameter_id"] == by_name[row["parameter_name"]]["id"], "Inherited node GUID differs")
        step("inherited_collection_nodes", lambda: invoke(ours, "list_material_collection_parameter_nodes", {"material_path": packages["material"]}), node_check)
        def metadata_check(value):
            require_success(value)
            data = value["data"]["ue58_collection_semantics"]
            require(data["effective_values_verified"] is False and data["nested_function_collections_verified"] is False, "Graph metadata overclaims semantic evaluation")
            require(len(data["collections"]) == 1 and data["collections"][0]["base_collection"] == objects["mid"], "Graph lacks inherited collection metadata")
        for tool in ("list_material_nodes", "analyze_material_graph"):
            step(tool + "_metadata", lambda tool=tool: invoke(ours, tool, {"material_path": packages["material"]}), metadata_check)
        if args.phase == "author":
            report["save"]["attempts"] = 1
            step("save_once", lambda: invoke(asset, "save_assets", {"asset_paths": list(packages.values())}, True), true, True)
            report["save"]["saved"] = True
        else:
            report["reload"] = "passed"
        for key, package in packages.items():
            step("clean_" + key, lambda package=package: invoke(asset, "is_dirty", {"asset_path": package}), lambda value: require(value is False, "Fixture remains dirty"))
        report["asset_files"] = disk_manifest()
        if parent:
            require(report["asset_files"] == parent["asset_files"], "Read-only reload altered package files")
        report["coverage"].update(base_free_and_inherited_names_defaults_ids="passed", direct_graph_metadata="passed")
        report["status"] = "pass"
    except Exception as error:
        report.update(status="fail", error=str(error))
    finally:
        report["calls"] = client.observations
        client.close()
        report["elapsed_seconds"] = (time.time_ns() - report["started_ns"]) / 1e9
        persist()
    print(json.dumps({"status": report["status"], "output": str(output), "phase": args.phase}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
