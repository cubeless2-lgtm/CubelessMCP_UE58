"""Official-MCP Material fixture authoring and separate cold-reload verification.

An external client, never an MCP server or engine launcher. Author creates only a
fresh /Game/CubelessMCPTests/Material_<uuid> fixture. No mutation is retried.
Compile/save are explicit final stages after the batch of graph edits.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import MCPError, OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result, require_success
from verification.material_pipeline_report import verify as verify_report


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def fingerprint(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def reference(path):
    # Installed ToolsetRegistry ReferenceConverter.cpp accepts refPath objects.
    return {"refPath": path}


def ref_path(value):
    path = value.get("refPath") if isinstance(value, dict) else value
    require(isinstance(path, str) and path.startswith("/"), "Expected official UObject refPath: " + repr(value))
    return path


def field(value, name):
    require(isinstance(value, dict), "Expected object while reading " + name)
    matches = [item for key, item in value.items() if canonical(key) == canonical(name)]
    require(len(matches) == 1, "Missing/ambiguous result field: " + name)
    return matches[0]


def graph_state(result):
    require_success(result)
    data = result["data"]
    require(isinstance(data.get("nodes"), list), "Material graph result lacks nodes")
    return {key: data.get(key) for key in ("material", "graph_type", "node_count", "nodes", "material_properties")}


def node_by_path(result, path):
    nodes = graph_state(result)["nodes"]
    matches = [node for node in nodes if node.get("object_path") == path]
    require(len(matches) == 1, "Expected one expression at " + path)
    return matches[0]


def check_mutation(result):
    require_success(result)
    require(result.get("validation", {}).get("native_postcondition_passed") is True, "Native mutation lacks postcondition evidence")
    native = result.get("data", {}).get("native_outcome", {})
    require(native.get("postcondition_passed") is True, "Native result lacks verified postcondition")
    require(native.get("change_state_known") is True, "Native change state is unknown")
    require(not result.get("compile", {}).get("requested") and not result.get("save", {}).get("saved"), "Mutation unexpectedly requested compile/save")
    require(result.get("retry", {}).get("automatic_retry") is False, "Unexpected mutation retry")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--expected-project", type=Path, required=True)
    parser.add_argument("--phase", choices=["author", "reload"], required=True)
    parser.add_argument("--parent-report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=180)
    parser.add_argument("--exercise-add-set", action="store_true", help="Author a fresh v2 fixture exercising Cubeless add/set and cold-reload its retained property")
    args = parser.parse_args(argv)
    output = args.output.resolve()
    require(not output.exists(), "Refusing to overwrite prior evidence")
    parent = None
    if args.phase == "reload":
        require(args.parent_report is not None, "Reload requires the successful author report")
        parent = json.loads(args.parent_report.read_text(encoding="utf-8"))
        require(verify_report(parent, require_phase="author")["passed"], "Parent failed independent Material report verification")
        require(parent.get("schema") in {"cubeless_material_pipeline_v1", "cubeless_material_pipeline_v2"} and parent.get("phase") == "author" and parent.get("status") == "pass", "Parent is not a passing Material author report")
        require(parent.get("compile", {}).get("explicit_calls") == 1 and parent.get("compile", {}).get("passed") is True, "Parent lacks the explicit final compile")
        require(parent.get("save", {}).get("explicit_calls") == 1 and parent.get("save", {}).get("saved") is True, "Parent lacks the explicit final save")
        require(parent.get("outcome_unknown") is False and parent.get("automatic_retries") == 0, "Parent mutation outcome is uncertain")
        require(Path(parent["project_file"]).resolve() == args.expected_project.resolve(), "Parent belongs to a different project")
    extended = parent.get("schema") == "cubeless_material_pipeline_v2" if parent else args.exercise_add_set
    require(not parent or not args.exercise_add_set or extended, "Cannot add mutation stages to a legacy reload")
    run_id = parent["run_id"] if parent else uuid.uuid4().hex
    require(re.fullmatch(r"[a-f0-9]{32}", run_id), "Invalid run ownership ID")
    package_root = "/Game/CubelessMCPTests/Material_" + run_id
    assets = {"material": package_root + "/M_Contract", "collection": package_root + "/MPC_Contract"}
    if parent:
        require(parent.get("assets") == assets, "Parent fixture does not match exact run-owned paths")
    report = {"schema": "cubeless_material_pipeline_v2" if extended else "cubeless_material_pipeline_v1", "phase": args.phase, "status": "running", "run_id": run_id,
              "project_file": str(args.expected_project.resolve()), "endpoint": args.endpoint, "asset_root": package_root, "assets": assets, "objects": {},
              "started_ns": time.time_ns(), "stages": [], "changes": [], "compile": {"explicit_calls": 0, "passed": False},
              "save": {"explicit_calls": 0, "saved": False}, "reload": {"status": "not_run"}, "cleanup": {"status": "not_run"},
              "packaging": {"status": "not_run"}, "automatic_retries": 0, "outcome_unknown": False, "atomic_rollback": False,
              "scope": "Nine named tools on a small Material and scalar/vector MPC; Function/Instance, large graphs, rendering and full domain parity remain separate",
              "fixture_api": "Official MaterialTools, ObjectTools and AssetTools; no separate fixture server or hidden Python executor"}
    if extended:
        report["scope"] = "Eleven Material tools on a retained Constant node plus original graph/MPC fixture; native guards, property readback, explicit save and separate reload. Full Material parity and shader completion not claimed."
    report["input_fingerprint"] = fingerprint({key: report[key] for key in ("phase", "run_id", "project_file", "endpoint", "assets")})
    if parent:
        report["author_report_sha256"] = hashlib.sha256(args.parent_report.read_bytes()).hexdigest()
        report["author_editor"] = parent["editor"]
        report["fixture"] = parent["fixture"]
        report["objects"] = parent["objects"]
        for kind, path in assets.items():
            require(report["objects"].get(kind) == path + "." + path.rsplit("/", 1)[1], "Parent object reference differs from exact fixture object path")
    schemas = {}
    client = OfficialMCPClient(args.endpoint, args.timeout)

    def persist():
        output.parent.mkdir(parents=True, exist_ok=True)
        temp = output.with_suffix(output.suffix + ".partial")
        temp.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        temp.replace(output)

    def invoke(toolset, tool, arguments=None, mutation=False):
        choices = [row for row in schemas[toolset]["tools"] if row["name"].rsplit(".", 1)[-1] == tool]
        require(len(choices) == 1, "Expected tool in actual registered schema: " + tool)
        props = choices[0].get("inputSchema", {}).get("properties", {})
        mapped = {}
        for key, value in (arguments or {}).items():
            matches = [name for name in props if canonical(name) == canonical(key)]
            require(len(matches) == 1, "Input absent from live schema: " + tool + "." + key)
            mapped[matches[0]] = value
        missing = set(choices[0].get("inputSchema", {}).get("required", [])) - set(mapped)
        require(not missing, "Missing live-schema required inputs: " + tool + " " + str(missing))
        raw = client.call_toolset(toolset, tool, mapped, mutation=mutation)
        try:
            return operation_result(raw)
        except MCPError as error:
            error.outcome_unknown = error.outcome_unknown or mutation
            raise
        except (ValueError, TypeError, KeyError) as error:
            raise MCPError("Cannot decode official tool result: " + str(error), outcome_unknown=mutation, detail=raw) from error

    def step(name, action, check=lambda value: None, mutation=False):
        item = {"name": name, "passed": False, "mutation": mutation, "started_ns": time.time_ns()}
        report["stages"].append(item)
        persist()
        try:
            value = action()
            item["result"] = value
            check(value)
            item["passed"] = True
            if isinstance(value, dict) and value.get("changes"):
                report["changes"].extend(value["changes"])
            return value
        except Exception as error:
            item["error"] = str(error)
            if isinstance(error, MCPError):
                item["mcp_error"] = error.detail
                report["outcome_unknown"] = report["outcome_unknown"] or error.outcome_unknown
            raise
        finally:
            item["elapsed_seconds"] = (time.time_ns() - item["started_ns"]) / 1e9
            persist()

    def expect_true(value):
        require(value is True, "Official API returned false instead of confirmed success")

    try:
        persist()
        report["server"] = client.initialize()
        report["top_level_schema"] = client.list_tools()
        catalog = tool_payload(client.call("list_toolsets"))
        require(isinstance(catalog, str), "Unexpected official toolset catalog format")
        names = re.findall(r"^- ([\w.]+):", catalog, flags=re.MULTILINE)
        def discover(suffix):
            matches = [name for name in names if name.endswith("." + suffix)]
            require(len(matches) == 1, "Expected exactly one registered " + suffix + ": " + str(matches))
            name = matches[0]
            schemas[name] = tool_payload(client.call("describe_toolset", {"toolset_name": name}))
            return name
        native = discover("CubelessNativeToolset")
        cubeless = discover("CubelessMaterialTools")
        official = discover("MaterialTools")
        asset_tools = discover("AssetTools")
        object_tools = discover("ObjectTools")
        report["discovered_schemas"] = schemas
        def preflight(value):
            require_success(value)
            require(Path(value["project_file"]).resolve() == args.expected_project.resolve(), "Endpoint is a different project")
            require(str(value["engine_version"]).startswith("5.8.3"), "Only UE5.8.3 is supported")
            require(value["game_thread"] is True and value["pie_or_sie_active"] is False, "Require idle editor game thread")
            require(not value.get("dirty_packages"), "Refusing to mix fixture work with unsaved packages")
        report["editor"] = step("preflight", lambda: invoke(native, "GetEditorDiagnostics"), preflight)

        def mat(tool, arguments=None, *, mutation=False, operation_id=None):
            parameters = {"material_path": assets["material"], **(arguments or {})}
            if mutation:
                parameters["operation_id"] = operation_id or run_id + "_" + str(len(report["stages"])) + "_" + tool
            return invoke(cubeless, tool, parameters, mutation)

        def collection_check(value):
            require_success(value)
            scalars = value["data"]["scalars"]
            vectors = value["data"]["vectors"]
            require(len(scalars) == 1 and scalars[0]["name"] == "CubelessScalar", "MPC scalar fixture missing")
            require(abs(scalars[0]["asset_default"] - 0.25) < 1e-6, "MPC scalar default differs")
            require(len(vectors) == 1 and vectors[0]["name"] == "CubelessVector", "MPC vector fixture missing")
            actual = vectors[0]["asset_default"]
            for key, expected in {"r": 0.1, "g": 0.2, "b": 0.3, "a": 1.0}.items():
                require(abs(actual[key] - expected) < 1e-5, "MPC vector default differs: " + key)
            report["mpc_runtime_observation"] = {"has_world": value["data"].get("has_world"), "has_runtime_instance": value["data"].get("has_runtime_instance"),
                                                 "scalar_runtime_resolved": scalars[0].get("runtime_resolved"), "vector_runtime_resolved": vectors[0].get("runtime_resolved")}

        def added_property_check(value):
            actual = field(value, "R")
            require(type(actual) in (int, float) and abs(actual - 0.875) < 1e-6, "Independent added-node R property differs")
            require(field(value, "Desc") == "Cubeless add-set retained fixture", "Independent added-node description differs")

        def read_added_property():
            return invoke(object_tools, "get_properties", {"instance": reference(report["fixture"]["nodes"]["added"]), "properties": ["R", "Desc"]})

        if args.phase == "author":
            for kind, tool in (("material", "create_material"), ("collection", "create_parameter_collection")):
                path = assets[kind]
                exists = step("guard_absent_" + kind, lambda path=path: invoke(asset_tools, "exists", {"path": path}))
                require(exists is False, "Fixture already exists; use a fresh run after inspecting prior outcome")
                created = step("create_" + kind, lambda path=path, tool=tool: invoke(official, tool, {"folder_path": package_root, "asset_name": path.rsplit("/", 1)[1]}, True), ref_path, True)
                require(ref_path(created).split(".", 1)[0] == path, "Factory returned an unexpected asset")
                report["objects"][kind] = ref_path(created)
                report["changes"].append({"kind": "fixture_asset_created", "asset_path": ref_path(created)})
            mpc_values = {"ScalarParameters": [{"ParameterName": "CubelessScalar", "DefaultValue": 0.25}],
                          "VectorParameters": [{"ParameterName": "CubelessVector", "DefaultValue": {"R": 0.1, "G": 0.2, "B": 0.3, "A": 1.0}}]}
            step("set_collection_defaults", lambda: invoke(object_tools, "set_properties", {"instance": reference(report["objects"]["collection"]), "values": json.dumps(mpc_values)}, True), expect_true, True)
            fixture = {"nodes": {}}
            report["fixture"] = fixture
            for kind, expression_class, x, y in (("source", "MaterialExpressionConstant", -600, 0), ("delete", "MaterialExpressionConstant", -600, 180),
                                                  ("add", "MaterialExpressionAdd", -200, 0), ("collection", "MaterialExpressionCollectionParameter", -600, 360)):
                created = step("create_expression_" + kind, lambda expression_class=expression_class, x=x, y=y: invoke(official, "add_expression", {"material_or_function": reference(report["objects"]["material"]), "expression_class": reference("/Script/Engine." + expression_class), "x": x, "y": y}, True), ref_path, True)
                fixture["nodes"][kind] = ref_path(created)
            for kind, values in (("source", {"R": 0.25, "Desc": "Cubeless fixture source"}), ("delete", {"R": 0.5, "Desc": "Cubeless fixture deletion"}),
                                 ("collection", {"Collection": reference(report["objects"]["collection"]), "ParameterName": "CubelessScalar", "Desc": "Cubeless fixture collection"})):
                step("set_expression_" + kind, lambda kind=kind, values=values: invoke(object_tools, "set_properties", {"instance": reference(fixture["nodes"][kind]), "values": json.dumps(values)}, True), expect_true, True)
            if extended:
                add_args = {"expression_class": "Constant", "node_position": [-700, 550],
                            "properties_json": json.dumps({"R": 0.125, "Desc": "Cubeless add-set retained fixture"})}
                add_id = run_id + "_add_retained"
                added = step("cubeless_add", lambda: mat("add_material_node", add_args, mutation=True, operation_id=add_id), check_mutation, True)
                fixture["nodes"]["added"] = added["data"]["object_path"]
                added_key = added["data"]["node_key"]
                require(fixture["nodes"]["added"].startswith(report["objects"]["material"] + ":"), "Added expression is outside the fixture Material")
                def replay_added(value):
                    check_mutation(value)
                    require(value["retry"].get("replayed") is True and value["data"]["object_path"] == fixture["nodes"]["added"], "Add replay created or selected another node")
                step("replay_cubeless_add", lambda: mat("add_material_node", add_args, mutation=True, operation_id=add_id), replay_added, True)
                step("cubeless_set", lambda: mat("set_material_node_property", {"node_id": added_key, "property_name": "R", "value_json": "0.875"}, mutation=True), check_mutation, True)
                step("independent_added_property", read_added_property, added_property_check)
                def rejected_property(value):
                    require(value.get("success") is False and bool(value.get("error_code")), "Invalid numeric property unexpectedly accepted")
                    native_outcome = value.get("data", {}).get("native_outcome", {})
                    require(native_outcome.get("mutation_attempted") is False and native_outcome.get("changed") is False, "Invalid property has unknown/changed state")
                step("invalid_added_property_rejected", lambda: mat("set_material_node_property", {"node_id": added_key, "property_name": "R", "value_json": '"not-a-number"'}, mutation=True), rejected_property, True)
                step("independent_rejected_property", read_added_property, added_property_check)
            step("resolve", lambda: mat("resolve_material_graph"), require_success)
            graph = step("initial_nodes", lambda: mat("list_material_nodes"), require_success)
            fixture["selectors"] = {kind: node_by_path(graph, path)["node_key"] for kind, path in fixture["nodes"].items()}
            require(graph_state(graph)["node_count"] == (5 if extended else 4), "Unexpected initial expression count")
            step("analyze", lambda: mat("analyze_material_graph", {"include_nodes": True, "include_usage": True, "max_referencers": 25}), require_success)
            def collection_nodes(value):
                require_success(value)
                require(value["data"].get("collection_parameter_node_count") == 1, "Expected one collection expression")
                entry = value["data"]["nodes"][0]
                require(entry.get("parameter_name") == "CubelessScalar", "Collection expression parameter differs")
                require(entry.get("collection_parameter_found") is True and entry.get("parameter_id_matches_collection") is True, "Collection parameter name/ID does not match its MPC")
            step("collection_nodes", lambda: mat("list_material_collection_parameter_nodes"), collection_nodes)
            step("collection_values", lambda: invoke(cubeless, "get_material_parameter_collection_values", {"collection_path": assets["collection"], "include_asset_defaults": True, "include_runtime": True}), collection_check)
            selectors = fixture["selectors"]
            connection = {"from_node_id": selectors["source"], "to_node_id": selectors["add"], "to_input": "A"}
            operation_id = run_id + "_connect_A"
            step("connect_A", lambda: mat("connect_material_nodes", connection, mutation=True, operation_id=operation_id), check_mutation, True)
            def replay_check(value):
                check_mutation(value)
                require(value["retry"]["replayed"] is True, "Duplicate operation ID was executed again")
            step("replay_connect_A", lambda: mat("connect_material_nodes", connection, mutation=True, operation_id=operation_id), replay_check, True)
            step("connect_B", lambda: mat("connect_material_nodes", {"from_node_id": selectors["delete"], "to_node_id": selectors["add"], "to_input": "B"}, mutation=True), check_mutation, True)
            def verify_inputs(value, expected_b):
                require(isinstance(value, list), "Expected official expression-input list")
                entries = {field(entry, "input_name"): entry for entry in value}
                require(ref_path(field(entries["A"], "expression")) == fixture["nodes"]["source"], "Independent A wiring differs")
                source_b = field(entries["B"], "expression")
                if expected_b:
                    require(ref_path(source_b) == expected_b, "Independent B wiring differs")
                else:
                    # UE58 ReferenceConverter returns no custom JSON for null UObject;
                    # JsonObjectConverter falls back to object ExportTextItem: "None".
                    require(source_b == "None", "Deleted expression B lacks the verified UE58 null-object sentinel")
            input_args = {"material_or_function": reference(report["objects"]["material"]), "expression": reference(fixture["nodes"]["add"])}
            step("independent_connection_readback", lambda: invoke(official, "get_expression_inputs", input_args), lambda value: verify_inputs(value, fixture["nodes"]["delete"]))
            baseline = step("before_invalid_pin", lambda: mat("list_material_nodes"), require_success)
            def rejected(value):
                require(value.get("success") is False and bool(value.get("error_code")), "Invalid pin unexpectedly succeeded")
                require(value.get("data", {}).get("native_outcome", {}).get("changed") is False, "Rejected pin has unknown/changed state")
                require(value.get("save", {}).get("saved") is False, "Rejected operation saved the asset")
            step("invalid_pin_rejected", lambda: mat("connect_material_nodes", {**connection, "to_input": "CubelessMissingPin"}, mutation=True), rejected, True)
            after_invalid = step("invalid_pin_readback", lambda: mat("list_material_nodes"), require_success)
            require(graph_state(baseline) == graph_state(after_invalid), "Invalid operation changed graph state")
            step("connect_property", lambda: mat("connect_material_property", {"from_node_id": selectors["add"], "property": "BaseColor"}, mutation=True), check_mutation, True)
            property_args = {"material": reference(report["objects"]["material"]), "material_property": "MP_BaseColor"}
            step("independent_property_readback", lambda: invoke(official, "get_property_input", property_args), lambda value: require(ref_path(field(value, "expression")) == fixture["nodes"]["add"], "Independent root property differs"))
            step("layout", lambda: mat("layout_material_nodes", mutation=True), check_mutation, True)
            step("delete", lambda: mat("delete_material_node", {"node_id": selectors["delete"]}, mutation=True), check_mutation, True)
            final_graph = step("deleted_readback", lambda: mat("list_material_nodes"), require_success)
            require(graph_state(final_graph)["node_count"] == (4 if extended else 3) and all(node.get("object_path") != fixture["nodes"]["delete"] for node in graph_state(final_graph)["nodes"]), "Delete readback still contains the expression")
            step("independent_deleted_link_readback", lambda: invoke(official, "get_expression_inputs", input_args), lambda value: verify_inputs(value, None))
            report["compile"]["explicit_calls"] = 1
            step("compile_once", lambda: invoke(official, "recompile", {"material_or_function": reference(report["objects"]["material"])}, True), mutation=True)
            report["compile"].update(passed=True, status="returned_without_reported_errors", shader_completion_verified=False,
                                     method="Official MaterialTools.recompile returned without reporting errors; completion of shader compilation is not proven", implicit_shader_compiles="possible_during_editor_changes_not_counted")
            report["save"]["explicit_calls"] = 1
            step("save_once", lambda: invoke(asset_tools, "save_assets", {"asset_paths": list(assets.values())}, True), expect_true, True)
            report["save"]["saved"] = True
            report["saved_graph"] = graph_state(step("saved_graph_readback", lambda: mat("list_material_nodes"), require_success))
            if extended:
                step("saved_added_property", read_added_property, added_property_check)
            for kind, path in assets.items():
                step("saved_clean_" + kind, lambda path=path: invoke(asset_tools, "is_dirty", {"asset_path": path}), lambda value: require(value is False, "Saved fixture remains dirty"))
        else:
            require(report["editor"]["instance_id"] != parent["editor"]["instance_id"], "Cold reload requires a different editor process instance")
            step("resolve_reloaded", lambda: mat("resolve_material_graph"), require_success)
            graph = step("reloaded_nodes", lambda: mat("list_material_nodes"), require_success)
            require(graph_state(graph) == parent["saved_graph"], "New-process graph differs from saved author state")
            step("reloaded_collection_values", lambda: invoke(cubeless, "get_material_parameter_collection_values", {"collection_path": assets["collection"], "include_asset_defaults": True, "include_runtime": True}), collection_check)
            fixture = report["fixture"]
            step("reloaded_property_readback", lambda: invoke(official, "get_property_input", {"material": reference(report["objects"]["material"]), "material_property": "MP_BaseColor"}), lambda value: require(ref_path(field(value, "expression")) == fixture["nodes"]["add"], "Cold-reload property wiring differs"))
            if extended:
                step("reloaded_added_property", read_added_property, added_property_check)
            for kind, path in assets.items():
                step("reload_clean_" + kind, lambda path=path: invoke(asset_tools, "is_dirty", {"asset_path": path}), lambda value: require(value is False, "Reloaded fixture is dirty"))
            report["reload"]["status"] = "passed"
        report["status"] = "pass"
    except Exception as error:
        report.update(status="fail", error=str(error), failed_stage=report["stages"][-1]["name"] if report["stages"] else "discovery")
        if isinstance(error, MCPError):
            report["outcome_unknown"] = report["outcome_unknown"] or error.outcome_unknown
    finally:
        report["ended_ns"] = time.time_ns()
        report["elapsed_seconds"] = (report["ended_ns"] - report["started_ns"]) / 1e9
        report["calls"] = client.observations
        client.close()
        persist()
    print(json.dumps({"status": report["status"], "phase": report["phase"], "output": str(output), "failed_stage": report.get("failed_stage")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
