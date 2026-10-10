"""Port the legacy caller-driven mouse timer author sequence to official UE58 MCP.

A fresh owned Actor or ActorComponent Blueprint isolates function authoring;
clone/parent proof is handled by the separate duplicate pipeline. No Tick/caller is installed, no graph is executed and no mutation retries.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from pipelines.component_authoring import PROJECT, RECORDS, SETTINGS, fingerprint, require, success, field
from pipelines.blueprint_variables import typed_equal, variable_success
from pipelines.blueprint_variable_graph import TOOLSETS, graph_view, node_identity, no_explicit_compile_save, graph_mutation, batch_children

FUNCTION = "CFNL_AdvanceMouseTime"
MEMBER = "TimeSinceLastClick"
MATH_CLASS = "/Script/Engine.KismetMathLibrary"
MATH_FUNCTION = "Add_DoubleDouble"
DEFAULTS = {MEMBER: 0.0}


def function_success(value, *, unchanged=False):
    success(value); no_explicit_compile_save(value)
    native = value["data"]["native_outcome"]
    require(native.get("change_state_known") is True and native.get("postcondition_passed") is True,
            "Function outcome lacks observed postconditions")
    if unchanged:
        require(native.get("changed") is False and native.get("mutation_attempted") is False
                and value.get("changes") == [], "Expected no function change")


def graph_metadata(data, package, expected_guid=None):
    value = data["graph"]
    require(value["graph_name"] == FUNCTION and value["graph_type"] == "function"
            and value["object_path"].startswith(package + ".") and uuid.UUID(value["graph_id"]).int,
            "Function graph identity differs")
    if expected_guid:
        require(uuid.UUID(value["graph_id"]).hex == uuid.UUID(expected_guid).hex, "Graph selector resolved another GUID")
    return value


def numeric_type(value, width):
    return value.get("category") == "real" and value.get("subcategory") == width


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", required=True); p.add_argument("--expected-project", required=True, type=Path)
    p.add_argument("--expected-pid", required=True, type=int); p.add_argument("--output", required=True, type=Path)
    p.add_argument("--parent-class", choices=("Actor", "ActorComponent"), default="Actor")
    a = p.parse_args(argv); out = a.output.resolve()
    require(a.expected_project.resolve() == PROJECT.resolve(), "Exact isolated project required")
    require(out.is_relative_to(RECORDS.resolve()) and not out.exists(), "Use a fresh verification report")
    run = uuid.uuid4().hex; folder = "/Game/CubelessMCPTests/FunctionTimer_" + run
    package = folder + "/BP_FunctionTimer"; asset_ref = package + ".BP_FunctionTimer"
    disk = PROJECT.parent / "Content" / (package[6:] + ".uasset")
    require(not disk.parent.exists(), "Owned UUID folder already exists")
    config = PROJECT.parent / "Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini"
    report = {"schema": "cubeless_blueprint_function_timer_v2", "status": "running", "run_id": run,
        "package": package, "project_file": str(PROJECT.resolve()), "parent_class": a.parent_class,
        "expected_parent_class_path": "/Script/Engine." + a.parent_class, "expected_defaults": DEFAULTS, "function_name": FUNCTION, "caller_gating_required": True, "unconditional_tick": False,
        "stages": [], "changes": [], "started_ns": time.time_ns(), "automatic_retries": 0,
        "outcome_unknown": False, "atomic_rollback": False, "cold_reload": "not_run", "packaging": "not_run",
        "runtime_execution": "not_run", "full_legacy_pipeline_parity": False, "full_legacy_node_parity": False,
        "legacy": {"id": "script:Python/scripts/analysis/build_cube_fluid_ninja_mouse_timer.py",
            "sha256": "696ea2d51014152d55cde30a3f7e7720ad4b230a1153ea6e9be6e8036319ae29",
            "scope": "function author sequence on a fresh " + a.parent_class + " BP; legacy clone/parent proof and caller runtime execution are separate"},
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), "config_before": fingerprint(config)}
    c = OfficialMCPClient(a.endpoint); schemas = {}

    def persist():
        out.parent.mkdir(parents=True, exist_ok=True); temp = out.with_suffix(out.suffix + ".partial")
        temp.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"); temp.replace(out)

    def call(label, group, tool, arguments=None, *, mutation=False, expect_success=True):
        report["stage"] = label
        choices = [t for t in schemas[group]["tools"] if t["name"].rsplit(".", 1)[-1] == tool]
        require(len(choices) == 1, "Missing unique tool " + tool); schema = choices[0]["inputSchema"]; mapped = {}
        for key, value in (arguments or {}).items():
            keys = [k for k in schema.get("properties", {}) if canonical(k) == canonical(key)]
            require(len(keys) == 1, "Missing input " + key); mapped[keys[0]] = value
        require(all(k in mapped for k in schema.get("required", [])), "Unexpected required argument")
        row = {"name": label, "toolset": TOOLSETS[group], "tool": tool, "arguments": mapped,
               "mutation": mutation, "state": "submitted", "started_ns": time.time_ns()}
        report["stages"].append(row); persist()
        try:
            row["raw"] = c.call_toolset(TOOLSETS[group], tool, mapped, mutation=mutation)
            value = operation_result(row["raw"]); row.update(result=value, state="responded")
            if isinstance(value, dict) and "changes" in value:
                report["changes"].append({"stage": label, "observations": value["changes"]})
                row.update(compile=value.get("compile"), save=value.get("save"))
                data = value.get("data", {})
                if (any(x.get("change_state_known") is False for x in value["changes"])
                        or data.get("mutation_state_unknown") or data.get("batch", {}).get("mutation_state_unknown")):
                    report["outcome_unknown"] = True
            if group in {"native", "bp", "graph", "batch"} and expect_success: success(value)
            return value
        except Exception as error:
            row["error"] = str(error)
            if row["state"] == "submitted" and mutation: report["outcome_unknown"] = True
            raise
        finally:
            row["elapsed_seconds"] = (time.time_ns() - row["started_ns"]) / 1e9; persist()

    def inspect(label, graph_name=FUNCTION):
        value = call(label, "graph", "inspect_graph", {"blueprint_name": package, "graph_name": graph_name})
        no_explicit_compile_save(value); require(value["changes"] == [], "Graph read reported changes")
        graph_view(value["data"]); return value["data"]

    def command(group, tool, **params):
        return {"type": TOOLSETS[group].rsplit(".", 1)[-1] + "." + tool, "params": {"blueprint_name": package, **params}}

    try:
        persist(); c.initialize()
        schemas.update({k: tool_payload(c.call("describe_toolset", {"toolset_name": v})) for k, v in TOOLSETS.items()})
        report["schemas"] = schemas
        required = {"resolve_blueprint_graph", "add_blueprint_function_parameter", "add_blueprint_call_function_node",
                    "add_blueprint_variable_get_node", "add_blueprint_variable_set_node", "connect_graph_pins", "inspect_graph"}
        require(required.issubset({t["name"].rsplit(".", 1)[-1] for t in schemas["graph"]["tools"]}), "U graph tools not registered")
        editor = call("preflight", "native", "GetEditorDiagnostics")
        require(Path(editor["project_file"]).resolve() == PROJECT.resolve() and editor["process_id"] == a.expected_pid
                and editor["game_thread"] is True and editor["engine_version"].startswith("5.8.3-")
                and not editor["pie_or_sie_active"] and editor["dirty_packages"] == [], "Wrong/dirty editor")
        report["editor"] = editor
        policy = call("save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
        report["save_policy_before"] = policy.get("SaveOnCompile")
        require(report["save_policy_before"] == "SoC_Never", "Requires Never; settings are not changed")
        require(call("absent", "assets", "exists", {"path": package}) is False, "Asset exists")
        created = call("create", "bp", "create_blueprint", {"name": "BP_FunctionTimer", "parent_class": a.parent_class,
            "package_path": folder, "operation_id": run + "_create"}, mutation=True)
        require(created["save"]["saved"] is False and not disk.exists(), "Creation saved unexpectedly")
        report["implicit_factory_compile"] = created["compile"].get("implicit_factory_compile")
        factory = inspect("factory_event_graph", "EventGraph")
        require(not graph_view(factory)[1], "Fresh factory event graph was already connected")
        report["factory_event_graph"] = factory
        commands = [command("bp", "add_blueprint_variable", variable_name=MEMBER, variable_type="Float",
                            default_value_json="0", is_exposed=False, category="Mouse Interaction"),
            command("graph", "resolve_blueprint_graph", graph_name=FUNCTION, graph_type="function", create_if_missing=True),
            command("graph", "add_blueprint_function_parameter", graph_name=FUNCTION, graph_type="function",
                    parameter_name="DeltaSeconds", parameter_type="float", direction="input")]
        prepared = call("member_function_parameter_batch", "batch", "execute_batch", {"commands_json": json.dumps(commands),
            "operation_id": run + "_prepare"}, mutation=True)
        children = batch_children(prepared, 3); variable_success(children[0])
        declaration = children[0]["data"]["verified_variable"]
        require(declaration["instance_editable"] is False and declaration["blueprint_visible"] is True,
                "Original private-to-instance but Blueprint-visible member flags differ")
        for child in children[1:]: function_success(child)
        require(not prepared["compile"]["entries"] and not prepared["save"]["entries"], "Preparation explicitly compiled/saved")
        resolved = children[1]["data"]; parameter = children[2]["data"]
        require(resolved["resolved"] is True and resolved["created"] is True, "Expected a newly created function graph")
        metadata = graph_metadata(resolved, package); graph_guid = metadata["graph_id"]
        graph_metadata(parameter, package, graph_guid)
        require(parameter["parameter_name"] == "DeltaSeconds" and parameter["direction"] == "input"
                and numeric_type(parameter["pin_type"], "float") and parameter["pin_type"]["is_array"] is False
                and parameter["default_requested"] is False, "Function input/default contract differs")
        initial = inspect("function_entry"); nodes, edges = graph_view(initial)
        require(initial["graph_path"] == metadata["object_path"] and len(nodes) == 1 and not edges, "Expected one unlinked entry node")
        entry = next(iter(nodes.values()))
        require(entry["class"] == "K2Node_FunctionEntry" and entry["node_id"] == parameter["object_path"]
                and uuid.UUID(entry["node_guid"]).hex == uuid.UUID(parameter["node_id"]).hex,
                "Parameter result and actual entry identity differ")
        require({(pin["name"], pin["direction"]) for pin in entry["pins"]} == {("then", "output"), ("DeltaSeconds", "output")},
                "Entry pin signature differs from the original one-input timer")
        report.update(function_graph=metadata, entry_graph=initial)
        duplicate = call("reject_duplicate_parameter", "graph", "add_blueprint_function_parameter", {"blueprint_name": package,
            "graph_id": graph_guid, "graph_type": "function", "parameter_name": "DeltaSeconds", "parameter_type": "float",
            "operation_id": run + "_duplicate"}, mutation=True, expect_success=False)
        require(duplicate.get("success") is False and duplicate.get("stage") == "preflight"
                and duplicate.get("error_code") == "function_parameter_collision" and duplicate.get("changes") == []
                and duplicate["data"]["native_outcome"]["mutation_attempted"] is False, "Duplicate parameter did not reject before change")
        no_explicit_compile_save(duplicate)
        require(inspect("duplicate_parameter_unchanged") == initial, "Duplicate request changed entry/signature")
        commands = [command("graph", "add_blueprint_variable_get_node", graph_name=FUNCTION, variable_name=MEMBER, x=-400, y=160),
            command("graph", "add_blueprint_call_function_node", graph_id=graph_guid, graph_type="function",
                    function_class=MATH_CLASS, function_name=MATH_FUNCTION, param_defaults_json="{}", node_position_json="[0,160]"),
            command("graph", "add_blueprint_variable_set_node", graph_name=FUNCTION, variable_name=MEMBER, x=400, y=0)]
        authored = call("getter_call_setter_batch", "batch", "execute_batch", {"commands_json": json.dumps(commands),
            "operation_id": run + "_nodes"}, mutation=True)
        children = batch_children(authored, 3)
        graph_mutation(children[0]); function_success(children[1]); graph_mutation(children[2])
        require(not authored["compile"]["entries"] and not authored["save"]["entries"] and not disk.exists(), "Node authoring compiled/saved")
        getter_result, math_result, setter_result = [child["data"] for child in children]
        graph_metadata(math_result, package, graph_guid)
        require(math_result["function_name"] == MATH_FUNCTION and math_result["function_class"] == MATH_CLASS
                and math_result["is_static"] is True and math_result["is_pure"] is True and math_result["is_latent"] is False
                and math_result["param_defaults_applied"] in ({}, []), "Validated runtime math call differs")
        before_links = inspect("timer_nodes"); nodes, edges = graph_view(before_links)
        created_ids = {getter_result["node_id"], math_result["object_path"], setter_result["node_id"]}
        require(len(created_ids) == 3 and set(nodes) == {entry["node_id"]} | created_ids and not edges
                and nodes[entry["node_id"]] == entry, "Unexpected timer nodes/links or altered entry")
        getter, math_node, setter = [nodes[node] for node in (getter_result["node_id"], math_result["object_path"], setter_result["node_id"])]
        for node, result, kind in ((getter, getter_result, "K2Node_VariableGet"), (setter, setter_result, "K2Node_VariableSet")):
            require(node["class"] == kind and node["variable_name"] == MEMBER and node["self_context"] is True
                    and node["variable_property_path"] == result["member_preflight"]["property_path"], "Own timer member node differs")
        require(math_result["node_class"] == "/Script/BlueprintGraph." + math_node["class"]
                and uuid.UUID(math_node["node_guid"]).hex == uuid.UUID(math_result["node_id"]).hex
                and math_node["function_name"] == MATH_FUNCTION and math_node["function_owner_path"] == MATH_CLASS
                and math_node["function_path"] == math_result["function_path"], "Independent math function identity differs")
        report["authored_graph"] = before_links
        links = [(getter, MEMBER, math_node, "A"), (entry, "DeltaSeconds", math_node, "B"),
                 (math_node, "ReturnValue", setter, MEMBER), (entry, "then", setter, "execute")]
        commands = [command("graph", "connect_graph_pins", graph_name=FUNCTION, source_node_id=s["node_guid"],
            source_pin_name=sp, target_node_id=t["node_guid"], target_pin_name=tp) for s, sp, t, tp in links]
        commands += [command("bp", "compile_and_save_blueprint", save=True)]
        connected = call("timer_links_compile_save_batch", "batch", "execute_batch", {"commands_json": json.dumps(commands),
            "operation_id": run + "_links_save"}, mutation=True)
        children = batch_children(connected, 5); plans = []
        for index, child in enumerate(children[:4]):
            graph_mutation(child); plan = child["data"]["connection_plan"]; plans.append(plan)
            require(plan["direct_connection_allowed"] is True and child["data"]["topology_preserved"] is True, "Schema did not permit a direct connection")
            source_type, target_type = plan["source_pin_type"], plan["target_pin_type"]
            if index < 3:
                widths = ("double", "float") if index == 2 else ("float", "double")
                require(numeric_type(source_type, widths[0]) and numeric_type(target_type, widths[1])
                        and plan["type_exactly_equal"] is False, "Original real-width contract was silently changed")
            else:
                require(source_type["category"] == target_type["category"] == "exec" and plan["type_exactly_equal"] is True,
                        "Execution pin contract differs")
        compiled = children[-1]; success(compiled)
        native_compile = compiled["data"].get("compile", compiled["data"])
        require(compiled["compile"].get("compiled") is True and compiled["compile"].get("error_count") == 0
                and native_compile.get("validation_pass") is True and compiled["save"].get("saved") is True
                and len(connected["compile"]["entries"]) == len(connected["save"]["entries"]) == 1, "Expected one validated full compile/save")
        saved = fingerprint(disk); require(saved is not None, "Timer asset not saved")
        final_graph = inspect("saved_timer_graph"); final_nodes, final_edges = graph_view(final_graph)
        expected_edges = set()
        for s, sp, t, tp in links:
            expected_edges |= {(s["node_id"], "output", sp, t["node_id"], tp), (t["node_id"], "input", tp, s["node_id"], sp)}
        require({k: node_identity(v) for k, v in final_nodes.items()} == {k: node_identity(v) for k, v in nodes.items()}
                and final_edges == expected_edges and len(final_nodes) == 4, "Timer graph identity/topology changed at compile/save")
        final_math = final_nodes[math_node["node_id"]]
        require(all(final_math.get(key) == math_node.get(key) for key in ("function_name", "function_owner_path", "function_path")),
                "Compiled call references another function")
        require(inspect("event_graph_untouched", "EventGraph") == factory, "Unexpected Tick/event caller installation")
        final_resolve = call("saved_graph_by_guid", "graph", "resolve_blueprint_graph", {"blueprint_name": package,
            "graph_id": graph_guid, "graph_type": "function", "operation_id": run + "_resolve_saved"})
        function_success(final_resolve, unchanged=True); final_metadata = graph_metadata(final_resolve["data"], package, graph_guid)
        require(final_resolve["data"]["resolved"] is True and final_resolve["data"]["created"] is False
                and final_metadata["object_path"] == final_graph["graph_path"] and final_metadata["node_count"] == 4,
                "Saved graph GUID did not resolve the authored timer")
        cdo = call("official_default_object", "official_bp", "get_default_object", {"blueprint": {"refPath": asset_ref}})
        require(isinstance(cdo, dict) and isinstance(cdo.get("refPath"), str) and cdo["refPath"].startswith(package + "."), "CDO escaped owned asset")
        parent = call("official_parent", "official_bp", "get_parent", {"blueprint": {"refPath": asset_ref}})
        require(parent == {"refPath": report["expected_parent_class_path"]}, "Official parent class differs")
        report["official_parent"] = parent
        values = call("official_cdo_default", "objects", "get_properties", {"instance": cdo, "properties": [MEMBER]})
        observed = field(values, MEMBER); require(typed_equal(observed, 0.0), "Unexecuted timer default differs")
        require(call("asset_clean", "assets", "is_dirty", {"asset_path": package}) is False and fingerprint(disk) == saved, "Readback changed saved asset")
        final = call("final_editor", "native", "GetEditorDiagnostics")
        require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == [] and not report["outcome_unknown"], "Editor changed/dirty or uncertain result")
        report.update(status="pass", saved_graph=final_graph, saved_function_graph=final_metadata, connection_plans=plans,
            expected_edges=[list(e) for e in sorted(expected_edges)], asset_file=saved, generated_cdo=cdo, cdo_values={MEMBER: observed},
            explicit_full_compile_requests=1, explicit_save_requests=1, duplicate_parameter_rejection_verified=True,
            direct_connections=4, direct_real_width_compatibility_verified=True, real_width_conversion_execution_verified=False,
            caller_event_graph_unchanged=True,
            graph_execution_scope="Caller-driven timer topology compiled only; function was not invoked")
    except Exception as error:
        report.update(status="failed", error=str(error), failed_stage=report.get("stage", "discovery"), do_not_retry=True)
    finally:
        original_stage = report.get("stage")
        if report.get("editor") and report.get("save_policy_before") == "SoC_Never":
            try:
                policy = call("final_save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
                report["save_policy_after"] = policy.get("SaveOnCompile")
                report["save_policy_unchanged"] = report["save_policy_after"] == report["save_policy_before"]
                require(report["save_policy_unchanged"], "SaveOnCompile changed")
            except Exception as error:
                report["final_save_policy_error"] = str(error)
                if report.get("status") != "failed": report.update(error=str(error), failed_stage="final_save_policy")
                report["status"] = "failed"
            finally: report["stage"] = original_stage
        report.update(asset_after=fingerprint(disk), config_after=fingerprint(config), finished_ns=time.time_ns(), http_observations=c.observations)
        report["config_changed"] = report["config_after"] != report["config_before"]
        report["config_observation_scope"] = "Ini bytes observed only; SaveOnCompile checked separately."
        persist(); c.close()
    print(json.dumps({k: report.get(k) for k in ("status", "package", "failed_stage", "error", "outcome_unknown")}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
