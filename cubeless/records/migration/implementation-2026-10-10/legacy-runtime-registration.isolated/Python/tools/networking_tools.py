"""UE 5.6.1 Actor Blueprint networking authoring tools."""

import logging
from typing import Any, Dict, Optional

from mcp.server.fastmcp import FastMCP
from pydantic import StrictBool, StrictFloat, StrictStr

from tools.dependency_guard import reject_mcp_dependency_reference
from tools.exact_json_arguments import enforce_exact_json_fields
from tools.response_schema import tool_pre_dispatch_error

logger = logging.getLogger("UnrealMCP")


def _error(message: str, error_code: str, **fields: Any) -> Dict[str, Any]:
    return tool_pre_dispatch_error(
        message,
        error_code=error_code,
        **fields,
    )


def _send(command: str, blueprint_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
    from unreal_mcp_server import get_unreal_connection

    try:
        reject_mcp_dependency_reference("blueprint_name", blueprint_name)
    except ValueError as exc:
        return _error(str(exc), "invalid_networking_request")

    unreal = get_unreal_connection()
    if not unreal:
        return _error("Failed to connect to Unreal Engine", "unreal_connection_failed")

    try:
        response = unreal.send_command(command, params, timeout_seconds=60)
    except Exception as exc:
        logger.exception("Networking bridge command failed: %s", command)
        return tool_pre_dispatch_error(
            "Networking bridge command failed",
            error_code="networking_tool_failed",
            details={"exception_type": type(exc).__name__},
        )
    if response is None:
        return _error("No response from Unreal Engine", "unreal_no_response")
    return response


def register_networking_tools(mcp: FastMCP) -> None:
    """Register the UE 5.6.1-only networking surface."""

    @mcp.tool()
    def inspect_blueprint_networking_settings(
        blueprint_name: StrictStr,
    ) -> Dict[str, Any]:
        """Inspect replication, dormancy, relevancy, frequency, and priority on an Actor Blueprint CDO."""
        return _send(
            "networking_inspect_blueprint_settings",
            blueprint_name,
            {"blueprint_name": blueprint_name},
        )

    @mcp.tool()
    def set_blueprint_networking_settings(
        blueprint_name: StrictStr,
        replicates: Optional[StrictBool] = None,
        replicate_movement: Optional[StrictBool] = None,
        net_dormancy: Optional[StrictStr] = None,
        always_relevant: Optional[StrictBool] = None,
        only_relevant_to_owner: Optional[StrictBool] = None,
        net_update_frequency: Optional[StrictFloat] = None,
        min_net_update_frequency: Optional[StrictFloat] = None,
        net_priority: Optional[StrictFloat] = None,
        net_cull_distance_squared: Optional[StrictFloat] = None,
        dry_run: StrictBool = True,
    ) -> Dict[str, Any]:
        """Set UE 5.6 Actor networking defaults in one editor transaction with compile/save readback.

        Unspecified values are preserved. ``dry_run`` defaults to true. Supported
        dormancy values and numeric constraints are documented in
        ``Docs/Tools/networking_tools.md`` and enforced by the native handler.
        """
        values: Dict[str, Any] = {
            "replicates": replicates,
            "replicate_movement": replicate_movement,
            "net_dormancy": net_dormancy,
            "always_relevant": always_relevant,
            "only_relevant_to_owner": only_relevant_to_owner,
            "net_update_frequency": net_update_frequency,
            "min_net_update_frequency": min_net_update_frequency,
            "net_priority": net_priority,
            "net_cull_distance_squared": net_cull_distance_squared,
        }
        params = {"blueprint_name": blueprint_name, "dry_run": dry_run}
        params.update({name: value for name, value in values.items() if value is not None})
        return _send("networking_set_blueprint_settings", blueprint_name, params)

    enforce_exact_json_fields(
        mcp,
        "inspect_blueprint_networking_settings",
        {"blueprint_name"},
    )
    enforce_exact_json_fields(
        mcp,
        "set_blueprint_networking_settings",
        {
            "blueprint_name",
            "replicates",
            "replicate_movement",
            "net_dormancy",
            "always_relevant",
            "only_relevant_to_owner",
            "net_update_frequency",
            "min_net_update_frequency",
            "net_priority",
            "net_cull_distance_squared",
            "dry_run",
        },
    )
