"""
Blueprint Tools for Unreal MCP.

This module provides tools for creating and manipulating Blueprint assets in Unreal Engine.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

from tools.dependency_guard import (
    reject_mcp_dependency_reference,
    reject_mcp_dependency_references,
)
from tools.response_schema import tool_error

# Get logger
logger = logging.getLogger("UnrealMCP")


def _unreal_connection_error() -> Dict[str, Any]:
    return tool_error(
        "Failed to connect to Unreal Engine",
        error_code="unreal_connection_failed",
    )


def _unreal_no_response_error() -> Dict[str, Any]:
    return tool_error(
        "No response from Unreal Engine",
        error_code="unreal_no_response",
    )


def _blueprint_exception_error(message: str, exc: Exception) -> Dict[str, Any]:
    return tool_error(
        message,
        error_code="blueprint_tool_failed",
        details={"exception_type": type(exc).__name__},
    )


def register_blueprint_tools(mcp: FastMCP):
    """Register Blueprint tools with the MCP server."""
    
    @mcp.tool()
    def create_blueprint(
        ctx: Context,
        name: str,
        parent_class: str,
        package_path: str = ""
    ) -> Dict[str, Any]:
        """
        Create a new Blueprint class.

        Args:
            name: Name for the new Blueprint asset
            parent_class: Parent class for the Blueprint
            package_path: Optional package directory under /Game
        """
        # Import inside function to avoid circular imports
        from unreal_mcp_server import get_unreal_connection
        
        try:
            reject_mcp_dependency_reference("parent_class", parent_class)
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
                
            params = {
                "name": name,
                "parent_class": parent_class
            }
            if package_path:
                params["package_path"] = package_path

            response = unreal.send_command("create_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Blueprint creation response: {response}")
            return response or {}
            
        except Exception as e:
            error_msg = f"Error creating blueprint: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)
    
    @mcp.tool()
    def add_component_to_blueprint(
        ctx: Context,
        blueprint_name: str,
        component_type: str,
        component_name: str,
        location: List[float] = [],
        rotation: List[float] = [],
        scale: List[float] = [],
        parent_component_name: str = "",
        component_properties: Dict[str, Any] = {}
    ) -> Dict[str, Any]:
        """
        Add a component to a Blueprint.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_type: Type of component to add (use component class name without U prefix)
            component_name: Name for the new component
            location: [X, Y, Z] coordinates for component's position
            rotation: [Pitch, Yaw, Roll] values for component's rotation
            scale: [X, Y, Z] values for component's scale
            parent_component_name: Optional existing scene component to attach this component under
            component_properties: Additional properties to set on the component
        
        Returns:
            Information about the added component
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            reject_mcp_dependency_reference("component_type", component_type)
            reject_mcp_dependency_references("component_properties", component_properties)
            # Ensure all parameters are properly formatted
            params = {
                "blueprint_name": blueprint_name,
                "component_type": component_type,
                "component_name": component_name,
                "location": location or [0.0, 0.0, 0.0],
                "rotation": rotation or [0.0, 0.0, 0.0],
                "scale": scale or [1.0, 1.0, 1.0]
            }

            if parent_component_name:
                params["parent_component_name"] = parent_component_name
            
            # Add component_properties if provided
            if component_properties and len(component_properties) > 0:
                params["component_properties"] = component_properties
            
            # Validate location, rotation, and scale formats
            for param_name in ["location", "rotation", "scale"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return tool_error(
                        f"Invalid {param_name} format. Must be a list of 3 float values.",
                        error_code="blueprint_invalid_vector_parameter",
                    )
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
                
            logger.info(f"Adding component to blueprint with params: {params}")
            response = unreal.send_command("add_component_to_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Component addition response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding component to blueprint: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def list_blueprint_components(
        ctx: Context,
        blueprint_name: str,
        component_name: str = "",
    ) -> Dict[str, Any]:
        """
        List Blueprint SimpleConstructionScript components and selected defaults.

        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Optional component name filter

        Returns:
            Component metadata including class, template path, relative transform, and static mesh when available.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            reject_mcp_dependency_reference("blueprint_name", blueprint_name)
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params = {"blueprint_name": blueprint_name}
            if component_name:
                params["component_name"] = component_name

            logger.info(f"Listing blueprint components with params: {params}")
            response = unreal.send_command("list_blueprint_components", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()

            logger.info(f"List blueprint components response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error listing blueprint components: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)
    
    @mcp.tool()
    def set_static_mesh_properties(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        static_mesh: str = "/Engine/BasicShapes/Cube.Cube"
    ) -> Dict[str, Any]:
        """
        Set static mesh properties on a StaticMeshComponent.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the StaticMeshComponent
            static_mesh: Path to the static mesh asset (e.g., "/Engine/BasicShapes/Cube.Cube")
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "static_mesh": static_mesh
            }
            
            logger.info(f"Setting static mesh properties with params: {params}")
            response = unreal.send_command("set_static_mesh_properties", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Set static mesh properties response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting static mesh properties: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)
    
    @mcp.tool()
    def set_component_property(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """Set a property on a component in a Blueprint."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "property_name": property_name,
                "property_value": property_value
            }
            
            logger.info(f"Setting component property with params: {params}")
            response = unreal.send_command("set_component_property", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Set component property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting component property: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_actor_tick_settings(
        ctx: Context,
        blueprint_name: str,
        tick_hz: float = 60.0,
        tick_interval: float = -1.0,
        can_ever_tick: bool = True,
        start_with_tick_enabled: bool = True,
        allow_tick_on_dedicated_server: bool = True,
        tick_even_when_paused: bool = False,
        dry_run: bool = True,
        compile: bool = True,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Set Actor CDO tick defaults on a Blueprint without adding runtime dependencies.

        Args:
            blueprint_name: Target Blueprint name or object path.
            tick_hz: Desired tick frequency. 60 means TickInterval 1/60.
            tick_interval: Optional explicit TickInterval. Negative values use tick_hz.
            can_ever_tick: PrimaryActorTick.bCanEverTick.
            start_with_tick_enabled: PrimaryActorTick.bStartWithTickEnabled.
            allow_tick_on_dedicated_server: PrimaryActorTick.bAllowTickOnDedicatedServer.
            tick_even_when_paused: PrimaryActorTick.bTickEvenWhenPaused.
            dry_run: Preview changes without mutating the asset.
            compile: Compile after mutation.
            save: Save only after a successful compile.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            reject_mcp_dependency_reference("blueprint_name", blueprint_name)
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params = {
                "blueprint_name": blueprint_name,
                "tick_hz": float(tick_hz),
                "can_ever_tick": bool(can_ever_tick),
                "start_with_tick_enabled": bool(start_with_tick_enabled),
                "allow_tick_on_dedicated_server": bool(allow_tick_on_dedicated_server),
                "tick_even_when_paused": bool(tick_even_when_paused),
                "dry_run": bool(dry_run),
                "compile": bool(compile),
                "save": bool(save),
            }
            if tick_interval >= 0.0:
                params["tick_interval"] = float(tick_interval)

            response = unreal.send_command("set_blueprint_actor_tick_settings", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error setting Blueprint actor tick settings: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def duplicate_blueprint_to_temp(
        ctx: Context,
        blueprint_name: str,
        target_path: str,
        required_root: str = "/Game/_MCP_Temp",
        dry_run: bool = True,
        overwrite_existing: bool = False,
        allow_dirty_source: bool = False,
        compile: bool = False,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Duplicate a Blueprint to a temp content path for canary/preflight edits.

        Args:
            blueprint_name: Source Blueprint name or object path.
            target_path: Destination package path under required_root.
            required_root: Required destination root, default /Game/_MCP_Temp.
            dry_run: Preview only by default.
            overwrite_existing: Delete and replace an existing target asset.
            allow_dirty_source: Permit duplicating unsaved source package state.
            compile: Compile the duplicate after creation.
            save: Save the duplicate after creation or successful compile.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            reject_mcp_dependency_reference("blueprint_name", blueprint_name)
            reject_mcp_dependency_reference("target_path", target_path)
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params = {
                "blueprint_name": blueprint_name,
                "target_path": target_path,
                "required_root": required_root,
                "dry_run": bool(dry_run),
                "overwrite_existing": bool(overwrite_existing),
                "allow_dirty_source": bool(allow_dirty_source),
                "compile": bool(compile),
                "save": bool(save),
            }
            response = unreal.send_command("duplicate_blueprint_to_temp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error duplicating Blueprint to temp: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def replace_blueprint_component_class(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        new_component_class: str,
        sphere_radius: float = -1.0,
        dry_run: bool = True,
        compile: bool = True,
        save: bool = True,
        refresh_nodes: bool = True,
    ) -> Dict[str, Any]:
        """
        Replace a Blueprint SCS component template class while preserving the variable name and GUID.

        This is an editor authoring command only. The saved Blueprint receives only native Unreal
        component classes such as SphereComponent and does not gain an UnrealMCP dependency.

        Args:
            blueprint_name: Target Blueprint name or object path.
            component_name: Existing SCS component variable name.
            new_component_class: Replacement component class, for example SphereComponent.
            sphere_radius: Optional radius when replacing with SphereComponent.
            dry_run: Preview changes without mutating the asset.
            compile: Compile after mutation.
            save: Save only after a successful compile.
            refresh_nodes: Refresh Blueprint nodes during compile validation.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            reject_mcp_dependency_reference("blueprint_name", blueprint_name)
            reject_mcp_dependency_reference("new_component_class", new_component_class)
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "new_component_class": new_component_class,
                "dry_run": bool(dry_run),
                "compile": bool(compile),
                "save": bool(save),
                "refresh_nodes": bool(refresh_nodes),
            }
            if sphere_radius >= 0.0:
                params["sphere_radius"] = float(sphere_radius)

            response = unreal.send_command("replace_blueprint_component_class", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error replacing Blueprint component class: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def set_skeletal_mesh_component_anim_defaults(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        skeletal_mesh: str = "",
        anim_class: str = "",
        physics_asset: Optional[str] = None,
        simulate_physics: Optional[bool] = None,
        collision_enabled: Optional[str] = None,
        collision_response: Optional[str] = None,
        enable_gravity: Optional[bool] = None,
        visibility_based_anim_tick_option: Optional[str] = None,
        compile: bool = True,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Safely set SkeletalMeshComponent mesh and AnimBP defaults on a Blueprint component template.

        Args:
            blueprint_name: Target Blueprint name or object path.
            component_name: SkeletalMeshComponent variable name.
            skeletal_mesh: Skeletal mesh object path.
            anim_class: AnimBlueprint generated class path ending in _C.
            physics_asset: Optional PhysicsAsset override object path; an empty string clears it.
            simulate_physics: Optional component simulation default. Custom PhysicsAsset body types remain authoritative.
            collision_enabled: Optional NoCollision, QueryOnly, PhysicsOnly, or QueryAndPhysics.
            collision_response: Optional uniform Ignore, Overlap, or Block response.
            enable_gravity: Optional component gravity default.
            visibility_based_anim_tick_option: Optional animation tick policy such as AlwaysTickPoseAndRefreshBones.
            compile: Compile after mutation.
            save: Save after successful compile.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            reject_mcp_dependency_reference("blueprint_name", blueprint_name)
            if skeletal_mesh:
                reject_mcp_dependency_reference("skeletal_mesh", skeletal_mesh)
            if anim_class:
                reject_mcp_dependency_reference("anim_class", anim_class)
            if physics_asset:
                reject_mcp_dependency_reference("physics_asset", physics_asset)
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "compile": compile,
                "save": save,
            }
            if skeletal_mesh:
                params["skeletal_mesh"] = skeletal_mesh
            if anim_class:
                params["anim_class"] = anim_class
            if physics_asset is not None:
                params["physics_asset"] = physics_asset
            if simulate_physics is not None:
                params["simulate_physics"] = bool(simulate_physics)
            if collision_enabled is not None:
                params["collision_enabled"] = collision_enabled
            if collision_response is not None:
                params["collision_response"] = collision_response
            if enable_gravity is not None:
                params["enable_gravity"] = bool(enable_gravity)
            if visibility_based_anim_tick_option is not None:
                params["visibility_based_anim_tick_option"] = visibility_based_anim_tick_option

            logger.info(f"Setting skeletal mesh component defaults with params: {params}")
            response = unreal.send_command("set_skeletal_mesh_component_anim_defaults", params)
            return response or _unreal_no_response_error()

        except Exception as e:
            error_msg = f"Error setting skeletal mesh component defaults: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def get_component_property(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        property_name: str,
    ) -> Dict[str, Any]:
        """Read a property from a Blueprint component template."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "property_name": property_name,
            }

            logger.info(f"Reading component property with params: {params}")
            response = unreal.send_command("get_component_property", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()

            logger.info(f"Get component property response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error reading component property: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)
    
    @mcp.tool()
    def set_physics_properties(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        simulate_physics: bool = True,
        gravity_enabled: bool = True,
        mass: float = 1.0,
        linear_damping: float = 0.01,
        angular_damping: float = 0.0
    ) -> Dict[str, Any]:
        """Set physics properties on a component."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "simulate_physics": simulate_physics,
                "gravity_enabled": gravity_enabled,
                "mass": float(mass),
                "linear_damping": float(linear_damping),
                "angular_damping": float(angular_damping)
            }
            
            logger.info(f"Setting physics properties with params: {params}")
            response = unreal.send_command("set_physics_properties", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Set physics properties response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting physics properties: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_blueprint_rigid_constraint_chain(
        ctx: Context,
        blueprint_name: str,
        body_components: List[Dict[str, Any]],
        root_component_name: str = "SceneRoot",
        constraint_prefix: str = "PC",
        swing1_limit_degrees: float = 50.0,
        swing2_limit_degrees: float = 50.0,
        twist_limit_degrees: float = 10.0,
        linear_damping: float = 0.05,
        angular_damping: float = 0.15,
        projection_linear_alpha: float = 0.25,
        projection_angular_alpha: float = 0.08,
        projection_linear_tolerance: float = 5.0,
        projection_angular_tolerance: float = 15.0,
        required_root: str = "/Game/Study/ChaosSkeletalMesh",
        save: bool = True,
        dry_run: bool = True,
    ) -> Dict[str, Any]:
        """
        Author an anchored StaticMesh rigid-body chain in an Actor Blueprint.

        The first body is kinematic and every following body simulates physics.
        Adjacent bodies are joined by engine PhysicsConstraintComponents at the
        child body's relative location. Linear motion is locked, angular motion
        is limited, self/world collision responses are ignored, and simple
        collision is required on every source StaticMesh so Chaos still has
        physical bodies and inertia.

        Each body_components item requires component_name, static_mesh, and
        relative_location=[X,Y,Z]. Optional keys are relative_rotation,
        mass_kg, and simulate_physics; when supplied, simulate_physics must be
        false for the first item and true for every later item.

        dry_run defaults to true and performs all asset, collision, topology,
        name-collision, and required-root checks without changing the Blueprint.
        Existing body or constraint component names are never overwritten.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            reject_mcp_dependency_reference("blueprint_name", blueprint_name)
            reject_mcp_dependency_reference("root_component_name", root_component_name)
            reject_mcp_dependency_reference("constraint_prefix", constraint_prefix)
            reject_mcp_dependency_reference("required_root", required_root)
            reject_mcp_dependency_references("body_components", body_components)

            if not isinstance(body_components, list) or not 2 <= len(body_components) <= 16:
                return tool_error(
                    "body_components must contain between 2 and 16 objects",
                    error_code="blueprint_rigid_chain_invalid_bodies",
                )
            normalized_bodies: List[Dict[str, Any]] = []
            for index, body in enumerate(body_components):
                if not isinstance(body, dict):
                    return tool_error(
                        f"body_components[{index}] must be an object",
                        error_code="blueprint_rigid_chain_invalid_body",
                    )
                if not body.get("component_name") or not body.get("static_mesh"):
                    return tool_error(
                        f"body_components[{index}] requires component_name and static_mesh",
                        error_code="blueprint_rigid_chain_invalid_body",
                    )
                location = body.get("relative_location")
                if not isinstance(location, list) or len(location) != 3:
                    return tool_error(
                        f"body_components[{index}].relative_location must be three numbers",
                        error_code="blueprint_rigid_chain_invalid_body_transform",
                    )
                normalized = dict(body)
                normalized["relative_location"] = [float(value) for value in location]
                if "relative_rotation" in normalized:
                    rotation = normalized["relative_rotation"]
                    if not isinstance(rotation, list) or len(rotation) != 3:
                        return tool_error(
                            f"body_components[{index}].relative_rotation must be three numbers",
                            error_code="blueprint_rigid_chain_invalid_body_transform",
                        )
                    normalized["relative_rotation"] = [float(value) for value in rotation]
                if "mass_kg" in normalized:
                    normalized["mass_kg"] = float(normalized["mass_kg"])
                normalized_bodies.append(normalized)

            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            return unreal.send_command(
                "ensure_blueprint_rigid_constraint_chain",
                {
                    "blueprint_name": blueprint_name,
                    "body_components": normalized_bodies,
                    "root_component_name": root_component_name,
                    "constraint_prefix": constraint_prefix,
                    "swing1_limit_degrees": float(swing1_limit_degrees),
                    "swing2_limit_degrees": float(swing2_limit_degrees),
                    "twist_limit_degrees": float(twist_limit_degrees),
                    "linear_damping": float(linear_damping),
                    "angular_damping": float(angular_damping),
                    "projection_linear_alpha": float(projection_linear_alpha),
                    "projection_angular_alpha": float(projection_angular_alpha),
                    "projection_linear_tolerance": float(projection_linear_tolerance),
                    "projection_angular_tolerance": float(projection_angular_tolerance),
                    "required_root": required_root,
                    "save": bool(save),
                    "dry_run": bool(dry_run),
                },
            ) or {}
        except Exception as e:
            error_msg = f"Error ensuring Blueprint rigid constraint chain: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)
    
    @mcp.tool()
    def compile_blueprint(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """Compile a Blueprint."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            params = {
                "blueprint_name": blueprint_name
            }
            
            logger.info(f"Compiling blueprint: {blueprint_name}")
            response = unreal.send_command("compile_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Compile blueprint response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error compiling blueprint: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def compile_and_save_blueprint(
        ctx: Context,
        blueprint_name: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Compile a Blueprint and optionally save the loaded asset.

        Args:
            blueprint_name: Name or path of the target Blueprint
            save: Whether to save the Blueprint after compiling

        Returns:
            Response indicating compile and save status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params = {
                "blueprint_name": blueprint_name,
                "save": save
            }

            logger.info(f"Compiling and saving blueprint: {blueprint_name}")
            response = unreal.send_command("compile_and_save_blueprint", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()

            logger.info(f"Compile and save blueprint response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error compiling and saving blueprint: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def compile_and_validate_blueprint(
        ctx: Context,
        blueprint_name: str,
        save: bool = False,
        refresh_nodes: bool = True
    ) -> Dict[str, Any]:
        """
        Compile a Blueprint and return structured validation status.

        Args:
            blueprint_name: Name or path of the target Blueprint
            save: Whether to save the Blueprint after a successful compile
            refresh_nodes: Whether to refresh Blueprint nodes before compile

        Returns:
            Response containing compiled, validation_pass, status, counts, and diagnostics
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            params = {
                "blueprint_name": blueprint_name,
                "save": save,
                "refresh_nodes": refresh_nodes
            }

            logger.info(f"Compiling and validating blueprint: {blueprint_name}")
            response = unreal.send_command("compile_and_validate_blueprint", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()

            logger.info(f"Compile and validate blueprint response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error compiling and validating blueprint: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_property(
        ctx: Context,
        blueprint_name: str,
        property_name: str,
        property_value
    ) -> Dict[str, Any]:
        """
        Set a property on a Blueprint class default object.
        
        Args:
            blueprint_name: Name of the target Blueprint
            property_name: Name of the property to set
            property_value: Value to set the property to
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            params = {
                "blueprint_name": blueprint_name,
                "property_name": property_name,
                "property_value": property_value
            }
            
            logger.info(f"Setting blueprint property with params: {params}")
            response = unreal.send_command("set_blueprint_property", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Set blueprint property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting blueprint property: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def set_pawn_properties(
        ctx: Context,
        blueprint_name: str,
        auto_possess_player: str = "",
        use_controller_rotation_yaw: Optional[bool] = None,
        use_controller_rotation_pitch: Optional[bool] = None,
        use_controller_rotation_roll: Optional[bool] = None,
        can_be_damaged: Optional[bool] = None
    ) -> Dict[str, Any]:
        """
        Set common Pawn/Character defaults on a Blueprint through the native bridge command.
        
        Args:
            blueprint_name: Name of the target Blueprint (must be a Pawn or Character)
            auto_possess_player: Auto possess player setting (None, "Disabled", "Player0", "Player1", etc.)
            use_controller_rotation_yaw: Whether the pawn should use the controller's yaw rotation
            use_controller_rotation_pitch: Whether the pawn should use the controller's pitch rotation
            use_controller_rotation_roll: Whether the pawn should use the controller's roll rotation
            can_be_damaged: Whether the pawn can be damaged
            
        Returns:
            Response indicating success or failure with detailed results for each property
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            params: Dict[str, Any] = {"blueprint_name": blueprint_name}
            if auto_possess_player and auto_possess_player != "":
                params["auto_possess_player"] = auto_possess_player
            
            if use_controller_rotation_yaw is not None:
                params["use_controller_rotation_yaw"] = use_controller_rotation_yaw
            if use_controller_rotation_pitch is not None:
                params["use_controller_rotation_pitch"] = use_controller_rotation_pitch
            if use_controller_rotation_roll is not None:
                params["use_controller_rotation_roll"] = use_controller_rotation_roll
            if can_be_damaged is not None:
                params["can_be_damaged"] = can_be_damaged
                
            if len(params) == 1:
                logger.warning("No properties specified to set")
                return {"success": True, "message": "No properties specified to set", "results": {}}
            
            logger.info(f"Setting pawn properties with params: {params}")
            response = unreal.send_command("set_pawn_properties", params)
            return response or _unreal_no_response_error()
            
        except Exception as e:
            error_msg = f"Error setting pawn properties: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_interface(
        ctx: Context,
        blueprint_name: str,
        interface_class: str
    ) -> Dict[str, Any]:
        """
        Add a Blueprint Interface implementation to a Blueprint.

        Args:
            blueprint_name: Name or path of the target Blueprint
            interface_class: Interface class or Blueprint Interface asset path
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            reject_mcp_dependency_reference("interface_class", interface_class)
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            response = unreal.send_command(
                "add_blueprint_interface",
                {
                    "blueprint_name": blueprint_name,
                    "interface_class": interface_class,
                },
            )
            return response or _unreal_no_response_error()

        except Exception as e:
            error_msg = f"Error adding Blueprint Interface: {e}"
            logger.error(error_msg)
            return _blueprint_exception_error(error_msg, e)
    
    logger.info("Blueprint tools registered successfully")
