"""Small official MCP recorder; never retries engine calls."""
import json
import os
import time

from official_mcp_client import OfficialMCPClient, tool_payload
from pipelines.result_contracts import canonical, operation_result, require, success
from pipelines.recorded_call_storage import ReceiptStorage, StorageError


class RecordedCalls:
    def __init__(self, endpoint, output, report, toolsets, *, storage_v2=False):
        self.storage = ReceiptStorage(output, report) if storage_v2 else None
        self.client = OfficialMCPClient(endpoint)
        self.output, self.report, self.toolsets = output, report, toolsets
        self.schemas = {}

    def persist(self):
        if self.storage is not None:
            return self.storage.checkpoint(self.report)
        self.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.output.with_suffix(self.output.suffix + ".partial")
        with temporary.open("w", encoding="utf-8") as stream:
            stream.write(json.dumps(self.report, ensure_ascii=False, indent=2))
            stream.flush()
            os.fsync(stream.fileno())
        # A Windows reader without FILE_SHARE_DELETE can briefly block an
        # atomic rename. Retry only this filesystem step, never an MCP request.
        delays = (0.025, 0.05, 0.1, 0.1, 0.1)
        for attempt in range(len(delays) + 1):
            try:
                temporary.replace(self.output)
                return
            except PermissionError as error:
                if (os.name != "nt" or getattr(error, "winerror", None) not in (5, 32)
                        or attempt == len(delays)):
                    raise
                time.sleep(delays[attempt])

    def discover(self):
        self.persist()
        self.client.initialize()
        for key, name in self.toolsets.items():
            self.schemas[key] = tool_payload(self.client.call("describe_toolset", {"toolset_name": name}))
        self.report["schemas"] = self.schemas
        if self.storage is not None:
            self.storage.record_schemas(self.schemas)
            self.persist()

    def call(self, label, group, tool, arguments=None, *, mutation=False, expect_success=True, plain_string=False):
        self.report["stage"] = label
        choices = [item for item in self.schemas[group]["tools"] if item["name"].rsplit(".", 1)[-1] == tool]
        require(len(choices) == 1, "Missing unique tool: " + tool)
        schema, mapped = choices[0]["inputSchema"], {}
        for key, value in (arguments or {}).items():
            matches = [name for name in schema.get("properties", {}) if canonical(name) == canonical(key)]
            require(len(matches) == 1, "Missing input: " + key)
            require(matches[0] not in mapped, "Duplicate normalized argument: " + key)
            mapped[matches[0]] = value
        require(all(key in mapped for key in schema.get("required", [])), "Unexpected required argument")
        row = {"name": label, "toolset": self.toolsets[group], "tool": tool, "arguments": mapped,
               "mutation": mutation, "state": "submitted", "started_ns": time.time_ns()}
        self.report["stages"].append(row)
        if self.storage is not None:
            self.storage.before_dispatch(row, self.report)
        else:
            self.persist()
        try:
            row["raw"] = self.client.call_toolset(self.toolsets[group], tool, mapped, mutation=mutation)
            if self.storage is not None:
                self.storage.raw_returned(row["raw"], self.report)
            # Some named official tools return a scalar label/path. The default
            # still requires JSON for native diagnostics and operation envelopes.
            value = tool_payload(row["raw"]) if plain_string else operation_result(row["raw"])
            if plain_string:
                require(type(value) is str, "Expected an actual official scalar string")
            row.update(result=value, state="responded")
            if isinstance(value, dict) and "changes" in value:
                self.report["changes"].append({"stage": label, "observations": value["changes"]})
                row.update(compile=value.get("compile"), save=value.get("save"))
                data = value.get("data", {})
                if (any(item.get("change_state_known") is False for item in value["changes"])
                        or data.get("mutation_state_unknown") or data.get("dispatch_outcome_unknown")
                        or data.get("batch", {}).get("mutation_state_unknown")):
                    self.report["outcome_unknown"] = True
            if self.storage is not None:
                self.storage.decoded_returned(row, self.report)
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
            if self.storage is None:
                self.persist()
            elif not self.storage.failed:
                try:
                    self.storage.completed(row, self.report)
                except StorageError as error:
                    row["error"] = str(error)
                    row["receipt_storage_failed"] = True
                    if error.outcome_unknown:
                        row["outcome_unknown"] = True
                        self.report["outcome_unknown"] = True
                    raise

    def finish(self):
        self.report.update(finished_ns=time.time_ns(), http_observations=self.client.observations)
        if self.storage is not None:
            try:
                self.storage.finish(self.report)
            finally:
                self.client.close()
            return
        self.persist()
        self.client.close()
