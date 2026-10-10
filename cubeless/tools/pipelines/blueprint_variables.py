"""Extract UE56 physics-variable state into an isolated official-MCP workflow.

This migrates variable authoring and readback, not the source's component,
physics, event/function graph or spawn stages. No editor launch or retry.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from pipelines.component_authoring import PROJECT, RECORDS, SETTINGS, fingerprint, require, success, field

TOOLSETS = {
    "native": "CubelessMCPUE58Editor.CubelessNativeToolset",
    "bp": "cubeless_mcp_ue58.registration.toolsets.CubelessBlueprintTools",
    "batch": "cubeless_mcp_ue58.registration.batch_toolset.CubelessBatchTools",
    "assets": "editor_toolset.toolsets.asset.AssetTools",
    "objects": "editor_toolset.toolsets.object.ObjectTools",
    "official_bp": "editor_toolset.toolsets.blueprint.BlueprintTools",
}
INITIAL = {"Mass": 10.0, "RotationSpeed": 100.0, "BounceFactor": 0.8, "IsTrigger": False}
FINAL = {**INITIAL, "Mass": 25.0}
METADATA = {"Units": "kg", "ClampMin": "0.0", "ClampMax": "1000.0"}
TOOLTIP = "Mass control from UE56"


def typed_equal(actual, expected):
    if type(expected) is bool:
        return actual is expected
    return type(actual) in (int, float) and math.isfinite(actual) and math.isclose(actual, expected, rel_tol=1e-6, abs_tol=1e-6)


def stored_equal(actual, expected):
    if not isinstance(actual, str):
        return False
    if type(expected) is bool:
        return actual.casefold() == ("true" if expected else "false")
    try:
        return typed_equal(float(actual), expected)
    except ValueError:
        return False


def variable_success(value, *, unchanged=False):
    success(value)
    native = value["data"]["native_outcome"]
    require(native.get("change_state_known") is True and native.get("postcondition_passed") is True, "Variable state/postcondition unverified")
    require(value["compile"].get("requested") is False and value["compile"].get("attempted") is False
            and value["save"].get("requested") is False and value["save"].get("attempted") is False
            and value["save"].get("saved") is False, "Unexpected explicit compile/save")
    if unchanged:
        require(native.get("changed") is False and native.get("mutation_attempted") is False and value.get("changes") == [], "Expected no asset change")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", required=True); p.add_argument("--expected-project", required=True, type=Path)
    p.add_argument("--expected-pid", required=True, type=int); p.add_argument("--output", required=True, type=Path)
    a = p.parse_args(argv); out = a.output.resolve()
    require(a.expected_project.resolve() == PROJECT.resolve(), "Exact isolated project required")
    require(out.is_relative_to(RECORDS.resolve()) and not out.exists(), "Use a fresh verification report")
    run = uuid.uuid4().hex; variable_guid = uuid.uuid4().hex
    folder = "/Game/CubelessMCPTests/Variables_" + run; package = folder + "/BP_VariableState"
    asset_ref = package + ".BP_VariableState"
    disk = PROJECT.parent / "Content" / (package[6:] + ".uasset")
    require(not disk.parent.exists(), "Owned UUID folder already exists")
    config = PROJECT.parent / "Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini"
    report = {"schema": "cubeless_blueprint_variables_v1", "status": "running", "run_id": run,
        "package": package, "project_file": str(PROJECT.resolve()), "variable_guid": variable_guid,
        "initial_values": INITIAL, "expected_values": FINAL, "metadata_requested": METADATA,
        "stages": [], "changes": [], "started_ns": time.time_ns(), "automatic_retries": 0,
        "outcome_unknown": False, "atomic_rollback": False, "cold_reload": "not_run", "packaging": "not_run",
        "full_legacy_pipeline_parity": False, "full_legacy_variable_parity": False,
        "legacy": {"id": "script:Python/scripts/node/test_physics_variables.py",
            "sha256": "d67c35ca2eb6f6a6652091f07614a05bcf65abc293caae1e04831fe52605d9da",
            "scope": "four member-variable names/types/defaults only; metadata, hidden GUID/editable and default update are explicit extensions"},
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
                if any(x.get("change_state_known") is False for x in value["changes"]): report["outcome_unknown"] = True
                if value.get("data", {}).get("batch", {}).get("mutation_state_unknown"): report["outcome_unknown"] = True
            if group in {"native", "bp", "batch"} and expect_success: success(value)
            return value
        except Exception as error:
            row["error"] = str(error)
            if row["state"] == "submitted" and mutation: report["outcome_unknown"] = True
            raise
        finally:
            row["elapsed_seconds"] = (time.time_ns() - row["started_ns"]) / 1e9; persist()

    def inspect(label):
        value = call(label, "bp", "inspect_blueprint_variables", {"blueprint_name": package})
        variable_success(value, unchanged=True); return value["data"]

    try:
        persist(); c.initialize()
        schemas.update({k: tool_payload(c.call("describe_toolset", {"toolset_name": v})) for k, v in TOOLSETS.items()})
        report["schemas"] = schemas
        required = {"add_blueprint_variable", "inspect_blueprint_variables", "inspect_blueprint_variable_metadata", "set_blueprint_variable_default"}
        require(required.issubset({t["name"].rsplit(".", 1)[-1] for t in schemas["bp"]["tools"]}), "S variable tools not registered")
        editor = call("preflight", "native", "GetEditorDiagnostics")
        require(Path(editor["project_file"]).resolve() == PROJECT.resolve() and editor["process_id"] == a.expected_pid
                and editor["game_thread"] is True and editor["engine_version"].startswith("5.8.3-")
                and not editor["pie_or_sie_active"] and editor["dirty_packages"] == [], "Wrong/dirty editor")
        report["editor"] = editor
        policy = call("save_policy", "objects", "get_properties", {"instance": {"refPath": SETTINGS}, "properties": ["SaveOnCompile"]})
        report["save_policy_before"] = policy.get("SaveOnCompile")
        require(report["save_policy_before"] == "SoC_Never", "Requires Never; settings are not changed")
        require(call("absent", "assets", "exists", {"path": package}) is False, "Asset exists")
        created = call("create", "bp", "create_blueprint", {"name": "BP_VariableState", "parent_class": "Actor",
            "package_path": folder, "operation_id": run + "_create"}, mutation=True)
        require(created["save"]["saved"] is False and not disk.exists(), "Creation saved unexpectedly")
        report["implicit_factory_compile"] = created["compile"].get("implicit_factory_compile")
        commands = []
        for name, value in INITIAL.items():
            params = {"blueprint_name": package, "variable_name": name,
                "variable_type": "Boolean" if type(value) is bool else "Float",
                "default_value_json": json.dumps(value), "is_exposed": name != "Mass"}
            if name == "Mass":
                params.update(is_editable=True, variable_guid=variable_guid, category="Physics State",
                              tooltip=TOOLTIP, friendly_name="Mass Control", metadata_json=json.dumps(METADATA))
            commands.append({"type": "CubelessBlueprintTools.add_blueprint_variable", "params": params})
        commands += [{"type": "CubelessBlueprintTools.set_blueprint_variable_default", "params": {"blueprint_name": package,
            "variable_name": "Mass", "default_value_json": "25", "compile": False, "save": False}},
            {"type": "CubelessBlueprintTools.compile_and_save_blueprint", "params": {"blueprint_name": package, "save": True}}]
        batch = call("variables_batch", "batch", "execute_batch", {"commands_json": json.dumps(commands), "operation_id": run + "_author"}, mutation=True)
        summary = batch["data"]["batch"]
        require(summary["succeeded"] == summary["executed"] == 6 and summary["failed"] == 0 and summary["not_run"] == []
                and not summary["mutation_state_unknown"], "Variable batch incomplete")
        children = [r["response"] for r in summary["results"]]; require(len(children) == 6, "Missing child result")
        for child in children[:5]: variable_success(child)
        for (name, wanted), child in zip(INITIAL.items(), children[:4]):
            require(child["data"]["variable_name"] == name and stored_equal(child["data"]["stored_default"], wanted), "Initial stored default differs")
        setter_data = children[4]["data"]
        require(stored_equal(setter_data["previous_stored_default"], 10.0)
                and stored_equal(setter_data["requested_stored_default"], 25.0)
                and stored_equal(setter_data["stored_default_after"], 25.0), "Precompile Mass default transition differs")
        compiled = children[-1]; success(compiled)
        require(compiled["compile"].get("compiled") is True and compiled["compile"].get("error_count") == 0
                and compiled["save"].get("saved") is True and compiled["save"].get("requested") is True, "Compile/save failed")
        native_compile = compiled["data"].get("compile", compiled["data"])
        require(native_compile.get("validation_pass") is True and native_compile.get("compiled") is True, "Native compilation not validated")
        require(len(batch["compile"]["entries"]) == len(batch["save"]["entries"]) == 1, "Expected one explicit compile/save")
        # The detailed native row contract is checked below; full compilation can
        # consume descriptor defaults, so generated CDO values are authoritative.
        saved = fingerprint(disk); require(saved is not None, "Variable asset was not saved")
        baseline = inspect("saved_variables")
        rows = baseline["new_variables"]
        require(baseline["new_variable_count"] == len(rows) == 4 and {r["name"] for r in rows} == set(INITIAL), "Wrong declared variable set")
        by_name = {r["name"]: r for r in rows}
        for name, row in by_name.items():
            require(row["scope"] == "new" and row["instance_editable"] is True and row["blueprint_visible"] is True, "Variable exposure differs")
            require(uuid.UUID(row["guid"]).int != 0, "Missing variable GUID")
            pin = row["pin_type"]
            require(pin["is_array"] is False and pin["category"] == ("bool" if name == "IsTrigger" else "real"), "Variable type differs")
            if name != "IsTrigger": require(pin["subcategory"] == "float", "Float width differs")
        mass = by_name["Mass"]
        require(uuid.UUID(mass["guid"]).hex == variable_guid and mass["category"] == "Physics State"
                and mass["friendly_name"] == "Mass Control", "Mass identity/metadata differs")
        metadata = call("mass_metadata", "bp", "inspect_blueprint_variable_metadata", {"blueprint_name": package, "variable_name": "Mass"})
        variable_success(metadata, unchanged=True); md = metadata["data"]
        require(md["variable_name"] == "Mass" and md["scope"] == "new" and uuid.UUID(md["guid"]).hex == variable_guid, "Metadata variable identity differs")
        entries = md["metadata_entries"]
        require(isinstance(entries, list) and all(isinstance(e, dict) and "key" in e and "value" in e for e in entries), "Ordered metadata entries missing")
        require({e["key"]: e["value"] for e in entries} == md["metadata"] and len({e["key"].casefold() for e in entries}) == len(entries), "Metadata map/order disagree")
        for name, wanted in {**METADATA, "Tooltip": TOOLTIP}.items(): require(field(md["metadata"], name) == wanted, "Metadata value differs: " + name)
        bad = call("reject_duplicate_variable", "bp", "add_blueprint_variable", {"blueprint_name": package, "variable_name": "Mass",
            "variable_type": "Float", "default_value_json": "999", "operation_id": run + "_duplicate"}, mutation=True, expect_success=False)
        require(bad.get("success") is False and bad.get("stage") == "preflight" and bad.get("error_code") == "variable_name_collision"
                and bad.get("changes") == [] and bad["data"]["native_outcome"]["mutation_attempted"] is False
                and not bad["compile"]["attempted"] and not bad["save"]["saved"], "Duplicate variable did not reject before change")
        dry = call("dry_run_mass", "bp", "set_blueprint_variable_default", {"blueprint_name": package, "variable_name": "Mass",
            "default_value_json": "99", "compile": False, "save": False, "dry_run": True, "operation_id": run + "_dry"})
        variable_success(dry, unchanged=True)
        noop = call("same_value_new_operation", "bp", "set_blueprint_variable_default", {"blueprint_name": package, "variable_name": "Mass",
            "default_value_json": "25", "compile": False, "save": False, "operation_id": run + "_same"}, mutation=True)
        variable_success(noop, unchanged=True)
        after = inspect("variables_unchanged")
        require(after["new_variables"] == rows and after["generated_variables"] == baseline["generated_variables"], "Representative probes changed variables")
        cdo = call("official_default_object", "official_bp", "get_default_object", {"blueprint": {"refPath": asset_ref}})
        require(isinstance(cdo, dict) and isinstance(cdo.get("refPath"), str) and cdo["refPath"].startswith(package + "."), "CDO escaped owned asset")
        require(cdo["refPath"] == baseline["generated_cdo_path"], "Native and official CDO references differ")
        values = call("official_cdo_values", "objects", "get_properties", {"instance": cdo, "properties": list(FINAL)})
        observed = {name: field(values, name) for name in FINAL}
        require(all(typed_equal(observed[name], wanted) for name, wanted in FINAL.items()), "Independent generated CDO value differs")
        require(call("asset_clean", "assets", "is_dirty", {"asset_path": package}) is False and fingerprint(disk) == saved, "Probes changed disk/dirty state")
        final = call("final_editor", "native", "GetEditorDiagnostics")
        require(final["instance_id"] == editor["instance_id"] and final["dirty_packages"] == [], "Editor changed/dirty")
        require(not report["outcome_unknown"], "Uncertain variable result")
        report.update(status="pass", variables=rows, metadata=md, generated_cdo=cdo, cdo_values=observed, asset_file=saved,
                      explicit_full_compile_requests=1, explicit_save_requests=1, duplicate_error_code=bad["error_code"],
                      dry_run_verified=True, new_id_same_value_unchanged=True, descriptor_default_scope="Full compilation can consume stored defaults; official CDO values checked")
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


if __name__ == "__main__": raise SystemExit(main())
