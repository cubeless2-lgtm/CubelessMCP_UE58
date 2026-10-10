"""Offline contract checks for one bounded Custom HLSL Material fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from verification.material_add_set_edges_report import decoded_raw, field, mutation, require

ROOT = Path(__file__).resolve().parents[3]
ALLOWED_PROJECT = ROOT / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
DEFAULT_CODE = "return float3(0.125,0.25,0.5);"
ALIAS_CODE = "return (Signal + Bias) * CUBE_SCALE;"
DESCRIPTION = 'Cubeless Custom "contract"'
PROPERTIES = ["Code", "OutputType", "Description", "Desc", "Inputs", "AdditionalDefines", "IncludeFilePaths", "MaterialExpressionEditorX", "MaterialExpressionEditorY"]
ALIAS_ARGUMENTS = {"code": ALIAS_CODE, "output_type": "FLOAT3", "inputs_json": json.dumps(["Signal", {"input_name": "Bias"}]), "description": DESCRIPTION, "node_position_json": "[-400,100]", "additional_defines_json": json.dumps([{"name": "CUBE_SCALE", "value": .5}, {"define_name": "CUBE_FLAG", "define_value": "1"}, {"name": "CUBE_DEFAULT"}])}
NEGATIVES = {
    "identifier": ({"inputs_json": '["bad-name"]'}, "custom_material_input_invalid"),
    "duplicate": ({"inputs_json": '["Signal","signal"]'}, "custom_material_input_invalid"),
    "long_identifier": ({"inputs_json": json.dumps(["A" * 1024])}, "custom_material_input_invalid"),
    "output": ({"output_type": "CMOT_MAX"}, "custom_material_input_invalid"),
    "json": ({"inputs_json": "["}, "invalid_material_json"),
    "nonarray": ({"inputs_json": "null"}, "invalid_custom_material_array"),
    "define": ({"additional_defines_json": '[{"name":"9bad","value":"1"}]'}, "custom_material_input_invalid"),
    "includes": ({"include_file_paths_json": '["/Project/CubelessNotImported.ush"]'}, "custom_shader_includes_unsupported"),
    "code_include": ({"code": '#include "/Project/CubelessNotImported.ush"\nreturn float3(0,0,0);'}, "custom_shader_includes_unsupported"),
}
AUTHOR_STAGES = ["preflight", "guard_absent", "create_material", "add_default", "read_default", "add_alias", "read_alias", "replay_alias", "replay_readback", "conflict", "conflict_readback"]
for _name in NEGATIVES:
    AUTHOR_STAGES += ["negative_" + _name, "nodes_after_" + _name, "properties_after_" + _name, "reject_" + _name]
AUTHOR_STAGES += ["connect_Signal", "connect_Bias", "independent_inputs", "connect_root", "independent_root", "compile_once", "save_once", "saved_default", "saved_alias", "saved_nodes", "saved_clean", "final_editor"]
RELOAD_STAGES = ["preflight", "reload_default", "reload_alias", "reload_nodes", "independent_inputs", "independent_root", "reload_clean", "final_editor"]


def ref(value):
    return value.get("refPath") if isinstance(value, dict) else value


def properties(value, kind):
    require(field(value, "Code") == (DEFAULT_CODE if kind == "default" else ALIAS_CODE), "Custom Code differs")
    # Engine JsonObjectConverter exports this TEnumAsByte as its authored name.
    require(field(value, "OutputType") == "CMOT_Float3", "Custom enum/default/alias differs")
    expected_description = "" if kind == "default" else DESCRIPTION
    require(field(value, "Description") == expected_description and field(value, "Desc") == expected_description, "Custom description differs")
    inputs = field(value, "Inputs")
    require(isinstance(inputs, list) and [field(row, "InputName") for row in inputs] == ([] if kind == "default" else ["Signal", "Bias"]), "Custom inputs differ")
    defines = field(value, "AdditionalDefines")
    require(isinstance(defines, list) and [(field(row, "DefineName"), field(row, "DefineValue")) for row in defines] == ([] if kind == "default" else [("CUBE_SCALE", "0.5"), ("CUBE_FLAG", "1"), ("CUBE_DEFAULT", "1")]), "Custom defines differ")
    require(field(value, "IncludeFilePaths") == [], "External include dependency appeared")
    position = [field(value, "MaterialExpressionEditorX"), field(value, "MaterialExpressionEditorY")]
    require(all(type(v) is int for v in position) and position == ([0, 0] if kind == "default" else [-400, 100]), "Custom position differs")


def expression_set(value, nodes):
    require(isinstance(value, list) and len(value) == 2 and {ref(v) for v in value} == set(nodes.values()), "Expression set changed")


def inputs(value, nodes):
    require(isinstance(value, list) and len(value) == 2 and {field(r, "input_name") for r in value} == {"Signal", "Bias"}, "Input pin set differs")
    require(all(ref(field(r, "expression")) == nodes["default"] and field(r, "output_name") == "" for r in value), "Independent input connection differs")


def rejection(value, expected):
    require(value.get("success") is False and value.get("error_code") == expected and bool(value.get("error")), "Wrong negative result/category")
    require(value.get("changes") == [] and value.get("retry", {}).get("automatic_retry") is False and not value.get("compile", {}).get("requested") and not value.get("save", {}).get("saved"), "Rejected operation changed/compiled/saved/retried")
    native = value.get("data", {}).get("native_outcome")
    if expected in {"custom_material_input_invalid", "custom_shader_includes_unsupported"}:
        require(isinstance(native, dict) and value.get("stage") == "preflight", "Native rejection has no preflight evidence")
    if native is not None:
        require(native.get("mutation_attempted") is False and native.get("change_state_known") is True and native.get("changed") is False, "Native rejection outcome is not unchanged")
    return expected


def verify(report, *, parent=None, parent_bytes=None, check_files=True):
    errors = []
    try:
        phase = report.get("phase")
        require(report.get("schema") == "cubeless_custom_material_pipeline_v1" and phase in {"author", "reload"}, "Wrong report schema/phase")
        require(report.get("status") == "pass" and report.get("outcome_unknown") is False and report.get("automatic_retries") == 0, "Failed/unknown/retried outcome")
        require(Path(report["project_file"]).resolve() == ALLOWED_PROJECT.resolve(), "Wrong isolated project")
        require(report.get("full_legacy_custom_parity") is False and report.get("atomic_rollback") is False and report.get("packaging") == "not_run" and report.get("shader_completion_verified") is False and report.get("external_includes_supported") is False, "Overstated coverage")
        run = report["run_id"]; require(re.fullmatch(r"[0-9a-f]{32}", run) is not None, "Invalid run ID")
        package = "/Game/CubelessMCPTests/Custom_" + run + "/M_Custom"
        require(report.get("material_package") == package and report.get("material_object") == package + ".M_Custom", "Wrong fixture owner")
        nodes = report["nodes"]
        require(set(nodes) == {"default", "alias"} and len(set(nodes.values())) == 2 and all(p.startswith(report["material_object"] + ":") for p in nodes.values()), "Wrong node ownership")
        rows = report["stages"]
        require([r["name"] for r in rows] == (AUTHOR_STAGES if phase == "author" else RELOAD_STAGES) and all(r.get("passed") is True for r in rows), "Missing/reordered/failed stage")
        byname = {r["name"]: r for r in rows}; result = {k: r["result"] for k, r in byname.items()}; calls = report["tool_calls"]
        require([i for row in rows for i in row["call_indices"]] == list(range(len(calls))), "Unbound/duplicated calls")
        for row in rows:
            indices = row["call_indices"]
            require(len(indices) == (0 if row["name"].startswith("reject_") else 1), "Wrong call count per stage")
            if indices:
                c = calls[indices[0]]
                require(decoded_raw(c["raw"]) == c["decoded"] == row["result"], "Raw call/result binding differs")
        def call(name, tool, arguments, mutation_call=False):
            c = calls[byname[name]["call_indices"][0]]
            require(c["tool"] == tool and c["arguments"] == arguments and c["mutation"] is mutation_call, "Wrong call contract: " + name)
            suffix = {"GetEditorDiagnostics": "CubelessNativeToolset", "get_properties": "ObjectTools", "exists": "AssetTools", "save_assets": "AssetTools", "is_dirty": "AssetTools", "add_custom_material_node": "CubelessMaterialTools", "connect_material_nodes": "CubelessMaterialTools", "connect_material_property": "CubelessMaterialTools"}.get(tool, "MaterialTools")
            require(c.get("toolset", "").endswith("." + suffix), "Wrong toolset provenance: " + name)
        editor = report["editor"]
        require(result["preflight"] == editor, "Editor summary differs")
        for name in ("preflight", "final_editor"):
            e = result[name]
            require(e.get("success") is True and e.get("instance_id") == editor.get("instance_id") and bool(e.get("instance_id")) and e.get("game_thread") is True and e.get("pie_or_sie_active") is False and e.get("dirty_packages") == [] and str(e.get("engine_version", "")).startswith("5.8.3-") and Path(e["project_file"]).resolve() == ALLOWED_PROJECT.resolve(), "Wrong/dirty editor")
            call(name, "GetEditorDiagnostics", {})
        def custom_args(kind):
            return {"material_path": package, **({"code": DEFAULT_CODE} if kind == "default" else ALIAS_ARGUMENTS), "operation_id": run + "_" + kind}
        def prop_call(name, kind):
            properties(result[name], kind)
            call(name, "get_properties", {"instance": {"refPath": nodes[kind]}, "properties": PROPERTIES})
        if phase == "author":
            require(result["guard_absent"] is False and result["create_material"] == {"refPath": report["material_object"]}, "Creation guard differs")
            call("guard_absent", "exists", {"path": package})
            call("create_material", "create_material", {"folder_path": package.rsplit("/", 1)[0], "asset_name": "M_Custom"}, True)
            for kind in ("default", "alias"):
                mutation(result["add_" + kind]); call("add_" + kind, "add_custom_material_node", custom_args(kind), True)
                data = result["add_" + kind]["data"]
                require(data.get("object_path") == nodes[kind] and data.get("created_custom_node") is True and data.get("input_count") == (0 if kind == "default" else 2), "Created Custom identity/details differ")
                require(data.get("node_key") == report["selectors"][kind] and data.get("custom_shader_code_compiled") is False, "Custom selector/shader claim differs")
                prop_call("read_" + kind, kind)
            mutation(result["replay_alias"]); call("replay_alias", "add_custom_material_node", custom_args("alias"), True)
            require(result["replay_alias"]["retry"].get("replayed") is True and result["replay_alias"]["data"] == result["add_alias"]["data"], "Replay repeated creation or changed historical result")
            conflict_args = {**custom_args("alias"), "code": DEFAULT_CODE}
            rejection(result["conflict"], "operation_id_conflict"); call("conflict", "add_custom_material_node", conflict_args, True)
            for name in ("replay_readback", "conflict_readback", "saved_nodes"):
                expression_set(result[name], nodes); call(name, "get_expressions", {"material_or_function": {"refPath": report["material_object"]}})
            for key, (args, code) in NEGATIVES.items():
                call("negative_" + key, "add_custom_material_node", {"material_path": package, "code": DEFAULT_CODE, **args, "operation_id": run + "_negative_" + key}, True)
                require(result["reject_" + key] == rejection(result["negative_" + key], code), "Rejection classification differs")
                expression_set(result["nodes_after_" + key], nodes); call("nodes_after_" + key, "get_expressions", {"material_or_function": {"refPath": report["material_object"]}})
                prop_call("properties_after_" + key, "alias")
                require(result["properties_after_" + key] == result["read_alias"], "Rejected creation altered existing node properties")
            for pin in ("Signal", "Bias"):
                mutation(result["connect_" + pin]); call("connect_" + pin, "connect_material_nodes", {"material_path": package, "from_node_id": report["selectors"]["default"], "to_node_id": report["selectors"]["alias"], "to_input": pin, "operation_id": run + "_connect_" + pin}, True)
            mutation(result["connect_root"]); call("connect_root", "connect_material_property", {"material_path": package, "from_node_id": report["selectors"]["alias"], "property": "BaseColor", "operation_id": run + "_root"}, True)
            require(report.get("compile") == {"explicit_calls": 1, "status": "returned_without_reported_errors"} and result["compile_once"] in (None, {}), "Compile evidence differs")
            call("compile_once", "recompile", {"material_or_function": {"refPath": report["material_object"]}}, True)
            require(report.get("save") == {"explicit_calls": 1, "saved": True} and result["save_once"] is True, "Save contract differs")
            call("save_once", "save_assets", {"asset_paths": [package]}, True)
            for kind in ("default", "alias"): prop_call("saved_" + kind, kind)
            require(result["saved_clean"] is False, "Saved asset dirty")
            call("saved_clean", "is_dirty", {"asset_path": package})
        else:
            require(parent is not None and parent_bytes is not None and json.loads(parent_bytes) == parent and parent.get("phase") == "author" and verify(parent, check_files=check_files)["passed"], "Invalid author evidence")
            require(report.get("author_sha256") == hashlib.sha256(parent_bytes).hexdigest() and editor["instance_id"] != parent["editor"]["instance_id"], "Parent hash/same process differs")
            require(all(report[k] == parent[k] for k in ("nodes", "selectors", "run_id", "material_package", "material_object", "asset_file")), "Reload identity/disk differs")
            require(report.get("compile") == {"explicit_calls": 0, "status": "not_requested"} and report.get("save") == {"explicit_calls": 0, "saved": False} and all(c["mutation"] is False for c in calls), "Reload compiled/saved/mutated")
            for kind in ("default", "alias"):
                prop_call("reload_" + kind, kind)
                require(result["reload_" + kind] == next(s["result"] for s in parent["stages"] if s["name"] == "saved_" + kind), "Cold property readback differs")
            expression_set(result["reload_nodes"], nodes); call("reload_nodes", "get_expressions", {"material_or_function": {"refPath": report["material_object"]}})
            require(result["reload_clean"] is False, "Reload asset dirty"); call("reload_clean", "is_dirty", {"asset_path": package})
        inputs(result["independent_inputs"], nodes)
        call("independent_inputs", "get_expression_inputs", {"material_or_function": {"refPath": report["material_object"]}, "expression": {"refPath": nodes["alias"]}})
        require(ref(field(result["independent_root"], "expression")) == nodes["alias"], "Root material connection differs")
        call("independent_root", "get_property_input", {"material": {"refPath": report["material_object"]}, "material_property": "MP_BaseColor"})
        path = ALLOWED_PROJECT.parent / "Content" / (package[len("/Game/"):] + ".uasset")
        require(Path(report["asset_file"]["path"]).resolve() == path.resolve() and re.fullmatch(r"[0-9a-f]{64}", report["asset_file"]["sha256"]), "Wrong asset disk evidence")
        if check_files: require(path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == report["asset_file"]["sha256"], "Asset bytes changed")
    except (AssertionError, AttributeError, ValueError, TypeError, KeyError, IndexError, OSError, StopIteration) as error:
        errors.append(str(error))
    return {"schema": "cubeless_custom_material_verification_v1", "passed": not errors, "errors": errors, "phase": report.get("phase"), "full_legacy_custom_parity": False, "shader_completion_verified": False}


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument("report", type=Path); p.add_argument("--parent-report", type=Path); p.add_argument("--output", required=True, type=Path)
    a = p.parse_args(); raw = a.report.read_bytes(); parent_raw = a.parent_report.read_bytes() if a.parent_report else None
    result = verify(json.loads(raw), parent=json.loads(parent_raw) if parent_raw else None, parent_bytes=parent_raw)
    result["report_sha256"] = hashlib.sha256(raw).hexdigest()
    with a.output.open("x", encoding="utf-8") as f: json.dump(result, f, indent=2)
    print(json.dumps(result)); return 0 if result["passed"] else 1


if __name__ == "__main__":
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    raise SystemExit(main())
