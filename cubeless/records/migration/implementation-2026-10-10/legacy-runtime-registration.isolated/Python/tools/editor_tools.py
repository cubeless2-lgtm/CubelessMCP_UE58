"""
Editor Tools for Unreal MCP.

This module provides tools for controlling the Unreal Editor viewport and other editor functionality.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

from tools.dependency_guard import reject_mcp_dependency_reference
from tools.mutation_result_contract import composite_failure_safety_fields
from tools.response_schema import tool_error

# Get logger
logger = logging.getLogger("UnrealMCP")


def _unreal_connection_error(**fields: Any) -> Dict[str, Any]:
    return tool_error(
        "Failed to connect to Unreal Engine",
        error_code="unreal_connection_failed",
        **fields,
    )


def _unreal_no_response_error(**fields: Any) -> Dict[str, Any]:
    return tool_error(
        "No response from Unreal Engine",
        error_code="unreal_no_response",
        **fields,
    )


def _editor_exception_error(message: str, exc: Exception, **fields: Any) -> Dict[str, Any]:
    return tool_error(
        message,
        error_code="editor_tool_failed",
        details={"exception_type": type(exc).__name__},
        **fields,
    )


def register_editor_tools(mcp: FastMCP):
    """Register editor tools with the MCP server."""
    
    @mcp.tool()
    def get_actors_in_level(ctx: Context) -> Dict[str, Any]:
        """Get all actors in the current level as structured JSON with actor_count and actors."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning("Failed to connect to Unreal Engine")
                return _unreal_connection_error(actor_count=0, actors=[])
                
            response = unreal.send_command("get_actors_in_level", {})
            
            if not response:
                logger.warning("No response from Unreal Engine")
                return _unreal_no_response_error(actor_count=0, actors=[])
                
            # Log the complete response for debugging
            logger.info(f"Complete response from Unreal: {response}")
            
            # Check response format
            if "result" in response and "actors" in response["result"]:
                actors = response["result"]["actors"]
                logger.info(f"Found {len(actors)} actors in level")
                return {"success": True, "actor_count": len(actors), "actors": actors, "unreal_response": response}
            elif "actors" in response:
                actors = response["actors"]
                logger.info(f"Found {len(actors)} actors in level")
                return {"success": True, "actor_count": len(actors), "actors": actors, "unreal_response": response}
                
            logger.warning(f"Unexpected response format: {response}")
            return tool_error(
                "Unexpected response format from Unreal Engine",
                error_code="editor_unexpected_actor_response",
                actor_count=0,
                actors=[],
                unreal_response=response,
            )
            
        except Exception as e:
            logger.error(f"Error getting actors: {e}")
            return _editor_exception_error(str(e), e, actor_count=0, actors=[])

    @mcp.tool()
    def find_actors_by_name(ctx: Context, pattern: str) -> Dict[str, Any]:
        """
        Find actors by name pattern and return structured actor data.

        Args:
            pattern: Substring to match against actor object names.
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning("Failed to connect to Unreal Engine")
                return _unreal_connection_error(actor_count=0, actors=[])
                
            response = unreal.send_command("find_actors_by_name", {
                "pattern": pattern
            })
            
            if not response:
                return _unreal_no_response_error(actor_count=0, actors=[])

            response_result = response.get("result")
            if (
                response.get("status") == "error"
                or response.get("success") is False
                or (isinstance(response_result, dict) and response_result.get("success") is False)
            ):
                return tool_error(
                    str(response.get("error") or response.get("message") or "Actor search failed"),
                    error_code="editor_find_actors_failed",
                    actor_count=0,
                    actors=[],
                    **composite_failure_safety_fields(
                        response,
                        failed_stage="find_actors_by_name",
                        mutation_relevant=False,
                    ),
                )

            if "result" in response and "actors" in response["result"]:
                actors = response["result"]["actors"]
            else:
                actors = response.get("actors", [])

            return {"success": True, "actor_count": len(actors), "actors": actors, "unreal_response": response}
            
        except Exception as e:
            logger.error(f"Error finding actors: {e}")
            return _editor_exception_error(str(e), e, actor_count=0, actors=[])
    
    @mcp.tool()
    def spawn_actor(
        ctx: Context,
        name: str,
        type: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """Create a new actor in the current level.
        
        Args:
            ctx: The MCP context
            name: The name to give the new actor (must be unique)
            type: The type of actor to create (e.g. StaticMeshActor, PointLight)
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            
        Returns:
            Dict containing the created actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            # Ensure all parameters are properly formatted
            params = {
                "name": name,
                "type": type.upper(),  # Make sure type is uppercase
                "location": location,
                "rotation": rotation
            }
            
            # Validate location and rotation formats
            for param_name in ["location", "rotation"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return tool_error(f"Invalid {param_name} format. Must be a list of 3 float values.", error_code="editor_invalid_vector_parameter")
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            logger.info(f"Creating actor '{name}' of type '{type}' with params: {params}")
            response = unreal.send_command("spawn_actor", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            # Log the complete response for debugging
            logger.info(f"Actor creation response: {response}")
            
            # Handle error responses correctly
            if response.get("status") == "error":
                error_message = response.get("error", "Unknown error")
                logger.error(f"Error creating actor: {error_message}")
                return tool_error(
                    error_message,
                    error_code="editor_command_failed",
                    unreal_response=response,
                )
            
            return response
            
        except Exception as e:
            error_msg = f"Error creating actor: {e}"
            logger.error(error_msg)
            return _editor_exception_error(error_msg, e)
    
    @mcp.tool()
    def delete_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Delete an actor by name."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
                
            response = unreal.send_command("delete_actor", {
                "name": name
            })
            return response or _unreal_no_response_error()
            
        except Exception as e:
            logger.error(f"Error deleting actor: {e}")
            return {}
    
    @mcp.tool()
    def set_actor_transform(
        ctx: Context,
        name: str,
        location: List[float]  = None,
        rotation: List[float]  = None,
        scale: List[float] = None
    ) -> Dict[str, Any]:
        """Set the transform of an actor."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
                
            params = {"name": name}
            if location is not None:
                params["location"] = location
            if rotation is not None:
                params["rotation"] = rotation
            if scale is not None:
                params["scale"] = scale
                
            response = unreal.send_command("set_actor_transform", params)
            return response or _unreal_no_response_error()
            
        except Exception as e:
            logger.error(f"Error setting transform: {e}")
            return {}
    
    @mcp.tool()
    def get_actor_properties(ctx: Context, name: str) -> Dict[str, Any]:
        """Get all properties of an actor."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
                
            response = unreal.send_command("get_actor_properties", {
                "name": name
            })
            return response or _unreal_no_response_error()
            
        except Exception as e:
            logger.error(f"Error getting properties: {e}")
            return {}

    @mcp.tool()
    def line_trace_physical_material(
        ctx: Context,
        start: List[float],
        end: List[float],
        trace_channel: str = "visibility",
        trace_complex: bool = False,
        return_physical_material: bool = True,
    ) -> Dict[str, Any]:
        """
        Run a native Unreal line trace and return hit physical-material data.

        Args:
            start: Trace start as [X, Y, Z].
            end: Trace end as [X, Y, Z].
            trace_channel: Channel alias such as visibility, camera, world_static, or world_dynamic.
            trace_complex: Whether to trace complex collision.
            return_physical_material: Whether to request FHitResult.PhysMaterial.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            for param_name, value in (("start", start), ("end", end)):
                if not isinstance(value, list) or len(value) != 3:
                    return tool_error(
                        f"Invalid {param_name} format. Must be a list of 3 float values.",
                        error_code="editor_invalid_vector_parameter",
                    )

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params = {
                "start": [float(v) for v in start],
                "end": [float(v) for v in end],
                "trace_channel": trace_channel,
                "trace_complex": bool(trace_complex),
                "return_physical_material": bool(return_physical_material),
            }
            response = unreal.send_command("line_trace_physical_material", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error tracing physical material: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def inject_pie_input_key(
        ctx: Context,
        key: str,
        event: str,
        pie_instance: int = -1,
    ) -> Dict[str, Any]:
        """Inject one simulated digital key event into a local begun-play PIE controller.

        ``event`` must be ``pressed`` or ``released``. Press and release are
        intentionally separate calls so runtime QA can observe the gameplay
        result between them. The command rejects SIE and ambiguous local PIE
        instances; pass ``pie_instance`` only when multiple local PIE worlds exist.
        """
        from unreal_mcp_server import get_unreal_connection

        normalized_key = key.strip()
        normalized_event = event.strip().lower()
        if not normalized_key:
            return tool_error(
                "key must be non-empty",
                error_code="editor_input_key_required",
            )
        if normalized_event not in {"pressed", "released"}:
            return tool_error(
                "event must be 'pressed' or 'released'",
                error_code="editor_input_event_invalid",
            )

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()
            params: Dict[str, Any] = {
                "key": normalized_key,
                "event": normalized_event,
            }
            if pie_instance >= 0:
                params["pie_instance"] = int(pie_instance)
            response = unreal.send_command("inject_pie_input_key", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            logger.error(f"Error injecting PIE input key: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def set_actor_property(
        ctx: Context,
        name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """
        Set a property on an actor.
        
        Args:
            name: Name of the actor
            property_name: Name of the property to set
            property_value: Value to set the property to
            
        Returns:
            Dict containing response from Unreal with operation status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
                
            response = unreal.send_command("set_actor_property", {
                "name": name,
                "property_name": property_name,
                "property_value": property_value
            })
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Set actor property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting actor property: {e}"
            logger.error(error_msg)
            return _editor_exception_error(error_msg, e)

    @mcp.tool()
    def focus_viewport(
        ctx: Context,
        target: Optional[str] = None,
        location: Optional[List[float]] = None,
        distance: float = 1000.0,
        orientation: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """
        Focus the viewport on a specific actor or location.
        
        Args:
            target: Name of the actor to focus on (if provided, location is ignored)
            location: [X, Y, Z] coordinates to focus on (used if target is None)
            distance: Distance from the target/location
            orientation: Optional [Pitch, Yaw, Roll] for the viewport camera
            
        Returns:
            Response from Unreal Engine
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
                
            params = {}
            if target:
                params["target"] = target
            elif location:
                params["location"] = location
            
            if distance:
                params["distance"] = distance
                
            if orientation:
                params["orientation"] = orientation
                
            response = unreal.send_command("focus_viewport", params)
            return response or _unreal_no_response_error()
            
        except Exception as e:
            logger.error(f"Error focusing viewport: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def take_screenshot(ctx: Context, filepath: str) -> Dict[str, Any]:
        """
        Capture the active editor viewport to a PNG file.

        Args:
            filepath: Output PNG path. The native bridge appends .png when omitted.

        Returns:
            Response from Unreal Engine with the saved filepath or an error.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            response = unreal.send_command("take_screenshot", {"filepath": filepath})
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error taking screenshot: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def list_viewport_bookmarks(ctx: Context) -> Dict[str, Any]:
        """
        List bookmark slots available to the active editor viewport.

        Returns:
            Dict with max_bookmark_count, per-slot existence, existing_indices,
            and the current viewport transform.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            response = unreal.send_command("list_viewport_bookmarks", {})
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error listing viewport bookmarks: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def set_active_viewport_realtime(
        ctx: Context,
        realtime: bool = True,
        realtime_frame_count: int = 300,
    ) -> Dict[str, Any]:
        """
        Set the active editor viewport realtime state for visual review.

        Args:
            realtime: Enable or remove the review realtime override.
            realtime_frame_count: Extra realtime frames to request after enabling realtime.

        Returns:
            Dict with previous/current realtime state and the active viewport transform.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            response = unreal.send_command("set_active_viewport_realtime", {
                "realtime": bool(realtime),
                "realtime_frame_count": max(0, int(realtime_frame_count)),
            })
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error setting active viewport realtime: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def capture_viewport_bookmark_screenshot(
        ctx: Context,
        filepath: str,
        bookmark_index: int = -1,
        redraw_count: int = 2,
        clean_game_view: bool = False,
        view_location: Optional[List[float]] = None,
        view_rotation: Optional[List[float]] = None,
        view_fov: Optional[float] = None,
        restore_view_after_capture: bool = True,
    ) -> Dict[str, Any]:
        """
        Capture the active editor viewport, optionally after jumping to a bookmark.

        Args:
            filepath: PNG output path.
            bookmark_index: Bookmark slot to jump to before capture. Use -1 for active viewport.
            redraw_count: Number of forced viewport draws before pixel readback, clamped by the bridge.
            clean_game_view: Temporarily enable game view and hide editor overlays for the capture.
            view_location: Optional temporary [X, Y, Z] viewport camera location for this capture.
            view_rotation: Optional temporary [Pitch, Yaw, Roll] viewport camera rotation for this capture.
            view_fov: Optional temporary horizontal viewport FOV in degrees for this capture.
            restore_view_after_capture: Restore the previous viewport camera after capture when an override is used.

        Returns:
            Dict containing screenshot path, size, file size, capture mode, and viewport transform.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "filepath": filepath,
                "redraw_count": redraw_count,
                "clean_game_view": clean_game_view,
                "restore_view_after_capture": restore_view_after_capture,
            }
            if bookmark_index >= 0:
                params["bookmark_index"] = bookmark_index
            if view_location is not None:
                if not isinstance(view_location, list) or len(view_location) != 3:
                    return tool_error("view_location must be a list of 3 numbers", error_code="editor_invalid_view_location")
                params["view_location"] = [float(value) for value in view_location]
            if view_rotation is not None:
                if not isinstance(view_rotation, list) or len(view_rotation) != 3:
                    return tool_error("view_rotation must be a list of 3 numbers", error_code="editor_invalid_view_rotation")
                params["view_rotation"] = [float(value) for value in view_rotation]
            if view_fov is not None:
                view_fov_value = float(view_fov)
                if view_fov_value <= 0.0 or view_fov_value >= 180.0:
                    return tool_error("view_fov must be greater than 0 and less than 180 degrees", error_code="editor_invalid_view_fov")
                params["view_fov"] = view_fov_value

            response = unreal.send_command("capture_viewport_bookmark_screenshot", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error capturing viewport screenshot: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def open_editor_level(
        ctx: Context,
        level_path: str,
        dry_run: bool = True,
        allow_dirty_packages: bool = False,
        load_as_template: bool = False,
        show_progress: bool = True,
        deferred_load_delay_seconds: float = 1.0,
    ) -> Dict[str, Any]:
        """
        Safely preflight or schedule opening an editor level through the native bridge.

        Args:
            level_path: Long package path, object path, or .umap filename.
            dry_run: If true, only validate the transition and report blockers.
            allow_dirty_packages: If false, block real transitions when dirty packages exist.
            load_as_template: Forwarded to FEditorFileUtils::LoadMap for real loads.
            show_progress: Forwarded to FEditorFileUtils::LoadMap for real loads.
            deferred_load_delay_seconds: Delay before the real load runs on a later editor tick.

        Returns:
            Dict with target path, current world, dirty package summary,
            can_load, blocked_reasons, load_scheduled, request_id, and loaded.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            response = unreal.send_command(
                "open_editor_level",
                {
                    "level_path": level_path,
                    "dry_run": dry_run,
                    "allow_dirty_packages": allow_dirty_packages,
                    "load_as_template": load_as_template,
                    "show_progress": show_progress,
                    "deferred_load_delay_seconds": deferred_load_delay_seconds,
                },
            )
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error opening editor level: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def get_editor_level_open_status(
        ctx: Context,
        request_id: str = "",
    ) -> Dict[str, Any]:
        """
        Poll the latest deferred open_editor_level request.

        Args:
            request_id: Optional request id returned by open_editor_level.

        Returns:
            Dict with pending/current-world state and last_open_status.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            response = unreal.send_command(
                "get_editor_level_open_status",
                {"request_id": request_id},
            )
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error polling editor level open status: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def clear_dirty_package_flags(
        ctx: Context,
        package_paths: Optional[List[str]] = None,
        package_path: str = "",
        clear_all_dirty: bool = False,
        dry_run: bool = True,
        allow_non_temp_path: bool = False,
        required_root: str = "/Game/_MCP_Temp",
    ) -> Dict[str, Any]:
        """
        Clear loaded package dirty flags through the native editor-only bridge.

        The default is a dry run and the default scope is /Game/_MCP_Temp.
        The native command rejects PIE/SIE and does not load, save, or reload packages.

        Args:
            package_paths: Long package paths, object paths, or package filenames.
            package_path: Convenience single-package alias.
            clear_all_dirty: If true, target all currently dirty packages.
            dry_run: If true, report what would be cleared without changing flags.
            allow_non_temp_path: If false, require targets under required_root.
            required_root: Required package root for safe temporary packages.

        Returns:
            Dict with target package decisions, blockers, dirty package delta, and cleared_count.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "package_paths": package_paths or [],
                "package_path": package_path,
                "clear_all_dirty": clear_all_dirty,
                "dry_run": dry_run,
                "allow_non_temp_path": allow_non_temp_path,
                "required_root": required_root,
            }

            response = unreal.send_command("clear_dirty_package_flags", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error clearing dirty package flags: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def safe_new_preview_map(
        ctx: Context,
        target_path: str = "",
        map_path: str = "",
        dry_run: bool = True,
        allow_dirty_packages: bool = False,
        overwrite_existing: bool = False,
        allow_non_temp_path: bool = False,
        required_root: str = "/Game/_MCP_Temp",
        is_partitioned_world: bool = False,
    ) -> Dict[str, Any]:
        """
        Safely preflight or create a new blank preview map through the native bridge.

        Args:
            target_path: Long package path, object path, or .umap filename for the new map.
            map_path: Backwards-compatible alias for target_path.
            dry_run: If true, only validate the creation request and report blockers.
            allow_dirty_packages: If false, block real creation when dirty packages exist.
            overwrite_existing: If false, block when the target map already exists.
            allow_non_temp_path: If false, require the target to be under required_root.
            required_root: Required package root for temporary preview maps.
            is_partitioned_world: Forwarded to GEditor->NewMap for real creation.

        Returns:
            Dict with target path, current world, dirty package summary,
            can_create, blocked_reasons, create_attempted, created, and saved.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            response = unreal.send_command(
                "safe_new_preview_map",
                {
                    "target_path": target_path or map_path,
                    "map_path": map_path or target_path,
                    "dry_run": dry_run,
                    "allow_dirty_packages": allow_dirty_packages,
                    "overwrite_existing": overwrite_existing,
                    "allow_non_temp_path": allow_non_temp_path,
                    "required_root": required_root,
                    "is_partitioned_world": is_partitioned_world,
                },
            )
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error creating preview map: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def spawn_blueprint_actor(
        ctx: Context,
        blueprint_name: str,
        actor_name: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0],
        scale: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """Spawn an actor from a Blueprint.
        
        Args:
            ctx: The MCP context
            blueprint_name: Blueprint short name, package path, object path, or generated class path
            actor_name: Name to give the spawned actor
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            scale: Optional [x, y, z] actor scale
            
        Returns:
            Dict containing the spawned actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            # Ensure all parameters are properly formatted
            params = {
                "blueprint_name": blueprint_name,
                "actor_name": actor_name,
                "location": location or [0.0, 0.0, 0.0],
                "rotation": rotation or [0.0, 0.0, 0.0]
            }
            if scale is not None:
                params["scale"] = scale
            
            # Validate location and rotation formats
            for param_name in ["location", "rotation", "scale"]:
                if param_name not in params:
                    continue
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return tool_error(f"Invalid {param_name} format. Must be a list of 3 float values.", error_code="editor_invalid_vector_parameter")
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            logger.info(f"Spawning blueprint actor with params: {params}")
            response = unreal.send_command("spawn_blueprint_actor", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Spawn blueprint actor response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error spawning blueprint actor: {e}"
            logger.error(error_msg)
            return _editor_exception_error(error_msg, e)

    @mcp.tool()
    def open_niagara_preview_player(ctx: Context, system_path: str = "") -> Dict[str, Any]:
        """Open the level-independent Niagara Preview Player window.

        The current MVP is a Slate drop surface. It accepts Content Browser
        assets and World Outliner actors, then exposes the latest drop through
        get_niagara_preview_player_state().

        Args:
            system_path: Optional Niagara System path to load into the player after opening.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params = {}
            if system_path:
                params["system_path"] = system_path
            response = unreal.send_command("open_niagara_preview_player", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error opening Niagara Preview Player: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def get_niagara_preview_player_state(ctx: Context) -> Dict[str, Any]:
        """Get Niagara Preview Player window/drop state."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            response = unreal.send_command("get_niagara_preview_player_state", {})
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error getting Niagara Preview Player state: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def get_niagara_preview_lab_state(ctx: Context) -> Dict[str, Any]:
        """Get the current Niagara Preview Lab map, dirty, safety, and preview actor state."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            response = unreal.send_command("get_niagara_preview_lab_state", {})
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error getting Niagara Preview Lab state: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def cleanup_niagara_preview_lab(ctx: Context) -> Dict[str, Any]:
        """Delete Niagara Preview Lab preview actors without saving or reloading the map."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            response = unreal.send_command("cleanup_niagara_preview_lab", {})
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error cleaning Niagara Preview Lab: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def capture_niagara_preview_lab_view(
        ctx: Context,
        filepath: str,
        view: int = 1,
        auto_frame: bool = True,
        camera_location: Optional[List[float]] = None,
        camera_rotation: Optional[List[float]] = None,
        camera_fov: Optional[float] = None,
        allow_current_map_preview: bool = False,
    ) -> Dict[str, Any]:
        """Capture a clean Niagara Preview Lab screenshot.

        When Preview Lab actors exist, Unreal auto-frames those temporary actors.
        The view number is a near/mid/far distance hint and a fallback when no
        preview actor exists.

        Args:
            filepath: PNG output path. Relative paths are resolved under Saved/MCP/NiagaraReviews.
            view: Preview Lab view number. Use 1 first; use 2 or 3 only when the auto-framed effect is too large, clipped, or not reviewable.
            auto_frame: Auto-frame temporary preview actors when present. Set false for manual camera captures.
            camera_location: Optional [x, y, z] capture camera location override.
            camera_rotation: Optional [pitch, yaw, roll] capture camera rotation override.
            camera_fov: Optional capture camera FOV override.
            allow_current_map_preview: Allow preview capture in the current editor map when the dedicated Preview Lab map is unavailable.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "filepath": filepath,
                "view": int(view),
                "auto_frame": bool(auto_frame),
                "allow_current_map_preview": bool(allow_current_map_preview),
            }
            if camera_location is not None:
                params["camera_location"] = camera_location
            if camera_rotation is not None:
                params["camera_rotation"] = camera_rotation
            if camera_fov is not None:
                params["camera_fov"] = float(camera_fov)

            response = unreal.send_command("capture_niagara_preview_lab_view", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error capturing Niagara Preview Lab view: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def preview_niagara_system_in_preview_lab(
        ctx: Context,
        system_path: str,
        filepath: str,
        view: int = 1,
        label: str = "",
        warmup_time: float = 0.35,
        warmup_tick_delta: float = 1.0 / 30.0,
        cleanup_before: bool = True,
        cleanup_after: bool = True,
        location: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        auto_frame: bool = True,
        camera_location: Optional[List[float]] = None,
        camera_rotation: Optional[List[float]] = None,
        camera_fov: Optional[float] = None,
        user_parameter_overrides: Optional[Dict[str, Any]] = None,
        preserve_override_parameters: bool = True,
        allow_current_map_preview: bool = False,
    ) -> Dict[str, Any]:
        """Run a one-call Niagara Preview Lab still preview.

        This optimized route loads a read-only Niagara system, deletes prior
        Preview Lab actors if requested, spawns one transient preview actor,
        applies optional component-only User parameter overrides, advances
        simulation for warmup, captures with auto framing, and optionally
        removes the actor afterward.

        Args:
            system_path: Niagara system object path or package path.
            filepath: PNG output path. Relative paths are resolved under Saved/MCP/NiagaraReviews.
            view: Near/mid/far distance hint, 1 to 3.
            label: Optional preview actor label suffix.
            warmup_time: Seconds to advance simulation before capture.
            warmup_tick_delta: Simulation tick size for warmup.
            cleanup_before: Delete existing Preview Lab actors before spawning.
            cleanup_after: Delete Preview Lab actors after capture.
            location: Optional [x, y, z] spawn location. Defaults to [0, 0, 120].
            scale: Optional [x, y, z] actor scale. Defaults to [1, 1, 1].
            auto_frame: Auto-frame temporary preview actors when present. Set false for manual camera captures.
            camera_location: Optional [x, y, z] capture camera location override.
            camera_rotation: Optional [pitch, yaw, roll] capture camera rotation override.
            camera_fov: Optional capture camera FOV override.
            user_parameter_overrides: Optional transient component overrides, e.g. {"Amount": 90000.0, "Custom Camera Position": [0, -3200, 650]}.
            preserve_override_parameters: Preserve existing component override parameters when assigning the system. Keep true for Blueprint/component swap parity.
            allow_current_map_preview: Allow preview capture in the current editor map when the dedicated Preview Lab map is unavailable.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "system_path": system_path,
                "filepath": filepath,
                "view": int(view),
                "warmup_time": float(warmup_time),
                "warmup_tick_delta": float(warmup_tick_delta),
                "cleanup_before": bool(cleanup_before),
                "cleanup_after": bool(cleanup_after),
                "auto_frame": bool(auto_frame),
                "preserve_override_parameters": bool(preserve_override_parameters),
                "allow_current_map_preview": bool(allow_current_map_preview),
            }
            if label:
                params["label"] = label
            if location is not None:
                params["location"] = location
            if scale is not None:
                params["scale"] = scale
            if camera_location is not None:
                params["camera_location"] = camera_location
            if camera_rotation is not None:
                params["camera_rotation"] = camera_rotation
            if camera_fov is not None:
                params["camera_fov"] = float(camera_fov)
            if user_parameter_overrides:
                params["user_parameter_overrides"] = user_parameter_overrides

            response = unreal.send_command("preview_niagara_system_in_preview_lab", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error previewing Niagara system in Preview Lab: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def sample_niagara_system_in_preview_lab(
        ctx: Context,
        system_path: str,
        output_dir: str = "",
        label: str = "",
        warmup_times: Optional[List[float]] = None,
        views: Optional[List[int]] = None,
        warmup_tick_delta: float = 1.0 / 30.0,
        cleanup_before: bool = True,
        cleanup_after_each: bool = True,
        cleanup_after_all: bool = True,
        location: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        auto_frame: bool = True,
        camera_location: Optional[List[float]] = None,
        camera_rotation: Optional[List[float]] = None,
        camera_fov: Optional[float] = None,
        user_parameter_overrides: Optional[Dict[str, Any]] = None,
        preserve_override_parameters: bool = True,
        allow_current_map_preview: bool = False,
    ) -> Dict[str, Any]:
        """Capture multiple Niagara Preview Lab candidates in one MCP round trip.

        Use this when a Niagara effect is timing-sensitive or not visible in the
        first still. The Unreal-side command loops over warmup times and views,
        captures each candidate with auto framing, and returns sample metadata.

        Args:
            system_path: Niagara system object path or package path.
            output_dir: Relative output folder under Saved/MCP/NiagaraReviews.
            label: File and actor label stem.
            warmup_times: Seconds to advance simulation before each capture.
            views: Near/mid/far distance hints, values 1 to 3.
            warmup_tick_delta: Simulation tick size for warmup.
            cleanup_before: Delete existing Preview Lab actors before sampling.
            cleanup_after_each: Delete the sample actor after each capture.
            cleanup_after_all: Final cleanup pass after all samples.
            location: Optional [x, y, z] spawn location.
            scale: Optional [x, y, z] actor scale.
            auto_frame: Auto-frame temporary preview actors when present. Set false for manual camera captures.
            camera_location: Optional [x, y, z] capture camera location override.
            camera_rotation: Optional [pitch, yaw, roll] capture camera rotation override.
            camera_fov: Optional capture camera FOV override.
            user_parameter_overrides: Optional transient component overrides applied to each sample actor.
            preserve_override_parameters: Preserve existing component override parameters when assigning the system for each sample.
            allow_current_map_preview: Allow sampling in the current editor map when the dedicated Preview Lab map is unavailable.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "system_path": system_path,
                "warmup_tick_delta": float(warmup_tick_delta),
                "cleanup_before": bool(cleanup_before),
                "cleanup_after_each": bool(cleanup_after_each),
                "cleanup_after_all": bool(cleanup_after_all),
                "auto_frame": bool(auto_frame),
                "preserve_override_parameters": bool(preserve_override_parameters),
                "allow_current_map_preview": bool(allow_current_map_preview),
            }
            if output_dir:
                params["output_dir"] = output_dir
            if label:
                params["label"] = label
            if warmup_times is not None:
                params["warmup_times"] = warmup_times
            if views is not None:
                params["views"] = views
            if location is not None:
                params["location"] = location
            if scale is not None:
                params["scale"] = scale
            if camera_location is not None:
                params["camera_location"] = camera_location
            if camera_rotation is not None:
                params["camera_rotation"] = camera_rotation
            if camera_fov is not None:
                params["camera_fov"] = float(camera_fov)
            if user_parameter_overrides:
                params["user_parameter_overrides"] = user_parameter_overrides

            response = unreal.send_command("sample_niagara_system_in_preview_lab", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error sampling Niagara system in Preview Lab: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def inspect_niagara_component_runtime_state(
        ctx: Context,
        actor_name: str = "",
        actor_label: str = "",
        component_name: str = "",
        system_path: str = "",
        parameter_name_contains: str = "",
        max_components: int = 32,
        max_parameters: int = 256,
        include_parameter_values: bool = True,
        include_raw_bytes: bool = False,
        max_raw_bytes: int = 64,
        include_asset_exposed_parameters: bool = False,
        include_instance_parameters: bool = False,
        include_parameter_object_properties: bool = False,
        include_emitter_runtime: bool = True,
        max_emitter_instances: int = 128,
        max_object_properties: int = 80,
        max_array_elements: int = 16,
        max_property_value_chars: int = 512,
    ) -> Dict[str, Any]:
        """Inspect live Niagara state in PIE/SIE when active, otherwise the editor world.

        This is a read-only diagnostic route for comparing Blueprint-owned
        Niagara components after asset swaps. It serializes component metadata
        plus the component override parameter store, and optionally the live
        system-instance parameter store, without creating, mutating, loading,
        saving, reloading, compiling, or persisting content.

        Args:
            actor_name: Optional substring filter against actor name, label, or path.
            actor_label: Optional substring filter against actor label.
            component_name: Optional substring filter against component name or path.
            system_path: Optional substring filter against the assigned Niagara system path.
            parameter_name_contains: Optional substring filter for parameter names.
            max_components: Maximum matching components to return.
            max_parameters: Maximum parameters to return per store.
            include_parameter_values: Include decoded primitive/vector/color parameter values.
            include_raw_bytes: Include raw byte previews for unsupported parameter types.
            max_raw_bytes: Maximum bytes per unsupported parameter raw preview.
            include_asset_exposed_parameters: Also include assigned system exposed user parameter store.
            include_instance_parameters: Also include live FNiagaraSystemInstance parameter store when available.
            include_parameter_object_properties: Include limited UPROPERTY previews for UObject/DataInterface parameter values.
            include_emitter_runtime: Include live FNiagaraSystemInstance/FNiagaraEmitterInstance counters.
            max_emitter_instances: Maximum emitter runtime entries to return per component.
            max_object_properties: Maximum UObject/DataInterface properties to return per object.
            max_array_elements: Maximum array elements to sample per UObject/DataInterface property.
            max_property_value_chars: Maximum export-text characters per property value.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "max_components": int(max_components),
                "max_parameters": int(max_parameters),
                "include_parameter_values": bool(include_parameter_values),
                "include_raw_bytes": bool(include_raw_bytes),
                "max_raw_bytes": int(max_raw_bytes),
                "include_asset_exposed_parameters": bool(include_asset_exposed_parameters),
                "include_instance_parameters": bool(include_instance_parameters),
                "include_parameter_object_properties": bool(include_parameter_object_properties),
                "include_emitter_runtime": bool(include_emitter_runtime),
                "max_emitter_instances": int(max_emitter_instances),
                "max_object_properties": int(max_object_properties),
                "max_array_elements": int(max_array_elements),
                "max_property_value_chars": int(max_property_value_chars),
            }
            if actor_name:
                params["actor_name"] = actor_name
            if actor_label:
                params["actor_label"] = actor_label
            if component_name:
                params["component_name"] = component_name
            if system_path:
                params["system_path"] = system_path
            if parameter_name_contains:
                params["parameter_name_contains"] = parameter_name_contains

            response = unreal.send_command("inspect_niagara_component_runtime_state", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error inspecting Niagara component runtime state: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def inspect_niagara_component_parameter_store(
        ctx: Context,
        actor_name: str = "",
        actor_label: str = "",
        component_name: str = "",
        system_path: str = "",
        parameter_name_contains: str = "",
        max_components: int = 8,
        max_parameters: int = 64,
        include_raw_bytes: bool = False,
        max_raw_bytes: int = 64,
    ) -> Dict[str, Any]:
        """Compare live Niagara override and instance parameter stores.

        This is a narrow wrapper around inspect_niagara_component_runtime_state
        for runtime User-parameter propagation probes. It returns component
        OverrideParameters and FNiagaraSystemInstance InstanceParameters with
        decoded primitive/vector/color values when a live system instance exists.

        Args:
            actor_name: Optional substring filter against actor name, label, or path.
            actor_label: Optional substring filter against actor label.
            component_name: Optional substring filter against component name or path.
            system_path: Optional substring filter against the assigned Niagara system path.
            parameter_name_contains: Optional substring filter for parameter names, e.g. V4_SeedEnabled.
            max_components: Maximum matching components to return.
            max_parameters: Maximum parameters to return per store.
            include_raw_bytes: Include raw byte previews for unsupported value types.
            max_raw_bytes: Maximum bytes per unsupported parameter raw preview.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "max_components": int(max_components),
                "max_parameters": int(max_parameters),
                "include_raw_bytes": bool(include_raw_bytes),
                "max_raw_bytes": int(max_raw_bytes),
            }
            if actor_name:
                params["actor_name"] = actor_name
            if actor_label:
                params["actor_label"] = actor_label
            if component_name:
                params["component_name"] = component_name
            if system_path:
                params["system_path"] = system_path
            if parameter_name_contains:
                params["parameter_name_contains"] = parameter_name_contains

            response = unreal.send_command("inspect_niagara_component_parameter_store", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error inspecting Niagara component parameter stores: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def sample_render_target2d_region_stats(
        ctx: Context,
        render_target_path: str,
        x0: int = 0,
        x1: int = -1,
        y0: int = 0,
        y1: int = -1,
        channel: str = "max",
        threshold: float = 0.01,
        max_first_hits: int = 8,
        probe_x: int = -1,
        probe_y: int = -1,
        probe_radius: int = 8,
    ) -> Dict[str, Any]:
        """Read a TextureRenderTarget2D once and return region/channel statistics.

        This is an authoring-only readback probe for render-target smoke tests.
        It computes count, sum, max, threshold-hit bounds, centroid, optional
        same-read probe stats, and first-hit samples in C++ so tests do not
        need slow per-pixel Python loops.

        Args:
            render_target_path: RT package/object path.
            x0, x1, y0, y1: Inclusive pixel region. Negative x1/y1 uses the RT maximum bound.
            channel: r, g, b, a, luma, or max. Default max checks any channel.
            threshold: Strictly-greater-than threshold for nonzero classification.
            max_first_hits: Maximum hit samples returned for debugging.
            probe_x, probe_y: Optional probe center. Set both to >= 0 to return same-read probe stats.
            probe_radius: Inclusive square probe radius in pixels.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "render_target_path": render_target_path,
                "x0": int(x0),
                "y0": int(y0),
                "channel": channel,
                "threshold": float(threshold),
                "max_first_hits": int(max_first_hits),
            }
            if int(probe_x) >= 0 and int(probe_y) >= 0:
                params["probe_x"] = int(probe_x)
                params["probe_y"] = int(probe_y)
                params["probe_radius"] = int(probe_radius)
            if int(x1) >= 0:
                params["x1"] = int(x1)
            if int(y1) >= 0:
                params["y1"] = int(y1)
            response = unreal.send_command("sample_render_target2d_region_stats", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error sampling render target region stats: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def inspect_niagara_component_grid2d_runtime(
        ctx: Context,
        actor_name: str = "",
        actor_label: str = "",
        component_name: str = "",
        system_path: str = "",
        max_components: int = 16,
        max_grid_interfaces: int = 16,
        include_internal: bool = True,
    ) -> Dict[str, Any]:
        """Read live Niagara Grid2D runtime NumCells from matching components.

        This is a read-only runtime probe for Grid2D smoke tests. It scans live
        Niagara components, finds Grid2D Collection data interfaces, and returns
        both static asset fields and game-thread runtime instance data when the
        system instance exposes it.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "max_components": int(max_components),
                "max_grid_interfaces": int(max_grid_interfaces),
                "include_internal": bool(include_internal),
            }
            if actor_name:
                params["actor_name"] = actor_name
            if actor_label:
                params["actor_label"] = actor_label
            if component_name:
                params["component_name"] = component_name
            if system_path:
                params["system_path"] = system_path
            response = unreal.send_command("inspect_niagara_component_grid2d_runtime", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error inspecting Niagara Grid2D runtime: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def preview_niagara_component_asset_swap(
        ctx: Context,
        replacement_system_path: str,
        actor_name: str = "",
        actor_label: str = "",
        component_name: str = "",
        system_path: str = "",
        parameter_name_contains: str = "",
        restore_original_asset: bool = True,
        preserve_override_parameters: bool = True,
        activate_after_swap: bool = True,
        advance_time: float = 0.35,
        advance_tick_delta: float = 1.0 / 30.0,
        max_parameters: int = 256,
        max_emitter_instances: int = 128,
        include_parameter_values: bool = True,
        include_raw_bytes: bool = False,
        max_raw_bytes: int = 64,
        include_parameter_object_properties: bool = False,
        include_emitter_runtime: bool = True,
        max_object_properties: int = 80,
        max_array_elements: int = 16,
        max_property_value_chars: int = 512,
    ) -> Dict[str, Any]:
        """Temporarily preview a Niagara System asset swap on one live component.

        This is an authoring-only BP manager parity route. It finds exactly one
        live editor UNiagaraComponent by the supplied actor/component/system
        filters, assigns the replacement Niagara System with override
        preservation, samples runtime and override state, then restores the
        original asset and active state before returning.

        Args:
            replacement_system_path: Niagara system object path or package path to test on the component.
            actor_name: Optional substring filter against actor name, label, or path.
            actor_label: Optional substring filter against actor label.
            component_name: Optional substring filter against component name or path, e.g. RainFX.
            system_path: Optional substring filter against the currently assigned Niagara system path.
            parameter_name_contains: Optional substring filter for parameter names.
            restore_original_asset: Must remain true. Persistent component asset swaps are not admitted by this tool.
            preserve_override_parameters: Must remain true. False can reset Blueprint component override state.
            activate_after_swap: Activate and advance the component after the temporary swap for runtime readback.
            advance_time: Seconds to advance simulation after the temporary swap.
            advance_tick_delta: Simulation tick size for the advance.
            max_parameters: Maximum parameters to return per store.
            max_emitter_instances: Maximum emitter runtime entries to return per component snapshot.
            include_parameter_values: Include decoded primitive/vector/color parameter values.
            include_raw_bytes: Include raw byte previews for unsupported parameter types.
            max_raw_bytes: Maximum bytes per unsupported parameter raw preview.
            include_parameter_object_properties: Include limited UPROPERTY previews for UObject/DataInterface parameter values.
            include_emitter_runtime: Include live system/emitter counters for the after-swap snapshot.
            max_object_properties: Maximum UObject/DataInterface properties to return per object.
            max_array_elements: Maximum array elements to sample per UObject/DataInterface property.
            max_property_value_chars: Maximum export-text characters per property value.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "replacement_system_path": replacement_system_path,
                "restore_original_asset": bool(restore_original_asset),
                "preserve_override_parameters": bool(preserve_override_parameters),
                "activate_after_swap": bool(activate_after_swap),
                "advance_time": float(advance_time),
                "advance_tick_delta": float(advance_tick_delta),
                "max_parameters": int(max_parameters),
                "max_emitter_instances": int(max_emitter_instances),
                "include_parameter_values": bool(include_parameter_values),
                "include_raw_bytes": bool(include_raw_bytes),
                "max_raw_bytes": int(max_raw_bytes),
                "include_parameter_object_properties": bool(include_parameter_object_properties),
                "include_emitter_runtime": bool(include_emitter_runtime),
                "max_object_properties": int(max_object_properties),
                "max_array_elements": int(max_array_elements),
                "max_property_value_chars": int(max_property_value_chars),
            }
            if actor_name:
                params["actor_name"] = actor_name
            if actor_label:
                params["actor_label"] = actor_label
            if component_name:
                params["component_name"] = component_name
            if system_path:
                params["system_path"] = system_path
            if parameter_name_contains:
                params["parameter_name_contains"] = parameter_name_contains

            response = unreal.send_command("preview_niagara_component_asset_swap", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error previewing Niagara component asset swap: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def inspect_blueprint_niagara_component_template(
        ctx: Context,
        blueprint_path: str,
        component_name: str = "",
        expected_system_path: str = "",
        parameter_name_contains: str = "",
        include_inherited_scs_templates: bool = True,
        require_exactly_one: bool = False,
        require_expected_system_match: bool = False,
        max_components: int = 64,
        max_parameters: int = 256,
        include_parameter_values: bool = True,
        include_raw_bytes: bool = False,
        max_raw_bytes: int = 64,
        include_parameter_object_properties: bool = False,
        include_component_object_properties: bool = False,
        max_object_properties: int = 80,
        max_array_elements: int = 16,
        max_property_value_chars: int = 512,
    ) -> Dict[str, Any]:
        """Inspect Blueprint SCS Niagara component templates without writing assets.

        This read-only preflight loads a Blueprint asset, scans its own and
        optionally inherited SimpleConstructionScript component templates, and
        reports matching UNiagaraComponent asset assignment plus override
        parameter store data. It never saves, reloads, compiles, or mutates the
        Blueprint.

        Args:
            blueprint_path: Blueprint asset package or object path, e.g. /Game/MCPWeather/BP_MCPWeatherManager.
            component_name: Optional substring filter against SCS variable name, template name, path, or class.
            expected_system_path: Optional Niagara system package/object path to compare against the template assignment.
            parameter_name_contains: Optional substring filter for override parameter names.
            include_inherited_scs_templates: Include parent Blueprint SCS templates as resolved for the target generated class.
            require_exactly_one: Set preflight_pass=false unless exactly one template matches component_name.
            require_expected_system_match: Set preflight_pass=false unless all matching templates use expected_system_path.
            max_components: Maximum matching component templates to return.
            max_parameters: Maximum parameters to return per override store.
            include_parameter_values: Include decoded primitive/vector/color parameter values.
            include_raw_bytes: Include raw byte previews for unsupported parameter types.
            max_raw_bytes: Maximum bytes per unsupported parameter raw preview.
            include_parameter_object_properties: Include limited UPROPERTY previews for UObject/DataInterface parameter values.
            include_component_object_properties: Include limited reflected properties from the component template itself.
            max_object_properties: Maximum UObject/DataInterface or component properties to return.
            max_array_elements: Maximum array elements to sample per reflected property.
            max_property_value_chars: Maximum export-text characters per reflected property value.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "blueprint_path": blueprint_path,
                "include_inherited_scs_templates": bool(include_inherited_scs_templates),
                "require_exactly_one": bool(require_exactly_one),
                "require_expected_system_match": bool(require_expected_system_match),
                "max_components": int(max_components),
                "max_parameters": int(max_parameters),
                "include_parameter_values": bool(include_parameter_values),
                "include_raw_bytes": bool(include_raw_bytes),
                "max_raw_bytes": int(max_raw_bytes),
                "include_parameter_object_properties": bool(include_parameter_object_properties),
                "include_component_object_properties": bool(include_component_object_properties),
                "max_object_properties": int(max_object_properties),
                "max_array_elements": int(max_array_elements),
                "max_property_value_chars": int(max_property_value_chars),
            }
            if component_name:
                params["component_name"] = component_name
            if expected_system_path:
                params["expected_system_path"] = expected_system_path
            if parameter_name_contains:
                params["parameter_name_contains"] = parameter_name_contains

            response = unreal.send_command("inspect_blueprint_niagara_component_template", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error inspecting Blueprint Niagara component template: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def dry_run_blueprint_niagara_component_template_asset_swap(
        ctx: Context,
        blueprint_path: str,
        component_name: str,
        source_system_path: str,
        replacement_system_path: str,
        confirmation_token: str,
        operation: str = "durable_bp_component_template_asset_swap",
        dry_run: bool = True,
        preserve_override_parameters: bool = True,
        allow_unrealmcp_dependency: bool = False,
        parameter_name_contains: str = "",
        include_inherited_scs_templates: bool = True,
        require_exactly_one: bool = True,
        require_source_system_match: bool = True,
        max_components: int = 8,
        max_parameters: int = 256,
        include_parameter_values: bool = True,
        include_raw_bytes: bool = False,
        max_raw_bytes: int = 64,
        include_parameter_object_properties: bool = False,
        include_component_object_properties: bool = False,
        max_object_properties: int = 80,
        max_array_elements: int = 16,
        max_property_value_chars: int = 512,
    ) -> Dict[str, Any]:
        """Dry-run a reviewed Blueprint Niagara component-template asset swap.

        This guarded preflight loads the Blueprint plus source/replacement
        Niagara Systems, resolves the matching SCS UNiagaraComponent template,
        snapshots its current asset assignment and override parameter store, and
        reports the planned replacement. It does not call SetAsset, compile,
        save, reload, or persist any project content.

        Args:
            blueprint_path: Blueprint asset package or object path.
            component_name: Required substring filter for the SCS Niagara component template, e.g. RainFX.
            source_system_path: Niagara System currently expected on the component template.
            replacement_system_path: Niagara System that would be assigned by a future reviewed executor.
            confirmation_token: Must be ALLOW_DURABLE_BP_COMPONENT_TEMPLATE_MUTATION.
            operation: Reviewed operation name. Defaults to durable_bp_component_template_asset_swap.
            dry_run: Must remain true. False is rejected by the native command.
            preserve_override_parameters: Must remain true to preserve component template override state.
            allow_unrealmcp_dependency: Must remain false; finished assets must not depend on UnrealMCP.
            parameter_name_contains: Optional substring filter for override parameter names.
            include_inherited_scs_templates: Include parent Blueprint SCS templates as resolved for the target generated class.
            require_exactly_one: Require exactly one matching component template.
            require_source_system_match: Require matching templates to use source_system_path.
            max_components: Maximum matching component templates to return.
            max_parameters: Maximum parameters to return per override store.
            include_parameter_values: Include decoded primitive/vector/color parameter values.
            include_raw_bytes: Include raw byte previews for unsupported parameter types.
            max_raw_bytes: Maximum bytes per unsupported parameter raw preview.
            include_parameter_object_properties: Include limited UPROPERTY previews for UObject/DataInterface parameter values.
            include_component_object_properties: Include limited reflected properties from the component template itself.
            max_object_properties: Maximum UObject/DataInterface or component properties to return.
            max_array_elements: Maximum array elements to sample per reflected property.
            max_property_value_chars: Maximum export-text characters per reflected property value.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "blueprint_path": blueprint_path,
                "component_name": component_name,
                "source_system_path": source_system_path,
                "replacement_system_path": replacement_system_path,
                "confirmation_token": confirmation_token,
                "operation": operation,
                "dry_run": bool(dry_run),
                "preserve_override_parameters": bool(preserve_override_parameters),
                "allow_unrealmcp_dependency": bool(allow_unrealmcp_dependency),
                "include_inherited_scs_templates": bool(include_inherited_scs_templates),
                "require_exactly_one": bool(require_exactly_one),
                "require_source_system_match": bool(require_source_system_match),
                "max_components": int(max_components),
                "max_parameters": int(max_parameters),
                "include_parameter_values": bool(include_parameter_values),
                "include_raw_bytes": bool(include_raw_bytes),
                "max_raw_bytes": int(max_raw_bytes),
                "include_parameter_object_properties": bool(include_parameter_object_properties),
                "include_component_object_properties": bool(include_component_object_properties),
                "max_object_properties": int(max_object_properties),
                "max_array_elements": int(max_array_elements),
                "max_property_value_chars": int(max_property_value_chars),
            }
            if parameter_name_contains:
                params["parameter_name_contains"] = parameter_name_contains

            response = unreal.send_command("dry_run_blueprint_niagara_component_template_asset_swap", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error dry-running Blueprint Niagara component template asset swap: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def execute_blueprint_niagara_component_template_asset_swap_canary(
        ctx: Context,
        source_blueprint_path: str,
        target_blueprint_path: str,
        component_name: str,
        source_system_path: str,
        replacement_system_path: str,
        confirmation_token: str,
        operation: str = "temp_bp_component_template_asset_swap_canary",
        dry_run: bool = True,
        preserve_override_parameters: bool = True,
        allow_unrealmcp_dependency: bool = False,
        replace_existing_target: bool = False,
        allow_dirty_source_blueprint: bool = False,
        compile_blueprint: bool = False,
        save_blueprint: bool = False,
        parameter_name_contains: str = "",
        include_inherited_scs_templates: bool = False,
        require_exactly_one: bool = True,
        require_source_system_match: bool = True,
        max_components: int = 8,
        max_parameters: int = 256,
        include_parameter_values: bool = True,
        include_raw_bytes: bool = False,
        max_raw_bytes: int = 64,
        include_parameter_object_properties: bool = False,
        include_component_object_properties: bool = False,
        max_object_properties: int = 80,
        max_array_elements: int = 16,
        max_property_value_chars: int = 512,
    ) -> Dict[str, Any]:
        """Execute a temp-only Blueprint Niagara component-template asset-swap canary.

        This duplicates the source Blueprint to a required `/Game/_MCP_Temp`
        target, mutates only that temp target's local SCS `UNiagaraComponent`
        template with override preservation, and optionally compiles/saves the
        temp canary asset. It never writes the original Blueprint and does not
        admit durable project-content mutation.

        Args:
            source_blueprint_path: Source Blueprint package or object path.
            target_blueprint_path: Temp canary Blueprint path under `/Game/_MCP_Temp`.
            component_name: Required local SCS Niagara component filter, e.g. RainFX.
            source_system_path: Niagara System currently expected on the template.
            replacement_system_path: Niagara System to assign to the temp canary template.
            confirmation_token: Must be ALLOW_TEMP_BP_COMPONENT_TEMPLATE_MUTATION.
            operation: Must be temp_bp_component_template_asset_swap_canary.
            dry_run: True previews the canary; False duplicates and mutates only the temp target.
            preserve_override_parameters: Must remain true.
            allow_unrealmcp_dependency: Must remain false.
            replace_existing_target: Allow replacing an existing temp canary asset.
            allow_dirty_source_blueprint: Allow duplicating when the source package was already dirty.
            compile_blueprint: Compile the temp target after mutation.
            save_blueprint: Save the temp target after mutation.
            parameter_name_contains: Optional substring filter for override parameter names.
            include_inherited_scs_templates: Must remain false for mutation safety.
            require_exactly_one: Require exactly one matching component template.
            require_source_system_match: Require matching templates to use source_system_path.
            max_components: Maximum matching component templates to return.
            max_parameters: Maximum parameters to return per override store.
            include_parameter_values: Include decoded primitive/vector/color parameter values.
            include_raw_bytes: Include raw byte previews for unsupported parameter types.
            max_raw_bytes: Maximum bytes per unsupported parameter raw preview.
            include_parameter_object_properties: Include limited UPROPERTY previews for UObject/DataInterface parameter values.
            include_component_object_properties: Include limited reflected properties from the component template itself.
            max_object_properties: Maximum UObject/DataInterface or component properties to return.
            max_array_elements: Maximum array elements to sample per reflected property.
            max_property_value_chars: Maximum export-text characters per reflected property value.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "source_blueprint_path": source_blueprint_path,
                "target_blueprint_path": target_blueprint_path,
                "component_name": component_name,
                "source_system_path": source_system_path,
                "replacement_system_path": replacement_system_path,
                "confirmation_token": confirmation_token,
                "operation": operation,
                "dry_run": bool(dry_run),
                "preserve_override_parameters": bool(preserve_override_parameters),
                "allow_unrealmcp_dependency": bool(allow_unrealmcp_dependency),
                "replace_existing_target": bool(replace_existing_target),
                "allow_dirty_source_blueprint": bool(allow_dirty_source_blueprint),
                "compile_blueprint": bool(compile_blueprint),
                "save_blueprint": bool(save_blueprint),
                "include_inherited_scs_templates": bool(include_inherited_scs_templates),
                "require_exactly_one": bool(require_exactly_one),
                "require_source_system_match": bool(require_source_system_match),
                "max_components": int(max_components),
                "max_parameters": int(max_parameters),
                "include_parameter_values": bool(include_parameter_values),
                "include_raw_bytes": bool(include_raw_bytes),
                "max_raw_bytes": int(max_raw_bytes),
                "include_parameter_object_properties": bool(include_parameter_object_properties),
                "include_component_object_properties": bool(include_component_object_properties),
                "max_object_properties": int(max_object_properties),
                "max_array_elements": int(max_array_elements),
                "max_property_value_chars": int(max_property_value_chars),
            }
            if parameter_name_contains:
                params["parameter_name_contains"] = parameter_name_contains

            response = unreal.send_command(
                "execute_blueprint_niagara_component_template_asset_swap_canary",
                params,
            )
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error executing Blueprint Niagara component template canary asset swap: {e}")
            return _editor_exception_error(str(e), e)

    @mcp.tool()
    def execute_blueprint_niagara_component_template_asset_swap_durable(
        ctx: Context,
        blueprint_path: str,
        component_name: str,
        source_system_path: str,
        replacement_system_path: str,
        confirmation_token: str,
        operation: str = "durable_bp_component_template_asset_swap",
        dry_run: bool = True,
        preserve_override_parameters: bool = True,
        allow_unrealmcp_dependency: bool = False,
        allow_dirty_blueprint: bool = False,
        compile_blueprint: bool = False,
        save_blueprint: bool = False,
        parameter_name_contains: str = "",
        include_inherited_scs_templates: bool = False,
        require_exactly_one: bool = True,
        require_source_system_match: bool = True,
        max_components: int = 8,
        max_parameters: int = 256,
        include_parameter_values: bool = True,
        include_raw_bytes: bool = False,
        max_raw_bytes: int = 64,
        include_parameter_object_properties: bool = False,
        include_component_object_properties: bool = False,
        max_object_properties: int = 80,
        max_array_elements: int = 16,
        max_property_value_chars: int = 512,
    ) -> Dict[str, Any]:
        """Execute the guarded durable Blueprint Niagara component-template asset swap.

        The default is a dry-run preflight. Real durable execution requires the
        native command to see the explicit durable execute token, a clean source
        Blueprint package, exactly one local SCS Niagara component match,
        compile_blueprint=true, and save_blueprint=true. It performs a post-save
        static UnrealMCP marker scan when a save occurs.

        Args:
            blueprint_path: Blueprint package or object path to mutate.
            component_name: Required local SCS Niagara component filter, e.g. RainFX.
            source_system_path: Niagara System currently expected on the template.
            replacement_system_path: Niagara System to assign to the original Blueprint template.
            confirmation_token: Must be ALLOW_DURABLE_BP_COMPONENT_TEMPLATE_EXECUTE.
            operation: Must be durable_bp_component_template_asset_swap.
            dry_run: True previews the durable execution. False can compile/save the original Blueprint.
            preserve_override_parameters: Must remain true.
            allow_unrealmcp_dependency: Must remain false.
            allow_dirty_blueprint: Allow execution when the Blueprint package was already dirty.
            compile_blueprint: Required true when dry_run=False.
            save_blueprint: Required true when dry_run=False.
            parameter_name_contains: Optional substring filter for override parameter names.
            include_inherited_scs_templates: Must remain false for mutation safety.
            require_exactly_one: Require exactly one matching component template.
            require_source_system_match: Require matching templates to use source_system_path.
            max_components: Maximum matching component templates to return.
            max_parameters: Maximum parameters to return per override store.
            include_parameter_values: Include decoded primitive/vector/color parameter values.
            include_raw_bytes: Include raw byte previews for unsupported parameter types.
            max_raw_bytes: Maximum bytes per unsupported parameter raw preview.
            include_parameter_object_properties: Include limited UPROPERTY previews for UObject/DataInterface parameter values.
            include_component_object_properties: Include limited reflected properties from the component template itself.
            max_object_properties: Maximum UObject/DataInterface or component properties to return.
            max_array_elements: Maximum array elements to sample per reflected property.
            max_property_value_chars: Maximum export-text characters per reflected property value.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "blueprint_path": blueprint_path,
                "component_name": component_name,
                "source_system_path": source_system_path,
                "replacement_system_path": replacement_system_path,
                "confirmation_token": confirmation_token,
                "operation": operation,
                "dry_run": bool(dry_run),
                "preserve_override_parameters": bool(preserve_override_parameters),
                "allow_unrealmcp_dependency": bool(allow_unrealmcp_dependency),
                "allow_dirty_blueprint": bool(allow_dirty_blueprint),
                "compile_blueprint": bool(compile_blueprint),
                "save_blueprint": bool(save_blueprint),
                "include_inherited_scs_templates": bool(include_inherited_scs_templates),
                "require_exactly_one": bool(require_exactly_one),
                "require_source_system_match": bool(require_source_system_match),
                "max_components": int(max_components),
                "max_parameters": int(max_parameters),
                "include_parameter_values": bool(include_parameter_values),
                "include_raw_bytes": bool(include_raw_bytes),
                "max_raw_bytes": int(max_raw_bytes),
                "include_parameter_object_properties": bool(include_parameter_object_properties),
                "include_component_object_properties": bool(include_component_object_properties),
                "max_object_properties": int(max_object_properties),
                "max_array_elements": int(max_array_elements),
                "max_property_value_chars": int(max_property_value_chars),
            }
            if parameter_name_contains:
                params["parameter_name_contains"] = parameter_name_contains

            response = unreal.send_command(
                "execute_blueprint_niagara_component_template_asset_swap_durable",
                params,
            )
            return response or _unreal_no_response_error()

        except Exception as e:
            logger.error(f"Error executing durable Blueprint Niagara component template asset swap: {e}")
            return _editor_exception_error(str(e), e)

    logger.info("Editor tools registered successfully")
