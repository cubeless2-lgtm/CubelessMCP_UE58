# Confirmed transport-only removals

`native:ping`: source `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/UnrealMCPBridge.cpp:695–699` only returns `{message: pong}` through the former TCP bridge. It calls no domain handler or engine mutation. This wire-level route is removed because Cubeless supplies official Registry tools and has no private TCP bridge. Official MCP initialize and Cubeless diagnostics were actually called through the isolated official server; they provide new-architecture liveness, not legacy protocol compatibility.

No other route is classified as removed on this evidence. In particular legacy batch, validation and capability/provider behavior require independent migration; removal of their transport does not remove their responsibilities.
