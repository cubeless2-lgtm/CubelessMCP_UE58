"""External stdlib-only client for the installed UE5.8 official MCP HTTP server.

This starts no server and adds no plugin dependency. No automatic request retry.
Observed source: ModelContextProtocolServer.cpp / ModelContextProtocol.h (5.8.3).
"""
from __future__ import annotations

import argparse
import json
import time
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import urlparse
from urllib.request import Request, build_opener, ProxyHandler


class MCPError(RuntimeError):
    def __init__(self, message, *, outcome_unknown=False, detail=None):
        super().__init__(message)
        self.outcome_unknown = outcome_unknown
        self.detail = detail


def tool_payload(result):
    """Decode official structuredContent, with JSON text compatibility fallback."""
    if result.get("isError"):
        raise MCPError("Official MCP tool returned isError", detail=result)
    value = result.get("structuredContent")
    if value is None:
        texts = [x.get("text", "") for x in result.get("content", []) if x.get("type") == "text"]
        if len(texts) == 1:
            try:
                value = json.loads(texts[0])
            except json.JSONDecodeError:
                value = texts[0]
        else:
            value = texts
    if isinstance(value, dict) and "returnValue" in value:
        return value["returnValue"]
    return value


class OfficialMCPClient:
    def __init__(self, endpoint, timeout=120):
        parts = urlparse(endpoint)
        if parts.scheme not in {"http", "https"} or not parts.hostname or parts.username or parts.password:
            raise ValueError("Use an explicit HTTP(S) endpoint without embedded credentials")
        self.endpoint = endpoint
        self.timeout = timeout
        self.session_id = None
        self.protocol = "2025-11-25"
        self.next_id = 0
        self.observations = []
        # Local UE should not be sent through an ambient corporate HTTP proxy.
        self.opener = build_opener(ProxyHandler({}))

    def request(self, method, params=None, *, notification=False, mutation=False):
        payload = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            payload["params"] = params
        if not notification:
            self.next_id += 1
            payload["id"] = self.next_id
        headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
            headers["Mcp-Protocol-Version"] = self.protocol
        began = time.perf_counter()
        entry = {"method": method, "request_id": payload.get("id"), "mutation": mutation, "status": "started"}
        self.observations.append(entry)
        try:
            req = Request(self.endpoint, data=json.dumps(payload).encode("utf-8"), headers=headers, method="POST")
            with self.opener.open(req, timeout=self.timeout) as response:
                raw = response.read()
                sid = response.headers.get("Mcp-Session-Id")
                if sid:
                    self.session_id = sid
                entry["http_status"] = response.status
            if notification and not raw:
                entry["status"] = "accepted"
                return None
            doc = json.loads(raw)
            if doc.get("id") != payload.get("id"):
                raise MCPError("MCP response id mismatch", outcome_unknown=mutation, detail=doc)
            if "error" in doc:
                raise MCPError("MCP JSON-RPC error", outcome_unknown=mutation, detail=doc["error"])
            entry["status"] = "received"
            return doc["result"]
        except HTTPError as exc:
            entry["status"] = "http_error"
            detail = exc.read().decode("utf-8", errors="replace")
            raise MCPError(f"MCP HTTP {exc.code}: {detail}", outcome_unknown=mutation) from exc
        except (URLError, TimeoutError, OSError, json.JSONDecodeError) as exc:
            entry["status"] = "transport_or_decode_error"
            raise MCPError(str(exc), outcome_unknown=mutation) from exc
        finally:
            entry["elapsed_seconds"] = round(time.perf_counter() - began, 6)

    def initialize(self):
        result = self.request("initialize", {"protocolVersion": self.protocol, "capabilities": {}, "clientInfo": {"name": "CubelessMCP_UE58_verification", "version": "0.1.0"}})
        self.protocol = result["protocolVersion"]
        if not self.session_id:
            raise MCPError("UE initialize returned no Mcp-Session-Id")
        self.request("notifications/initialized", notification=True)
        return result

    def list_tools(self):
        return self.request("tools/list", {})

    def call(self, name, arguments=None, *, mutation=False):
        return self.request("tools/call", {"name": name, "arguments": arguments or {}}, mutation=mutation)

    def call_toolset(self, toolset_name, tool_name, arguments=None, *, mutation=False):
        return self.call("call_tool", {"toolset_name": toolset_name, "tool_name": tool_name, "arguments": arguments or {}}, mutation=mutation)

    def close(self):
        if not self.session_id:
            return
        request = Request(self.endpoint, headers={"Mcp-Session-Id": self.session_id, "Mcp-Protocol-Version": self.protocol}, method="DELETE")
        try:
            with self.opener.open(request, timeout=min(self.timeout, 10)) as response:
                response.read()
        except (HTTPError, URLError, TimeoutError, OSError):
            # Session cleanup cannot mask the primary pipeline result.
            pass
        self.session_id = None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--describe-toolset")
    parser.add_argument("--expected-project", type=Path, help="Verify the identity of this bundle's running Editor")
    parser.add_argument("--expected-pid", type=int, help="PID observed owning the local listener or recorded at launch")
    args = parser.parse_args()
    if (args.expected_project is None) != (args.expected_pid is None):
        parser.error("--expected-project and --expected-pid must be supplied together")
    if args.expected_project is not None:
        from pipelines.layout import editor_project_matches, project_file, verification_output

        if args.expected_pid <= 0:
            parser.error("--expected-pid must be positive")
        if urlparse(args.endpoint).hostname not in {"127.0.0.1", "::1", "localhost"}:
            parser.error("Editor identity checks require a local loopback endpoint")
        try:
            args.expected_project = project_file(args.expected_project)
            args.output = verification_output(args.output, args.expected_project)
        except (OSError, ValueError) as exc:
            parser.error(str(exc))
    if args.output.exists() or args.output.is_symlink():
        parser.error("Refusing to overwrite existing evidence")
    client = OfficialMCPClient(args.endpoint, timeout=10 if args.expected_project else 120)
    report = {"schema": "cubeless_official_mcp_discovery_v1", "endpoint": args.endpoint, "mutation": False}
    if args.expected_project:
        report.update(expected_project=str(args.expected_project), expected_pid=args.expected_pid)
    try:
        report["stage"] = "initialize"
        report["initialize"] = client.initialize()
        report["stage"] = "discovery"
        report["tools"] = client.list_tools()
        names = {x["name"] for x in report["tools"].get("tools", [])}
        if args.expected_project:
            if "call_tool" not in names:
                raise MCPError("Official toolset dispatcher is missing")
            report["stage"] = "identity"
            diagnostics = tool_payload(client.call_toolset("CubelessMCPUE58Editor.CubelessNativeToolset", "GetEditorDiagnostics"))
            if isinstance(diagnostics, str):
                diagnostics = json.loads(diagnostics)
            report["diagnostics"] = diagnostics
            if not isinstance(diagnostics, dict) or diagnostics.get("success") is not True:
                raise MCPError("Editor diagnostics failed")
            observed = diagnostics.get("project_file")
            if not (isinstance(observed, str) and Path(observed).is_absolute()
                    and editor_project_matches(observed, args.expected_project)):
                raise MCPError("Editor project does not match --expected-project")
            if type(diagnostics.get("process_id")) is not int or diagnostics["process_id"] != args.expected_pid:
                raise MCPError("Editor PID does not match --expected-pid")
            if (diagnostics.get("is_commandlet") is not False
                    or not str(diagnostics.get("engine_version", "")).startswith("5.8.")):
                raise MCPError("Expected a UE 5.8.x Editor")
        report["stage"] = "discovery"
        if "list_toolsets" in names:
            report["toolsets"] = client.call("list_toolsets")
            tool_payload(report["toolsets"])
        if args.describe_toolset:
            report["description"] = client.call("describe_toolset", {"toolset_name": args.describe_toolset})
            tool_payload(report["description"])
        report.update(status="pass", stage="complete")
    except Exception as exc:
        report.update(status="fail", error=str(exc))
    finally:
        report["calls"] = client.observations
        client.close()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
    print(json.dumps({"status": report["status"], "stage": report["stage"], "output": str(args.output)}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
