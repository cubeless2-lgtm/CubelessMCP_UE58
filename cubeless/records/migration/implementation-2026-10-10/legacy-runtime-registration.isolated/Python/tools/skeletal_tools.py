"""Skeletal asset mutation tools for Unreal MCP.

First pass: bone tree read, socket CRUD, and bone translation retargeting
on USkeleton assets. skeleton_path accepts a Skeleton or SkeletalMesh
asset path (the mesh's skeleton is used).
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

from tools.dependency_guard import reject_mcp_dependency_reference
from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")


def register_skeletal_tools(mcp: FastMCP):
    """Register skeletal tools with the MCP server."""

    def send_skeletal_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
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

    def skeletal_exception_error(message: str, exc: Exception) -> Dict[str, Any]:
        return tool_error(
            message,
            error_code="skeletal_tool_failed",
            details={"exception_type": type(exc).__name__},
        )

    @mcp.tool()
    def list_skeleton_bones(
        ctx: Context,
        skeleton_path: str,
        bone_name_contains: str = "",
        max_bones: int = 500,
    ) -> Dict[str, Any]:
        """
        List skeleton bones with parent indices and translation retargeting modes.

        Args:
            skeleton_path: Skeleton or SkeletalMesh asset path
            bone_name_contains: Optional bone-name substring filter
            max_bones: Maximum bones to return (1..4096)
        """
        try:
            params = {
                "skeleton_path": skeleton_path,
                "bone_name_contains": bone_name_contains,
                "max_bones": max_bones,
            }
            return send_skeletal_command("list_skeleton_bones", params)
        except Exception as e:
            error_msg = f"Error listing skeleton bones: {e}"
            logger.error(error_msg)
            return skeletal_exception_error(error_msg, e)

    @mcp.tool()
    def list_skeletal_sockets(ctx: Context, skeleton_path: str) -> Dict[str, Any]:
        """
        List skeleton sockets with bone, location, rotation, and scale.

        Args:
            skeleton_path: Skeleton or SkeletalMesh asset path
        """
        try:
            return send_skeletal_command("list_skeletal_sockets", {"skeleton_path": skeleton_path})
        except Exception as e:
            error_msg = f"Error listing skeletal sockets: {e}"
            logger.error(error_msg)
            return skeletal_exception_error(error_msg, e)

    @mcp.tool()
    def add_skeletal_socket(
        ctx: Context,
        skeleton_path: str,
        socket_name: str,
        bone_name: str,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Add a socket to a skeleton (fails if the socket name already exists).

        Args:
            skeleton_path: Skeleton or SkeletalMesh asset path
            socket_name: New socket name
            bone_name: Parent bone name (must exist in the skeleton)
            location: Relative location [x, y, z] (default 0,0,0)
            rotation: Relative rotation [roll, pitch, yaw] degrees (default 0,0,0)
            scale: Relative scale [x, y, z] (default 1,1,1)
            save: Save the skeleton asset afterwards
        """
        try:
            reject_mcp_dependency_reference("socket_name", socket_name)
            params: Dict[str, Any] = {
                "skeleton_path": skeleton_path,
                "socket_name": socket_name,
                "bone_name": bone_name,
                "save": save,
            }
            if location is not None:
                params["location"] = location
            if rotation is not None:
                params["rotation"] = rotation
            if scale is not None:
                params["scale"] = scale
            return send_skeletal_command("add_skeletal_socket", params)
        except Exception as e:
            error_msg = f"Error adding skeletal socket: {e}"
            logger.error(error_msg)
            return skeletal_exception_error(error_msg, e)

    @mcp.tool()
    def set_skeletal_socket(
        ctx: Context,
        skeleton_path: str,
        socket_name: str,
        bone_name: str = "",
        new_socket_name: str = "",
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Update an existing socket's bone, name, and/or transform (absent fields keep current values).

        Args:
            skeleton_path: Skeleton or SkeletalMesh asset path
            socket_name: Existing socket name
            bone_name: Optional new parent bone
            new_socket_name: Optional rename
            location: Optional relative location [x, y, z]
            rotation: Optional relative rotation [roll, pitch, yaw] degrees
            scale: Optional relative scale [x, y, z]
            save: Save the skeleton asset afterwards
        """
        try:
            if new_socket_name:
                reject_mcp_dependency_reference("new_socket_name", new_socket_name)
            params: Dict[str, Any] = {
                "skeleton_path": skeleton_path,
                "socket_name": socket_name,
                "save": save,
            }
            if bone_name:
                params["bone_name"] = bone_name
            if new_socket_name:
                params["new_socket_name"] = new_socket_name
            if location is not None:
                params["location"] = location
            if rotation is not None:
                params["rotation"] = rotation
            if scale is not None:
                params["scale"] = scale
            return send_skeletal_command("set_skeletal_socket", params)
        except Exception as e:
            error_msg = f"Error setting skeletal socket: {e}"
            logger.error(error_msg)
            return skeletal_exception_error(error_msg, e)

    @mcp.tool()
    def remove_skeletal_socket(
        ctx: Context,
        skeleton_path: str,
        socket_name: str,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Remove a socket from a skeleton.

        Args:
            skeleton_path: Skeleton or SkeletalMesh asset path
            socket_name: Socket name to remove
            save: Save the skeleton asset afterwards
        """
        try:
            params = {
                "skeleton_path": skeleton_path,
                "socket_name": socket_name,
                "save": save,
            }
            return send_skeletal_command("remove_skeletal_socket", params)
        except Exception as e:
            error_msg = f"Error removing skeletal socket: {e}"
            logger.error(error_msg)
            return skeletal_exception_error(error_msg, e)

    @mcp.tool()
    def set_bone_translation_retargeting(
        ctx: Context,
        skeleton_path: str,
        bone_name: str,
        mode: str,
        recursive: bool = False,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Set a bone's translation retargeting mode (optionally for all children).

        Args:
            skeleton_path: Skeleton or SkeletalMesh asset path
            bone_name: Bone to modify
            mode: Animation, Skeleton, AnimationScaled, AnimationRelative, or OrientAndScale
            recursive: Apply to all child bones too
            save: Save the skeleton asset afterwards
        """
        try:
            params = {
                "skeleton_path": skeleton_path,
                "bone_name": bone_name,
                "mode": mode,
                "recursive": recursive,
                "save": save,
            }
            return send_skeletal_command("set_bone_translation_retargeting", params)
        except Exception as e:
            error_msg = f"Error setting bone translation retargeting: {e}"
            logger.error(error_msg)
            return skeletal_exception_error(error_msg, e)

    @mcp.tool()
    def create_skeletal_physics_asset(
        ctx: Context,
        skeletal_mesh_path: str,
        target_physics_asset_path: str,
        expected_bones: Optional[List[str]] = None,
        kinematic_root_bone: str = "",
        root_kinematic: bool = True,
        disable_collision: bool = True,
        swing1_limit_degrees: float = 45.0,
        swing2_limit_degrees: float = 45.0,
        twist_limit_degrees: float = 25.0,
        linear_damping: float = 0.15,
        angular_damping: float = 0.5,
        tip_mass_scale: float = 3.0,
        overwrite_existing: bool = False,
        save: bool = True,
        dry_run: bool = True,
    ) -> Dict[str, Any]:
        """
        Create a persistent PhysicsAsset for every bone in a SkeletalMesh.

        The generated asset uses capsule bodies and parent-child constraints.
        One selected root body can be kinematic while all other bodies are
        simulated. Collision responses and every body-pair collision are
        disabled by default, preserving rigid-body inertia without requiring
        scene collision. The source SkeletalMesh is not assigned or modified;
        use the returned PhysicsAsset as a component override.

        Args:
            skeletal_mesh_path: Source SkeletalMesh asset path
            target_physics_asset_path: New /Game package path for the PhysicsAsset
            expected_bones: Optional exact reference-skeleton bone list/order guard
            kinematic_root_bone: Anchored body bone; defaults to skeleton root
            root_kinematic: Keep the selected root body kinematic
            disable_collision: Disable body responses, self-collision pairs, and joint collision
            swing1_limit_degrees: First swing angular limit
            swing2_limit_degrees: Second swing angular limit
            twist_limit_degrees: Twist angular limit
            linear_damping: Per-body linear damping
            angular_damping: Per-body angular damping
            tip_mass_scale: Mass scale for the final generated body
            overwrite_existing: Rebuild an existing PhysicsAsset in place
            save: Save the PhysicsAsset after verified creation
            dry_run: Validate topology and target policy without mutation; defaults true

        Returns:
            Generated body/constraint readback, save status, and source-mutation guard.
        """
        try:
            reject_mcp_dependency_reference("target_physics_asset_path", target_physics_asset_path)
            params: Dict[str, Any] = {
                "skeletal_mesh_path": skeletal_mesh_path,
                "target_physics_asset_path": target_physics_asset_path,
                "root_kinematic": root_kinematic,
                "disable_collision": disable_collision,
                "swing1_limit_degrees": swing1_limit_degrees,
                "swing2_limit_degrees": swing2_limit_degrees,
                "twist_limit_degrees": twist_limit_degrees,
                "linear_damping": linear_damping,
                "angular_damping": angular_damping,
                "tip_mass_scale": tip_mass_scale,
                "overwrite_existing": overwrite_existing,
                "save": save,
                "dry_run": dry_run,
            }
            if expected_bones is not None:
                params["expected_bones"] = expected_bones
            if kinematic_root_bone:
                params["kinematic_root_bone"] = kinematic_root_bone
            return send_skeletal_command("create_skeletal_physics_asset", params)
        except Exception as e:
            error_msg = f"Error creating skeletal PhysicsAsset: {e}"
            logger.error(error_msg)
            return skeletal_exception_error(error_msg, e)

    @mcp.tool()
    def create_skeletal_bone_constraint_fixture(
        ctx: Context,
        skeletal_mesh_path: str,
        actor_name: str = "MCP_SkeletalBoneConstraint_Ghost",
        bone_name1: str = "hand_l",
        bone_name2: str = "hand_r",
        world_context: str = "editor",
        location: Optional[List[float]] = None,
        constraint_component_name: str = "MCP_SkeletalBoneConstraint",
        swing1_limit_degrees: float = 45.0,
        swing2_limit_degrees: float = 45.0,
        twist_limit_degrees: float = 30.0,
        disable_collision: bool = True,
        simulate_all_bodies: bool = True,
        dry_run: bool = True,
    ) -> Dict[str, Any]:
        """
        Create a temporary native PhysicsConstraintComponent between two
        SkeletalMesh PhysicsAsset bodies, or validate the request when dry_run.

        world_context may be editor, pie, game, or game_preview. The command
        never saves the level or the SkeletalMesh/PhysicsAsset. Use
        inspect_skeletal_bone_constraint_fixture in editor or PIE/SIE to read
        back the actual constrained components, bone names, forces, and angular
        state.
        """
        try:
            reject_mcp_dependency_reference("skeletal_mesh_path", skeletal_mesh_path)
            reject_mcp_dependency_reference("actor_name", actor_name)
            reject_mcp_dependency_reference("bone_name1", bone_name1)
            reject_mcp_dependency_reference("bone_name2", bone_name2)
            reject_mcp_dependency_reference("constraint_component_name", constraint_component_name)
            if world_context not in {"editor", "pie", "game", "game_preview"}:
                return tool_error(
                    "world_context must be editor, pie, game, or game_preview",
                    error_code="skeletal_constraint_invalid_world_context",
                )
            if location is not None:
                if not isinstance(location, list) or len(location) != 3:
                    return tool_error(
                        "location must be a list of three numbers",
                        error_code="skeletal_constraint_invalid_location",
                    )
                location = [float(value) for value in location]
            params: Dict[str, Any] = {
                "skeletal_mesh_path": skeletal_mesh_path,
                "actor_name": actor_name,
                "bone_name1": bone_name1,
                "bone_name2": bone_name2,
                "world_context": world_context,
                "constraint_component_name": constraint_component_name,
                "swing1_limit_degrees": float(swing1_limit_degrees),
                "swing2_limit_degrees": float(swing2_limit_degrees),
                "twist_limit_degrees": float(twist_limit_degrees),
                "disable_collision": bool(disable_collision),
                "simulate_all_bodies": bool(simulate_all_bodies),
                "dry_run": bool(dry_run),
            }
            if location is not None:
                params["location"] = location
            return send_skeletal_command("create_skeletal_bone_constraint_fixture", params)
        except Exception as e:
            error_msg = f"Error creating skeletal bone constraint fixture: {e}"
            logger.error(error_msg)
            return skeletal_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_skeletal_bone_constraint_fixture(
        ctx: Context,
        actor_name: str,
        world_context: str = "auto",
        constraint_component_name: str = "",
    ) -> Dict[str, Any]:
        """
        Read back a temporary SkeletalMesh bone constraint from editor or PIE/SIE.

        world_context may be auto, editor, pie, game, or game_preview. This is
        read-only and reports the native constrained components/bones, break
        state, current angular values, forces, and per-bone simulation state.
        """
        try:
            reject_mcp_dependency_reference("actor_name", actor_name)
            reject_mcp_dependency_reference("constraint_component_name", constraint_component_name)
            if world_context not in {"auto", "editor", "pie", "game", "game_preview"}:
                return tool_error(
                    "world_context must be auto, editor, pie, game, or game_preview",
                    error_code="skeletal_constraint_invalid_world_context",
                )
            return send_skeletal_command(
                "inspect_skeletal_bone_constraint_fixture",
                {
                    "actor_name": actor_name,
                    "world_context": world_context,
                    "constraint_component_name": constraint_component_name,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting skeletal bone constraint fixture: {e}"
            logger.error(error_msg)
            return skeletal_exception_error(error_msg, e)
