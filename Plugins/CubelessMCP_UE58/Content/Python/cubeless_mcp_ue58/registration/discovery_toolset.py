"""Legacy discovery contracts; work logic uses only local official Registry APIs."""
import json
import unreal
import toolset_registry
from .discovery_result import CubelessDiscoveryAsyncResult


@unreal.uclass()
class CubelessDiscoveryTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def list_toolsets(query: str = "") -> str:
        """List original category/count/effect/query metadata against current exact registered Cubeless bindings. Preserves actual433 catalog and separately documents discovery3 exposure-only identities (union436). No Engine operation is dispatched. Availability is source/schema binding evidence, never behavior completion or imported old process-local policy state; current module-local overrides affect this compatibility frontdoor only."""
        from ..operations.tool_discovery import list_toolsets
        return json.dumps(list_toolsets(query), ensure_ascii=False, sort_keys=True)

    @toolset_registry.tool_call
    @staticmethod
    def describe_toolset(toolset: str, query: str = "", offset: int = 0, limit: int = 20,
            include_schema: bool = True, verbosity: str = "full") -> str:
        """Describe original paginated contracts/policies with actual current binding metadata. Accepts all/* and original category; limit1..50, offset>=0 and full/terse, with original defaults/errors. Original source schema remains separate from the authoritative current logical Registry schema; unconnected/ambiguous contracts stay explicit incomplete."""
        from ..operations.tool_discovery import describe_toolset
        return json.dumps(describe_toolset(toolset, query, offset, limit, include_schema, verbosity), ensure_ascii=False, sort_keys=True)

    @toolset_registry.tool_call
    @staticmethod
    def call_tool(toolset: str, tool_name: str, arguments: unreal.JsonObjectWrapper | None = None,
            confirm: bool = False) -> CubelessDiscoveryAsyncResult:
        """Call one exact current registered Cubeless tool after original category/confirmation/dry-run policy and current logical input-schema checks. arguments is an object or null, never a JSON-quoted FString carrier. Reuses official local Registry execution and stock async future once, preserving partial/save/unknown receipts without retry/cancel. Original433 static policy/defaults are source facts; old server enable overrides are not imported; current manage_tools overrides apply only to compatibility discovery calls. Renamed/ambiguous bindings, Pydantic coercion-only inputs and every-handler output equivalence remain incomplete. Exposure-only discovery3 cannot dispatch itself; synchronous reentry is rejected. Metadata or a generic successful dispatch does not complete each migrated API."""
        from ..operations.tool_discovery import call_tool
        value = None if arguments is None else json.loads(arguments.export_text() or "{}")
        return call_tool(toolset, tool_name, value, confirm)


    @toolset_registry.tool_call
    @staticmethod
    def manage_tools(action: str = "status", tool_name: str = "", category: str = "",
            enabled: bool | None = None, include_tools: bool = True,
            include_recent: bool = True, ping_unreal: bool = False) -> str:
        """Inspect or adjust original process-local compatibility catalog policy. Default status is read-only. Enable/disable/set tool/category and reset affect only this discovery frontdoor; official Named tools and direct Engine calls are outside its scope. manage_tools is protected from disabling, including category disable. Overrides reset on Python module reload, are never persisted, and do not register/unregister any toolset. Health retains actual local Editor/Registry/provider metadata; old TCP ping/counters/server guard are null/retired with explicit provenance, never replaced by fake official HTTP values. include_recent and ping_unreal requests remain reported without transport I/O."""
        from ..operations.tool_discovery import manage_tools
        return json.dumps(manage_tools(action, tool_name, category, enabled, include_tools, include_recent, ping_unreal), ensure_ascii=False, sort_keys=True)
