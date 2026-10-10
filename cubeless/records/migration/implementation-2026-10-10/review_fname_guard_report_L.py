"""Offline independent audit of saved L guard evidence. No UE/network imports."""
import copy
import hashlib
import json
from pathlib import Path


def require(value, message):
    if not value:
        raise ValueError(message)


def decode(raw):
    require(not raw.get("isError"), "MCP protocol/tool error")
    value = raw.get("structuredContent")
    if value is None:
        texts = [item["text"] for item in raw["content"] if item["type"] == "text"]
        require(len(texts) == 1, "single JSON text payload required")
        value = json.loads(texts[0])
    if isinstance(value, dict) and "returnValue" in value:
        value = value["returnValue"]
    if isinstance(value, str):
        value = json.loads(value)
    if isinstance(value, dict):
        fields = {key.replace("_", "").lower(): item for key, item in value.items()}
        if "changesjson" in fields:
            result = {key: fields[key.replace("_", "")] for key in ("success", "operation_id", "stage", "error_code", "error")}
            for key in ("changes", "compile", "save", "validation", "retry", "data"):
                result[key] = json.loads(fields[key + "json"])
            return result
    return value


def review(report):
    require(report["schema"] == "cubeless_fname_guard_probe_v1" and report["status"] == "pass", "not a passing guard report")
    require(report["automatic_retries"] == 0 and report["outcome_unknown"] is False, "retry or unknown state")
    cases = [mode + "_" + kind for mode in ("add", "set") for kind in ("property_name", "direct_fname", "nested_fname", "long_valid_aggregate")]
    suffixes = ["custom", "custom_graph", "collection", "collection_graph"]
    expected = ["preflight"] + ["before_" + suffix for suffix in suffixes]
    for case in cases:
        expected += [case] + [case + "_after_" + suffix for suffix in suffixes]
    expected += ["long_mpc_filter", "empty_filter", "short_filter", "long_package_name"]
    expected += ["final_" + suffix for suffix in suffixes] + ["final_clean"]
    require([stage["name"] for stage in report["stages"]] == expected, "missing/reordered/extra stage")
    stages = {stage["name"]: stage for stage in report["stages"]}
    for stage in report["stages"]:
        require(decode(stage["raw"]) == stage["result"], "raw/result mismatch: " + stage["name"])
        require(stage["mutation"] == (stage["name"] in cases), "incorrect mutation annotation")
    for label in ("preflight", "final_clean"):
        item = stages[label]["result"]
        require(item["success"] is True and item["process_id"] == report["expected_pid"], "diagnostic process mismatch")
        require(item["project_file"].replace("\\", "/").endswith("/cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"), "wrong test project")
        require(item["dirty_packages"] == [] and item["game_thread"] is True and item["pie_or_sie_active"] is False, "dirty/play/thread invariant failed")
    for case in cases:
        item = stages[case]["result"]
        require(item["success"] is False and item["stage"] == "preflight" and item["error_code"] == "material_mutation_target_rejected", "guard did not reject before mutation")
        native = item["data"]["native_outcome"]
        require(native["mutation_attempted"] is False and native["changed"] is False and native["change_state_known"] is True, "mutation/change observation contradiction")
        require(native["dirty_before"] is False and native["dirty_after"] is False and item["changes"] == [], "guard dirtied or reported changes")
        require(item["compile"]["attempted"] is False and item["save"]["attempted"] is False, "compile/save was attempted")
        require("NAME_SIZE" in item["error"], "expected name limit diagnostic absent")
        for suffix in suffixes:
            before = stages["before_" + suffix]["result"]
            after = stages[case + "_after_" + suffix]["result"]
            if suffix.endswith("_graph"):
                require(before["success"] is True and after["success"] is True, "graph read failed")
                before, after = before["data"]["nodes"], after["data"]["nodes"]
            require(before == after, "independent observed state changed: " + case + ":" + suffix)
    for suffix in suffixes:
        before, after = stages["before_" + suffix]["result"], stages["final_" + suffix]["result"]
        if suffix.endswith("_graph"):
            before, after = before["data"]["nodes"], after["data"]["nodes"]
        require(before == after, "final readback changed")
    mpc = stages["long_mpc_filter"]["result"]
    require(mpc["success"] is False and mpc["error_code"] == "material_preflight_failed" and mpc["data"]["error_code"] == "collection_parameter_filter_invalid", "MPC rejection absent")
    for label in ("empty_filter", "short_filter"):
        require(stages[label]["result"]["success"] is True, "valid filter failed")
    audit = stages["long_package_name"]["result"]
    require(audit["success"] is False and "NAME_SIZE" in json.dumps(audit), "package guard absent")
    require(all((call.get("status") == "received" and call.get("http_status") == 200) or
                (call.get("method") == "notifications/initialized" and call.get("request_id") is None and call.get("status") == "accepted" and call.get("http_status") == 202)
                for call in report["calls"]), "transport evidence includes failed call")
    return {"status": "pass", "stages": len(expected), "negative_guard_requests": 10,
            "mutation_negative_requests": 8, "readback_calls": 40,
            "diagnostic_calls": 2, "positive_filter_controls": 2,
            "all_raw_payloads_match_decoded_results": True}


def main():
    here = Path(__file__).resolve().parent
    verification = here.parents[1] / "verification" / here.name
    path = verification / "material-fname-guards-L2.json"
    report = json.loads(path.read_text(encoding="utf-8"))
    result = review(report)
    counterexamples = {}
    for name in ("missing_stage", "raw_decoded_disagreement", "dirty_end", "after_state_change"):
        altered = copy.deepcopy(report)
        if name == "missing_stage":
            altered["stages"].pop(5)
        elif name == "raw_decoded_disagreement":
            altered["stages"][5]["result"]["success"] = True
        elif name == "dirty_end":
            altered["stages"][-1]["result"]["dirty_packages"] = ["/Game/Changed"]
            altered["stages"][-1]["raw"] = {"content": [{"type": "text", "text": json.dumps({"returnValue": json.dumps(altered["stages"][-1]["result"])})}]}
        else:
            stage = altered["stages"][6]
            stage["result"]["Code"] = "changed by synthetic counterexample"
            stage["raw"] = {"content": [{"type": "text", "text": json.dumps({"returnValue": stage["result"]})}]}
        try:
            review(altered)
            raise AssertionError("Corrupted evidence accepted: " + name)
        except ValueError as error:
            counterexamples[name] = str(error)
    result.update(source=str(path), source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                  synthetic_counterexamples_rejected=counterexamples, engine_or_network_actions=0,
                  limitations=["Disk DLL hash plus diagnostics PID is reported; this offline review does not independently hash a loaded module",
                               "Selected official properties and complete helper node arrays are compared; arbitrary UObject state is outside this observation",
                               "No successful long FString operation or save/reload is established by this rejection report",
                               "Short filter control returns zero rows; it proves accepted safe unknown-name lookup, not a positive matching parameter",
                               "Audit negative package is 1030 UTF-16 units, not an exact 1024 boundary test"])
    output = verification / "material-fname-guards-L2-independent-review.json"
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
