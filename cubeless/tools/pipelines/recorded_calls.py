"""Small external pipeline recorder for the official MCP client; never retries."""
import json
import time

from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.blueprint_mutation_smoke import canonical, operation_result
from pipelines.component_authoring import require, success


class RecordedCalls:
    def __init__(self, endpoint, output, report, toolsets):
        self.client = OfficialMCPClient(endpoint)
        self.output, self.report, self.toolsets = output, report, toolsets
        self.schemas = {}

    def persist(self):
        self.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.output.with_suffix(self.output.suffix + ".partial")
        temporary.write_text(json.dumps(self.report, ensure_ascii=False, indent=2), encoding="utf-8")
        temporary.replace(self.output)

    def discover(self):
        self.persist()
        self.client.initialize()
        for key, name in self.toolsets.items():
            self.schemas[key] = tool_payload(self.client.call("describe_toolset", {"toolset_name": name}))
        self.report["schemas"] = self.schemas

    def call(self, label, group, tool, arguments=None, *, mutation=False, expect_success=True):
        self.report["stage"] = label
        choices = [item for item in self.schemas[group]["tools"] if item["name"].rsplit(".", 1)[-1] == tool]
        require(len(choices) == 1, "Missing unique tool: " + tool)
        schema, mapped = choices[0]["inputSchema"], {}
        for key, value in (arguments or {}).items():
            matches = [name for name in schema.get("properties", {}) if canonical(name) == canonical(key)]
            require(len(matches) == 1, "Missing input: " + key)
            mapped[matches[0]] = value
        require(all(key in mapped for key in schema.get("required", [])), "Unexpected required argument")
        row = {"name": label, "toolset": self.toolsets[group], "tool": tool, "arguments": mapped,
               "mutation": mutation, "state": "submitted", "started_ns": time.time_ns()}
        self.report["stages"].append(row)
        self.persist()
        try:
            row["raw"] = self.client.call_toolset(self.toolsets[group], tool, mapped, mutation=mutation)
            value = operation_result(row["raw"])
            row.update(result=value, state="responded")
            if isinstance(value, dict) and "changes" in value:
                self.report["changes"].append({"stage": label, "observations": value["changes"]})
                row.update(compile=value.get("compile"), save=value.get("save"))
                data = value.get("data", {})
                if (any(item.get("change_state_known") is False for item in value["changes"])
                        or data.get("mutation_state_unknown") or data.get("dispatch_outcome_unknown")
                        or data.get("batch", {}).get("mutation_state_unknown")):
                    self.report["outcome_unknown"] = True
            if expect_success and group in {"native", "bp", "graph", "batch"}:
                success(value)
            return value
        except Exception as error:
            row["error"] = str(error)
            if getattr(error, "detail", None) is not None:
                row["error_detail"] = error.detail
            if getattr(error, "outcome_unknown", False):
                row["outcome_unknown"] = True
                self.report["outcome_unknown"] = True
            if mutation and row["state"] == "submitted":
                self.report["outcome_unknown"] = True
            raise
        finally:
            row["elapsed_seconds"] = (time.time_ns() - row["started_ns"]) / 1e9
            self.persist()

    def finish(self):
        self.report.update(finished_ns=time.time_ns(), http_observations=self.client.observations)
        self.persist()
        self.client.close()
