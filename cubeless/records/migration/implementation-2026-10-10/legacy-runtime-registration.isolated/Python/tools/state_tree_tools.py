"""Focused Unreal Engine 5.6 StateTree asset tools."""

import logging
from typing import Any, Dict

from mcp.server.fastmcp import FastMCP
from pydantic import StrictBool

from tools.dependency_guard import reject_mcp_dependency_reference
from tools.exact_json_arguments import enforce_exact_json_fields
from tools.response_schema import tool_pre_dispatch_error

logger = logging.getLogger("UnrealMCP")


def register_state_tree_tools(mcp: FastMCP) -> None:
    """Register the UE 5.6-only StateTree foundation."""

    def send_state_tree_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        from unreal_mcp_server import get_unreal_connection

        unreal = get_unreal_connection()
        if not unreal:
            return tool_pre_dispatch_error(
                "Failed to connect to Unreal Engine",
                error_code="unreal_connection_failed",
            )
        response = unreal.send_command(command_name, params)
        if response is None:
            return tool_pre_dispatch_error(
                "No response from Unreal Engine",
                error_code="unreal_no_response",
            )
        return response

    def state_tree_error(message: str, exc: Exception) -> Dict[str, Any]:
        logger.exception(message)
        return tool_pre_dispatch_error(
            message,
            error_code="state_tree_tool_failed",
            details={"exception_type": type(exc).__name__},
        )

    @mcp.tool()
    def list_state_trees(
        directory: str = "/Game",
        recursive: bool = True,
        max_results: int = 200,
        offset: int = 0,
    ) -> Dict[str, Any]:
        """List StateTree assets with bounded deterministic pagination."""
        try:
            reject_mcp_dependency_reference("directory", directory)
            return send_state_tree_command(
                "state_tree_list_assets",
                {
                    "directory": directory,
                    "recursive": bool(recursive),
                    "max_results": int(max_results),
                    "offset": int(offset),
                },
            )
        except Exception as exc:
            return state_tree_error(f"Error listing StateTrees: {exc}", exc)

    @mcp.tool()
    def inspect_state_tree(
        asset_path: str,
        max_states: int = 500,
    ) -> Dict[str, Any]:
        """Inspect a UE 5.6 StateTree schema, compile state, and bounded state hierarchy."""
        try:
            reject_mcp_dependency_reference("asset_path", asset_path)
            return send_state_tree_command(
                "state_tree_inspect_asset",
                {"asset_path": asset_path, "max_states": int(max_states)},
            )
        except Exception as exc:
            return state_tree_error(f"Error inspecting StateTree: {exc}", exc)

    @mcp.tool()
    def create_state_tree(
        name: str,
        package_path: str = "/Game/AI/StateTrees",
        schema_type: str = "component",
    ) -> Dict[str, Any]:
        """
        Create and save an idempotent UE 5.6 StateTree in one editor transaction.

        Args:
            name: New asset name
            package_path: Destination package directory under /Game
            schema_type: component or ai, both provided by UE 5.6 GameplayStateTree
        """
        try:
            reject_mcp_dependency_reference("name", name)
            reject_mcp_dependency_reference("package_path", package_path)
            return send_state_tree_command(
                "state_tree_create_asset",
                {"name": name, "package_path": package_path, "schema": schema_type},
            )
        except Exception as exc:
            return state_tree_error(f"Error creating StateTree: {exc}", exc)

    @mcp.tool()
    def add_state_tree_state(
        asset_path: str,
        state_name: str,
        parent_path: str = "Root",
        state_type: str = "state",
        selection_behavior: str = "children_in_order",
        dry_run: StrictBool = True,
    ) -> Dict[str, Any]:
        """Add a child state in one editor transaction; dry-run defaults on."""
        try:
            reject_mcp_dependency_reference("asset_path", asset_path)
            reject_mcp_dependency_reference("state_name", state_name)
            return send_state_tree_command(
                "state_tree_add_state",
                {
                    "asset_path": asset_path,
                    "state_name": state_name,
                    "parent_path": parent_path,
                    "state_type": state_type,
                    "selection_behavior": selection_behavior,
                    "dry_run": bool(dry_run),
                },
            )
        except Exception as exc:
            return state_tree_error(f"Error adding StateTree state: {exc}", exc)

    @mcp.tool()
    def compile_state_tree(asset_path: str, save: bool = True) -> Dict[str, Any]:
        """Validate and compile a StateTree in one UE 5.6 editor transaction."""
        try:
            reject_mcp_dependency_reference("asset_path", asset_path)
            return send_state_tree_command(
                "state_tree_compile_asset",
                {"asset_path": asset_path, "save": bool(save)},
            )
        except Exception as exc:
            return state_tree_error(f"Error compiling StateTree: {exc}", exc)

    enforce_exact_json_fields(mcp, "add_state_tree_state", {"dry_run"})
