# MCP Runtime Hardening

This layer keeps the existing stdio MCP server and Unreal TCP bridge stable
while adding read-only health, request telemetry, and conservative runtime
guards.

## Defaults

- Existing stdio MCP usage remains allowed.
- The Unreal bridge target defaults to `127.0.0.1:55557`.
- Loopback bridge targets are allowed.
- Non-loopback bridge targets are blocked by default in hard guard mode.
- Rate limiting is disabled by default.
- Tool/category disable policy remains runtime-memory only and resets on server
  restart.

## Environment

- `UNREAL_MCP_HOST`: Unreal bridge host. Default: `127.0.0.1`.
- `UNREAL_MCP_PORT`: Unreal bridge port. Default: `55557`.
- `UNREAL_MCP_GUARD_MODE`: `hard`, `soft`, or `off`. Default: `hard`.
- `UNREAL_MCP_ALLOW_NON_LOOPBACK`: allow a non-loopback bridge target when set
  to a truthy value. Default: false.
- `UNREAL_MCP_REQUIRE_TOKEN_FOR_NON_LOOPBACK`: require a configured capability
  token for non-loopback bridge targets. Default: true.
- `UNREAL_MCP_CAPABILITY_TOKEN`: marks the local runtime as explicitly
  configured for non-loopback use. This is not exposed in reports.
- `UNREAL_MCP_RATE_LIMIT_PER_MINUTE`: per-tool runtime call limit. Default: `0`
  means disabled.

## Guard Modes

- `hard`: block disallowed non-loopback bridge targets and configured rate-limit
  excesses.
- `soft`: record would-block guard events but allow the request.
- `off`: skip runtime guard checks.

The stdio transport does not use a remote peer token today. Capability token
enforcement is therefore only used as an explicit operator signal before a
non-loopback bridge target can be allowed. Future HTTP/SSE work must add
per-request token validation before exposing any non-loopback inbound endpoint.

## Health And Telemetry

Use `manage_tools` with `action="health"` for registry/telemetry status and an
optional bridge ping:

```json
{
  "action": "health",
  "include_tools": false,
  "include_recent": true,
  "ping_unreal": false
}
```

Set `ping_unreal` to `true` only when a live bridge check is desired. The ping
uses the existing native `ping` command and does not mutate Unreal assets.

Use `action="heartbeat"` when a live Unreal bridge check is required. Heartbeat
forces the ping path and returns `success=false` when the bridge is not
connected.

The health report includes:

- tool registry counts and runtime enablement state
- telemetry totals, failures, active requests, and recent events
- guard configuration without revealing the token value
- Unreal bridge target and optional ping result

## Promotion Rule

Do not add HTTP/SSE or broader token enforcement until this hardening layer
continues to pass the parity audit, management tests, runtime import/call smoke,
and UnrealBuildTool build.
