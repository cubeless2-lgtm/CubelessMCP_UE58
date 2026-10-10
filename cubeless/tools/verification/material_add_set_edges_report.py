"""Independent evidence checks for Material Add/Set JSON/array edge contracts."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
ALLOWED_PROJECT = ROOT / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
POSITION_CASES = {"short": [1], "long": [1, 2, 3], "fractional": [1.5, 2], "boolean": [True, 2], "overflow": [2147483648, 0]}
VALUE_CASES = {"boolean": "true", "null": "null", "array": "[]", "object": "{}", "trailing": "0.5 trailing", "multiple": "0.5,0.75"}
PROPERTIES = ["R", "Desc", "MaterialExpressionEditorX", "MaterialExpressionEditorY"]
INITIAL_DESC = 'Cubeless initial "caption"'
FINAL_DESC = 'Cubeless quoted "caption" \\ path'
POSITION_CONVERTER_ERROR = "Parameter error: Cubeless node_position requires exactly two finite int32 JSON numbers; coercions and malformed non-null arrays are rejected before the operation.."
AUTHOR_STAGES = ["preflight"] + ["position_" + key for key in POSITION_CASES] + ["position_null_observation", "post_position_editor", "guard_absent", "create_material", "add_default", "read_default", "add_position", "read_position", "set_number", "read_number", "set_quoted", "read_quoted"]
for _key in VALUE_CASES:
    AUTHOR_STAGES += ["value_" + _key, "read_after_" + _key, "reject_" + _key]
AUTHOR_STAGES += ["official_expression_array", "save_once", "saved_default", "saved_position", "saved_clean", "final_editor"]
RELOAD_STAGES = ["preflight", "reload_default", "reload_position", "official_expression_array", "reload_clean", "final_editor"]


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def field(value, key):
    require(isinstance(value, dict), "Expected property object")
    matches = [v for k, v in value.items() if k.replace("_", "").lower() == key.replace("_", "").lower()]
    require(len(matches) == 1, "Missing/ambiguous property: " + key)
    return matches[0]


def readback(value, number, description, position):
    r = field(value, "R")
    require(type(r) in (int, float) and abs(r - number) < 1e-6, "R value/type differs")
    require(field(value, "Desc") == description, "Desc differs")
    require(type(field(value, "MaterialExpressionEditorX")) is int and type(field(value, "MaterialExpressionEditorY")) is int, "Coordinates are not integers")
    require([field(value, "MaterialExpressionEditorX"), field(value, "MaterialExpressionEditorY")] == position, "Position differs")


def mutation(value):
    require(isinstance(value, dict) and value.get("success") is True, "Mutation failed")
    native = value.get("data", {}).get("native_outcome", {})
    require(native.get("change_state_known") is True and native.get("postcondition_passed") is True and value.get("validation", {}).get("native_postcondition_passed") is True, "Missing known mutation postcondition")
    require(not value.get("compile", {}).get("requested") and not value.get("save", {}).get("saved") and value.get("retry", {}).get("automatic_retry") is False, "Mutation compile/save/retry contract differs")


def decoded_raw(raw):
    """Re-decode recorded raw envelope independently of its cached decoded copy."""
    require(not raw.get("isError"), "Raw official error is not a successful result")
    value = raw.get("structuredContent")
    if value is None:
        texts = [row.get("text", "") for row in raw.get("content", []) if row.get("type") == "text"]
        require(len(texts) == 1, "Raw result has no unique payload")
        value = json.loads(texts[0])
    if isinstance(value, dict) and "returnValue" in value: value = value["returnValue"]
    if isinstance(value, str): value = json.loads(value)
    if not isinstance(value, dict): return value
    fields = {k.replace("_", "").lower(): v for k, v in value.items()}
    if "changesjson" not in fields: return value
    result = {k: fields[k.replace("_", "")] for k in ("success", "operation_id", "stage", "error_code", "error")}
    result.update({k: json.loads(fields[k + "json"]) for k in ("changes", "compile", "save", "validation", "retry", "data")})
    return result


def rejected(probe, kind):
    """Classify source-backed failure stages, never any-error-as-success."""
    raw, value = probe.get("raw", {}), probe.get("decoded")
    if raw.get("isError"):
        text = "\n".join(row.get("text", "") for row in raw.get("content", []) if row.get("type") == "text")
        generic_decline = 'Function "add_material_node", could not convert incoming function input params Json to a UStruct.' in text and '"node_position"' in text
        require(kind == "position" and (generic_decline or text == POSITION_CONVERTER_ERROR), "Unexpected official error; not a proven array converter rejection")
        require(value is None, "Converter rejection has a fabricated decoded result")
        return "official_parameter_conversion_rejected"
    require(isinstance(value, dict) and value.get("success") is False and value.get("changes") == [], "Negative input accepted or mutated")
    require(value.get("retry", {}).get("automatic_retry") is False and not value.get("compile", {}).get("requested") and not value.get("save", {}).get("saved"), "Negative input has compile/save/retry side effects")
    if kind == "position":
        require(value.get("error_code") == "invalid_material_node_position", "Array was not rejected by strict position validation")
        return "python_position_validation_rejected"
    if kind == "parse":
        require(value.get("error_code") == "invalid_material_json", "Malformed scalar was not rejected by JSON parsing")
        return "python_json_parse_rejected"
    require(value.get("error_code") == "material_mutation_target_rejected", "Expected native property-type preflight rejection")
    native = value.get("data", {}).get("native_outcome", {})
    require(native.get("mutation_attempted") is False and native.get("changed") is False and native.get("change_state_known") is True, "Property rejection outcome is not unchanged")
    require("Numeric property requires a finite number: R" in value.get("error", ""), "Failure is not the numeric property-type guard")
    return "native_numeric_property_type_rejected"


def null_observation(probe):
    """Null can be skipped above the custom array converter; never require rejection."""
    if probe.get("raw", {}).get("isError"):
        require(probe.get("decoded") is None, "Null official error has fabricated result")
        return "official_error_observed_on_empty_asset_path"
    value = probe.get("decoded", {})
    require(value.get("success") is False and value.get("changes") == [] and not value.get("save", {}).get("saved") and not value.get("compile", {}).get("requested"), "Null observation unexpectedly mutated or resolved an empty asset path")
    return "decoded_failure_observed_on_empty_asset_path:" + value.get("error_code", "")


def verify(report, *, phase=None, parent=None, parent_bytes=None):
    errors = []
    try:
        actual_phase = report.get("phase")
        require(report.get("schema") == "cubeless_material_add_set_edges_v1" and actual_phase in {"author", "reload"} and (phase is None or phase == actual_phase), "Wrong edge report schema/phase")
        require(report.get("status") == "pass" and report.get("outcome_unknown") is False and report.get("automatic_retries") == 0, "Failed/uncertain/retried phase")
        require(Path(report["project_file"]).resolve() == ALLOWED_PROJECT.resolve(), "Wrong isolated project")
        run = report.get("run_id", "")
        require(re.fullmatch("[0-9a-f]{32}", run) is not None, "Invalid run ID")
        package = "/Game/CubelessMCPTests/MaterialEdges_" + run + "/M_Edges"
        require(report.get("material_package") == package and report.get("material_object") == package + ".M_Edges", "Material ownership differs")
        expected_fingerprint = hashlib.sha256(json.dumps({k: report[k] for k in ("phase", "run_id", "project_file", "endpoint", "material_package")}, sort_keys=True).encode()).hexdigest()
        require(report.get("input_fingerprint") == expected_fingerprint, "Input fingerprint differs")
        rows = report.get("stages", [])
        require([r.get("name") for r in rows] == (AUTHOR_STAGES if actual_phase == "author" else RELOAD_STAGES) and all(r.get("passed") is True for r in rows), "Stage coverage/order/status differs")
        results = {r["name"]: r["result"] for r in rows}; byname = {r["name"]: r for r in rows}
        editor = report.get("editor", {})
        require(results["preflight"] == editor and editor.get("success") is True and editor.get("game_thread") is True and editor.get("pie_or_sie_active") is False and not editor.get("dirty_packages"), "Bad preflight")
        require(str(editor.get("engine_version", "")).startswith("5.8.3") and Path(editor["project_file"]).resolve() == ALLOWED_PROJECT.resolve() and bool(editor.get("instance_id")), "Wrong editor identity")
        require(report.get("compile") == {"explicit_calls": 0, "shader_completion_verified": False}, "Unexpected compile/shader proof")
        require(report.get("atomic_rollback") is False and report.get("full_material_parity") is False and report.get("packaging") == "not_run", "Overstated scope")
        nodes = report["nodes"]
        require(set(nodes) == {"default", "position"} and nodes["default"] != nodes["position"] and all(isinstance(v, str) and v.startswith(report["material_object"] + ":") for v in nodes.values()), "Node ownership differs")
        calls = report.get("tool_calls", [])
        covered = [i for r in rows for i in r.get("call_indices", [])]
        require(covered == list(range(len(calls))), "Unbound, repeated or reordered calls")
        for item in calls:
            if not item.get("raw", {}).get("isError"):
                require(decoded_raw(item["raw"]) == item.get("decoded"), "Raw/decoded call evidence differs")
        for row in rows:
            if row.get("call_indices"):
                require(len(row["call_indices"]) == 1, "Expected one call per stage")
                item = calls[row["call_indices"][0]]
                expected = {"raw": item["raw"], "decoded": item.get("decoded")} if row["name"].startswith(("position_", "value_")) else item.get("decoded")
                require(row["result"] == expected, "Stage/raw call binding differs: " + row["name"])
        def call(stage, tool):
            indices = byname[stage].get("call_indices", [])
            require(len(indices) == 1 and type(indices[0]) is int and 0 <= indices[0] < len(calls), "Stage call binding missing: " + stage)
            item = calls[indices[0]]; require(item.get("tool") == tool, "Wrong tool call: " + stage)
            if stage.startswith(("position_", "value_")):
                require(results[stage] == {"raw": item["raw"], "decoded": item.get("decoded")}, "Probe evidence differs from actual call")
            else:
                require(results[stage] == item.get("decoded"), "Stage differs from actual call")
            return item
        for name in (("read_default", "saved_default") if actual_phase == "author" else ("reload_default",)):
            readback(results[name], 0.125, INITIAL_DESC, [0, 0])
        for name in (("read_number",) if actual_phase == "author" else ()):
            readback(results[name], 0.875, INITIAL_DESC, [-700, 550])
        for name in (("read_quoted", "saved_position") if actual_phase == "author" else ("reload_position",)):
            readback(results[name], 0.875, FINAL_DESC, [-700, 550])
        expressions = results["official_expression_array"]
        require(isinstance(expressions, list) and {v.get("refPath") if isinstance(v, dict) else v for v in expressions} == set(nodes.values()) and len(expressions) == 2, "Official array-return expression read differs")
        call("official_expression_array", "get_expressions")
        for row in rows:
            if row["name"].startswith(("read_", "saved_default", "saved_position", "reload_default", "reload_position")):
                item = call(row["name"], "get_properties")
                require(item["arguments"].get("properties") == PROPERTIES, "Official string-array request altered")
                expected_node = nodes["default"] if row["name"] in {"read_default", "saved_default", "reload_default"} else nodes["position"]
                require(item["arguments"].get("instance") == {"refPath": expected_node}, "Readback selected wrong node")
        require(results["final_editor"].get("success") is True and results["final_editor"].get("instance_id") == editor["instance_id"] and not results["final_editor"].get("dirty_packages"), "Final editor is dirty or differs")
        if actual_phase == "author":
            require(results["guard_absent"] is False and results["create_material"] == {"refPath": report["material_object"]}, "Creation guard/reference differs")
            require(not results["post_position_editor"].get("dirty_packages"), "Negative array probes left dirty packages")
            for key, position in POSITION_CASES.items():
                stage = "position_" + key; item = call(stage, "add_material_node")
                require(item["arguments"].get("material_path") == "" and item["arguments"].get("node_position") == position, "Negative array request differs")
                require(results[stage]["raw"] == item["raw"], "Raw negative evidence differs")
                rejected(results[stage], "position")
            null_call = call("position_null_observation", "add_material_node")
            require(null_call["arguments"].get("material_path") == "" and "node_position" in null_call["arguments"] and null_call["arguments"]["node_position"] is None, "Null observation input differs")
            require(report.get("null_position_observation") == null_observation(results["position_null_observation"]) and report.get("null_position_must_reject") is False, "Null was incorrectly treated as mandatory rejection")
            for name in ("add_default", "add_position", "set_number", "set_quoted"):
                mutation(results[name])
            default = call("add_default", "add_material_node")["arguments"]
            positioned = call("add_position", "add_material_node")["arguments"]
            require("node_position" not in default and positioned.get("node_position") == [-700, 550], "Omission/default or explicit position not exercised")
            for name, item in (("default", default), ("position", positioned)):
                require(item.get("material_path") == package and item.get("expression_class") == "Constant" and json.loads(item["properties_json"]) == {"R": 0.125, "Desc": INITIAL_DESC}, "Add input properties differ")
                require(results["add_" + name]["data"].get("object_path") == nodes[name], "Created node identity differs")
            readback(results["read_position"], 0.125, INITIAL_DESC, [-700, 550])
            for stage, expected in (("set_number", "0.875"), ("set_quoted", json.dumps(FINAL_DESC))):
                params = call(stage, "set_material_node_property")["arguments"]
                require(params.get("value_json") == expected and params.get("property_name") == ("R" if stage == "set_number" else "Desc") and params.get("node_id") == report["selectors"]["position"] and params.get("material_path") == package, "Primitive/string payload or target differs")
            for key, text in VALUE_CASES.items():
                item = call("value_" + key, "set_material_node_property")
                require(item["arguments"].get("value_json") == text and item["arguments"].get("property_name") == "R", "Negative scalar input differs")
                require(results["value_" + key]["raw"] == item["raw"], "Raw scalar evidence differs")
                expected = rejected(results["value_" + key], "parse" if key in {"trailing", "multiple"} else "property")
                require(results["reject_" + key] == expected, "Failure category differs")
                readback(results["read_after_" + key], 0.875, FINAL_DESC, [-700, 550])
            require(report.get("save") == {"explicit_calls": 1, "saved": True} and results["save_once"] is True and results["saved_clean"] is False, "Save count/status/clean differs")
            require(call("save_once", "save_assets")["arguments"].get("asset_paths") == [package], "Save target differs")
        else:
            require(parent is not None and parent_bytes is not None and verify(parent, phase="author")["passed"], "Passing actual author bytes required")
            require(report.get("author_report_sha256") == hashlib.sha256(parent_bytes).hexdigest() and report.get("author_editor") == parent["editor"], "Parent binding differs")
            require(editor["instance_id"] != parent["editor"]["instance_id"] and report["run_id"] == parent["run_id"] and nodes == parent["nodes"], "Reload reuses process or wrong fixture")
            require(report.get("save") == {"explicit_calls": 0, "saved": False} and results["reload_clean"] is False and all(c.get("mutation") is False for c in calls), "Reload mutated or saved")
    except (AssertionError, ValueError, TypeError, KeyError, IndexError) as error:
        errors.append(str(error))
    return {"schema": "cubeless_material_add_set_edges_verification_v1", "passed": not errors, "errors": errors, "phase": report.get("phase"), "full_material_parity": False, "scope": "Input conversion, strict rejection, independent value/position readback, save and optional new-process reload; no shader/render proof"}


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument("report", type=Path); p.add_argument("--phase", choices=["author", "reload"]); p.add_argument("--parent-report", type=Path); p.add_argument("--output", type=Path)
    a = p.parse_args(); raw = a.parent_report.read_bytes() if a.parent_report else None
    result = verify(json.loads(a.report.read_text(encoding="utf-8-sig")), phase=a.phase, parent=json.loads(raw) if raw else None, parent_bytes=raw)
    if a.output:
        with a.output.open("x", encoding="utf-8") as f: json.dump(result, f, indent=2)
    print(json.dumps(result)); return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
