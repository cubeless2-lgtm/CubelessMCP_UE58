"""Independent offline checks for the UE58 base/mid/leaf MPC fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import re


PROJECT = Path(__file__).resolve().parents[3] / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"


def verify(report, parent=None, parent_bytes=None, check_files=True):
    errors = []
    def require(value, message):
        if not value:
            errors.append(message)
    def field(value, key):
        if not isinstance(value, dict):
            return None
        rows = [v for k, v in value.items() if k.replace("_", "").lower() == key.replace("_", "").lower()]
        return rows[0] if len(rows) == 1 else None
    def ref(value):
        return value.get("refPath") if isinstance(value, dict) else value
    def close(value, expected):
        return isinstance(value, (int, float)) and not isinstance(value, bool) and abs(value - expected) < 1e-5
    def read_result(result, asset):
        require(result.get("success") is True and result.get("stage") == "complete" and not result.get("error") and not result.get("error_code"), "Read operation failed")
        require(result.get("changes") == [] and result.get("retry", {}).get("automatic_retry") is False, "Read mutated/retried")
        require(all(result.get(section, {}).get(flag) is False for section, flags in (("compile", ("requested", "attempted")), ("save", ("requested", "attempted", "saved"))) for flag in flags), "Read compiled/saved")
        data = result["data"]
        native = data["native_outcome"]
        require(native.get("success") is True and native.get("asset_path") == asset and native.get("postcondition_passed") is True and native.get("change_state_known") is True and native.get("changed") is False and native.get("mutation_attempted") is False, "Native read identity/postcondition differs")
        require(native.get("dirty_before") is native.get("dirty_after") and type(native.get("dirty_before")) is bool, "Read changed dirty state")
        require(native.get("verification", {}).get("passed") is True and result.get("validation", {}).get("native_postcondition_passed") is True, "Read verification failed")
        require(data["before"] == data["after"], "Read snapshots differ")

    try:
        require(report.get("schema") == "cubeless_mpc_inheritance_fixture_v1", "Wrong schema")
        phase = report.get("phase")
        require(phase in {"author", "reload"}, "Wrong phase")
        require(report.get("status") == "pass", "Execution did not pass")
        require(report.get("outcome_unknown") is False and report.get("automatic_retries") == 0, "Unknown/retried outcome")
        require(report.get("atomic_rollback") is False, "Atomic rollback must not be claimed")
        run = report.get("run_id", "")
        require(re.fullmatch(r"[a-f0-9]{32}", run) is not None, "Invalid run identity")
        root = "/Game/CubelessMCPTests/MPCInheritance_" + run
        packages = {"base": root + "/MPC_Base", "mid": root + "/MPC_Mid", "leaf": root + "/MPC_Leaf", "material": root + "/M_Inheritance"}
        objects = {k: v + "." + v.rsplit("/", 1)[1] for k, v in packages.items()}
        require(report.get("packages") == packages and report.get("objects") == objects, "Fixture ownership mismatch")
        require(Path(report["project_file"]).resolve() == PROJECT.resolve(), "Wrong isolated project")
        stages = report.get("stages", [])
        expected = ["preflight"]
        if phase == "author":
            expected += ["absent_" + k for k in packages] + ["create_" + k for k in packages]
            expected += ["base_parameters", "mid_base_and_parameter", "leaf_base_and_parameter", "node_RootScalar", "node_reference_RootScalar", "node_RootVector", "node_reference_RootVector"]
        expected += ["independent_leaf_properties"]
        for key in ("base", "mid", "leaf", "leaf"):
            expected.append("effective_" + key + "_" + str(len(expected)))
        expected += ["filter_inherited_scalar", "inherited_collection_nodes", "list_material_nodes_metadata", "analyze_material_graph_metadata"]
        if phase == "author":
            expected.append("save_once")
        expected += ["clean_" + k for k in packages]
        require([s.get("name") for s in stages] == expected, "Stage coverage/order mismatch")
        require(all(s.get("passed") is True for s in stages), "A stage failed")
        mutations = {"create_" + k for k in packages} | {"base_parameters", "mid_base_and_parameter", "leaf_base_and_parameter", "node_RootScalar", "node_reference_RootScalar", "node_RootVector", "node_reference_RootVector", "save_once"}
        require(all(s.get("mutation") is (phase == "author" and s["name"] in mutations) for s in stages), "Stage mutation scope differs")
        lookup = {s["name"]: s.get("result") for s in stages}
        editor = report.get("editor", {})
        require(lookup.get("preflight") == editor and editor.get("success") is True, "Missing editor evidence")
        require(Path(editor["project_file"]).resolve() == PROJECT.resolve(), "Endpoint project mismatch")
        require(str(editor.get("engine_version", "")).startswith("5.8.3-") and editor.get("game_thread") is True and editor.get("pie_or_sie_active") is False and editor.get("dirty_packages") == [], "Invalid/dirty editor")
        require(bool(editor.get("instance_id")), "Missing process identity")
        own = lookup["independent_leaf_properties"]
        require(ref(field(own, "Base")) == objects["mid"] and field(own, "ScalarParameters") == [], "Leaf independent base/scalar readback mismatch")
        local_vectors = field(own, "VectorParameters")
        require(isinstance(local_vectors, list) and len(local_vectors) == 1 and field(local_vectors[0], "ParameterName") == "LeafVector", "Leaf local vector mismatch")
        definitions = {"RootScalar": ("base", .25), "RootVector": ("base", {"r": .1, "g": .2, "b": .3, "a": 1}), "MidScalar": ("mid", .5), "LeafVector": ("leaf", {"r": .4, "g": .5, "b": .6, "a": 1})}
        require(all(close(field(field(local_vectors[0], "DefaultValue"), k), v) for k, v in definitions["LeafVector"][1].items()), "Leaf independent default mismatch")
        local_counts = {"base": (1, 1), "mid": (1, 0), "leaf": (0, 1)}
        def chain_rows(keys):
            return [{"collection": objects[k], "local_scalar_count": local_counts[k][0], "local_vector_count": local_counts[k][1]} for k in keys]
        effective_stages = [s for s in stages if s["name"].startswith("effective_")] + [next(s for s in stages if s["name"] == "filter_inherited_scalar")]
        ids = {}
        leaf_data = None
        for index, (stage, key, names) in enumerate(zip(effective_stages, ("base", "mid", "leaf", "leaf", "leaf"), ({"RootScalar", "RootVector"}, {"RootScalar", "RootVector", "MidScalar"}, set(definitions), set(definitions), {"RootScalar"}))):
            value = stage["result"]
            read_result(value, objects[key])
            require(value.get("success") is True, "Effective value query failed")
            data = value["data"]
            require(data.get("collection") == objects[key] and data.get("collection_semantics_version") == "ue58_effective_collection_v1", "Wrong effective collection/semantics")
            chain = {"base": ["base"], "mid": ["mid", "base"], "leaf": ["leaf", "mid", "base"]}[key]
            require(data["base_chain"] == chain_rows(chain) and data.get("base_chain_order") == "queried_collection_to_root_base", "Base chain differs")
            require(data.get("include_runtime") is False and data.get("has_runtime_instance") is False and data.get("rendered_runtime_verified") is False, "Unexpected runtime scope")
            require(data.get("include_asset_defaults") is (index != 3), "Default query scope differs")
            require(data.get("effective_parameter_count_before_filter") == {"base": 2, "mid": 3, "leaf": 4}[key], "Effective count differs")
            require({r["name"] for r in data["scalars"]} == names & {"RootScalar", "MidScalar"} and {r["name"] for r in data["vectors"]} == names & {"RootVector", "LeafVector"}, "Scalar/vector classification differs")
            require(data.get("scalar_count") == len(data["scalars"]) and data.get("vector_count") == len(data["vectors"]), "Typed counts differ")
            require(all(data.get(k) == v for k, v in data["before"].items()), "Effective result differs from independent snapshots")
            rows = data["scalars"] + data["vectors"]
            require({row["name"] for row in rows} == names and len(rows) == len(names), "Effective parameter set differs")
            for row in rows:
                owner, value = definitions[row["name"]]
                require(row.get("declared_in") == objects[owner] and row.get("inherited") is (owner != key), "Declaration origin mismatch")
                guid = row.get("id", "")
                require(re.fullmatch(r"[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}", guid) is not None and guid.replace("-", "") != "0" * 32, "Invalid GUID")
                require(ids.setdefault(row["name"], guid) == guid, "GUID changes through inheritance")
                require(row.get("runtime_resolved") is False and "runtime_value" not in row, "Unexpected runtime result")
                if index == 3:
                    require("asset_default" not in row, "Default suppression ignored")
                elif isinstance(value, dict):
                    require(all(close(row.get("asset_default", {}).get(k), v) for k, v in value.items()), "Vector default mismatch")
                else:
                    require(close(row.get("asset_default"), value), "Scalar default mismatch")
            if index == 2:
                leaf_data = data
        require(len(set(ids.values())) == len(definitions), "Parameter GUID collision")
        require(field(local_vectors[0], "Id") == ids.get("LeafVector"), "Leaf independent GUID mismatch")
        require(report.get("effective_leaf_manifest") == {k: leaf_data[k] for k in ("collection", "scalars", "vectors", "base_chain")}, "Leaf summary not bound to observed result")
        nodes = report["nodes"]
        require(set(nodes) == {"RootScalar", "RootVector"} and len(set(nodes.values())) == 2 and all(p.startswith(objects["material"] + ":") for p in nodes.values()), "Node ownership mismatch")
        collection_nodes = lookup["inherited_collection_nodes"]
        read_result(collection_nodes, objects["material"])
        require(collection_nodes.get("success") is True, "Collection node query failed")
        data = collection_nodes["data"]
        require(data.get("material") == objects["material"] and data.get("graph_type") == "material" and data.get("collection_semantics_version") == "ue58_effective_collection_v1", "Collection node graph identity differs")
        require(all(data.get(k) == v for k, v in data["before"]["effective_collection_nodes"].items()), "Collection nodes differ from independent snapshots")
        require(data.get("collection_parameter_node_count") == 2 and data.get("missing_collection_parameter_count") == data.get("mismatched_id_count") == 0, "Collection node counts mismatch")
        require(len(data["nodes"]) == 2 and {r["parameter_name"] for r in data["nodes"]} == set(nodes), "Unexpected collection expressions")
        for row in data["nodes"]:
            name = row["parameter_name"]
            require(row.get("object_path") == nodes[name] and row.get("declared_in") == objects["base"] and row.get("inherited") is True and row.get("parameter_id_matches_collection") is True and row.get("collection_parameter_id") == ids[name], "Collection expression identity/GUID mismatch")
            require(row.get("collection") == objects["leaf"] and row.get("collection_parameter_found") is True and row.get("parameter_id") == ids[name] and row.get("collection_parameter_type") == ("scalar" if name == "RootScalar" else "vector") and row.get("base_chain") == chain_rows(("leaf", "mid", "base")), "Collection expression reference/type differs")
        for name in ("list_material_nodes_metadata", "analyze_material_graph_metadata"):
            result = lookup[name]
            read_result(result, objects["material"])
            require(result.get("success") is True, "Metadata query failed")
            data = result["data"]["ue58_collection_semantics"]
            require(all(data.get(k) is False for k in ("effective_values_verified", "nested_function_collections_verified", "parameter_ids_verified", "rendered_runtime_verified", "collection_transform_semantics_verified")), "Metadata overclaims evaluation")
            require(data["collections"] == [{"base_chain": chain_rows(("leaf", "mid", "base")), "base_collection": objects["mid"], "collection": objects["leaf"], "effective_parameter_count": 4, "has_inherited_parameters": True}] and data.get("unique_chain_parameter_count") == 4, "Metadata inheritance mismatch")
            require(result["data"]["before"].get("ue58_collection_semantics") == data, "Metadata differs from independent snapshots")
        require(all(lookup.get("clean_" + k) is False for k in packages), "Saved/reloaded package is dirty")
        if phase == "author":
            require(report.get("save") == {"attempts": 1, "saved": True} and lookup.get("save_once") is True and report.get("reload") == "not_run", "Invalid author save/reload status")
            for key in packages:
                require(lookup.get("absent_" + key) is False and ref(lookup.get("create_" + key)) == objects[key], "Author asset creation mismatch")
            for name in ("base_parameters", "mid_base_and_parameter", "leaf_base_and_parameter", "node_reference_RootScalar", "node_reference_RootVector"):
                require(lookup.get(name) is True, "Property write did not succeed: " + name)
            for name in nodes:
                require(ref(lookup.get("node_" + name)) == nodes[name], "Expression creation mismatch")
        else:
            require(parent is not None and parent_bytes is not None, "Cold phase requires author evidence")
            require(report.get("save") == {"attempts": 0, "saved": False} and report.get("reload") == "passed" and all(s.get("mutation") is False for s in stages), "Reload mutated/saved assets")
            if parent is not None and parent_bytes is not None:
                require(parent.get("phase") == "author" and verify(parent, check_files=check_files)["status"] == "pass", "Parent author invalid")
                require(json.loads(parent_bytes) == parent, "Parent bytes do not match verified author object")
                require(report.get("parent_report_sha256") == hashlib.sha256(parent_bytes).hexdigest(), "Parent hash mismatch")
                require(parent["editor"]["instance_id"] != editor["instance_id"], "Same-process reload")
                require(all(report.get(k) == parent.get(k) for k in ("run_id", "packages", "objects", "nodes", "effective_leaf_manifest", "asset_files")), "Reload identities/values/files differ")
        files = report.get("asset_files", {})
        require(set(files) == set(packages), "Missing asset file evidence")
        for key, package in packages.items():
            expected_path = (PROJECT.parent / "Content" / (package[len("/Game/"):] + ".uasset")).resolve()
            require(Path(files[key]["path"]).resolve() == expected_path, "File path ownership mismatch")
            require(re.fullmatch(r"[0-9a-f]{64}", files[key]["sha256"]) is not None, "Invalid file hash")
            if check_files:
                require(expected_path.is_file() and hashlib.sha256(expected_path.read_bytes()).hexdigest() == files[key]["sha256"], "Saved file hash mismatch")
        coverage = report.get("coverage", {})
        require(coverage.get("base_free_and_inherited_names_defaults_ids") == coverage.get("direct_graph_metadata") == "passed", "Missing scope coverage")
        require(all(str(coverage.get(k, "")).startswith("not_run") for k in ("asset_base_override_values", "runtime_overrides", "depth_cycle_ambiguous_corruption", "nested_functions_and_collection_transform", "shader_render", "packaging")), "Unexecuted scope claimed")
    except (AttributeError, IndexError, KeyError, TypeError, ValueError, StopIteration, OSError) as error:
        errors.append("Malformed/incomplete evidence: " + str(error))
    return {"status": "pass" if not errors else "fail", "errors": errors}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--parent-report", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Refusing to overwrite evidence")
    raw = args.report.read_bytes()
    parent_raw = args.parent_report.read_bytes() if args.parent_report else None
    result = verify(json.loads(raw), json.loads(parent_raw) if parent_raw else None, parent_raw)
    result.update(schema="cubeless_mpc_inheritance_independent_v1", report_sha256=hashlib.sha256(raw).hexdigest())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2, ensure_ascii=False)
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
