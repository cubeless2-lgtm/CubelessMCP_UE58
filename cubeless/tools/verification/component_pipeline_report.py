"""Independently verify bounded component reads from exact raw official MCP evidence."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verification.material_add_set_edges_report import decoded_raw, field

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"
ROLES = ("local", "empty", "inherited")
FIXED_VALUES = {
    ("MeshChild", "RelativeLocation"): [12, 34, 56],
    ("MeshChild", "RelativeRotation"): [10, 20, 30],
    ("MeshChild", "RelativeScale3D"): [1.25, .75, 2],
    ("MeshChild", "StaticMesh"): "/Engine/BasicShapes/Cube.Cube",
    ("MeshChild", "bVisible"): False,
    ("MeshChild", "ComponentTags"): ["CubelessComponentFixture", "ReadOnlyProof"],
    ("MeshChild", "Mobility"): 2,
    ("LightChild", "Intensity"): 1234.5,
    ("LightChild", "IntensityUnits"): "Lumens",
    ("LightChild", "AttachSocketName"): "None",
}
ROW_FIELDS = ("component_name", "variable_name", "component_class", "component_class_path", "template_name", "template_path", "parent_component_name")


def require(value, message):
    if not value: raise AssertionError(message)


def sha(value): return hashlib.sha256(value).hexdigest()


def equivalent(actual, expected):
    if type(expected) in (float, int):
        return type(actual) in (float, int) and math.isfinite(actual) and abs(actual - expected) < 0.0001
    if type(actual) is not type(expected): return False
    if type(expected) is list: return len(actual) == len(expected) and all(equivalent(a, b) for a, b in zip(actual, expected))
    if type(expected) is dict: return actual.keys() == expected.keys() and all(equivalent(actual[k], expected[k]) for k in expected)
    return actual == expected


def fixture_contract(fixture):
    require(fixture.get("schema") == "cubeless_component_read_fixture_v1" and fixture.get("success") is True and fixture.get("stage") == "complete", "Fixture creation did not pass")
    run = fixture["run_id"]; require(re.fullmatch(r"[a-f0-9]{32}", run), "Invalid fixture run")
    root = "/Game/CubelessMCPTests/Components_" + run
    require(fixture["root"] == root and Path(fixture["project_file"]).resolve() == PROJECT.resolve(), "Fixture ownership differs")
    require(str(fixture["engine_version"]).startswith("5.8.3-") and fixture["dirty_before"] == fixture["dirty_after"] == [], "Wrong version or dirty fixture")
    require(fixture["compile"].get("success") is True and fixture["save"].get("saved") is True, "Fixture compile/save is incomplete")
    require(fixture.get("automatic_retries") == 0 and fixture.get("atomic_rollback") is False and fixture.get("outcome_unknown") is False, "Fixture retry/rollback/uncertainty scope differs")
    require(fixture.get("status") == "pass" and fixture["dirty_final"] == [] and fixture["preservation"].get("passed") is True and fixture["preservation"].get("changed_or_missing") == [], "Fixture final preservation/dirty observation failed")
    require(fixture["save_on_compile"].get("value") == 0 and fixture["save_on_compile"].get("changed_by_fixture") is False and fixture["compile"].get("implicit_engine_compilation_possible") is True, "Factory implicit compilation/save policy scope missing")
    require(fixture["compile"]["explicit_calls"] == 3 and len(fixture["compile"]["results"]) == 3 and all(r.get("success") is True and r.get("compiled") is True and r.get("saved") is False for r in fixture["compile"]["results"]), "Three explicit fixture compiles were not verified without save")
    require(fixture["save"].get("explicit_calls") == 1, "Fixture did not use exactly one explicit save batch")
    bps = fixture["blueprints"]; require(set(bps) == set(ROLES), "Fixture role set differs")
    expected_files = {}
    for role, name in zip(ROLES, ("BP_Local", "BP_Empty", "BP_Inherited")):
        row = bps[role]; package = root + "/" + name
        require(row["package"] == package and row["object_path"] == package + "." + name, "Blueprint path escaped fixture")
        require(type(row["has_scs"]) is bool, "Missing SCS presence expectation")
        expected_files[package] = PROJECT.parent / "Content" / (package[6:] + ".uasset")
    require(bps["local"]["has_scs"] is True and bps["empty"]["has_scs"] is False, "Local/empty SCS scope differs")
    require(bps["empty"]["expected_components"] == bps["inherited"]["expected_components"] == [] and bps["empty"]["properties"] == bps["inherited"]["properties"] == [], "Empty/inherited role must not pretend effective components are local")
    require(bps["inherited"]["parent_blueprint"] == bps["local"]["object_path"] and bps["inherited"]["inherited_components_excluded"] is True, "Inheritance boundary evidence missing")
    rows = bps["local"]["expected_components"]
    expected = {"DefaultSceneRoot": ("SceneComponent", ""), "MeshChild": ("StaticMeshComponent", "DefaultSceneRoot"), "LightChild": ("PointLightComponent", "MeshChild")}
    require(len(rows) == 3 and {r["component_name"] for r in rows} == set(expected), "Need exactly the three declared local nodes")
    for row in rows:
        kind, parent = expected[row["component_name"]]
        require(all(k in row for k in ROW_FIELDS) and row["variable_name"] == row["component_name"] and row["component_class"] == kind and row["component_class_path"] == "/Script/Engine." + kind and row["parent_component_name"] == parent, "Component independent class/hierarchy differs")
        require(row["template_path"].startswith(bps["local"]["package"] + ".") and ":" in row["template_path"], "Template path escaped local Blueprint package")
    mesh = next(r for r in rows if r["component_name"] == "MeshChild")
    require(equivalent(mesh["relative_transform"], {"location": [12,34,56], "rotation": [10,20,30], "scale": [1.25,.75,2]}) and mesh["static_mesh"] == "/Engine/BasicShapes/Cube.Cube", "Declared mesh transform/asset differs")
    props = bps["local"]["properties"]
    require(len(props) == 10 and {(p["component_name"], p["property_name"]) for p in props} == set(FIXED_VALUES) and len({p["id"] for p in props}) == len(props), "Expected ten independent property cases")
    for prop in props:
        key = (prop["component_name"], prop["property_name"])
        require(equivalent(prop["declared_expected_value"], FIXED_VALUES[key]) and equivalent(prop["independent_readback_value"], FIXED_VALUES[key]), "Independent public getter differs from declared fixture values")
        encoding = "legacy_ue_export_text_bounded_scalar_array" if key[1] == "ComponentTags" else "legacy_typed_json"
        require(prop["property_encoding"] == encoding, "Wrong legacy property encoding contract")
        if key[1] == "AttachSocketName":
            require(prop["official_property_baseline"] is None and prop["official_property_status"] == "not_accessible_by_generic_tool", "FName independent getter exception must remain explicit")
        else:
            require(type(prop["official_property_baseline"]) is dict and len(prop["official_property_baseline"]) == 1, "Official property baseline is missing")
            official = field(prop["official_property_baseline"], key[1])
            require(official is not None, "Official property value is null")
            if key[1] in {"bVisible", "ComponentTags", "Intensity", "IntensityUnits"}:
                require(equivalent(official, FIXED_VALUES[key]), "Official scalar/array baseline contradicts direct getter")
            elif key[1] == "Mobility": require(official == "Movable", "Official byte enum does not match declared Movable value")
            elif key[1] == "StaticMesh": require(official == {"refPath": FIXED_VALUES[key]}, "Official object reference differs from direct getter")
    files = fixture["asset_files"]
    require(len(files) == 3 and {r["package"] for r in files} == set(expected_files), "Exact three saved assets required")
    for row in files:
        require(Path(row["path"]).resolve() == expected_files[row["package"]].resolve() and re.fullmatch(r"[a-f0-9]{64}", row["sha256"]), "Wrong asset file path/hash")
    return bps, files


def plan(fixture):
    bps, _ = fixture_contract(fixture); local = bps["local"]["package"]; rows = []
    def add(name, tool, arguments, check, **expected):
        rows.append({"name": name, "toolset_suffix": "CubelessComponentTools", "tool": tool, "arguments": arguments, "check": check, "expected": expected})
    def listing(name, role="local", component=None, check="list", **kwargs):
        args = {"blueprint_name": bps[role]["package"]}
        if component is not None: args["component_name"] = component
        add(name, "list_blueprint_components", args, check, role=role, selected=component or "", **kwargs)
    rows.append({"name": "preflight", "toolset_suffix": "CubelessNativeToolset", "tool": "GetEditorDiagnostics", "arguments": {}, "check": "editor", "expected": {}})
    listing("local_first"); listing("local_repeat", already_loaded=True)
    listing("local_exact_filter", component="MeshChild"); listing("local_missing_filter", component="NoSuchComponent")
    listing("local_empty_filter", component="")
    add("local_null_filter", "list_blueprint_components", {"blueprint_name": local, "component_name": None}, "list", role="local", selected="")
    listing("empty_scs", role="empty"); listing("inherited_scope", role="inherited")
    for index, prop in enumerate(bps["local"]["properties"]):
        args = {"blueprint_name": local, "component_name": prop["component_name"], "property_name": prop["property_name"]}
        add("property_" + str(index), "get_component_property", args, "property", property_index=index)
        if prop["official_property_baseline"] is not None:
            template = next(r["template_path"] for r in bps["local"]["expected_components"] if r["component_name"] == prop["component_name"])
            rows.append({"name": "official_" + str(index), "toolset_suffix": "ObjectTools", "tool": "get_properties", "arguments": {"instance": {"refPath": template}, "properties": [prop["property_name"]]}, "check": "official_property", "expected": {"property_index": index}})
    base = {"blueprint_name": local, "component_name": "MeshChild", "property_name": "bVisible"}
    negatives = [
        ("missing_component", "get_component_property", {**base, "component_name": "MissingComponent"}, "component_not_found"),
        ("missing_property", "get_component_property", {**base, "property_name": "CubelessNoSuchProperty"}, "component_property_not_found"),
        ("unsupported_struct", "get_component_property", {**base, "property_name": "BodyInstance"}, "component_property_type_unsupported"),
        ("node_budget", "list_blueprint_components", {"blueprint_name": local, "max_components": 2}, "component_budget_exceeded"),
        ("text_budget", "get_component_property", {**base, "property_name": "ComponentTags", "max_value_chars": 64}, "component_property_type_unsupported"),
        ("long_selector", "get_component_property", {**base, "property_name": "X" * 1024}, "component_selector_invalid"),
        ("empty_property", "get_component_property", {**base, "property_name": ""}, "component_selector_invalid"),
        ("empty_scs_property", "get_component_property", {**base, "blueprint_name": bps["empty"]["package"]}, "component_not_found"),
        ("inherited_not_local", "get_component_property", {**base, "blueprint_name": bps["inherited"]["package"]}, "component_not_found"),
    ]
    for name, tool, args, code in negatives:
        add("negative_" + name, tool, args, "reject", code=code)
        listing("after_" + name)
    listing("final_local")
    for role in ROLES:
        rows.append({"name": "clean_" + role, "toolset_suffix": "AssetTools", "tool": "is_dirty", "arguments": {"asset_path": bps[role]["package"]}, "check": "clean", "expected": {}})
    rows.append({"name": "final_editor", "toolset_suffix": "CubelessNativeToolset", "tool": "GetEditorDiagnostics", "arguments": {}, "check": "editor", "expected": {}})
    return rows


def normalized_property(value, prop):
    if prop["property_name"] != "ComponentTags": return value
    require(type(value) is str and value.startswith("(") and value.endswith(")"), "Legacy scalar array must remain UE text")
    # Deliberately only parse this fixture's two simple names, never generic UE
    # ExportText. Raw text is retained and compared across cold processes too.
    tokens = value[1:-1].split(",")
    require(len(tokens) == 2 and all(re.fullmatch(r'(?:[A-Za-z]+|"[A-Za-z]+")', t.strip()) for t in tokens), "Unexpected scalar-array text syntax")
    return [t.strip().strip('"') for t in tokens]


def read_observation(value, *, success=True):
    require(value.get("success") is success and value.get("changes") == [], "Read failed or reported scoped changes")
    require(value["compile"].get("requested") is False and value["compile"].get("attempted") is False and value["save"].get("requested") is False and value["save"].get("attempted") is False and value["save"].get("saved") is False and value["retry"].get("automatic_retry") is False, "Read requested compile/save/retry")
    data = value["data"]; require(data.get("journaled") is False and data.get("loading_side_effects_verified") is False and data.get("full_legacy_component_parity") is False, "Read scope overclaim")
    native = data.get("native_outcome")
    if native is not None:
        require(all(native.get(k) is False for k in ("mutation_attempted", "compile_requested", "save_requested", "saved", "changed")) and native.get("change_state_known") is True, "Native read changes/unknown outcome")
        require(type(native.get("loaded_during_read")) is bool and native.get("implicit_postload_work_possible") is True and bool(native.get("verification_scope")), "Missing loading/observation caveat")
        if "dirty_before" in native:
            require(native["dirty_before"] is False and native.get("dirty_after") is False and native.get("blueprint_status_before") == native.get("blueprint_status_after"), "Dirty/status changed or fixture was dirty")
        if success: require(native.get("postcondition_passed") is True and value["validation"].get("independent_value_check") == "not_run", "Native read claims independent value verification or lacks postcondition")
    if success: require(native is not None, "Missing native read evidence")
    return data, native


def list_value(value, expected, *, selected="", first_guids=None):
    data, native = read_observation(value)
    require(data["blueprint_path"] == expected["object_path"] and data["has_simple_construction_script"] is expected["has_scs"], "Blueprint/SCS identity differs")
    require(data.get("inherited_effective_defaults_verified") is False and data.get("native_cdo_components_included") is False and data.get("orphan_allnodes_entries_inspected") is False, "Enumeration scope overclaim")
    target = [r for r in expected["expected_components"] if not selected or r["component_name"] == selected]
    rows = data["components"]; require(len(rows) == data["component_count"] == len(target), "Wrong filtered component count")
    require([r["variable_name"] for r in rows] == sorted(r["variable_name"] for r in rows), "Rows are not sorted by local name")
    indexed = {r["component_name"]: r for r in rows}; require(len(indexed) == len(rows), "Duplicate local component")
    require(len({r["node_guid"] for r in rows}) == len(rows), "Duplicate local node GUID")
    for expected_row in target:
        actual = indexed.get(expected_row["component_name"]); require(actual is not None, "Component absent")
        for key, item in expected_row.items():
            if key != "node_guid": require(key in actual and equivalent(actual[key], item), "Independent component readback differs: " + key)
        require(re.fullmatch(r"(?:[A-Fa-f0-9]{32}|[A-Fa-f0-9]{8}(?:-[A-Fa-f0-9]{4}){3}-[A-Fa-f0-9]{12})", actual["node_guid"]) and set(actual["node_guid"].replace("-", "")) != {"0"}, "Missing/invalid node GUID")
        if first_guids is not None: require(first_guids.get(actual["component_name"]) == actual["node_guid"], "Node GUID changed within/cross process")
    require(data["local_reachable_node_count"] == len(expected["expected_components"]) and native["performance"]["local_scs_nodes_visited"] == len(expected["expected_components"]), "Traversal budget/count scope differs")
    return {name: row["node_guid"] for name, row in indexed.items()}


def verify(report, *, fixture=None, fixture_bytes=None, parent=None, parent_bytes=None, check_files=True):
    try:
        require(fixture is not None and fixture_bytes is not None and json.loads(fixture_bytes) == fixture, "Exact fixture raw bytes required")
        bps, files = fixture_contract(fixture)
        require(report.get("schema") == "cubeless_component_read_pipeline_v1" and report.get("status") == "pass" and report["phase"] in {"read", "reload"}, "Incomplete/wrong component pipeline")
        require(report["fixture_sha256"] == sha(fixture_bytes) and report["run_id"] == fixture["run_id"] and Path(report["project_file"]).resolve() == PROJECT.resolve(), "Pipeline ownership differs")
        require(report.get("automatic_retries") == 0 and report.get("outcome_unknown") is False and report.get("explicit_compile_calls") == report.get("explicit_save_calls") == 0 and report.get("packaging") == "not_run" and report.get("full_legacy_component_parity") is False and report.get("guid_independently_verified") is False, "Pipeline scope overclaim")
        if report["phase"] == "reload":
            require(parent is not None and parent_bytes is not None and json.loads(parent_bytes) == parent and parent.get("phase") == "read" and verify(parent, fixture=fixture, fixture_bytes=fixture_bytes, check_files=check_files)["passed"], "Invalid first-read parent")
            require(report["parent_sha256"] == sha(parent_bytes), "Parent bytes differ")
        expected = plan(fixture); stages = report["stages"]; calls = report["tool_calls"]
        require([s["name"] for s in stages] == [s["name"] for s in expected] and len(calls) == len(stages) and [s["call_index"] for s in stages] == list(range(len(calls))) and all(s.get("passed") is True for s in stages), "Missing/reordered/failed component stage")
        first_guids = parent["node_guids"] if parent else None; editor_id = None; property_values = {}
        for spec, stage, call in zip(expected, stages, calls):
            require(call["toolset"].endswith("." + spec["toolset_suffix"]) and call["tool"] == spec["tool"] and call["arguments"] == spec["arguments"] and call["mutation"] is False, "Wrong named call/provenance/arguments")
            value = decoded_raw(call["raw"]); require(value == call["decoded"] == stage["result"], "Raw/cached/stage mismatch")
            kind = spec["check"]; e = spec["expected"]
            if kind == "editor":
                require(value.get("success") is True and Path(value["project_file"]).resolve() == PROJECT.resolve() and value["engine_version"].startswith("5.8.3-") and value["game_thread"] is True and value["pie_or_sie_active"] is False and value["dirty_packages"] == [], "Wrong/nonidle/dirty editor")
                if editor_id is None: editor_id = value["instance_id"]
                require(editor_id and editor_id == value["instance_id"], "Editor changed mid-pipeline")
                if parent: require(editor_id != parent["editor"]["instance_id"], "Cold proof reused original process")
            elif kind == "clean": require(value is False, "Fixture Blueprint dirty")
            elif kind == "list":
                guids = list_value(value, bps[e["role"]], selected=e["selected"], first_guids=first_guids if e["role"] == "local" else None)
                if spec["name"] == "local_first": first_guids = guids
                if e.get("already_loaded"): require(value["data"]["native_outcome"]["loaded_during_read"] is False, "Immediate second read loaded Blueprint again")
            elif kind == "property":
                data, native = read_observation(value); prop = bps["local"]["properties"][e["property_index"]]
                require(data["blueprint_path"] == bps["local"]["object_path"] and data["component_name"] == prop["component_name"] and data["property_name"] == prop["property_name"] and data["property_encoding"] == prop["property_encoding"] and bool(data["property_type"]), "Wrong property identity/encoding")
                if prop.get("property_type") is not None: require(data["property_type"] == prop["property_type"], "C++ property type differs")
                require(equivalent(normalized_property(data["property_value"], prop), prop["independent_readback_value"]), "Independent property value differs")
                property_values[prop["id"]] = data["property_value"]
            elif kind == "official_property":
                prop = bps["local"]["properties"][e["property_index"]]
                require(type(value) is dict and len(value) == 1 and equivalent(field(value, prop["property_name"]), field(prop["official_property_baseline"], prop["property_name"])), "Independent official property read differs")
            elif kind == "reject":
                read_observation(value, success=False)
                require(value.get("error_code") == e["code"] and bool(value.get("error")), "Wrong rejection category")
        require(report["node_guids"] == first_guids and report["property_values"] == property_values and report["editor"]["instance_id"] == editor_id, "Summary differs from raw observations")
        if parent: require(report["property_values"] == parent["property_values"] and report["node_guids"] == parent["node_guids"], "Cold values/GUIDs differ from saved first read")
        require(report["asset_files_before"] == report["asset_files_after"] == files, "Read changed or omitted saved asset bytes")
        if "asset_files_final_observation" in report: require(report["asset_files_final_observation"] == files, "Final disk observation differs")
        if check_files:
            for row in files: require(Path(row["path"]).is_file() and sha(Path(row["path"]).read_bytes()) == row["sha256"], "Saved asset bytes changed")
        return {"schema": "cubeless_component_read_independent_v1", "passed": True, "errors": [], "stages": len(stages), "scope": "Three exact local/empty/inherited Blueprint fixtures and ten property cases; GUID stability only", "guid_independently_verified": False, "full_legacy_component_parity": False, "packaging": "not_run"}
    except (AssertionError, TypeError, ValueError, KeyError, OSError, IndexError) as error:
        return {"schema": "cubeless_component_read_independent_v1", "passed": False, "errors": [str(error)], "full_legacy_component_parity": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("report", type=Path)
    parser.add_argument("--fixture-report", type=Path, required=True); parser.add_argument("--parent-report", type=Path); parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(); require(not args.output.exists(), "Refusing evidence overwrite")
    fixture_bytes = args.fixture_report.read_bytes(); parent_bytes = args.parent_report.read_bytes() if args.parent_report else None
    result = verify(json.loads(args.report.read_bytes()), fixture=json.loads(fixture_bytes), fixture_bytes=fixture_bytes, parent=json.loads(parent_bytes) if parent_bytes else None, parent_bytes=parent_bytes)
    with args.output.open("x", encoding="utf-8") as stream: json.dump(result, stream, ensure_ascii=False, indent=2)
    print(json.dumps(result)); return 0 if result["passed"] else 1


if __name__ == "__main__": raise SystemExit(main())
