"""
Material graph tools for Unreal MCP.

These tools route to editor-only UnrealMCP C++ commands so material expression
graphs can be enumerated and edited without relying on protected Unreal Python
properties such as UMaterial.Expressions.
"""

import logging
from typing import Any, Dict, List

from mcp.server.fastmcp import Context, FastMCP

from tools.dependency_guard import (
    reject_mcp_dependency_reference,
    reject_mcp_dependency_references,
)
from tools.response_schema import tool_error

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


def _material_exception_error(message: str, exc: Exception) -> Dict[str, Any]:
    return tool_error(
        message,
        error_code="material_tool_failed",
        details={"exception_type": type(exc).__name__},
    )


def register_material_tools(mcp: FastMCP):
    """Register Material and Material Function graph tools with the MCP server."""

    def send_material_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        from unreal_mcp_server import get_unreal_connection

        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return _unreal_connection_error()

        response = unreal.send_command(command_name, params)
        if not response:
            logger.error("No response from Unreal Engine")
            return _unreal_no_response_error()

        return response

    @mcp.tool()
    def resolve_material_graph(
        ctx: Context,
        material_path: str,
        graph_type: str = "auto",
    ) -> Dict[str, Any]:
        """
        Resolve a Material or Material Function by short name, package path, or object path.

        Args:
            material_path: Material or Material Function name/path
            graph_type: auto, material, or function
        """
        try:
            return send_material_command(
                "resolve_material_graph",
                {"material_path": material_path, "graph_type": graph_type},
            )
        except Exception as e:
            error_msg = f"Error resolving material graph: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def list_material_nodes(
        ctx: Context,
        material_path: str,
        graph_type: str = "auto",
        node_type: str = "",
        desc_contains: str = "",
        include_pins: bool = True,
    ) -> Dict[str, Any]:
        """
        List nodes, inputs, outputs, links, and material property connections in a material graph.

        Args:
            material_path: Material or Material Function name/path
            graph_type: auto, material, or function
            node_type: Optional expression class/name substring filter
            desc_contains: Optional node description substring filter
            include_pins: Include input/output metadata and links
        """
        try:
            params = {
                "material_path": material_path,
                "graph_type": graph_type,
                "node_type": node_type,
                "desc_contains": desc_contains,
                "include_pins": include_pins,
            }
            return send_material_command("list_material_nodes", params)
        except Exception as e:
            error_msg = f"Error listing material nodes: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def analyze_material_graph(
        ctx: Context,
        material_path: str,
        graph_type: str = "auto",
        include_nodes: bool = False,
        include_usage: bool = True,
        max_referencers: int = 25,
    ) -> Dict[str, Any]:
        """
        Analyze a Material, Material Function, or Material Instance without editing assets.

        Args:
            material_path: Material, Material Function, or Material Instance name/path
            graph_type: auto, material, or function. Material Instances are accepted in auto/material mode.
            include_nodes: Include full node/pin data in addition to summaries
            include_usage: Include Asset Registry package referencer hints
            max_referencers: Maximum referencer packages to include when include_usage is true
        """
        try:
            params = {
                "material_path": material_path,
                "graph_type": graph_type,
                "include_nodes": include_nodes,
                "include_usage": include_usage,
                "max_referencers": max_referencers,
            }
            return send_material_command("analyze_material_graph", params)
        except Exception as e:
            error_msg = f"Error analyzing material graph: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def list_material_collection_parameter_nodes(
        ctx: Context,
        material_path: str,
        graph_type: str = "auto",
    ) -> Dict[str, Any]:
        """
        List MaterialExpressionCollectionParameter nodes with their MPC parameter names and types.

        Args:
            material_path: Material or Material Function name/path
            graph_type: auto, material, or function
        """
        try:
            params = {
                "material_path": material_path,
                "graph_type": graph_type,
            }
            return send_material_command("list_material_collection_parameter_nodes", params)
        except Exception as e:
            error_msg = f"Error listing material collection parameter nodes: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def mirror_material_parameter_collection(
        ctx: Context,
        source_collection_path: str,
        target_collection_path: str,
        parameter_names: List[str] | None = None,
        copy_defaults: bool = True,
        preserve_ids: bool = True,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Ensure a target MPC has same-name parameters from a source MPC.

        Args:
            source_collection_path: Source Material Parameter Collection path
            target_collection_path: Target Material Parameter Collection path
            parameter_names: Optional scalar/vector parameter names to mirror
            copy_defaults: Copy source asset default values into target
            preserve_ids: Preserve source parameter IDs for CollectionParameter node compatibility
            save: Save the target collection after mirroring
        """
        try:
            params = {
                "source_collection_path": source_collection_path,
                "target_collection_path": target_collection_path,
                "parameter_names": parameter_names or [],
                "copy_defaults": copy_defaults,
                "preserve_ids": preserve_ids,
                "save": save,
            }
            return send_material_command("mirror_material_parameter_collection", params)
        except Exception as e:
            error_msg = f"Error mirroring material parameter collection: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def replace_material_collection_references(
        ctx: Context,
        material_path: str,
        target_collection_path: str,
        source_collection_path: str = "",
        graph_type: str = "auto",
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Retarget MaterialExpressionCollectionParameter nodes to another MPC.

        Args:
            material_path: Material or Material Function name/path
            target_collection_path: Target Material Parameter Collection path
            source_collection_path: Optional source MPC filter; blank retargets all collection nodes
            graph_type: auto, material, or function
            save: Whether to save the material after retargeting
        """
        try:
            params = {
                "material_path": material_path,
                "target_collection_path": target_collection_path,
                "source_collection_path": source_collection_path,
                "graph_type": graph_type,
                "save": save,
            }
            return send_material_command("replace_material_collection_references", params)
        except Exception as e:
            error_msg = f"Error replacing material collection references: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def replace_material_collection_parameters(
        ctx: Context,
        material_path: str,
        use_runtime_values: bool = True,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Replace MaterialExpressionCollectionParameter nodes with ordinary scalar/vector parameter nodes.

        Args:
            material_path: Material path
            use_runtime_values: Use current editor-world MPC runtime values before falling back to asset defaults
            save: Whether to save the material after replacing and compiling
        """
        try:
            params = {
                "material_path": material_path,
                "use_runtime_values": use_runtime_values,
                "save": save,
            }
            return send_material_command("replace_material_collection_parameters", params)
        except Exception as e:
            error_msg = f"Error replacing material collection parameters: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def replace_material_texture_references(
        ctx: Context,
        material_path: str,
        replacements: Dict[str, str],
        graph_type: str = "auto",
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Replace texture references on MaterialExpressionTextureBase nodes in a Material or Material Function graph.

        Args:
            material_path: Material or Material Function name/path
            replacements: Mapping of source texture path/package path to replacement texture path
            graph_type: auto, material, or function
            save: Whether to save the material after replacing and compiling
        """
        try:
            params = {
                "material_path": material_path,
                "replacements": replacements,
                "graph_type": graph_type,
                "save": save,
            }
            return send_material_command("replace_material_texture_references", params)
        except Exception as e:
            error_msg = f"Error replacing material texture references: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def add_material_node(
        ctx: Context,
        material_path: str,
        expression_class: str,
        graph_type: str = "auto",
        node_position=None,
        selected_asset: str = "",
        properties: Dict[str, Any] | None = None,
    ) -> Dict[str, Any]:
        """
        Add a MaterialExpression node to a Material or Material Function graph.

        Args:
            material_path: Material or Material Function name/path
            expression_class: Material expression class, e.g. TextureSample, Multiply, Constant3Vector
            graph_type: auto, material, or function
            node_position: Optional [X, Y] editor position
            selected_asset: Optional asset path used by expression creation helpers
            properties: Optional property map to set after creation, e.g. {"Texture": "/Game/T.T"}
        """
        try:
            reject_mcp_dependency_reference("expression_class", expression_class)
            reject_mcp_dependency_reference("selected_asset", selected_asset)
            reject_mcp_dependency_references("properties", properties or {})
            if node_position is None:
                node_position = [0, 0]
            if properties is None:
                properties = {}
            params = {
                "material_path": material_path,
                "expression_class": expression_class,
                "graph_type": graph_type,
                "node_position": node_position,
                "selected_asset": selected_asset,
                "properties": properties,
            }
            return send_material_command("add_material_node", params)
        except Exception as e:
            error_msg = f"Error adding material node: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def add_custom_material_node(
        ctx: Context,
        material_path: str,
        code: str,
        output_type: str = "CMOT_Float3",
        inputs: List[str] | None = None,
        description: str = "",
        graph_type: str = "auto",
        node_position: List[int] | None = None,
        additional_defines: List[Dict[str, Any]] | None = None,
        include_file_paths: List[str] | None = None,
    ) -> Dict[str, Any]:
        """
        Add a MaterialExpressionCustom node with validated HLSL inputs and output type.

        Args:
            material_path: Material or Material Function name/path
            code: HLSL code for the Custom node
            output_type: CMOT_Float1, CMOT_Float2, CMOT_Float3, CMOT_Float4, CMOT_MaterialAttributes, or supported aliases
            inputs: Custom input pin names; each name must be a valid HLSL identifier
            description: Custom node description/caption
            graph_type: auto, material, or function
            node_position: Optional [X, Y] editor position
            additional_defines: Optional [{"name": "DEFINE_NAME", "value": "1"}] entries
            include_file_paths: Optional Custom node include file paths
        """
        try:
            if inputs is None:
                inputs = []
            if node_position is None:
                node_position = [0, 0]
            if additional_defines is None:
                additional_defines = []
            if include_file_paths is None:
                include_file_paths = []
            params = {
                "material_path": material_path,
                "code": code,
                "output_type": output_type,
                "inputs": inputs,
                "description": description,
                "graph_type": graph_type,
                "node_position": node_position,
                "additional_defines": additional_defines,
                "include_file_paths": include_file_paths,
            }
            return send_material_command("add_custom_material_node", params)
        except Exception as e:
            error_msg = f"Error adding custom material node: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def set_material_node_property(
        ctx: Context,
        material_path: str,
        node_id: str,
        property_name: str,
        value: Any,
        graph_type: str = "auto",
    ) -> Dict[str, Any]:
        """
        Set an editor property on a material graph node.

        Args:
            material_path: Material or Material Function name/path
            node_id: node_key from list/analyze output. guid:, name:, path:, legacy GUID, object name, and object path are also accepted.
            property_name: Property name on the expression object
            value: New property value. Object properties should use asset path strings.
            graph_type: auto, material, or function
        """
        try:
            reject_mcp_dependency_references("value", value)
            params = {
                "material_path": material_path,
                "node_id": node_id,
                "property_name": property_name,
                "value": value,
                "graph_type": graph_type,
            }
            return send_material_command("set_material_node_property", params)
        except Exception as e:
            error_msg = f"Error setting material node property: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def connect_material_nodes(
        ctx: Context,
        material_path: str,
        from_node_id: str,
        to_node_id: str,
        to_input: str,
        from_output: str = "",
        graph_type: str = "auto",
    ) -> Dict[str, Any]:
        """
        Connect one material expression output to another expression input.

        Args:
            material_path: Material or Material Function name/path
            from_node_id: Source node_key from list/analyze output
            to_node_id: Target node_key from list/analyze output
            to_input: Target input pin name
            from_output: Source output pin name. Empty string means first output.
            graph_type: auto, material, or function
        """
        try:
            params = {
                "material_path": material_path,
                "from_node_id": from_node_id,
                "from_output": from_output,
                "to_node_id": to_node_id,
                "to_input": to_input,
                "graph_type": graph_type,
            }
            return send_material_command("connect_material_nodes", params)
        except Exception as e:
            error_msg = f"Error connecting material nodes: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def connect_material_property(
        ctx: Context,
        material_path: str,
        from_node_id: str,
        property: str,
        from_output: str = "",
    ) -> Dict[str, Any]:
        """
        Connect a material expression output to a root Material property.

        Args:
            material_path: Material name/path. Material Functions are not supported here.
            from_node_id: Source node_key from list/analyze output
            property: Material property, e.g. BaseColor, Roughness, Normal, or MP_BaseColor
            from_output: Source output pin name. Empty string means first output.
        """
        try:
            params = {
                "material_path": material_path,
                "from_node_id": from_node_id,
                "from_output": from_output,
                "property": property,
            }
            return send_material_command("connect_material_property", params)
        except Exception as e:
            error_msg = f"Error connecting material property: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def delete_material_node(
        ctx: Context,
        material_path: str,
        node_id: str,
        graph_type: str = "auto",
    ) -> Dict[str, Any]:
        """
        Delete a material expression node and disconnect its links.

        Args:
            material_path: Material or Material Function name/path
            node_id: node_key from list/analyze output. guid:, name:, path:, legacy GUID, object name, and object path are also accepted.
            graph_type: auto, material, or function
        """
        try:
            params = {
                "material_path": material_path,
                "node_id": node_id,
                "graph_type": graph_type,
            }
            return send_material_command("delete_material_node", params)
        except Exception as e:
            error_msg = f"Error deleting material node: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def layout_material_nodes(
        ctx: Context,
        material_path: str,
        graph_type: str = "auto",
    ) -> Dict[str, Any]:
        """
        Layout material expression nodes in the editor graph.

        Args:
            material_path: Material or Material Function name/path
            graph_type: auto, material, or function
        """
        try:
            return send_material_command(
                "layout_material_nodes",
                {"material_path": material_path, "graph_type": graph_type},
            )
        except Exception as e:
            error_msg = f"Error laying out material nodes: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def compile_and_save_material(
        ctx: Context,
        material_path: str,
        graph_type: str = "auto",
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Recompile/update a Material or Material Function and optionally save it.

        Args:
            material_path: Material or Material Function name/path
            graph_type: auto, material, or function
            save: Whether to save the loaded asset after compiling
        """
        try:
            params = {
                "material_path": material_path,
                "graph_type": graph_type,
                "save": save,
            }
            return send_material_command("compile_and_save_material", params)
        except Exception as e:
            error_msg = f"Error compiling/saving material: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def refresh_material_cached_expression_data(
        ctx: Context,
        material_path: str,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Rebuild a Material asset's cached expression data, then compile/save and report dependencies.

        Use this after expanding Material Function calls or replacing texture/function references
        when Asset Registry dependencies still show stale Material Function packages.
        """
        try:
            params = {
                "material_path": material_path,
                "save": save,
            }
            return send_material_command("refresh_material_cached_expression_data", params)
        except Exception as e:
            error_msg = f"Error refreshing material cached expression data: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def expand_material_function_calls(
        ctx: Context,
        material_path: str,
        node_id: str = "",
        recursive: bool = True,
        exclude_engine_functions: bool = True,
        max_passes: int = 8,
        save: bool = True,
        allow_partial_save: bool = False,
    ) -> Dict[str, Any]:
        """
        Expand Material Function Call nodes inside a Material graph.

        Args:
            material_path: Material name/path
            node_id: Optional node_key/node_id for a single function call
            recursive: Keep expanding newly-created function calls until none remain or max_passes is reached
            exclude_engine_functions: Skip /Engine and /Script functions
            max_passes: Recursive expansion pass limit
            save: Whether to save the material after expanding
            allow_partial_save: Save successful partial expansions even if some requested expansions reported errors
        """
        try:
            params = {
                "material_path": material_path,
                "node_id": node_id,
                "recursive": recursive,
                "exclude_engine_functions": exclude_engine_functions,
                "max_passes": max_passes,
                "save": save,
                "allow_partial_save": allow_partial_save,
            }
            return send_material_command("expand_material_function_calls", params)
        except Exception as e:
            error_msg = f"Error expanding material function calls: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def get_material_parameter_collection_values(
        ctx: Context,
        collection_path: str,
        parameter_names: List[str] | None = None,
        include_asset_defaults: bool = True,
        include_runtime: bool = True,
    ) -> Dict[str, Any]:
        """
        Read Material Parameter Collection defaults and current editor-world runtime values.

        Args:
            collection_path: Material Parameter Collection name/path
            parameter_names: Optional list of scalar/vector parameter names to return
            include_asset_defaults: Include values stored on the MPC asset
            include_runtime: Include the current editor/PIE world MPC instance values
        """
        try:
            params = {
                "collection_path": collection_path,
                "parameter_names": parameter_names or [],
                "include_asset_defaults": include_asset_defaults,
                "include_runtime": include_runtime,
            }
            return send_material_command("get_material_parameter_collection_values", params)
        except Exception as e:
            error_msg = f"Error reading material parameter collection values: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def set_material_parameter_collection_values(
        ctx: Context,
        collection_path: str,
        scalars: Dict[str, float] | None = None,
        vectors: Dict[str, Dict[str, float]] | None = None,
        set_asset_defaults: bool = True,
        set_runtime: bool = True,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Set Material Parameter Collection scalar/vector values by parameter name.

        Args:
            collection_path: Material Parameter Collection name/path
            scalars: Mapping of scalar parameter names to float values
            vectors: Mapping of vector parameter names to {r,g,b,a} or {x,y,z,w}
            set_asset_defaults: Update values stored on the MPC asset
            set_runtime: Update current editor/PIE runtime collection instance
            save: Save the collection asset after updating defaults
        """
        try:
            params = {
                "collection_path": collection_path,
                "scalars": scalars or {},
                "vectors": vectors or {},
                "set_asset_defaults": set_asset_defaults,
                "set_runtime": set_runtime,
                "save": save,
            }
            return send_material_command("set_material_parameter_collection_values", params)
        except Exception as e:
            error_msg = f"Error setting material parameter collection values: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    @mcp.tool()
    def set_material_parameter_collection_sync(
        ctx: Context,
        action: str = "status",
        source_collection_path: str = "",
        target_collection_path: str = "",
        parameter_names: List[str] | None = None,
        interval_seconds: float = 0.1,
        set_asset_defaults_on_enable: bool = False,
        save_on_enable: bool = False,
    ) -> Dict[str, Any]:
        """
        Start, stop, or inspect same-name runtime MPC value sync between two collections.

        Args:
            action: status, enable/start, or disable/stop
            source_collection_path: Source MPC path for enable/start
            target_collection_path: Target MPC path for enable/start
            parameter_names: Optional same-name parameter filter
            interval_seconds: Sync interval while enabled
            set_asset_defaults_on_enable: Also copy runtime values into target asset defaults once
            save_on_enable: Save target asset if defaults are copied on enable
        """
        try:
            params = {
                "action": action,
                "parameter_names": parameter_names or [],
                "interval_seconds": interval_seconds,
                "set_asset_defaults_on_enable": set_asset_defaults_on_enable,
                "save_on_enable": save_on_enable,
            }
            if source_collection_path:
                params["source_collection_path"] = source_collection_path
            if target_collection_path:
                params["target_collection_path"] = target_collection_path
            return send_material_command("set_material_parameter_collection_sync", params)
        except Exception as e:
            error_msg = f"Error controlling material parameter collection sync: {e}"
            logger.error(error_msg)
            return _material_exception_error(error_msg, e)

    logger.info("Material tools registered successfully")
