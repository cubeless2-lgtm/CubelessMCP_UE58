"""Landscape / Foliage tools for Unreal MCP.

First pass: landscape discovery/info/material assignment and foliage type
listing/instance placement in the current editor level. Sculpt/paint
editing is deliberately out of scope. Level mutations mark the level
dirty; saving the map is left to the user per map-safety policy.
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

from tools.dependency_guard import reject_mcp_dependency_reference
from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")


def register_landscape_tools(mcp: FastMCP):
    """Register Landscape/Foliage tools with the MCP server."""

    def send_landscape_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        from unreal_mcp_server import get_unreal_connection

        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return tool_error("Failed to connect to Unreal Engine", error_code="unreal_connection_failed")

        response = unreal.send_command(command_name, params)
        if not response:
            logger.error("No response from Unreal Engine")
            return tool_error("No response from Unreal Engine", error_code="unreal_no_response")

        return response

    def landscape_exception_error(message: str, exc: Exception) -> Dict[str, Any]:
        return tool_error(
            message,
            error_code="landscape_tool_failed",
            details={"exception_type": type(exc).__name__},
        )

    @mcp.tool()
    def list_landscapes(ctx: Context) -> Dict[str, Any]:
        """List landscape actors in the current editor level with component counts and materials."""
        try:
            return send_landscape_command("list_landscapes", {})
        except Exception as e:
            error_msg = f"Error listing landscapes: {e}"
            logger.error(error_msg)
            return landscape_exception_error(error_msg, e)

    @mcp.tool()
    def get_landscape_info(ctx: Context, actor_label: str = "") -> Dict[str, Any]:
        """
        Read one landscape's material, components, scale, and target layer info objects.

        Args:
            actor_label: Landscape actor label; empty = the level's first landscape
        """
        try:
            return send_landscape_command("get_landscape_info", {"actor_label": actor_label})
        except Exception as e:
            error_msg = f"Error reading landscape info: {e}"
            logger.error(error_msg)
            return landscape_exception_error(error_msg, e)

    @mcp.tool()
    def create_landscape(
        ctx: Context,
        actor_label: str = "MCP_Landscape",
        material_path: str = "",
        location: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        component_count_x: int = 1,
        component_count_y: int = 1,
        sections_per_component: int = 1,
        quads_per_section: int = 7,
        height_value: int = 32768,
        allow_non_temp_level: bool = False,
        required_root: str = "/Game/_MCP_Temp",
    ) -> Dict[str, Any]:
        """
        Create a small flat Landscape in the current editor level.

        By default this only runs when the currently loaded level is under
        /Game/_MCP_Temp, matching map-safety policy for mutating map content.

        Args:
            actor_label: Label for the new Landscape actor
            material_path: Optional Material or Material Instance path
            location: Optional [x, y, z] actor location
            scale: Optional [x, y, z] actor scale, default [100, 100, 100]
            component_count_x: Landscape component count in X, clamped by bridge
            component_count_y: Landscape component count in Y, clamped by bridge
            sections_per_component: 1 or 2
            quads_per_section: Section quad count; must be one of 7, 15, 31, 63, 127, or 255
            height_value: Flat uint16 height value, default mid-height 32768
            allow_non_temp_level: Explicitly allow creation outside required_root
            required_root: Required package root for current level when guard is active
        """
        try:
            reject_mcp_dependency_reference("actor_label", actor_label)
            if material_path:
                reject_mcp_dependency_reference("material_path", material_path)
            params = {
                "actor_label": actor_label,
                "material_path": material_path,
                "location": location or [0.0, 0.0, 0.0],
                "scale": scale or [100.0, 100.0, 100.0],
                "component_count_x": int(component_count_x),
                "component_count_y": int(component_count_y),
                "sections_per_component": int(sections_per_component),
                "quads_per_section": int(quads_per_section),
                "height_value": int(height_value),
                "allow_non_temp_level": bool(allow_non_temp_level),
                "required_root": required_root,
            }
            return send_landscape_command("create_landscape", params)
        except Exception as e:
            error_msg = f"Error creating landscape: {e}"
            logger.error(error_msg)
            return landscape_exception_error(error_msg, e)

    @mcp.tool()
    def set_landscape_material(
        ctx: Context,
        material_path: str,
        actor_label: str = "",
    ) -> Dict[str, Any]:
        """
        Assign a landscape material (triggers landscape material update; level becomes dirty).

        Args:
            material_path: Material or Material Instance path
            actor_label: Landscape actor label; empty = the level's first landscape
        """
        try:
            reject_mcp_dependency_reference("material_path", material_path)
            params = {
                "material_path": material_path,
                "actor_label": actor_label,
            }
            return send_landscape_command("set_landscape_material", params)
        except Exception as e:
            error_msg = f"Error setting landscape material: {e}"
            logger.error(error_msg)
            return landscape_exception_error(error_msg, e)

    @mcp.tool()
    def list_foliage_types(ctx: Context) -> Dict[str, Any]:
        """List foliage types and instance counts registered in the current level."""
        try:
            return send_landscape_command("list_foliage_types", {})
        except Exception as e:
            error_msg = f"Error listing foliage types: {e}"
            logger.error(error_msg)
            return landscape_exception_error(error_msg, e)

    @mcp.tool()
    def add_foliage_instances(
        ctx: Context,
        foliage_type_path: str,
        transforms: List[Dict[str, Any]],
    ) -> Dict[str, Any]:
        """
        Place foliage instances of a FoliageType asset in the current level.

        Each transform: {"location": [x,y,z], "rotation": [roll,pitch,yaw], "scale": [x,y,z]}
        (rotation/scale optional). Max 5000 instances per call; the level becomes dirty.

        Args:
            foliage_type_path: FoliageType asset path (e.g. FoliageType_InstancedStaticMesh)
            transforms: Instance transform list (see above)
        """
        try:
            reject_mcp_dependency_reference("foliage_type_path", foliage_type_path)
            if not transforms:
                return tool_error("transforms must be a non-empty list", error_code="landscape_invalid_transforms")
            params = {
                "foliage_type_path": foliage_type_path,
                "transforms": transforms,
            }
            return send_landscape_command("add_foliage_instances", params)
        except Exception as e:
            error_msg = f"Error adding foliage instances: {e}"
            logger.error(error_msg)
            return landscape_exception_error(error_msg, e)

    @mcp.tool()
    def remove_foliage_instances(
        ctx: Context,
        foliage_type_path: str,
        instance_indices: Optional[List[int]] = None,
        remove_all: bool = False,
        max_remove_count: int = 5000,
    ) -> Dict[str, Any]:
        """
        Remove foliage instances for one FoliageType in the current level.

        By default this requires explicit instance indices. Set remove_all=True
        to clear every instance registered for that FoliageType. The level
        becomes dirty when instances are removed.

        Args:
            foliage_type_path: FoliageType asset path
            instance_indices: Optional zero-based instance indices to remove
            remove_all: Explicitly clear all instances for this FoliageType
            max_remove_count: Safety cap for one removal call
        """
        try:
            reject_mcp_dependency_reference("foliage_type_path", foliage_type_path)
            if not remove_all and not instance_indices:
                return tool_error(
                    "instance_indices must be provided unless remove_all=true",
                    error_code="landscape_invalid_instance_indices",
                )
            params = {
                "foliage_type_path": foliage_type_path,
                "instance_indices": [int(index) for index in (instance_indices or [])],
                "remove_all": bool(remove_all),
                "max_remove_count": int(max_remove_count),
            }
            return send_landscape_command("remove_foliage_instances", params)
        except Exception as e:
            error_msg = f"Error removing foliage instances: {e}"
            logger.error(error_msg)
            return landscape_exception_error(error_msg, e)
