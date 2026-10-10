"""
Blueprint Node Tools for Unreal MCP.

This module provides tools for manipulating Blueprint graph nodes and connections.
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


def _node_exception_error(message: str, exc: Exception) -> Dict[str, Any]:
    return tool_error(
        message,
        error_code="blueprint_node_tool_failed",
        details={"exception_type": type(exc).__name__},
    )


def register_blueprint_node_tools(mcp: FastMCP):
    """Register Blueprint node manipulation tools with the MCP server."""

    def send_node_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
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

    def add_graph_selector(
        params: Dict[str, Any],
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        if graph_name:
            params["graph_name"] = graph_name
        if graph_id:
            params["graph_id"] = graph_id
        if graph_type:
            params["graph_type"] = graph_type
        if create_graph_if_missing:
            params["create_graph_if_missing"] = create_graph_if_missing
        return params

    @mcp.tool()
    def add_blueprint_comment_box(
        ctx: Context,
        blueprint_name: str,
        comment_text: str,
        node_position: Optional[List[float]] = None,
        size: Optional[List[float]] = None,
        comment_color: Optional[List[float]] = None,
        font_size: int = 18,
        move_mode: str = "group",
        comment_bubble_visible: bool = False,
        color_comment_bubble: bool = False,
        node_details: str = "",
        node_ids: Optional[List[str]] = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a comment box to a Blueprint graph.

        Args:
            blueprint_name: Name of the target Blueprint
            comment_text: Text shown in the comment box
            node_position: Optional [X, Y] graph position
            size: Optional [Width, Height] box size
            comment_color: Optional [R, G, B, A] linear color
            font_size: Comment text font size
            move_mode: "group" or "no_group"
            comment_bubble_visible: Show the zoomed-out comment bubble
            color_comment_bubble: Tint the zoomed-out bubble with comment_color
            node_details: Optional details tooltip text
            node_ids: Optional graph node IDs to group under the comment
        """
        try:
            reject_mcp_dependency_reference("comment_text", comment_text)
            reject_mcp_dependency_reference("node_details", node_details)
            reject_mcp_dependency_references("node_ids", node_ids or [])

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "comment_text": comment_text,
                "font_size": font_size,
                "move_mode": move_mode,
                "comment_bubble_visible": comment_bubble_visible,
                "color_comment_bubble": color_comment_bubble,
            }
            if node_position is not None:
                params["node_position"] = node_position
            if size is not None:
                params["size"] = size
            if comment_color is not None:
                params["comment_color"] = comment_color
            if node_details:
                params["node_details"] = node_details
            if node_ids:
                params["node_ids"] = node_ids

            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_comment_box", params)
        except Exception as e:
            error_msg = f"Error adding blueprint comment box: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_node_comment(
        ctx: Context,
        blueprint_name: str,
        node_id: str,
        node_comment: str,
        comment_bubble_visible: bool = True,
        comment_bubble_pinned: bool = False,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
    ) -> Dict[str, Any]:
        """
        Set the comment bubble text for one Blueprint graph node.

        Args:
            blueprint_name: Name of the target Blueprint
            node_id: Node GUID returned by node inspection or creation tools
            node_comment: Comment text to store on the node
            comment_bubble_visible: Whether the comment bubble is visible
            comment_bubble_pinned: Whether the bubble stays pinned
        """
        try:
            reject_mcp_dependency_reference("node_comment", node_comment)

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "node_id": node_id,
                "node_comment": node_comment,
                "comment_bubble_visible": comment_bubble_visible,
                "comment_bubble_pinned": comment_bubble_pinned,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("set_blueprint_node_comment", params)
        except Exception as e:
            error_msg = f"Error setting blueprint node comment: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_node_positions(
        ctx: Context,
        blueprint_name: str,
        graph_name: str,
        updates: List[Dict[str, Any]],
        graph_id: str = "",
        graph_type: str = "",
        dry_run: bool = True,
        compile: bool = False,
        save: bool = False,
    ) -> Dict[str, Any]:
        """
        Guardedly move existing Blueprint nodes without changing graph topology.

        Every update must include node_id, position, expected_position,
        expected_class, and expected_title. Unreal validates the complete batch
        before changing any NodePosX/NodePosY value. Duplicate GUIDs and
        PIE/SIE execution are rejected. The batch is limited to 2048 updates.

        Args:
            blueprint_name: Name or asset path of the target Blueprint
            graph_name: Explicit target graph name
            updates: Guarded node position updates
            graph_id: Optional graph GUID used as an additional selector
            graph_type: Optional graph type used as an additional selector
            dry_run: Validate and report without mutation; defaults to True
            compile: Compile after a real edit; defaults to False
            save: Save after a real edit; defaults to False

        Returns:
            Per-node guard/readback results plus compile and save status
        """
        try:
            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "graph_name": graph_name,
                "updates": updates,
                "dry_run": dry_run,
                "compile": compile,
                "save": save,
            }
            if graph_id:
                params["graph_id"] = graph_id
            if graph_type:
                params["graph_type"] = graph_type
            return send_node_command("set_blueprint_node_positions", params)
        except Exception as e:
            error_msg = f"Error setting Blueprint node positions: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_function_description(
        ctx: Context,
        blueprint_name: str,
        description: str,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "function",
    ) -> Dict[str, Any]:
        """
        Set the Details panel description/tooltip for a Blueprint function graph.

        Args:
            blueprint_name: Name of the target Blueprint
            description: Text stored as the function description/tooltip
            graph_name: Optional target function graph name
            graph_id: Optional target graph id
            graph_type: Optional target graph type; defaults to function

        Returns:
            Response containing the function entry node, graph metadata, and stored description
        """
        try:
            reject_mcp_dependency_reference("description", description)
            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "description": description,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("set_blueprint_function_description", params)
        except Exception as e:
            error_msg = f"Error setting Blueprint function description: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_custom_event_metadata(
        ctx: Context,
        blueprint_name: str,
        custom_event_name: str = "",
        description: str = "",
        node_id: str = "",
        event_name: str = "",
        tooltip: str = "",
        call_in_editor: Optional[bool] = None,
        set_node_comment: bool = True,
        compile: bool = True,
        save: bool = False,
    ) -> Dict[str, Any]:
        """
        Set the Details panel description/tooltip metadata for a Blueprint custom event.

        Args:
            blueprint_name: Name of the target Blueprint
            custom_event_name: Name of the custom event to update
            description: Text stored as the custom event description/tooltip
            node_id: Optional node GUID alternative to custom_event_name
            event_name: Alias for custom_event_name
            tooltip: Alias for description
            call_in_editor: Optional value for the custom event's Call In Editor flag
            set_node_comment: Also mirror the text into the graph node comment
            compile: Compile the Blueprint after applying metadata
            save: Save the Blueprint asset after applying metadata

        Returns:
            Response containing the custom event node, stored metadata, compile status, and save status
        """
        try:
            target_event_name = custom_event_name or event_name
            target_description = description or tooltip
            if not target_event_name and not node_id:
                return tool_error(
                    "custom_event_name or node_id is required",
                    error_code="missing_custom_event_identifier",
                )
            if target_description:
                reject_mcp_dependency_reference("description", target_description)

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "set_node_comment": set_node_comment,
                "compile": compile,
                "save": save,
            }
            if target_event_name:
                params["custom_event_name"] = target_event_name
            if node_id:
                params["node_id"] = node_id
            if target_description:
                params["description"] = target_description
            if call_in_editor is not None:
                params["call_in_editor"] = call_in_editor

            return send_node_command("set_blueprint_custom_event_metadata", params)
        except Exception as e:
            error_msg = f"Error setting Blueprint custom event metadata: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)
    
    @mcp.tool()
    def add_blueprint_event_node(
        ctx: Context,
        blueprint_name: str,
        event_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add an event node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            event_name: Name of the event. Use 'Receive' prefix for standard events:
                       - 'ReceiveBeginPlay' for Begin Play
                       - 'ReceiveTick' for Tick
                       - etc.
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle default value within the method body
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "event_name": event_name,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            logger.info(f"Adding event node '{event_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_event_node", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Event node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding event node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)
    
    @mcp.tool()
    def add_blueprint_input_action_node(
        ctx: Context,
        blueprint_name: str,
        action_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add an input action event node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            action_name: Name of the input action to respond to
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle default value within the method body
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "action_name": action_name,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            logger.info(f"Adding input action node for '{action_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_input_action_node", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Input action node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding input action node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_input_key_node(
        ctx: Context,
        blueprint_name: str,
        key: str,
        node_position = None,
        control: bool = False,
        alt: bool = False,
        shift: bool = False,
        command: bool = False,
        consume_input: bool = True,
        execute_when_paused: bool = False,
        override_parent_binding: bool = False,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """Add a literal keyboard or button event node without changing project input mappings."""
        from unreal_mcp_server import get_unreal_connection

        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "key": key,
                "node_position": node_position,
                "control": control,
                "alt": alt,
                "shift": shift,
                "command": command,
                "consume_input": consume_input,
                "execute_when_paused": execute_when_paused,
                "override_parent_binding": override_parent_binding,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            response = unreal.send_command("add_blueprint_input_key_node", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            return response
        except Exception as e:
            error_msg = f"Error adding input key node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)
    
    @mcp.tool()
    def add_blueprint_function_node(
        ctx: Context,
        blueprint_name: str,
        target: str,
        function_name: str,
        params = None,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a function call node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            target: Target object for the function (component name or self)
            function_name: Name of the function to call
            params: Optional parameters to set on the function node
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle default values within the method body
            if params is None:
                params = {}
            if node_position is None:
                node_position = [0, 0]
            
            command_params = {
                "blueprint_name": blueprint_name,
                "target": target,
                "function_name": function_name,
                "params": params,
                "node_position": node_position
            }
            add_graph_selector(command_params, graph_name, graph_id, graph_type, create_graph_if_missing)
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            logger.info(f"Adding function node '{function_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_function_node", command_params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Function node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding function node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_call_function_node(
        ctx: Context,
        blueprint_name: str,
        function_class: str,
        function_name: str,
        param_defaults: Optional[Dict[str, Any]] = None,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
        allow_latent: bool = False,
        allow_editor_only: bool = False,
        allow_wildcard: bool = False,
        allow_deprecated: bool = False,
        allow_internal: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a validated Blueprint function call node.

        Args:
            blueprint_name: Blueprint name or path
            function_class: Owner class path/name, preferably /Script/Module.ClassName
            function_name: UFunction name to call
            param_defaults: Optional input pin defaults validated through the K2 schema
            node_position: Optional [X, Y] position in the graph
            allow_latent: Allow latent functions
            allow_editor_only: Allow editor-only functions
            allow_wildcard: Allow wildcard/custom thunk functions
            allow_deprecated: Allow deprecated functions
            allow_internal: Allow Blueprint-internal functions

        Returns:
            Response containing the created node metadata and pins
        """
        try:
            reject_mcp_dependency_reference("function_class", function_class)
            reject_mcp_dependency_references("param_defaults", param_defaults or {})
            if param_defaults is None:
                param_defaults = {}
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "function_class": function_class,
                "function_name": function_name,
                "param_defaults": param_defaults,
                "node_position": node_position,
                "allow_latent": allow_latent,
                "allow_editor_only": allow_editor_only,
                "allow_wildcard": allow_wildcard,
                "allow_deprecated": allow_deprecated,
                "allow_internal": allow_internal,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_call_function_node", params)
        except Exception as e:
            error_msg = f"Error adding validated function call node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_get_editor_subsystem_node(
        ctx: Context,
        blueprint_name: str,
        subsystem_class: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a Get Editor Subsystem node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            subsystem_class: Editor subsystem class path/name, for example /Script/UnrealEd.UnrealEditorSubsystem
            node_position: Optional [X, Y] position in the graph
        """
        try:
            reject_mcp_dependency_reference("subsystem_class", subsystem_class)
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "subsystem_class": subsystem_class,
                "node_position": node_position,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_get_editor_subsystem_node", params)
        except Exception as e:
            error_msg = f"Error adding Get Editor Subsystem node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_get_subsystem_from_pc_node(
        ctx: Context,
        blueprint_name: str,
        subsystem_class: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a Get Subsystem From Player Controller node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            subsystem_class: LocalPlayerSubsystem class path/name
            node_position: Optional [X, Y] position in the graph
        """
        try:
            reject_mcp_dependency_reference("subsystem_class", subsystem_class)
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "subsystem_class": subsystem_class,
                "node_position": node_position,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_get_subsystem_from_pc_node", params)
        except Exception as e:
            error_msg = f"Error adding Get Subsystem From Player Controller node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_construct_object_node(
        ctx: Context,
        blueprint_name: str,
        object_class: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a Construct Object node with its class pin set.

        Args:
            blueprint_name: Blueprint name or path
            object_class: UObject-derived class path/name to construct
            node_position: Optional [X, Y] position in the graph
        """
        try:
            reject_mcp_dependency_reference("object_class", object_class)
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "object_class": object_class,
                "node_position": node_position,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_construct_object_node", params)
        except Exception as e:
            error_msg = f"Error adding Construct Object node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)
            
    @mcp.tool()
    def connect_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        source_node_id: str,
        source_pin: str,
        target_node_id: str,
        target_pin: str,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        allow_pin_link_replacement: bool = False
    ) -> Dict[str, Any]:
        """
        Connect two nodes in a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            source_node_id: ID of the source node
            source_pin: Name of the output pin on the source node
            target_node_id: ID of the target node
            target_pin: Name of the input pin on the target node
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "source_node_id": source_node_id,
                "source_pin": source_pin,
                "target_node_id": target_node_id,
                "target_pin": target_pin,
                "allow_pin_link_replacement": allow_pin_link_replacement
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            logger.info(f"Connecting nodes in blueprint '{blueprint_name}'")
            response = unreal.send_command("connect_blueprint_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Node connection response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error connecting nodes: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def connect_blueprint_graph_pins(
        ctx: Context,
        blueprint_name: str,
        source_node_id: str,
        source_pin: str,
        target_node_id: str,
        target_pin: str,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        allow_pin_link_replacement: bool = False,
        compile: bool = False,
        save_asset: bool = False,
    ) -> Dict[str, Any]:
        """
        Connect two Blueprint graph pins through the graph schema.
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "source_node_id": source_node_id,
                "source_pin": source_pin,
                "target_node_id": target_node_id,
                "target_pin": target_pin,
                "allow_pin_link_replacement": allow_pin_link_replacement,
                "compile": compile,
                "save_asset": save_asset,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("connect_blueprint_graph_pins", params)
        except Exception as e:
            error_msg = f"Error connecting Blueprint graph pins: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def collapse_blueprint_nodes_to_function(
        ctx: Context,
        blueprint_name: str,
        graph_name: str,
        function_name: str,
        node_ids: list[str],
        graph_type: str = "event",
        expected_node_count: int = -1,
        allow_non_temp: bool = False,
        dry_run: bool = True,
        compile: bool = True,
        save_asset: bool = False,
    ) -> Dict[str, Any]:
        """Collapse a guarded Blueprint node selection with Unreal's native function extraction."""
        try:
            params = {
                "blueprint_name": blueprint_name,
                "graph_name": graph_name,
                "graph_type": graph_type,
                "function_name": function_name,
                "node_ids": node_ids,
                "expected_node_count": expected_node_count,
                "allow_non_temp": allow_non_temp,
                "dry_run": dry_run,
                "compile": compile,
                "save_asset": save_asset,
            }
            return send_node_command("collapse_blueprint_nodes_to_function", params)
        except Exception as e:
            error_msg = f"Error collapsing Blueprint nodes to a function: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def import_blueprint_graph_nodes_from_text(
        ctx: Context,
        blueprint_name: str,
        text: str = "",
        text_file_path: str = "",
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        clear_graph: bool = False,
        keep_root_node: bool = True,
        allow_non_sample: bool = False,
        validate_only: bool = False,
        dry_run: bool = False,
        compile: bool = True,
        save_asset: bool = False,
    ) -> Dict[str, Any]:
        """
        Import copied Blueprint graph node text into a target graph.
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "text": text,
                "text_file_path": text_file_path,
                "clear_graph": clear_graph,
                "keep_root_node": keep_root_node,
                "allow_non_sample": allow_non_sample,
                "validate_only": validate_only,
                "dry_run": dry_run,
                "compile": compile,
                "save_asset": save_asset,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("import_blueprint_graph_nodes_from_text", params)
        except Exception as e:
            error_msg = f"Error importing Blueprint graph nodes from text: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def repair_blueprint_pin_links(
        ctx: Context,
        blueprint_name: str,
        allow_non_sample: bool = False,
        validate_only: bool = False,
        dry_run: bool = False,
        compile: bool = True,
        save_asset: bool = False,
    ) -> Dict[str, Any]:
        """
        Repair malformed Blueprint pin link arrays after graph text import.
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "allow_non_sample": allow_non_sample,
                "validate_only": validate_only,
                "dry_run": dry_run,
                "compile": compile,
                "save_asset": save_asset,
            }
            return send_node_command("repair_blueprint_pin_links", params)
        except Exception as e:
            error_msg = f"Error repairing Blueprint pin links: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def delete_blueprint_node(
        ctx: Context,
        blueprint_name: str,
        node_id: str,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        expected_node_name: str = "",
        expected_node_class: str = "",
        expected_title_contains: str = "",
        allow_non_exec_linked_delete: bool = False,
        allow_exec_linked_delete: bool = False,
        allow_any_linked_delete: bool = False,
        allow_orphaned_event_delete: bool = False,
    ) -> Dict[str, Any]:
        """
        Delete one Blueprint graph node by GUID with safety guards.

        Args:
            blueprint_name: Name or asset path of the target Blueprint
            node_id: Node GUID to delete
            graph_name: Optional target graph name
            graph_id: Optional target graph GUID
            graph_type: Optional target graph type
            expected_node_name: Optional exact node object name guard
            expected_node_class: Optional node class guard, such as K2Node_VariableSet
            expected_title_contains: Optional title substring guard
            allow_non_exec_linked_delete: Allow deleting a node with data links
            allow_exec_linked_delete: Allow deleting a node with execution links
            allow_any_linked_delete: Allow deleting any linked node
            allow_orphaned_event_delete: Allow deleting a completely unlinked Event node when all expected-node guards are supplied
        """
        try:
            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "node_id": node_id,
                "expected_node_name": expected_node_name,
                "expected_node_class": expected_node_class,
                "expected_title_contains": expected_title_contains,
                "allow_non_exec_linked_delete": allow_non_exec_linked_delete,
                "allow_exec_linked_delete": allow_exec_linked_delete,
                "allow_any_linked_delete": allow_any_linked_delete,
                "allow_orphaned_event_delete": allow_orphaned_event_delete,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("delete_blueprint_node", params)
        except Exception as e:
            error_msg = f"Error deleting Blueprint node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)
    
    @mcp.tool()
    def create_user_defined_enum(
        ctx: Context,
        asset_path: str,
        display_names: List[str],
        tooltips: Optional[List[str]] = None,
        dry_run: bool = True,
        save: bool = False,
        skip_if_exists: bool = False,
    ) -> Dict[str, Any]:
        """Create a standard Unreal UserDefinedEnum asset through the native bridge.

        Args:
            asset_path: New enum package path under /Game, including the asset name.
            display_names: Ordered, unique display names; 1..32 entries.
            tooltips: Optional per-entry tooltips with the same length as display_names.
            dry_run: Validate and return the planned entries without creating an asset.
            save: Save the new asset package after creation.
            skip_if_exists: Return existing UserDefinedEnum readback instead of failing.
        """
        try:
            reject_mcp_dependency_reference("asset_path", asset_path)
            params: Dict[str, Any] = {
                "asset_path": asset_path,
                "display_names": display_names,
                "dry_run": dry_run,
                "save": save,
                "skip_if_exists": skip_if_exists,
            }
            if tooltips is not None:
                params["tooltips"] = tooltips
            return send_node_command("create_user_defined_enum", params)
        except Exception as e:
            error_msg = f"Error creating UserDefinedEnum: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        variable_type: str,
        is_exposed: bool = False,
        default_value: Any = None,
        category: str = "",
        tooltip: str = "",
        friendly_name: str = "",
        type_object: str = "",
        is_array: bool = False,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """
        Add a variable to a Blueprint.
        
        Args:
            blueprint_name: Name of the target Blueprint
            variable_name: Name of the variable
            variable_type: Type of the variable (Boolean, Integer, Float, Vector, etc.)
            is_exposed: Whether to expose the variable to the editor
            default_value: Optional default value
            category: Optional Blueprint variable category
            tooltip: Optional Blueprint variable tooltip
            friendly_name: Optional display name
            type_object: Optional class path/name for object or class variables
            is_array: Whether to create an array variable
            metadata: Optional Blueprint variable metadata
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
                "variable_type": variable_type,
                "is_exposed": is_exposed,
                "is_array": is_array,
            }
            if default_value is not None:
                params["default_value"] = default_value
            if category:
                params["category"] = category
            if tooltip:
                params["tooltip"] = tooltip
            if friendly_name:
                params["friendly_name"] = friendly_name
            if type_object:
                params["type_object"] = type_object
            if metadata:
                params["metadata"] = metadata
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            logger.info(f"Adding variable '{variable_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_variable", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Variable creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding variable: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_blueprint_variables(
        ctx: Context,
        blueprint_name: str,
    ) -> Dict[str, Any]:
        """
        Inspect Blueprint member variables and category sorting metadata.

        Args:
            blueprint_name: Name or asset path of the target Blueprint.

        Returns:
            Response containing new/generated variables, pin types, metadata, and category sorting.
        """
        try:
            return send_node_command(
                "inspect_blueprint_variables",
                {"blueprint_name": blueprint_name},
            )
        except Exception as e:
            error_msg = f"Error inspecting Blueprint variables: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_blueprint_variable_metadata(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
    ) -> Dict[str, Any]:
        """Inspect the exact metadata stored on one Blueprint member variable.

        Args:
            blueprint_name: Name or asset path of the target Blueprint.
            variable_name: Exact member-variable name to inspect.

        Returns:
            A read-only response with ordered metadata entries, a convenience
            metadata object, and dirty-state evidence.
        """
        try:
            return send_node_command(
                "inspect_blueprint_variable_metadata",
                {
                    "blueprint_name": blueprint_name,
                    "variable_name": variable_name,
                },
            )
        except Exception as e:
            error_msg = f"Error inspecting Blueprint variable metadata: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_variable_default(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        default_value: Any,
        compile: bool = True,
        save: bool = True,
        dry_run: bool = False,
    ) -> Dict[str, Any]:
        """Set the persisted default of an existing Blueprint member variable.

        Args:
            blueprint_name: Name or asset path of the target Blueprint.
            variable_name: Existing member variable declared by the Blueprint.
            default_value: New type-checked default value.
            compile: Compile the Blueprint so its generated CDO receives the default.
            save: Save the Blueprint after a successful update/compile.
            dry_run: Validate and report the change without mutating the Blueprint.

        Returns:
            Stored default before/after, compile/save state, and generated CDO readback.
        """
        try:
            reject_mcp_dependency_references("default_value", default_value)
            return send_node_command(
                "set_blueprint_variable_default",
                {
                    "blueprint_name": blueprint_name,
                    "variable_name": variable_name,
                    "default_value": default_value,
                    "compile": compile,
                    "save": save,
                    "dry_run": dry_run,
                },
            )
        except Exception as e:
            error_msg = f"Error setting Blueprint variable default: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_variable_order(
        ctx: Context,
        blueprint_name: str,
        variable_order: List[str],
        strict: bool = False,
        compile: bool = False,
        save: bool = False,
    ) -> Dict[str, Any]:
        """
        Reorder Blueprint member variables in the My Blueprint panel.

        Args:
            blueprint_name: Name or asset path of the target Blueprint.
            variable_order: Ordered variable names to move to the front.
            strict: Fail when any requested variable is missing.
            compile: Compile the Blueprint after changing the order.
            save: Save the Blueprint after changing the order.

        Returns:
            Response containing previous/new order, missing variables, and compile/save status.
        """
        try:
            return send_node_command(
                "set_blueprint_variable_order",
                {
                    "blueprint_name": blueprint_name,
                    "variable_order": variable_order,
                    "strict": strict,
                    "compile": compile,
                    "save": save,
                },
            )
        except Exception as e:
            error_msg = f"Error setting Blueprint variable order: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def remove_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
    ) -> Dict[str, Any]:
        """
        Remove one member variable declared on a Blueprint.

        Args:
            blueprint_name: Name or path of the target Blueprint
            variable_name: Member variable name to remove
        """
        try:
            reject_mcp_dependency_reference("variable_name", variable_name)
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
            }
            return send_node_command("remove_blueprint_variable", params)
        except Exception as e:
            error_msg = f"Error removing variable: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_variable_metadata(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        metadata: Optional[Dict[str, Any]] = None,
        is_editable: Optional[bool] = None,
        instance_editable: Optional[bool] = None,
        expose_on_spawn: Optional[bool] = None,
    ) -> Dict[str, Any]:
        """
        Set metadata or editor exposure flags on an existing Blueprint member variable.

        Args:
            blueprint_name: Name or asset path of the target Blueprint.
            variable_name: Existing Blueprint member variable name.
            metadata: Metadata key/value pairs, such as ClampMin, ClampMax, UIMin, UIMax, or ToolTip.
            is_editable: Optional alias for making the variable editable on instances.
            instance_editable: Optional explicit Instance Editable flag.
            expose_on_spawn: Optional Expose on Spawn flag.

        Returns:
            Response containing applied metadata, verified metadata, and flag edits.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
            }
            if metadata:
                params["metadata"] = metadata
            if is_editable is not None:
                params["is_editable"] = is_editable
            if instance_editable is not None:
                params["instance_editable"] = instance_editable
            if expose_on_spawn is not None:
                params["expose_on_spawn"] = expose_on_spawn

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            logger.info(
                "Setting metadata on variable '%s' in blueprint '%s'",
                variable_name,
                blueprint_name,
            )
            response = unreal.send_command("set_blueprint_variable_metadata", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()

            logger.info(f"Variable metadata response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting variable metadata: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_category_sorting(
        ctx: Context,
        blueprint_name: str,
        category_order: Optional[List[str]] = None,
        category_to_front: str = "",
        append_existing: bool = True,
        save: bool = True,
        compile: bool = False,
    ) -> Dict[str, Any]:
        """
        Set user-defined Blueprint category order for Details and My Blueprint panels.

        Args:
            blueprint_name: Name or asset path of the target Blueprint.
            category_order: Ordered category display names to put first.
            category_to_front: Optional single category shortcut.
            append_existing: Preserve existing Blueprint category order after requested categories.
            save: Save the Blueprint after changing the order.
            compile: Compile the Blueprint inside the native command if the order changes.

        Returns:
            Response containing previous_category_order, category_order, changed, saved, and compiled.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "append_existing": append_existing,
                "save": save,
                "compile": compile,
            }
            if category_order:
                params["category_order"] = category_order
            if category_to_front:
                params["category_to_front"] = category_to_front

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()

            logger.info("Setting Blueprint category sorting for '%s'", blueprint_name)
            response = unreal.send_command("set_blueprint_category_sorting", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()

            logger.info(f"Blueprint category sorting response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting Blueprint category sorting: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def bind_pcg_generation_settings_to_blueprint(
        ctx: Context,
        blueprint_name: str,
        component_variable: str = "ForestPCG",
        generation_trigger_variable: str = "PCGGenerationTrigger",
        regenerate_in_editor_variable: str = "PCGRegenerateInEditor",
        generate_on_drop_variable: str = "",
        generate_on_drop_default: bool = False,
        graph_name: str = "UserConstructionScript",
        graph_id: str = "",
        graph_type: str = "function",
        replace_existing: bool = True,
        sync_component_template_defaults: bool = True,
        function_class: str = "",
        function_name: str = "ApplyPCGGenerationSettings",
        compile: bool = False,
        save: bool = False,
    ) -> Dict[str, Any]:
        """
        Bind exposed Blueprint PCG generation variables to a PCG component in Construction Script.

        Args:
            blueprint_name: Name or asset path of the target Blueprint.
            component_variable: PCG component variable name, usually ForestPCG.
            generation_trigger_variable: Blueprint enum variable for GenerationTrigger.
            regenerate_in_editor_variable: Blueprint bool variable for bRegenerateInEditor.
            generate_on_drop_variable: Optional bool variable for on-demand drop generation.
            generate_on_drop_default: Default used when no generate_on_drop_variable is provided.
            graph_name: Construction Script graph name.
            graph_id: Optional exact graph GUID selector.
            graph_type: Target graph type, normally function.
            replace_existing: Remove and replace previous ApplyPCGGenerationSettings nodes.
            sync_component_template_defaults: Also sync PCG component template defaults from BP variable defaults.
            function_class: Runtime BlueprintFunctionLibrary class used for the Apply node.
            function_name: Blueprint callable function name used for the Apply node.
            compile: Compile the Blueprint after graph mutation.
            save: Save the Blueprint after graph mutation.

        Returns:
            Response containing created node IDs, graph metadata, and template sync status.
        """
        try:
            reject_mcp_dependency_reference("function_class", function_class)
            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "component_variable": component_variable,
                "generation_trigger_variable": generation_trigger_variable,
                "regenerate_in_editor_variable": regenerate_in_editor_variable,
                "generate_on_drop_default": generate_on_drop_default,
                "graph_name": graph_name,
                "graph_id": graph_id,
                "graph_type": graph_type,
                "replace_existing": replace_existing,
                "sync_component_template_defaults": sync_component_template_defaults,
                "function_name": function_name,
                "compile": compile,
                "save": save,
            }
            if function_class:
                params["function_class"] = function_class
            if generate_on_drop_variable:
                params["generate_on_drop_variable"] = generate_on_drop_variable

            return send_node_command("bind_pcg_generation_settings_to_blueprint", params)

        except Exception as e:
            error_msg = f"Error binding PCG generation settings to Blueprint: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_function_parameter(
        ctx: Context,
        blueprint_name: str,
        parameter_name: str,
        parameter_type: str,
        direction: str = "input",
        default_value: Any = None,
        type_object: str = "",
        is_array: bool = False,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "function",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add an input or output parameter pin to a Blueprint function graph.

        Args:
            blueprint_name: Blueprint name or path
            parameter_name: Function parameter name
            parameter_type: Type descriptor such as bool, int, float, string, vector, object, or class
            direction: input or output
            default_value: Optional default value for the created pin
            type_object: Optional class path/name for object or class parameters
            is_array: Whether to create an array parameter
        """
        try:
            reject_mcp_dependency_reference("type_object", type_object)
            reject_mcp_dependency_references("default_value", default_value)
            params = {
                "blueprint_name": blueprint_name,
                "parameter_name": parameter_name,
                "parameter_type": parameter_type,
                "direction": direction,
                "is_array": is_array,
            }
            if default_value is not None:
                params["default_value"] = default_value
            if type_object:
                params["type_object"] = type_object
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_function_parameter", params)
        except Exception as e:
            error_msg = f"Error adding function parameter: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_local_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        variable_type: str,
        default_value: Any = None,
        category: str = "",
        tooltip: str = "",
        type_object: str = "",
        is_array: bool = False,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "function",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a local variable declaration to a Blueprint function graph.

        Args:
            blueprint_name: Blueprint name or path
            variable_name: Local variable name
            variable_type: Type descriptor such as bool, int, float, string, vector, object, or class
            default_value: Optional default value
            category: Optional local variable category
            tooltip: Optional local variable tooltip
            type_object: Optional class path/name for object or class variables
            is_array: Whether to create an array local variable
        """
        try:
            reject_mcp_dependency_reference("type_object", type_object)
            reject_mcp_dependency_references("default_value", default_value)
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
                "variable_type": variable_type,
                "is_array": is_array,
            }
            if default_value is not None:
                params["default_value"] = default_value
            if category:
                params["category"] = category
            if tooltip:
                params["tooltip"] = tooltip
            if type_object:
                params["type_object"] = type_object
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_local_variable", params)
        except Exception as e:
            error_msg = f"Error adding local variable: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)
    
    @mcp.tool()
    def add_blueprint_get_self_component_reference(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a node that gets a reference to a component owned by the current Blueprint.
        This creates a node similar to what you get when dragging a component from the Components panel.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the component to get a reference to
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle None case explicitly in the function
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            logger.info(f"Adding self component reference node for '{component_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_get_self_component_reference", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Self component reference node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding self component reference node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_event_dispatcher(
        ctx: Context,
        blueprint_name: str,
        dispatcher_name: str,
        inputs: Optional[List[Dict[str, Any]]] = None,
        category: str = "",
        tooltip: str = "",
        friendly_name: str = "",
    ) -> Dict[str, Any]:
        """
        Add an Event Dispatcher declaration to a Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint
            dispatcher_name: Name of the Event Dispatcher
            inputs: Optional signature inputs. Each item accepts name/parameter_name and type/parameter_type.
            category: Optional Blueprint category
            tooltip: Optional Blueprint tooltip
            friendly_name: Optional display name

        Returns:
            Response containing the dispatcher name, multicast delegate pin type, and signature graph
        """
        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "dispatcher_name": dispatcher_name,
        }
        if inputs:
            params["inputs"] = inputs
        if category:
            params["category"] = category
        if tooltip:
            params["tooltip"] = tooltip
        if friendly_name:
            params["friendly_name"] = friendly_name

        try:
            logger.info(f"Adding Event Dispatcher '{dispatcher_name}' to blueprint '{blueprint_name}'")
            return send_node_command("add_blueprint_event_dispatcher", params)
        except Exception as e:
            error_msg = f"Error adding Event Dispatcher: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_event_dispatcher_call_node(
        ctx: Context,
        blueprint_name: str,
        dispatcher_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a call node for an existing Blueprint Event Dispatcher.

        Args:
            blueprint_name: Name of the target Blueprint
            dispatcher_name: Name of the Event Dispatcher to call
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name
            graph_id: Optional target graph id
            graph_type: Optional target graph type
            create_graph_if_missing: Whether to create the target graph when allowed

        Returns:
            Response containing the node id, pins, dispatcher name, signature function, and graph metadata
        """
        if node_position is None:
            node_position = [0, 0]

        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "dispatcher_name": dispatcher_name,
            "node_position": node_position,
        }
        add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)

        try:
            logger.info(f"Adding Event Dispatcher call node '{dispatcher_name}' to blueprint '{blueprint_name}'")
            return send_node_command("add_blueprint_event_dispatcher_call_node", params)
        except Exception as e:
            error_msg = f"Error adding Event Dispatcher call node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_custom_event_node(
        ctx: Context,
        blueprint_name: str,
        custom_event_name: str,
        node_position = None,
        signature_source_dispatcher_name: str = "",
        signature_source_class: str = "",
        signature_source_delegate_name: str = "",
        native_delegate_name: str = "",
        call_in_editor: Optional[bool] = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a custom event node to a Blueprint event graph.

        Args:
            blueprint_name: Name of the target Blueprint
            custom_event_name: Name of the custom event
            node_position: Optional [X, Y] position in the graph
            signature_source_dispatcher_name: Optional Event Dispatcher whose signature should be copied
            signature_source_class: Optional native class path that owns a BlueprintAssignable delegate
            signature_source_delegate_name: Optional native delegate property name whose signature should be copied
            native_delegate_name: Alias for signature_source_delegate_name
            call_in_editor: Optional value for the custom event's Call In Editor flag
            graph_name: Optional target graph name
            graph_id: Optional target graph id
            graph_type: Optional target graph type; custom events require an event graph
            create_graph_if_missing: Whether to create the target graph when supported

        Returns:
            Response containing the node id, pins, custom event name, signature function, and graph metadata
        """
        if node_position is None:
            node_position = [0, 0]

        if native_delegate_name and not signature_source_delegate_name:
            signature_source_delegate_name = native_delegate_name

        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "custom_event_name": custom_event_name,
            "node_position": node_position,
        }
        if signature_source_dispatcher_name:
            params["signature_source_dispatcher_name"] = signature_source_dispatcher_name
        if signature_source_class:
            try:
                reject_mcp_dependency_reference("signature_source_class", signature_source_class)
            except Exception as e:
                return _node_exception_error(str(e), e)
            params["signature_source_class"] = signature_source_class
        if signature_source_delegate_name:
            params["signature_source_delegate_name"] = signature_source_delegate_name
        if call_in_editor is not None:
            params["call_in_editor"] = call_in_editor
        add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)

        try:
            logger.info(f"Adding custom event node '{custom_event_name}' to blueprint '{blueprint_name}'")
            return send_node_command("add_blueprint_custom_event_node", params)
        except Exception as e:
            error_msg = f"Error adding custom event node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_event_dispatcher_bind_node(
        ctx: Context,
        blueprint_name: str,
        dispatcher_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a bind node for an existing Blueprint Event Dispatcher.

        Args:
            blueprint_name: Name of the target Blueprint
            dispatcher_name: Name of the Event Dispatcher to bind
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name
            graph_id: Optional target graph id
            graph_type: Optional target graph type such as event or function
            create_graph_if_missing: Whether to create the target graph when supported

        Returns:
            Response containing the node id, pins, dispatcher name, signature function, and graph metadata
        """
        if node_position is None:
            node_position = [0, 0]

        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "dispatcher_name": dispatcher_name,
            "node_position": node_position,
        }
        add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)

        try:
            logger.info(f"Adding Event Dispatcher bind node '{dispatcher_name}' to blueprint '{blueprint_name}'")
            return send_node_command("add_blueprint_event_dispatcher_bind_node", params)
        except Exception as e:
            error_msg = f"Error adding Event Dispatcher bind node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_native_delegate_bind_node(
        ctx: Context,
        blueprint_name: str,
        delegate_owner_class: str,
        delegate_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a bind node for a native BlueprintAssignable multicast delegate.

        Args:
            blueprint_name: Name of the target Blueprint
            delegate_owner_class: Native class path or name that owns the delegate property
            delegate_name: Native delegate property name
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name
            graph_id: Optional target graph id
            graph_type: Optional target graph type such as event or function
            create_graph_if_missing: Whether to create the target graph when supported

        Returns:
            Response containing the node id, pins, delegate name, signature function, and graph metadata
        """
        if node_position is None:
            node_position = [0, 0]

        try:
            reject_mcp_dependency_reference("delegate_owner_class", delegate_owner_class)
            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "delegate_owner_class": delegate_owner_class,
                "delegate_name": delegate_name,
                "node_position": node_position,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            logger.info(
                "Adding native delegate bind node '%s' from '%s' to blueprint '%s'",
                delegate_name,
                delegate_owner_class,
                blueprint_name,
            )
            return send_node_command("add_blueprint_native_delegate_bind_node", params)
        except Exception as e:
            error_msg = f"Error adding native delegate bind node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_interface_message_node(
        ctx: Context,
        blueprint_name: str,
        interface_class: str,
        function_name: str,
        target_node_id: str,
        target_pin_name: str = "ReturnValue",
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a Blueprint interface message call node and connect its target object pin.

        Args:
            blueprint_name: Name of the target Blueprint
            interface_class: Interface class path/name, including Blueprint interface generated classes
            function_name: Interface function name to call
            target_node_id: Node id that supplies the message target object
            target_pin_name: Output pin on target_node_id to connect to the message target
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name
            graph_id: Optional target graph id
            graph_type: Optional target graph type such as event or function
            create_graph_if_missing: Whether to create the target graph when supported

        Returns:
            Response containing the node id, pins, interface class, function, and graph metadata
        """
        if node_position is None:
            node_position = [0, 0]

        try:
            reject_mcp_dependency_reference("interface_class", interface_class)
            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "interface_class": interface_class,
                "function_name": function_name,
                "target_node_id": target_node_id,
                "target_pin_name": target_pin_name,
                "node_position": node_position,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            logger.info(
                "Adding interface message node '%s.%s' to blueprint '%s'",
                interface_class,
                function_name,
                blueprint_name,
            )
            return send_node_command("add_blueprint_interface_message_node", params)
        except Exception as e:
            error_msg = f"Error adding interface message node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_create_delegate_node(
        ctx: Context,
        blueprint_name: str,
        function_name: str,
        object_node_id: str,
        delegate_target_node_id: str,
        object_pin_name: str = "ReturnValue",
        delegate_target_pin_name: str = "Delegate",
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a Create Event delegate node, bind it to an object function, and connect it to a delegate input.

        Args:
            blueprint_name: Name of the target Blueprint
            function_name: Function/event name to bind on the supplied object
            object_node_id: Node id that supplies the object instance for the delegate
            delegate_target_node_id: Node id containing the delegate input pin
            object_pin_name: Output pin on object_node_id, usually ReturnValue
            delegate_target_pin_name: Delegate input pin name on delegate_target_node_id
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name
            graph_id: Optional target graph id
            graph_type: Optional target graph type such as event or function
            create_graph_if_missing: Whether to create the target graph when supported

        Returns:
            Response containing the node id, pins, selected function, and graph metadata
        """
        if node_position is None:
            node_position = [0, 0]

        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "function_name": function_name,
            "object_node_id": object_node_id,
            "object_pin_name": object_pin_name,
            "delegate_target_node_id": delegate_target_node_id,
            "delegate_target_pin_name": delegate_target_pin_name,
            "node_position": node_position,
        }
        add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)

        try:
            logger.info(
                "Adding Create Event delegate node for '%s' to blueprint '%s'",
                function_name,
                blueprint_name,
            )
            return send_node_command("add_blueprint_create_delegate_node", params)
        except Exception as e:
            error_msg = f"Error adding Create Event delegate node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_event_dispatcher_unbind_node(
        ctx: Context,
        blueprint_name: str,
        dispatcher_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add an unbind node for an existing Blueprint Event Dispatcher.

        Args:
            blueprint_name: Name of the target Blueprint
            dispatcher_name: Name of the Event Dispatcher to unbind
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name
            graph_id: Optional target graph id
            graph_type: Optional target graph type such as event or function
            create_graph_if_missing: Whether to create the target graph when supported

        Returns:
            Response containing the node id, pins, dispatcher name, signature function, and graph metadata
        """
        if node_position is None:
            node_position = [0, 0]

        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "dispatcher_name": dispatcher_name,
            "node_position": node_position,
        }
        add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)

        try:
            logger.info(f"Adding Event Dispatcher unbind node '{dispatcher_name}' to blueprint '{blueprint_name}'")
            return send_node_command("add_blueprint_event_dispatcher_unbind_node", params)
        except Exception as e:
            error_msg = f"Error adding Event Dispatcher unbind node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_event_dispatcher_clear_node(
        ctx: Context,
        blueprint_name: str,
        dispatcher_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add a clear node for an existing Blueprint Event Dispatcher.

        Args:
            blueprint_name: Name of the target Blueprint
            dispatcher_name: Name of the Event Dispatcher to clear
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name
            graph_id: Optional target graph id
            graph_type: Optional target graph type such as event or function
            create_graph_if_missing: Whether to create the target graph when supported

        Returns:
            Response containing the node id, pins, dispatcher name, signature function, and graph metadata
        """
        if node_position is None:
            node_position = [0, 0]

        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "dispatcher_name": dispatcher_name,
            "node_position": node_position,
        }
        add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)

        try:
            logger.info(f"Adding Event Dispatcher clear node '{dispatcher_name}' to blueprint '{blueprint_name}'")
            return send_node_command("add_blueprint_event_dispatcher_clear_node", params)
        except Exception as e:
            error_msg = f"Error adding Event Dispatcher clear node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_event_dispatcher_assign_node(
        ctx: Context,
        blueprint_name: str,
        dispatcher_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False,
    ) -> Dict[str, Any]:
        """
        Add an assign node for an existing Blueprint Event Dispatcher.

        Args:
            blueprint_name: Name of the target Blueprint
            dispatcher_name: Name of the Event Dispatcher to assign
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name
            graph_id: Optional target graph id
            graph_type: Optional target graph type; assign nodes require an event graph
            create_graph_if_missing: Whether to create the target graph when supported

        Returns:
            Response containing the node id, pins, dispatcher name, signature function, and graph metadata
        """
        if node_position is None:
            node_position = [0, 0]

        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "dispatcher_name": dispatcher_name,
            "node_position": node_position,
        }
        add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)

        try:
            logger.info(f"Adding Event Dispatcher assign node '{dispatcher_name}' to blueprint '{blueprint_name}'")
            return send_node_command("add_blueprint_event_dispatcher_assign_node", params)
        except Exception as e:
            error_msg = f"Error adding Event Dispatcher assign node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_self_reference(
        ctx: Context,
        blueprint_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a 'Get Self' node to a Blueprint's event graph that returns a reference to this actor.
        
        Args:
            blueprint_name: Name of the target Blueprint
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            if node_position is None:
                node_position = [0, 0]
                
            params = {
                "blueprint_name": blueprint_name,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            logger.info(f"Adding self reference node to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_self_reference", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Self reference node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding self reference node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)
    
    @mcp.tool()
    def find_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        node_type = None,
        event_type = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = ""
    ) -> Dict[str, Any]:
        """
        Find nodes in a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            node_type: Optional type of node to find (Event, Function, Variable, etc.)
            event_type: Optional specific event type to find (BeginPlay, Tick, etc.)
            
        Returns:
            Response containing array of found node IDs and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_type": node_type,
                "event_type": event_type,
                "event_name": event_type
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            logger.info(f"Finding nodes in blueprint '{blueprint_name}'")
            response = unreal.send_command("find_blueprint_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Node find response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error finding nodes: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def resolve_blueprint(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """
        Resolve a Blueprint by short name, object path, asset reference, or generated class path.

        Args:
            blueprint_name: Blueprint name or path to resolve

        Returns:
            Response containing the resolved asset path and candidate paths
        """
        try:
            return send_node_command("resolve_blueprint", {"blueprint_name": blueprint_name})
        except Exception as e:
            error_msg = f"Error resolving blueprint: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def list_blueprint_graphs(
        ctx: Context,
        blueprint_name: str,
        graph_type: str = ""
    ) -> Dict[str, Any]:
        """
        List graphs in a Blueprint.

        Args:
            blueprint_name: Blueprint name or path
            graph_type: Optional filter: event, function, macro, delegate, or any

        Returns:
            Response containing graph metadata including graph_id and graph_type
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "graph_type": graph_type
            }
            return send_node_command("list_blueprint_graphs", params)
        except Exception as e:
            error_msg = f"Error listing blueprint graphs: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def resolve_blueprint_graph(
        ctx: Context,
        blueprint_name: str,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Resolve or create a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            graph_name: Optional graph name
            graph_id: Optional graph GUID
            graph_type: event, function, macro, delegate, or any
            create_if_missing: Create a missing function graph when graph_type is function

        Returns:
            Response containing graph metadata
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "create_if_missing": create_if_missing
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("resolve_blueprint_graph", params)
        except Exception as e:
            error_msg = f"Error resolving blueprint graph: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def list_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        node_type: str = "",
        title_contains: str = "",
        include_pins: bool = True,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = ""
    ) -> Dict[str, Any]:
        """
        List nodes in a Blueprint event graph, optionally including pins and links.

        Args:
            blueprint_name: Blueprint name or path
            node_type: Optional class or title substring filter
            title_contains: Optional title substring filter
            include_pins: Include pin metadata and links in the result

        Returns:
            Response containing graph node metadata
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_type": node_type,
                "title_contains": title_contains,
                "include_pins": include_pins
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("list_blueprint_nodes", params)
        except Exception as e:
            error_msg = f"Error listing blueprint nodes: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_blueprint_graph_call_topology(
        ctx: Context,
        blueprint_name: str,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        graph_name_contains: str = "",
        node_type: str = "",
        title_contains: str = "",
        reference_contains: str = "",
        include_pins: bool = False,
        include_links: bool = True,
        include_references: bool = True,
        max_graphs: int = 64,
        max_nodes_per_graph: int = 512,
        max_links_per_graph: int = 1024,
        max_references_per_node: int = 64
    ) -> Dict[str, Any]:
        """
        Read static Blueprint graph call/link topology.

        This command is read-only. It reports graph nodes, classified K2 node kinds,
        function/variable/event member references, asset/reference paths, and pin links.
        It does not prove runtime execution.

        Args:
            blueprint_name: Blueprint name or path
            graph_name: Optional exact graph name selector
            graph_id: Optional graph GUID selector
            graph_type: Optional graph type selector: event, function, macro, delegate, or any
            graph_name_contains: Optional graph name substring filter
            node_type: Optional node class/title substring filter
            title_contains: Optional node title substring filter
            reference_contains: Optional substring filter across function, variable, event, pin, and reference text
            include_pins: Include full per-node pin metadata
            include_links: Include normalized pin links
            include_references: Include per-node reference_paths
            max_graphs: Maximum graphs to include, -1 for unlimited
            max_nodes_per_graph: Maximum matching nodes per graph, -1 for unlimited
            max_links_per_graph: Maximum links per graph, -1 for unlimited
            max_references_per_node: Maximum reference paths per node, -1 for unlimited

        Returns:
            Response containing static Blueprint graph topology.
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "graph_name_contains": graph_name_contains,
                "node_type": node_type,
                "title_contains": title_contains,
                "reference_contains": reference_contains,
                "include_pins": include_pins,
                "include_links": include_links,
                "include_references": include_references,
                "max_graphs": max_graphs,
                "max_nodes_per_graph": max_nodes_per_graph,
                "max_links_per_graph": max_links_per_graph,
                "max_references_per_node": max_references_per_node,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("inspect_blueprint_graph_call_topology", params)
        except Exception as e:
            error_msg = f"Error inspecting Blueprint graph call topology: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_controlrig_rigvm_topology(
        ctx: Context,
        control_rig_path: str = "",
        control_rig_asset_path: str = "",
        control_rig_class_path: str = "",
        include_pins: bool = True,
        include_sub_pins: bool = True,
        include_links: bool = True,
        include_pin_links: bool = False,
        max_nodes: int = 256,
        max_pins_per_node: int = 64,
        max_sub_pins_per_pin: int = 32,
        max_links: int = 512,
    ) -> Dict[str, Any]:
        """
        Read ControlRig Blueprint default RigVM topology.

        This command is read-only. It loads a ControlRig Blueprint asset or a
        generated ControlRig class, resolves the owning ControlRig Blueprint,
        and returns bounded RigVM graph nodes, pins, links, event metadata, and
        dirty-state guards. It does not mutate, compile, save, or execute the
        ControlRig. Use this before proposing RigVM link or pin-default authoring.

        Args:
            control_rig_path: ControlRig Blueprint asset path or generated class path
            control_rig_asset_path: Explicit ControlRig Blueprint asset path alias
            control_rig_class_path: Generated ControlRig class path alias
            include_pins: Include bounded per-node pin metadata
            include_sub_pins: Include bounded recursive sub-pin metadata
            include_links: Include graph-level RigVM links
            include_pin_links: Include per-pin link path summaries
            max_nodes: Maximum nodes to include
            max_pins_per_node: Maximum top-level pins per node
            max_sub_pins_per_pin: Maximum sub-pins per pin at each level
            max_links: Maximum graph links to include

        Returns:
            Read-only ControlRig RigVM topology response.
        """
        try:
            params: Dict[str, Any] = {
                "include_pins": include_pins,
                "include_sub_pins": include_sub_pins,
                "include_links": include_links,
                "include_pin_links": include_pin_links,
                "max_nodes": max_nodes,
                "max_pins_per_node": max_pins_per_node,
                "max_sub_pins_per_pin": max_sub_pins_per_pin,
                "max_links": max_links,
            }
            if control_rig_path:
                params["control_rig_path"] = control_rig_path
            if control_rig_asset_path:
                params["control_rig_asset_path"] = control_rig_asset_path
            if control_rig_class_path:
                params["control_rig_class_path"] = control_rig_class_path
            return send_node_command("inspect_controlrig_rigvm_topology", params)
        except Exception as e:
            error_msg = f"Error inspecting ControlRig RigVM topology: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def validate_controlrig_rigvm_links(
        ctx: Context,
        control_rig_path: str = "",
        control_rig_asset_path: str = "",
        control_rig_class_path: str = "",
        links: Optional[List[Any]] = None,
        unit_links: Optional[List[Any]] = None,
        allow_type_casting: bool = True,
        max_links: int = 64,
    ) -> Dict[str, Any]:
        """
        Validate proposed ControlRig RigVM links without mutating the graph.

        This command is read-only. It resolves a ControlRig Blueprint asset or
        generated ControlRig class, finds the requested RigVM pins, reports
        existing-link state, and calls Unreal's RigVM CanLink path for each
        bounded link spec. It does not add links, compile, save, execute, or
        solve the ControlRig.

        Args:
            control_rig_path: ControlRig Blueprint asset path or generated class path
            control_rig_asset_path: Explicit ControlRig Blueprint asset path alias
            control_rig_class_path: Generated ControlRig class path alias
            links: Link specs with source_pin_path/from and target_pin_path/to, or "Source -> Target" strings
            unit_links: Alias for links, useful when checking previously rejected unit link specs
            allow_type_casting: Allow RigVM type casting during CanLink validation
            max_links: Maximum link specs to validate

        Returns:
            Read-only ControlRig RigVM link feasibility response.
        """
        try:
            params: Dict[str, Any] = {
                "allow_type_casting": allow_type_casting,
                "max_links": max_links,
            }
            if control_rig_path:
                params["control_rig_path"] = control_rig_path
            if control_rig_asset_path:
                params["control_rig_asset_path"] = control_rig_asset_path
            if control_rig_class_path:
                params["control_rig_class_path"] = control_rig_class_path
            if links is not None:
                params["links"] = links
            if unit_links is not None:
                params["unit_links"] = unit_links
            return send_node_command("validate_controlrig_rigvm_links", params)
        except Exception as e:
            error_msg = f"Error validating ControlRig RigVM links: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def validate_controlrig_rigvm_pin_defaults(
        ctx: Context,
        control_rig_path: str = "",
        control_rig_asset_path: str = "",
        control_rig_class_path: str = "",
        pin_defaults: Optional[List[Any]] = None,
        defaults: Optional[List[Any]] = None,
        pins: Optional[List[Any]] = None,
        max_pins: int = 64,
    ) -> Dict[str, Any]:
        """
        Validate ControlRig RigVM pin default feasibility without writing values.

        This command is read-only. It resolves a ControlRig Blueprint asset or
        generated ControlRig class, finds requested RigVM pins, reports current
        default value metadata, and validates any proposed default strings with
        Unreal's RigVM pin default-value checks. It does not call
        SetPinDefaultValue, resize arrays, compile, save, execute, or solve.

        Args:
            control_rig_path: ControlRig Blueprint asset path or generated class path
            control_rig_asset_path: Explicit ControlRig Blueprint asset path alias
            control_rig_class_path: Generated ControlRig class path alias
            pin_defaults: Pin specs with pin_path/pin/path and optional default_value/value/default
            defaults: Alias for pin_defaults
            pins: Alias for pin_defaults, useful for read-only pin capability checks
            max_pins: Maximum pin specs to validate

        Returns:
            Read-only ControlRig RigVM pin default feasibility response.
        """
        try:
            params: Dict[str, Any] = {
                "max_pins": max_pins,
            }
            if control_rig_path:
                params["control_rig_path"] = control_rig_path
            if control_rig_asset_path:
                params["control_rig_asset_path"] = control_rig_asset_path
            if control_rig_class_path:
                params["control_rig_class_path"] = control_rig_class_path
            if pin_defaults is not None:
                params["pin_defaults"] = pin_defaults
            if defaults is not None:
                params["defaults"] = defaults
            if pins is not None:
                params["pins"] = pins
            return send_node_command("validate_controlrig_rigvm_pin_defaults", params)
        except Exception as e:
            error_msg = f"Error validating ControlRig RigVM pin defaults: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_controlrig_rigvm_links(
        ctx: Context,
        control_rig_path: str = "",
        control_rig_asset_path: str = "",
        control_rig_class_path: str = "",
        links: Optional[List[Any]] = None,
        unit_links: Optional[List[Any]] = None,
        allow_type_casting: bool = False,
        dry_run: bool = False,
        compile: bool = True,
        save_asset: bool = False,
        max_links: int = 64,
    ) -> Dict[str, Any]:
        """
        Add ControlRig RigVM links on a sample/temp ControlRig Blueprint.

        This command is a guarded writer. It refuses targets outside
        /Game/_MCP_Sample and /Game/_MCP_Temp, validates all requested links
        before mutation, and then calls Unreal's RigVM controller AddLink only
        for links that are feasible and not already connected. It does not
        create cast nodes, execute, solve, or modify original assets.

        Args:
            control_rig_path: Sample/temp ControlRig Blueprint asset path or generated class path
            control_rig_asset_path: Explicit ControlRig Blueprint asset path alias
            control_rig_class_path: Generated ControlRig class path alias
            links: Link specs with source_pin_path/from and target_pin_path/to, or "Source -> Target" strings
            unit_links: Alias for links, useful when applying reviewed unit link specs
            allow_type_casting: Allow type casting in feasibility checks; cast node creation remains disabled
            dry_run: Validate and report planned writes without mutation
            compile: Compile/reinitialize the target ControlRig after successful mutation
            save_asset: Save only the target sample/temp ControlRig asset after successful mutation
            max_links: Maximum link specs to process

        Returns:
            Sample/temp ControlRig RigVM link writer response.
        """
        try:
            params: Dict[str, Any] = {
                "allow_type_casting": allow_type_casting,
                "dry_run": dry_run,
                "compile": compile,
                "save_asset": save_asset,
                "max_links": max_links,
            }
            if control_rig_path:
                params["control_rig_path"] = control_rig_path
            if control_rig_asset_path:
                params["control_rig_asset_path"] = control_rig_asset_path
            if control_rig_class_path:
                params["control_rig_class_path"] = control_rig_class_path
            if links is not None:
                params["links"] = links
            if unit_links is not None:
                params["unit_links"] = unit_links
            return send_node_command("add_controlrig_rigvm_links", params)
        except Exception as e:
            error_msg = f"Error adding ControlRig RigVM links: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_controlrig_rigvm_pin_defaults(
        ctx: Context,
        control_rig_path: str = "",
        control_rig_asset_path: str = "",
        control_rig_class_path: str = "",
        pin_defaults: Optional[List[Any]] = None,
        defaults: Optional[List[Any]] = None,
        resize_arrays: bool = False,
        set_value_on_linked_pins: bool = False,
        dry_run: bool = False,
        compile: bool = True,
        save_asset: bool = False,
        max_pins: int = 64,
    ) -> Dict[str, Any]:
        """
        Set ControlRig RigVM pin defaults on a sample/temp ControlRig Blueprint.

        This command is a guarded writer. It refuses targets outside
        /Game/_MCP_Sample and /Game/_MCP_Temp, requires every requested pin spec
        to include a default value, validates all requested defaults before
        mutation, and then calls Unreal's RigVM controller SetPinDefaultValue.
        It does not execute, solve, or modify original assets.

        Args:
            control_rig_path: Sample/temp ControlRig Blueprint asset path or generated class path
            control_rig_asset_path: Explicit ControlRig Blueprint asset path alias
            control_rig_class_path: Generated ControlRig class path alias
            pin_defaults: Pin specs with pin_path/pin/path and default_value/value/default
            defaults: Alias for pin_defaults
            resize_arrays: Permit RigVM array resize while setting defaults
            set_value_on_linked_pins: Propagate value to linked pins
            dry_run: Validate and report planned writes without mutation
            compile: Compile/reinitialize the target ControlRig after successful mutation
            save_asset: Save only the target sample/temp ControlRig asset after successful mutation
            max_pins: Maximum pin specs to process

        Returns:
            Sample/temp ControlRig RigVM pin-default writer response.
        """
        try:
            params: Dict[str, Any] = {
                "resize_arrays": resize_arrays,
                "set_value_on_linked_pins": set_value_on_linked_pins,
                "dry_run": dry_run,
                "compile": compile,
                "save_asset": save_asset,
                "max_pins": max_pins,
            }
            if control_rig_path:
                params["control_rig_path"] = control_rig_path
            if control_rig_asset_path:
                params["control_rig_asset_path"] = control_rig_asset_path
            if control_rig_class_path:
                params["control_rig_class_path"] = control_rig_class_path
            if pin_defaults is not None:
                params["pin_defaults"] = pin_defaults
            if defaults is not None:
                params["defaults"] = defaults
            return send_node_command("set_controlrig_rigvm_pin_defaults", params)
        except Exception as e:
            error_msg = f"Error setting ControlRig RigVM pin defaults: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_anim_graph_protected_topology(
        ctx: Context,
        blueprint_name: str,
        node_id: str = "",
        node_type: str = "",
        title_contains: str = "",
        include_pins: bool = True,
        include_pose_pins: bool = True,
        include_links: bool = True,
        include_non_pose_links: bool = False,
        include_settings: bool = False,
        max_depth: int = 3,
        max_nodes: int = 512,
        max_links: int = 2048,
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function"
    ) -> Dict[str, Any]:
        """
        Read protected AnimGraph editor topology in a stable read-only format.

        This command reports AnimGraph nodes, pins, pose pins, and normalized links.
        It does not compile, save, modify assets, or prove runtime execution.

        Args:
            blueprint_name: Animation Blueprint name or path
            node_id: Optional node GUID filter
            node_type: Optional class or title substring filter
            title_contains: Optional title substring filter
            include_pins: Include full per-node pin metadata
            include_pose_pins: Include pose-pin summaries and preferred input/output pose pins
            include_links: Include normalized graph links
            include_non_pose_links: Include non-pose data links in addition to pose/component-pose links
            include_settings: Include reflected FAnimNode settings through the existing settings dumper
            max_depth: Maximum nested property depth when include_settings is true
            max_nodes: Maximum nodes to include, -1 for unlimited
            max_links: Maximum links to include, -1 for unlimited
            graph_name: Target animation graph name, defaults to AnimGraph
            graph_id: Optional target graph GUID
            graph_type: Graph selector type, defaults to function because AnimGraph is reported there

        Returns:
            Response containing static AnimGraph topology.
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_id": node_id,
                "node_type": node_type,
                "title_contains": title_contains,
                "include_pins": include_pins,
                "include_pose_pins": include_pose_pins,
                "include_links": include_links,
                "include_non_pose_links": include_non_pose_links,
                "include_settings": include_settings,
                "max_depth": max_depth,
                "max_nodes": max_nodes,
                "max_links": max_links,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("inspect_anim_graph_protected_topology", params)
        except Exception as e:
            error_msg = f"Error inspecting AnimGraph protected topology: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_anim_graph_node_settings(
        ctx: Context,
        blueprint_name: str,
        node_id: str = "",
        node_type: str = "",
        title_contains: str = "",
        include_pins: bool = True,
        max_depth: int = 4,
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function"
    ) -> Dict[str, Any]:
        """
        Read AnimGraph node runtime settings from the internal FAnimNode struct.

        Args:
            blueprint_name: Animation Blueprint name or path
            node_id: Optional node GUID filter
            node_type: Optional class or title substring filter
            title_contains: Optional title substring filter
            include_pins: Include pin metadata and links in the result
            max_depth: Maximum nested property depth for struct/array dumping
            graph_name: Target animation graph name, defaults to AnimGraph
            graph_id: Optional target graph GUID
            graph_type: Graph selector type, defaults to function because AnimGraph is reported there

        Returns:
            Response containing node metadata and reflected anim-node settings.
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_id": node_id,
                "node_type": node_type,
                "title_contains": title_contains,
                "include_pins": include_pins,
                "max_depth": max_depth,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("inspect_anim_graph_node_settings", params)
        except Exception as e:
            error_msg = f"Error inspecting AnimGraph node settings: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_anim_state_machine_transitions(
        ctx: Context,
        blueprint_name: str,
        state_machine_name: str = "",
        include_pins: bool = True,
        include_rule_graph_nodes: bool = True,
        include_state_nodes: bool = True,
        max_rule_graph_nodes: int = 256
    ) -> Dict[str, Any]:
        """
        Read Animation Blueprint state-machine transition topology.

        This command is read-only. It reports state-machine nodes, source/target states,
        transition rule graphs, rule graph nodes, and transition blend/priority settings.

        Args:
            blueprint_name: Animation Blueprint name or path
            state_machine_name: Optional state machine name/title substring filter
            include_pins: Include pin metadata and links on returned graph nodes
            include_rule_graph_nodes: Include nodes inside each transition rule graph
            include_state_nodes: Include state node summaries for each state machine
            max_rule_graph_nodes: Maximum rule graph nodes per transition, -1 for unlimited

        Returns:
            Response containing state machines and transition topology.
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "state_machine_name": state_machine_name,
                "include_pins": include_pins,
                "include_rule_graph_nodes": include_rule_graph_nodes,
                "include_state_nodes": include_state_nodes,
                "max_rule_graph_nodes": max_rule_graph_nodes,
            }
            return send_node_command("inspect_anim_state_machine_transitions", params)
        except Exception as e:
            error_msg = f"Error inspecting AnimBP state-machine transitions: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_animation_asset_metadata_guarded(
        ctx: Context,
        asset_path: str,
        include_notifies: bool = True,
        include_notify_details: bool = True,
        include_curves: bool = True,
        include_curve_keys: bool = False,
        include_sync_markers: bool = True,
        include_montage_sections: bool = True,
        include_montage_slots: bool = True,
        include_metadata: bool = False,
        max_notifies: int = 256,
        max_curves: int = 256,
        max_curve_keys_per_curve: int = 0,
        max_markers: int = 256,
        max_sections: int = 128,
        max_slots: int = 64,
        max_segments_per_slot: int = 256,
        require_supported_class: bool = False
    ) -> Dict[str, Any]:
        """
        Read source animation asset metadata without mutating assets.

        Supports UAnimSequenceBase/UAnimSequence/UAnimMontage metadata such as
        notifies, curves, sync markers, montage sections, slots, and segments.
        The command does not tick worlds, call notify handlers, compile, save, or
        mark assets dirty.

        Args:
            asset_path: Animation asset package path or object path
            include_notifies: Include notify and notify-state summaries
            include_notify_details: Include notify class/filter/branching details
            include_curves: Include curve names, flags, types, and counts
            include_curve_keys: Include bounded key data only when explicitly true
            include_sync_markers: Include authored sync markers
            include_montage_sections: Include Montage section summaries
            include_montage_slots: Include Montage slot and segment summaries
            include_metadata: Include asset metadata and user-data class summaries
            max_notifies: Maximum notifies to include
            max_curves: Maximum curves to include
            max_curve_keys_per_curve: Maximum keys per curve when include_curve_keys is true
            max_markers: Maximum sync markers to include
            max_sections: Maximum Montage sections to include
            max_slots: Maximum Montage slots to include
            max_segments_per_slot: Maximum segments per slot to include
            require_supported_class: Return an error for unsupported classes when true

        Returns:
            Response containing read-only animation asset metadata and truncation flags.
        """
        try:
            params = {
                "asset_path": asset_path,
                "include_notifies": include_notifies,
                "include_notify_details": include_notify_details,
                "include_curves": include_curves,
                "include_curve_keys": include_curve_keys,
                "include_sync_markers": include_sync_markers,
                "include_montage_sections": include_montage_sections,
                "include_montage_slots": include_montage_slots,
                "include_metadata": include_metadata,
                "max_notifies": max_notifies,
                "max_curves": max_curves,
                "max_curve_keys_per_curve": max_curve_keys_per_curve,
                "max_markers": max_markers,
                "max_sections": max_sections,
                "max_slots": max_slots,
                "max_segments_per_slot": max_segments_per_slot,
                "require_supported_class": require_supported_class,
            }
            return send_node_command("inspect_animation_asset_metadata_guarded", params)
        except Exception as e:
            error_msg = f"Error inspecting animation asset metadata: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_physics_asset_constraints_guarded(
        ctx: Context,
        physics_asset_path: str = "",
        skeletal_mesh_path: str = "",
        include_bodies: bool = True,
        include_constraints: bool = True,
        include_profiles: bool = True,
        include_solver_settings: bool = True,
        include_collision_summary: bool = True,
        max_bodies: int = 128,
        max_constraints: int = 128,
        max_profiles_per_item: int = 16,
        max_shapes_per_body: int = 32,
        require_supported_class: bool = False
    ) -> Dict[str, Any]:
        """
        Read PhysicsAsset bodies, constraints, profiles, and solver settings.

        Provide either physics_asset_path or skeletal_mesh_path. When a skeletal
        mesh is supplied, its assigned PhysicsAsset is resolved read-only. The
        command does not simulate physics, apply profiles, cook collision, save,
        or mutate assets.

        Args:
            physics_asset_path: Explicit PhysicsAsset package path or object path
            skeletal_mesh_path: SkeletalMesh path used to resolve a PhysicsAsset
            include_bodies: Include bounded body setup summaries
            include_constraints: Include bounded constraint summaries
            include_profiles: Include asset, body, and constraint profile summaries
            include_solver_settings: Include RBAN solver settings
            include_collision_summary: Include shape counts and bounded shape summaries
            max_bodies: Maximum body setups to include
            max_constraints: Maximum constraint templates to include
            max_profiles_per_item: Maximum profiles per body or constraint
            max_shapes_per_body: Maximum shape summaries per body
            require_supported_class: Return an error for unsupported classes when true

        Returns:
            Response containing read-only PhysicsAsset evidence and truncation flags.
        """
        try:
            params = {
                "physics_asset_path": physics_asset_path,
                "skeletal_mesh_path": skeletal_mesh_path,
                "include_bodies": include_bodies,
                "include_constraints": include_constraints,
                "include_profiles": include_profiles,
                "include_solver_settings": include_solver_settings,
                "include_collision_summary": include_collision_summary,
                "max_bodies": max_bodies,
                "max_constraints": max_constraints,
                "max_profiles_per_item": max_profiles_per_item,
                "max_shapes_per_body": max_shapes_per_body,
                "require_supported_class": require_supported_class,
            }
            return send_node_command("inspect_physics_asset_constraints_guarded", params)
        except Exception as e:
            error_msg = f"Error inspecting PhysicsAsset constraints: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def controlrig_direct_gate_probe(
        ctx: Context,
        control_rig_path: str = "",
        control_rig_class: str = "",
        cases: Optional[List[Dict[str, Any]]] = None,
        sample_elements: Optional[List[Any]] = None,
        control_values: Optional[Dict[str, Any]] = None,
        execute_events: Optional[List[str]] = None,
        should_trace_property: str = "ShouldDoIKTrace",
        interaction_location_property: str = "InteractionWorldLocation"
    ) -> Dict[str, Any]:
        """
        Run a transient direct ControlRig gate probe without modifying or saving assets.

        The default case set matches the StackOBot ControlRig learning pass: it toggles
        ShouldDoIKTrace, InteractionWorldLocation, IKBlend_l, and IK_blend_interact,
        executes Construction/Forwards Solve/Post Forwards Solve, samples hierarchy
        transforms, and reports deltas from the first case.

        Args:
            control_rig_path: ControlRig Blueprint asset path
            control_rig_class: Optional generated ControlRig class path
            cases: Optional case list. Each case may include name, properties, should_trace,
                loc or interaction_world_location, curves, and control_values.
            sample_elements: Optional hierarchy element names or {type, name} objects
            control_values: Optional transient Float/EulerTransform hierarchy control values
            execute_events: Optional ControlRig event names to execute
            should_trace_property: Property name used by shorthand should_trace
            interaction_location_property: Property name used by shorthand loc

        Returns:
            Response containing case outputs, transform samples, deltas, and errors.
        """
        try:
            params: Dict[str, Any] = {
                "should_trace_property": should_trace_property,
                "interaction_location_property": interaction_location_property,
            }
            if control_rig_path:
                params["control_rig_path"] = control_rig_path
            if control_rig_class:
                params["control_rig_class"] = control_rig_class
            if cases is not None:
                params["cases"] = cases
            if sample_elements is not None:
                params["sample_elements"] = sample_elements
            if control_values is not None:
                params["control_values"] = control_values
            if execute_events is not None:
                params["execute_events"] = execute_events
            return send_node_command("controlrig_direct_gate_probe", params)
        except Exception as e:
            error_msg = f"Error running ControlRig direct gate probe: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def sample_controlrig_pre_post_runtime_pose(
        ctx: Context,
        control_rig_path: str = "",
        control_rig_class: str = "",
        input_defaults: Optional[Dict[str, Any]] = None,
        curve_values: Optional[Dict[str, float]] = None,
        cases: Optional[List[Dict[str, Any]]] = None,
        sample_elements: Optional[List[Any]] = None,
        control_values: Optional[Dict[str, Any]] = None,
        execute_events: Optional[List[str]] = None,
        should_trace_property: str = "ShouldDoIKTrace",
        interaction_location_property: str = "InteractionWorldLocation"
    ) -> Dict[str, Any]:
        """
        Sample ControlRig hierarchy transforms before and after execute events.

        This is a read-only transient ControlRig pre/post probe. It does not
        instrument the compiled AnimGraph node stack; the response labels this as
        runtime_source=direct_transient_controlrig and runtime_graph_prepost=false.

        Args:
            control_rig_path: ControlRig Blueprint asset path
            control_rig_class: Optional generated ControlRig class path
            input_defaults: Optional ControlRig property defaults
            curve_values: Optional hierarchy curve values
            cases: Optional case list. Each case may include name, properties, should_trace,
                loc or interaction_world_location, curves, and control_values.
            sample_elements: Optional hierarchy element names or {type, name} objects
            control_values: Optional transient Float/EulerTransform hierarchy control values
            execute_events: Optional ControlRig event names to execute, default Forwards Solve
            should_trace_property: Property name used by shorthand should_trace
            interaction_location_property: Property name used by shorthand loc

        Returns:
            Response containing pre_pose, post_pose, deltas, echoes, and execution status.
        """
        try:
            if input_defaults is None:
                input_defaults = {
                    "ShouldDoIKTrace": True,
                    "InteractionWorldLocation": [80, -40, 80],
                }
            if curve_values is None:
                curve_values = {
                    "IK_blend_interact": 1.0,
                    "IKBlend_l": 1.0,
                }
            params: Dict[str, Any] = {
                "input_defaults": input_defaults,
                "curve_values": curve_values,
                "should_trace_property": should_trace_property,
                "interaction_location_property": interaction_location_property,
            }
            if control_rig_path:
                params["control_rig_path"] = control_rig_path
            if control_rig_class:
                params["control_rig_class"] = control_rig_class
            if cases is not None:
                params["cases"] = cases
            if sample_elements is not None:
                params["sample_elements"] = sample_elements
            if control_values is not None:
                params["control_values"] = control_values
            if execute_events is not None:
                params["execute_events"] = execute_events
            return send_node_command("sample_controlrig_pre_post_runtime_pose", params)
        except Exception as e:
            error_msg = f"Error sampling ControlRig pre/post runtime pose: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def sample_anim_node_pre_post_runtime_pose(
        ctx: Context,
        blueprint_name: str = "",
        anim_blueprint: str = "",
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function",
        node_id: str = "",
        node_type: str = "",
        title_contains: str = "",
        sample_bones: Optional[List[str]] = None,
        sample_sockets: Optional[List[str]] = None,
        mode: str = "active_component_tick_delta",
        skeletal_mesh: str = "",
        temp_root: str = "/Game/_MCP_Temp/AnimNodePrePost",
        cleanup: bool = True,
        overwrite_existing_temp_assets: bool = True,
        run_id: str = "",
        actor_label: str = "",
        actor_name: str = "",
        actor_path: str = "",
        component_name: str = "",
        anim_instance_source: str = "main",
        prefer_pie_world: bool = True,
        require_pie_world: bool = False,
        tick_count: int = 1,
        tick_delta_time: float = 1.0 / 30.0,
        settle_tick_count: int = 0,
        refresh_bone_transforms: bool = True,
        allow_missing_bones: bool = False,
        pose_link_max_depth: int = 4,
        input_pose_mode: str = "first",
        input_pose_field_path: str = "",
        input_pose_index: int = -1,
        include_pins: bool = True,
        max_depth: int = 3,
        dry_run: bool = True
    ) -> Dict[str, Any]:
        """
        Resolve a target AnimGraph node or run a limited runtime probe.

        With dry_run=True this is a read-only target resolver. With dry_run=False,
        mode="compiled_graph_mapping" maps the selected editor AnimGraph node to
        the compiled/live FAnimNode instance on a matched AnimInstance and reports
        runtime FPoseLink/FComponentSpacePoseLink link IDs plus mapping_support
        diagnostics for PoseWatch eligibility and evidence tier. With
        mode="pose_watch_capture", it temporarily sets the AnimBP debug object,
        registers transient PoseWatch debug entries for the selected compiled
        node output and selected runtime input pose link(s), and samples them
        during the same forced tick without adding PoseWatches to the AnimBP asset.
        With
        mode="active_component_tick_delta" samples final SkeletalMeshComponent pose
        before and after forced ticks on a matched live component. With
        mode="isolated_temp_components", it duplicates the AnimBP under _MCP_Temp,
        bypasses the selected node in a source copy, and compares that against a
        selected-node copy on separate transient components.

        Args:
            blueprint_name: Anim Blueprint name or path
            anim_blueprint: Alternative Anim Blueprint name or path
            graph_name: AnimGraph graph name, defaults to AnimGraph
            graph_id: Optional graph GUID
            graph_type: Graph type, defaults to function for AnimGraph
            node_id: Exact target node GUID. Required when filters are ambiguous.
            node_type: Node class/title filter such as AnimGraphNode_RigidBody
            title_contains: Node title substring filter
            sample_bones: Optional future sample bone list echoed in the response
            sample_sockets: Optional future sample socket list echoed in the response
            mode: Runtime mode for dry_run=False. compiled_graph_mapping, pose_watch_capture, active_component_tick_delta, or isolated_temp_components.
            skeletal_mesh: SkeletalMesh path required by isolated_temp_components
            temp_root: Temp asset root for isolated_temp_components
            cleanup: Delete transient actors/temp assets after isolated sampling
            overwrite_existing_temp_assets: Replace colliding temp assets
            run_id: Optional stable suffix for generated temp asset/actor names
            actor_label: Live actor label to sample for active_component_tick_delta
            actor_name: Live actor object name to sample
            actor_path: Live actor object path to sample
            component_name: SkeletalMeshComponent name to sample
            anim_instance_source: AnimInstance source on the sampled component: main or post_process
            prefer_pie_world: Prefer PIE/SIE/play worlds over editor world
            require_pie_world: Refuse editor-world fallback
            tick_count: Forced tick count between pre/post samples
            tick_delta_time: Delta time for forced ticks
            settle_tick_count: Forced ticks before taking the pre sample
            refresh_bone_transforms: Refresh bone transforms after forced ticks
            allow_missing_bones: Return partial samples instead of failing on missing bones/sockets
            pose_link_max_depth: Max struct recursion depth for compiled_graph_mapping and pose_watch_capture runtime pose-link inventory
            input_pose_mode: PoseWatch input selection mode: first or all
            input_pose_field_path: Exact runtime pose-link field path to capture
            input_pose_index: Zero-based valid runtime pose-link index to capture
            include_pins: Include full pin data for the selected node
            max_depth: Reflected settings depth
            dry_run: Resolve only when true; run the selected mode when false

        Returns:
            Resolver response or selected runtime probe response, including
            mapping_support when node attribution diagnostics are available.
        """
        try:
            params: Dict[str, Any] = {
                "graph_name": graph_name,
                "graph_type": graph_type,
                "mode": mode,
                "temp_root": temp_root,
                "cleanup": cleanup,
                "overwrite_existing_temp_assets": overwrite_existing_temp_assets,
                "prefer_pie_world": prefer_pie_world,
                "require_pie_world": require_pie_world,
                "tick_count": tick_count,
                "tick_delta_time": tick_delta_time,
                "settle_tick_count": settle_tick_count,
                "refresh_bone_transforms": refresh_bone_transforms,
                "allow_missing_bones": allow_missing_bones,
                "anim_instance_source": anim_instance_source,
                "pose_link_max_depth": pose_link_max_depth,
                "input_pose_mode": input_pose_mode,
                "include_pins": include_pins,
                "max_depth": max_depth,
                "dry_run": dry_run,
            }
            if blueprint_name:
                params["blueprint_name"] = blueprint_name
            if anim_blueprint:
                params["anim_blueprint"] = anim_blueprint
            if graph_id:
                params["graph_id"] = graph_id
            if skeletal_mesh:
                params["skeletal_mesh"] = skeletal_mesh
            if run_id:
                params["run_id"] = run_id
            if node_id:
                params["node_id"] = node_id
            if node_type:
                params["node_type"] = node_type
            if title_contains:
                params["title_contains"] = title_contains
            if actor_label:
                params["actor_label"] = actor_label
            if actor_name:
                params["actor_name"] = actor_name
            if actor_path:
                params["actor_path"] = actor_path
            if component_name:
                params["component_name"] = component_name
            if sample_bones is not None:
                params["sample_bones"] = sample_bones
            if sample_sockets is not None:
                params["sample_sockets"] = sample_sockets
            if input_pose_field_path:
                params["input_pose_field_path"] = input_pose_field_path
            if input_pose_index >= 0:
                params["input_pose_index"] = input_pose_index
            return send_node_command("sample_anim_node_pre_post_runtime_pose", params)
        except Exception as e:
            error_msg = f"Error resolving AnimGraph node pre/post runtime pose target: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def sample_skeletal_bones_in_sie(
        ctx: Context,
        actor_label: str = "",
        actor_name: str = "",
        actor_path: str = "",
        component_name: str = "",
        bones: Optional[List[str]] = None,
        sockets: Optional[List[str]] = None,
        prefer_pie_world: bool = True,
        require_pie_world: bool = False
    ) -> Dict[str, Any]:
        """
        Sample SkeletalMeshComponent bone/socket transforms from the active PIE/SIE/play world.

        This is a read-only immediate sampler. It prefers a live PIE/SIE/play world when
        available, falls back to the editor world, and reports the sampled world type.
        It does not start or tick SIE by itself.

        Args:
            actor_label: Optional actor label filter, case-insensitive exact match
            actor_name: Optional actor object name filter, case-insensitive exact match
            actor_path: Optional actor path filter, exact or path suffix match
            component_name: Optional SkeletalMeshComponent object name
            bones: Optional bone names to sample. Defaults to key StackOBot-style bones.
            sockets: Optional socket names to sample
            prefer_pie_world: Prefer active PIE/SIE/play world before editor world
            require_pie_world: Fail instead of falling back to the editor world when no PIE/SIE/play world matches

        Returns:
            Response containing actor/component metadata, sampled world type, and transforms.
        """
        try:
            params: Dict[str, Any] = {
                "actor_label": actor_label,
                "actor_name": actor_name,
                "actor_path": actor_path,
                "component_name": component_name,
                "prefer_pie_world": prefer_pie_world,
                "require_pie_world": require_pie_world,
            }
            if bones is not None:
                params["bones"] = bones
            if sockets is not None:
                params["sockets"] = sockets
            return send_node_command("sample_skeletal_bones_in_sie", params)
        except Exception as e:
            error_msg = f"Error sampling skeletal bones in SIE: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def sample_blendspace_runtime_pose_grid(
        ctx: Context,
        skeletal_mesh: str,
        blendspace_path: str = "",
        blendspaces: Optional[List[Any]] = None,
        samples: Optional[List[Dict[str, Any]]] = None,
        sample_bones: Optional[List[str]] = None,
        sample_sockets: Optional[List[str]] = None,
        actor_label: str = "MCP_BlendSpace_RuntimePoseGrid",
        sample_time: float = 0.25,
        settle_tick_count: int = 2,
        tick_count: int = 1,
        tick_delta_time: float = 1.0 / 30.0,
        include_axis_extremes: bool = False,
        allow_missing_bones: bool = False,
        refresh_bone_transforms: bool = True,
        cleanup: bool = True,
        prefer_pie_world: bool = True,
        require_pie_world: bool = True
    ) -> Dict[str, Any]:
        """
        Evaluate BlendSpace inputs on a transient runtime SkeletalMeshActor and sample bone/socket poses.

        This command does not start SIE by itself. By default it requires an active
        PIE/SIE/play world, spawns a transient SkeletalMeshActor, configures
        AnimationSingleNode BlendSpace playback, applies each requested input,
        forces narrow component ticks, samples requested bones/sockets, and cleans
        up the transient actor without saving assets.

        Args:
            skeletal_mesh: SkeletalMesh asset path for the transient probe actor
            blendspace_path: Single BlendSpace asset path
            blendspaces: Optional list of BlendSpace paths or objects with path/name/samples
            samples: Optional sample inputs for single blendspace_path. Each object may include label and input or x/y/z.
            sample_bones: Bone names to sample. Defaults to key humanoid bones in C++.
            sample_sockets: Socket names to sample
            actor_label: Transient actor label
            sample_time: Single-node playback time applied before sampling
            settle_tick_count: Forced ticks after setting each input before final tick
            tick_count: Forced ticks immediately before sampling
            tick_delta_time: Delta time for forced component ticks
            include_axis_extremes: Include min/center/max axis probes when samples are not supplied
            allow_missing_bones: Return partial sample data instead of failing on missing bones/sockets
            refresh_bone_transforms: Refresh bones after each forced tick
            cleanup: Destroy transient actor before returning
            prefer_pie_world: Prefer active PIE/SIE/play worlds over editor world
            require_pie_world: Fail without active PIE/SIE/play world

        Returns:
            Response with per-BlendSpace sample poses, weighted source samples, and deltas from each BlendSpace's first valid sample.
        """
        try:
            params: Dict[str, Any] = {
                "skeletal_mesh": skeletal_mesh,
                "actor_label": actor_label,
                "sample_time": sample_time,
                "settle_tick_count": settle_tick_count,
                "tick_count": tick_count,
                "tick_delta_time": tick_delta_time,
                "include_axis_extremes": include_axis_extremes,
                "allow_missing_bones": allow_missing_bones,
                "refresh_bone_transforms": refresh_bone_transforms,
                "cleanup": cleanup,
                "prefer_pie_world": prefer_pie_world,
                "require_pie_world": require_pie_world,
            }
            if blendspace_path:
                params["blendspace_path"] = blendspace_path
            if blendspaces is not None:
                params["blendspaces"] = blendspaces
            if samples is not None:
                params["samples"] = samples
            if sample_bones is not None:
                params["sample_bones"] = sample_bones
            if sample_sockets is not None:
                params["sample_sockets"] = sample_sockets
            return send_node_command("sample_blendspace_runtime_pose_grid", params)
        except Exception as e:
            error_msg = f"Error sampling BlendSpace runtime pose grid: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_blendspace_sample_variant(
        ctx: Context,
        source_blendspace: str,
        variant_name: str = "Variant",
        target_root: str = "/Game/_MCP_Sample/AnimStudy",
        target_blendspace: str = "",
        axis_edits: Optional[List[Dict[str, Any]]] = None,
        sample_edits: Optional[List[Dict[str, Any]]] = None,
        add_samples: Optional[List[Dict[str, Any]]] = None,
        overwrite_existing: bool = False,
        save: bool = True,
        dry_run: bool = False,
        validate_only: bool = False,
        allow_non_sample: bool = False,
        force_resample: bool = False,
    ) -> Dict[str, Any]:
        """
        Create or reuse a sample/temp BlendSpace variant and apply explicit axis/sample edits.

        By default this command refuses targets outside /Game/_MCP_Sample and
        /Game/_MCP_Temp. It duplicates the source BlendSpace to a sample/temp path,
        applies axis range/grid changes,
        edits existing sample coordinates or sample animations, optionally adds samples
        with compatible AnimSequence assets, validates/resamples the BlendSpace, and saves
        only the target asset. Use sample_blendspace_runtime_pose_grid after this command
        for runtime pose evidence.

        Args:
            source_blendspace: Source BlendSpace path
            variant_name: Name segment used for default target path
            target_root: Sample/temp output folder, defaults to /Game/_MCP_Sample/AnimStudy
            target_blendspace: Optional explicit target BlendSpace package path
            axis_edits: Optional axis edits. Each entry accepts index/axis plus min, max, grid_num, display_name, snap_to_grid, wrap_input.
            sample_edits: Optional existing sample edits. Select by index/sample_index, animation, or animation_name; set input/position/sample_value, x/y/z, offset/delta, or replacement_animation.
            add_samples: Optional samples to add. Each entry requires animation and input/position/sample_value or x/y/z.
            overwrite_existing: Delete and recreate existing target asset before applying edits
            save: Save the target BlendSpace after edits
            dry_run: Validate paths/request and return without editing assets
            validate_only: Same as dry_run for command-routing compatibility
            allow_non_sample: Allow target paths outside /Game/_MCP_Sample and /Game/_MCP_Temp
            force_resample: Rebuild BlendSpace runtime sampling data even when no edits changed values.
                When source and target are the same asset, this also requires allow_non_sample=True.

        Returns:
            Response with target path, before/after axes and samples, edit results, save result, and safety flags.
        """
        try:
            params: Dict[str, Any] = {
                "source_blendspace": source_blendspace,
                "variant_name": variant_name,
                "target_root": target_root,
                "overwrite_existing": overwrite_existing,
                "save": save,
                "dry_run": dry_run,
                "validate_only": validate_only,
                "allow_non_sample": allow_non_sample,
                "force_resample": force_resample,
            }
            if target_blendspace:
                params["target_blendspace"] = target_blendspace
            if axis_edits is not None:
                params["axis_edits"] = axis_edits
            if sample_edits is not None:
                params["sample_edits"] = sample_edits
            if add_samples is not None:
                params["add_samples"] = add_samples
            return send_node_command("ensure_blendspace_sample_variant", params)
        except Exception as e:
            error_msg = f"Error ensuring BlendSpace sample variant: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_anim_state_machine_sample_variant(
        ctx: Context,
        source_anim_bp_path: str,
        states: Optional[List[Dict[str, Any]]] = None,
        transitions: Optional[List[Dict[str, Any]]] = None,
        target_anim_bp_path: str = "",
        target_root: str = "/Game/_MCP_Sample/AnimStudy",
        variant_name: str = "StateMachineSample",
        state_machine_name: str = "MCP_StateMachine",
        entry_state: str = "",
        animation_asset_path: str = "",
        connect_demo_machine_to_output: bool = True,
        replace_existing_root_link: bool = True,
        replace_existing_entry_link: bool = True,
        allow_existing_target_reuse: bool = True,
        overwrite_existing: bool = False,
        compile: bool = True,
        save_asset: bool = True,
        dry_run: bool = False,
        validate_only: bool = False,
        max_states: int = 16,
        max_transitions: int = 32,
    ) -> Dict[str, Any]:
        """
        Create or reuse a sample-only AnimBP state-machine variant.

        The source AnimBP and animation assets are read-only. The command duplicates
        or reuses a target AnimBP under /Game/_MCP_Sample or /Game/_MCP_Temp, adds a
        state-machine node, creates regular states with one Sequence/BlendSpace asset
        player per state, connects entry/transitions through Unreal schemas, compiles,
        optionally saves, and reads back topology.

        Args:
            source_anim_bp_path: Source Animation Blueprint path or name
            states: State specs with name, animation_asset_path, optional position, always_reset_on_entry,
                and optional player_inputs entries that bind BlendSpacePlayer X/Y pins to float variables
                or numeric defaults.
            transitions: Transition specs with from_state, to_state, rule_mode, priority_order, crossfade_duration.
                Supported rule_mode values include always_true, always_false, automatic_remaining_time,
                bool_variable, float_compare, and enum_compare. Variable rules can pass rule_variable,
                expected_value, compare/compare_value, enum_type/enum_value, and
                create_rule_variable_if_missing.
            target_anim_bp_path: Optional explicit target sample AnimBP package path
            target_root: Default target folder when target_anim_bp_path is omitted
            variant_name: Name segment used for generated target path
            state_machine_name: State-machine node/graph name to create or reuse
            entry_state: Entry state name; defaults to the first state
            animation_asset_path: Top-level shorthand for a single-state request
            connect_demo_machine_to_output: Connect the sample machine to AnimGraph Result
            replace_existing_root_link: Replace existing target root pose link when connecting
            replace_existing_entry_link: Replace existing state-machine entry link
            allow_existing_target_reuse: Reuse an existing sample target AnimBP
            overwrite_existing: Delete and recreate an existing target sample AnimBP
            compile: Compile the target sample AnimBP after edits
            save_asset: Save the target sample AnimBP after edits
            dry_run: Validate request and return planned edits without mutating assets
            validate_only: Alias for dry_run
            max_states: Hard clamp for state count
            max_transitions: Hard clamp for transition count

        Returns:
            Response with safety flags, target path, compile/save results, authored states/transitions, and readback topology.
        """
        try:
            params: Dict[str, Any] = {
                "source_anim_bp_path": source_anim_bp_path,
                "target_root": target_root,
                "variant_name": variant_name,
                "state_machine_name": state_machine_name,
                "connect_demo_machine_to_output": connect_demo_machine_to_output,
                "replace_existing_root_link": replace_existing_root_link,
                "replace_existing_entry_link": replace_existing_entry_link,
                "allow_existing_target_reuse": allow_existing_target_reuse,
                "overwrite_existing": overwrite_existing,
                "compile": compile,
                "save_asset": save_asset,
                "dry_run": dry_run,
                "validate_only": validate_only,
                "max_states": max_states,
                "max_transitions": max_transitions,
            }
            if states is not None:
                params["states"] = states
            if transitions is not None:
                params["transitions"] = transitions
            if target_anim_bp_path:
                params["target_anim_bp_path"] = target_anim_bp_path
            if entry_state:
                params["entry_state"] = entry_state
            if animation_asset_path:
                params["animation_asset_path"] = animation_asset_path
            return send_node_command("ensure_anim_state_machine_sample_variant", params)
        except Exception as e:
            error_msg = f"Error ensuring AnimBP state-machine sample variant: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_anim_bp_event_graph_locomotion_driver(
        ctx: Context,
        source_anim_bp_path: str,
        target_anim_bp_path: str = "",
        target_root: str = "/Game/_MCP_Sample/AnimStudy",
        variant_name: str = "LocomotionEventGraph",
        speed_variable: str = "Speed",
        create_variable_if_missing: bool = True,
        include_direction: bool = False,
        direction_variable: str = "Direction",
        create_direction_variable_if_missing: bool = True,
        include_is_in_air: bool = False,
        is_in_air_variable: str = "IsInAir",
        create_is_in_air_variable_if_missing: bool = True,
        include_acceleration: bool = False,
        acceleration_variable: str = "Acceleration",
        create_acceleration_variable_if_missing: bool = True,
        include_is_accelerating: bool = False,
        is_accelerating_variable: str = "IsAccelerating",
        create_is_accelerating_variable_if_missing: bool = True,
        is_accelerating_threshold: float = 3.0,
        guard_pawn_owner: bool = True,
        replace_existing_links: bool = False,
        allow_existing_target_reuse: bool = True,
        overwrite_existing: bool = False,
        compile: bool = True,
        save_asset: bool = True,
        dry_run: bool = False,
        validate_only: bool = False,
    ) -> Dict[str, Any]:
        """
        Create or reuse a sample-only AnimBP EventGraph locomotion driver.

        The source AnimBP is read-only. The command duplicates or reuses a target
        AnimBP under /Game/_MCP_Sample or /Game/_MCP_Temp, ensures a float Speed
        variable by default, can optionally ensure float Direction and bool
        IsInAir, Acceleration, and IsAccelerating variables, authors Event Blueprint Update Animation ->
        TryGetPawnOwner -> optional IsValid Branch -> GetVelocity -> VSizeXY ->
        Set Speed plus optional CalculateDirection, IsFalling, and
        CharacterMovement acceleration setters using standard engine Blueprint
        nodes, compiles, optionally saves, and reads back EventGraph topology.

        Args:
            source_anim_bp_path: Source Animation Blueprint path or name
            target_anim_bp_path: Optional explicit target sample AnimBP package path
            target_root: Default target folder when target_anim_bp_path is omitted
            variant_name: Name segment used for generated target path
            speed_variable: Float variable updated from pawn XY velocity length
            create_variable_if_missing: Add speed_variable when it does not exist
            include_direction: Also update direction_variable from CalculateDirection(Velocity, ActorRotation)
            direction_variable: Float variable updated when include_direction is true
            create_direction_variable_if_missing: Add direction_variable when it does not exist
            include_is_in_air: Also update is_in_air_variable from MovementComponent.IsFalling()
            is_in_air_variable: Bool variable updated when include_is_in_air is true
            create_is_in_air_variable_if_missing: Add is_in_air_variable when it does not exist
            include_acceleration: Also update acceleration_variable from CharacterMovement.GetCurrentAcceleration XY length
            acceleration_variable: Float variable updated when include_acceleration is true
            create_acceleration_variable_if_missing: Add acceleration_variable when it does not exist
            include_is_accelerating: Also update is_accelerating_variable from acceleration > threshold
            is_accelerating_variable: Bool variable updated when include_is_accelerating is true
            create_is_accelerating_variable_if_missing: Add is_accelerating_variable when it does not exist
            is_accelerating_threshold: Acceleration magnitude threshold for is_accelerating_variable
            guard_pawn_owner: Gate Set Speed behind IsValid(TryGetPawnOwner())
            replace_existing_links: Replace conflicting pins inside the sample target graph
            allow_existing_target_reuse: Reuse an existing sample target AnimBP
            overwrite_existing: Delete and recreate an existing target sample AnimBP
            compile: Compile the target sample AnimBP after edits
            save_asset: Save the target sample AnimBP after edits
            dry_run: Validate request and return planned edits without mutating assets
            validate_only: Alias for dry_run

        Returns:
            Response with safety flags, target path, authored nodes/links, compile/save results, and EventGraph readback.
        """
        try:
            reject_mcp_dependency_reference("source_anim_bp_path", source_anim_bp_path)
            reject_mcp_dependency_reference("target_anim_bp_path", target_anim_bp_path)
            reject_mcp_dependency_reference("target_root", target_root)
            reject_mcp_dependency_reference("speed_variable", speed_variable)
            reject_mcp_dependency_reference("direction_variable", direction_variable)
            reject_mcp_dependency_reference("is_in_air_variable", is_in_air_variable)
            reject_mcp_dependency_reference("acceleration_variable", acceleration_variable)
            reject_mcp_dependency_reference("is_accelerating_variable", is_accelerating_variable)
            params: Dict[str, Any] = {
                "source_anim_bp_path": source_anim_bp_path,
                "target_root": target_root,
                "variant_name": variant_name,
                "speed_variable": speed_variable,
                "create_variable_if_missing": create_variable_if_missing,
                "include_direction": include_direction,
                "direction_variable": direction_variable,
                "create_direction_variable_if_missing": create_direction_variable_if_missing,
                "include_is_in_air": include_is_in_air,
                "is_in_air_variable": is_in_air_variable,
                "create_is_in_air_variable_if_missing": create_is_in_air_variable_if_missing,
                "include_acceleration": include_acceleration,
                "acceleration_variable": acceleration_variable,
                "create_acceleration_variable_if_missing": create_acceleration_variable_if_missing,
                "include_is_accelerating": include_is_accelerating,
                "is_accelerating_variable": is_accelerating_variable,
                "create_is_accelerating_variable_if_missing": create_is_accelerating_variable_if_missing,
                "is_accelerating_threshold": is_accelerating_threshold,
                "guard_pawn_owner": guard_pawn_owner,
                "replace_existing_links": replace_existing_links,
                "allow_existing_target_reuse": allow_existing_target_reuse,
                "overwrite_existing": overwrite_existing,
                "compile": compile,
                "save_asset": save_asset,
                "dry_run": dry_run,
                "validate_only": validate_only,
            }
            if target_anim_bp_path:
                params["target_anim_bp_path"] = target_anim_bp_path
            return send_node_command("ensure_anim_bp_event_graph_locomotion_driver", params)
        except Exception as e:
            error_msg = f"Error ensuring AnimBP EventGraph locomotion driver: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def create_anim_blueprint_sample(
        ctx: Context,
        target_anim_bp_path: str,
        skeleton_path: str = "",
        skeletal_mesh_path: str = "",
        preview_skeletal_mesh_path: str = "",
        parent_class_path: str = "/Script/Engine.AnimInstance",
        overwrite_existing: bool = False,
        compile: bool = True,
        save_asset: bool = False,
        dry_run: bool = False,
        validate_only: bool = False,
    ) -> Dict[str, Any]:
        """
        Create a sample-only Animation Blueprint from a Skeleton or SkeletalMesh.

        The target must be under /Game/_MCP_Sample or /Game/_MCP_Temp. Input
        Skeleton/SkeletalMesh assets are read-only. The command creates an empty
        AnimBP with a bounded AnimInstance parent, target skeleton, optional
        preview mesh, compile result, and optional save.

        Args:
            target_anim_bp_path: Target sample AnimBP package path
            skeleton_path: Explicit Skeleton path; required when no skeletal mesh is supplied
            skeletal_mesh_path: SkeletalMesh path used to derive Skeleton and preview mesh
            preview_skeletal_mesh_path: Optional preview mesh override
            parent_class_path: UAnimInstance subclass path
            overwrite_existing: Delete and recreate an existing sample target
            compile: Compile after creation
            save_asset: Save the created sample AnimBP
            dry_run: Validate request without creating assets
            validate_only: Alias for dry_run

        Returns:
            Response with target path, skeleton/preview readback, generated class, compile/save status, and safety flags.
        """
        try:
            params: Dict[str, Any] = {
                "target_anim_bp_path": target_anim_bp_path,
                "skeleton_path": skeleton_path,
                "skeletal_mesh_path": skeletal_mesh_path,
                "preview_skeletal_mesh_path": preview_skeletal_mesh_path,
                "parent_class_path": parent_class_path,
                "overwrite_existing": overwrite_existing,
                "compile": compile,
                "save_asset": save_asset,
                "dry_run": dry_run,
                "validate_only": validate_only,
            }
            return send_node_command("create_anim_blueprint_sample", params)
        except Exception as e:
            error_msg = f"Error creating AnimBP sample: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_anim_graph_controlrig_node_sample(
        ctx: Context,
        source_anim_bp_path: str,
        control_rig_asset_path: str = "",
        control_rig_class_path: str = "",
        target_anim_bp_path: str = "",
        target_root: str = "/Game/_MCP_Sample/AnimStudy",
        variant_name: str = "ControlRigNodeSample",
        allow_existing_target_reuse: bool = True,
        overwrite_existing: bool = False,
        replace_existing_links: bool = True,
        compile: bool = True,
        save_asset: bool = False,
        dry_run: bool = False,
        validate_only: bool = False,
    ) -> Dict[str, Any]:
        """
        Duplicate/reuse a sample AnimBP and add a ControlRig AnimGraph node chain.

        V1 supports only the predictable LinkedInputPose -> ControlRig -> Result
        chain in a target under /Game/_MCP_Sample or /Game/_MCP_Temp. It binds a
        ControlRig Blueprint generated class or native class, compiles, optionally
        saves, and reads back topology. Use set_anim_graph_controlrig_input_defaults
        afterwards for exposed pins/defaults.

        Args:
            source_anim_bp_path: Source AnimBP used as the read-only template
            control_rig_asset_path: ControlRig Blueprint asset path
            control_rig_class_path: Generated/native UControlRig class path
            target_anim_bp_path: Optional explicit target sample AnimBP package path
            target_root: Default target folder when target_anim_bp_path is omitted
            variant_name: Name segment used for generated target path
            allow_existing_target_reuse: Reuse an existing sample target
            overwrite_existing: Delete and recreate an existing target
            replace_existing_links: Replace pose links needed for the sample chain
            compile: Compile target sample after edits
            save_asset: Save target sample after edits
            dry_run: Validate without mutating assets
            validate_only: Alias for dry_run

        Returns:
            Response with target path, node id, pose-link results, compile/save result, and readback topology.
        """
        try:
            params: Dict[str, Any] = {
                "source_anim_bp_path": source_anim_bp_path,
                "target_root": target_root,
                "variant_name": variant_name,
                "allow_existing_target_reuse": allow_existing_target_reuse,
                "overwrite_existing": overwrite_existing,
                "replace_existing_links": replace_existing_links,
                "compile": compile,
                "save_asset": save_asset,
                "dry_run": dry_run,
                "validate_only": validate_only,
            }
            if control_rig_asset_path:
                params["control_rig_asset_path"] = control_rig_asset_path
            if control_rig_class_path:
                params["control_rig_class_path"] = control_rig_class_path
            if target_anim_bp_path:
                params["target_anim_bp_path"] = target_anim_bp_path
            return send_node_command("ensure_anim_graph_controlrig_node_sample", params)
        except Exception as e:
            error_msg = f"Error ensuring AnimGraph ControlRig node sample: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_linked_anim_graph_sample_variant(
        ctx: Context,
        source_anim_bp_path: str,
        linked_anim_bp_path: str,
        target_anim_bp_path: str = "",
        target_root: str = "/Game/_MCP_Sample/AnimStudy",
        variant_name: str = "LinkedAnimGraphSample",
        insert_mode: str = "input_pose_to_linked_graph_to_root",
        allow_existing_target_reuse: bool = True,
        overwrite_existing: bool = False,
        replace_existing_links: bool = True,
        require_input_pose_pin: bool = False,
        compile: bool = True,
        save_asset: bool = False,
        dry_run: bool = False,
        validate_only: bool = False,
    ) -> Dict[str, Any]:
        """
        Duplicate/reuse a sample AnimBP and add a Linked Anim Graph node chain.

        MVP supports only a target under /Game/_MCP_Sample or /Game/_MCP_Temp.
        It sets the Linked Anim Graph node to an existing linked AnimBP generated
        class with the same TargetSkeleton, connects output to the root, and
        connects LinkedInputPose -> LinkedAnimGraph when the linked class exposes
        a pose input pin. Linked Anim Layer/interface creation is intentionally
        out of scope.

        Args:
            source_anim_bp_path: Source AnimBP used as the read-only template
            linked_anim_bp_path: Existing AnimBP whose generated class is linked
            target_anim_bp_path: Optional explicit target sample AnimBP package path
            target_root: Default target folder when target_anim_bp_path is omitted
            variant_name: Name segment used for generated target path
            insert_mode: Must be input_pose_to_linked_graph_to_root
            allow_existing_target_reuse: Reuse an existing sample target
            overwrite_existing: Delete and recreate an existing target
            replace_existing_links: Replace pose links needed for the sample chain
            require_input_pose_pin: Fail when the linked graph class exposes no pose input pin
            compile: Compile target sample after edits
            save_asset: Save target sample after edits
            dry_run: Validate without mutating assets
            validate_only: Alias for dry_run

        Returns:
            Response with target path, linked class, pose-link results, compile/save result, and readback topology.
        """
        try:
            params: Dict[str, Any] = {
                "source_anim_bp_path": source_anim_bp_path,
                "linked_anim_bp_path": linked_anim_bp_path,
                "target_root": target_root,
                "variant_name": variant_name,
                "insert_mode": insert_mode,
                "allow_existing_target_reuse": allow_existing_target_reuse,
                "overwrite_existing": overwrite_existing,
                "replace_existing_links": replace_existing_links,
                "require_input_pose_pin": require_input_pose_pin,
                "compile": compile,
                "save_asset": save_asset,
                "dry_run": dry_run,
                "validate_only": validate_only,
            }
            if target_anim_bp_path:
                params["target_anim_bp_path"] = target_anim_bp_path
            return send_node_command("ensure_linked_anim_graph_sample_variant", params)
        except Exception as e:
            error_msg = f"Error ensuring Linked Anim Graph sample variant: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_controlrig_sample_asset(
        ctx: Context,
        target_control_rig_path: str,
        skeletal_mesh_path: str = "",
        skeleton_path: str = "",
        preview_skeletal_mesh_path: str = "",
        setup_mode: str = "empty_with_preview",
        curves: Optional[List[Any]] = None,
        nulls: Optional[List[Any]] = None,
        controls: Optional[List[Any]] = None,
        unit_nodes: Optional[List[Any]] = None,
        max_direct_curves: int = 32,
        max_direct_nulls: int = 32,
        max_direct_controls: int = 32,
        max_unit_nodes: int = 6,
        allow_existing_target_reuse: bool = True,
        overwrite_existing: bool = False,
        modular_rig: bool = False,
        compile: bool = True,
        save_asset: bool = False,
        dry_run: bool = False,
        validate_only: bool = False,
    ) -> Dict[str, Any]:
        """
        Create or reuse a sample-only ControlRig Blueprint asset.

        Creates a ControlRig Blueprint under /Game/_MCP_Sample or
        /Game/_MCP_Temp, optionally assigns a preview SkeletalMesh or imports
        a bounded preview hierarchy from a read-only SkeletalMesh/Skeleton,
        optionally adds bounded direct hierarchy curves, local/global transform
        nulls, direct float/bool/euler transform controls, or a bounded
        allowlisted single-instance ControlRig event RigVM unit node evidence pass,
        compiles or initializes it, optionally saves only the target, and
        returns generated class/readback evidence. Direct null/control objects
        may include parent/parent_name as a string resolved to an existing or
        earlier direct Bone/Null/Control hierarchy key. Direct null objects may
        include transform/global_transform/local_transform. Direct control
        objects may include shape_name, shape_color, and shape_transform
        metadata; transform controls accept transform objects only through
        value/default_value/initial_value/default. RigVM unit_nodes accepts only
        allowlisted single-instance event units such as begin_execution,
        pre_begin_execution, post_begin_execution, prepare_for_execution,
        post_prepare_for_execution, and inverse_execution, with optional name
        and position. Control transform fields, proxy/channel controls, arbitrary
        RigVM script structs, links, variables, functions, comments, reroutes,
        dispatch nodes, exposed pins, and graph replacement remain unsupported.

        Args:
            target_control_rig_path: Target sample ControlRig package path
            skeletal_mesh_path: Optional read-only SkeletalMesh used as preview input
            skeleton_path: Optional read-only Skeleton used for validation
            preview_skeletal_mesh_path: Optional explicit preview mesh override
            setup_mode: empty, empty_with_preview, or import_preview_hierarchy
            curves: Optional direct curve names or objects with name/value
            nulls: Optional direct null names or objects with name, optional parent/parent_name, and optional transform/global_transform/local_transform
            controls: Optional direct float/bool/euler transform control names or objects with name, value/default_value, optional parent/parent_name, and optional shape_name/shape_color/shape_transform
            unit_nodes: Optional allowlisted single-instance ControlRig event RigVM unit node specs
            max_direct_curves: Maximum direct curve entries accepted
            max_direct_nulls: Maximum direct null entries accepted
            max_direct_controls: Maximum direct control entries accepted
            max_unit_nodes: Maximum RigVM unit node entries accepted; clamped to 6
            allow_existing_target_reuse: Reuse an existing ControlRig sample target
            overwrite_existing: Delete and recreate an existing target
            modular_rig: Create a modular rig instead of a standard ControlRig
            compile: Compile/initialize after creation or preview assignment
            save_asset: Save the target sample ControlRig asset
            dry_run: Validate without mutating assets
            validate_only: Alias for dry_run

        Returns:
            Response with target path, generated class, preview readback, compile/save status, and source-dirty guards.
        """
        try:
            params: Dict[str, Any] = {
                "target_control_rig_path": target_control_rig_path,
                "skeletal_mesh_path": skeletal_mesh_path,
                "skeleton_path": skeleton_path,
                "preview_skeletal_mesh_path": preview_skeletal_mesh_path,
                "setup_mode": setup_mode,
                "max_direct_curves": max_direct_curves,
                "max_direct_nulls": max_direct_nulls,
                "max_direct_controls": max_direct_controls,
                "max_unit_nodes": max_unit_nodes,
                "allow_existing_target_reuse": allow_existing_target_reuse,
                "overwrite_existing": overwrite_existing,
                "modular_rig": modular_rig,
                "compile": compile,
                "save_asset": save_asset,
                "dry_run": dry_run,
                "validate_only": validate_only,
            }
            if curves is not None:
                params["curves"] = curves
            if nulls is not None:
                params["nulls"] = nulls
            if controls is not None:
                params["controls"] = controls
            if unit_nodes is not None:
                params["unit_nodes"] = unit_nodes
            return send_node_command("ensure_controlrig_sample_asset", params)
        except Exception as e:
            error_msg = f"Error ensuring ControlRig sample asset: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_anim_instance_runtime_state(
        ctx: Context,
        actor_label: str = "",
        actor_name: str = "",
        actor_path: str = "",
        component_name: str = "",
        state_machine_name: str = "",
        include_states: bool = True,
        include_state_weights: bool = True,
        include_relevant_anim_times: bool = False,
        include_transition_progress: bool = True,
        include_inactive_transition_progress: bool = False,
        include_montages: bool = True,
        include_curves: bool = False,
        curve_names: Optional[List[str]] = None,
        max_state_machines: int = 32,
        max_state_machine_instances_to_probe: int = 256,
        max_states_per_machine: int = 64,
        max_transitions_per_machine: int = 128,
        max_curves: int = 128,
        prefer_pie_world: bool = True,
        require_pie_world: bool = False
    ) -> Dict[str, Any]:
        """
        Inspect runtime AnimInstance state from an active PIE/SIE/play SkeletalMeshComponent.

        This is read-only. It prefers a live PIE/SIE/play world when available,
        falls back to the editor world, and does not start or tick SIE by itself.

        Args:
            actor_label: Optional actor label filter, case-insensitive exact match
            actor_name: Optional actor object name filter, case-insensitive exact match
            actor_path: Optional actor path filter, exact or path suffix match
            component_name: Optional SkeletalMeshComponent object name
            state_machine_name: Optional state-machine name substring filter
            include_states: Include per-state metadata for each state machine
            include_state_weights: Include runtime state/machine weights
            include_relevant_anim_times: Include relevant asset-player time/remaining values per state
            include_transition_progress: Include active transition elapsed/crossfade progress
            include_inactive_transition_progress: Include inactive transitions with zero progress
            include_montages: Include current active montage data
            include_curves: Include curve values
            curve_names: Optional curve names. If omitted and include_curves is true, active/all names are used.
            max_state_machines: Maximum state machines to include
            max_state_machine_instances_to_probe: Maximum runtime AnimNode indexes to probe for state-machine instances
            max_states_per_machine: Maximum states per included machine
            max_transitions_per_machine: Maximum transitions per included machine for transition progress
            max_curves: Maximum curves to include
            prefer_pie_world: Prefer active PIE/SIE/play world before editor world
            require_pie_world: Fail instead of falling back to the editor world when no PIE/SIE/play world matches

        Returns:
            Response containing AnimInstance metadata, state-machine state, montage, and curves.
        """
        try:
            params: Dict[str, Any] = {
                "actor_label": actor_label,
                "actor_name": actor_name,
                "actor_path": actor_path,
                "component_name": component_name,
                "state_machine_name": state_machine_name,
                "include_states": include_states,
                "include_state_weights": include_state_weights,
                "include_relevant_anim_times": include_relevant_anim_times,
                "include_transition_progress": include_transition_progress,
                "include_inactive_transition_progress": include_inactive_transition_progress,
                "include_montages": include_montages,
                "include_curves": include_curves,
                "max_state_machines": max_state_machines,
                "max_state_machine_instances_to_probe": max_state_machine_instances_to_probe,
                "max_states_per_machine": max_states_per_machine,
                "max_transitions_per_machine": max_transitions_per_machine,
                "max_curves": max_curves,
                "prefer_pie_world": prefer_pie_world,
                "require_pie_world": require_pie_world,
            }
            if curve_names is not None:
                params["curve_names"] = curve_names
            return send_node_command("inspect_anim_instance_runtime_state", params)
        except Exception as e:
            error_msg = f"Error inspecting AnimInstance runtime state: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_anim_instance_runtime_property_for_probe(
        ctx: Context,
        actor_label: str = "",
        actor_name: str = "",
        actor_path: str = "",
        component_name: str = "",
        properties: Optional[Dict[str, Any]] = None,
        property_name: str = "",
        value: Any = None,
        tick_after_set: bool = False,
        tick_count: int = 1,
        tick_delta_time: float = 1.0 / 30.0,
        refresh_bone_transforms: bool = True,
        include_snapshot_after: bool = True,
        include_previous_values: bool = True,
        state_machine_name: str = "",
        include_states: bool = True,
        include_state_weights: bool = True,
        include_relevant_anim_times: bool = False,
        include_transition_progress: bool = True,
        include_inactive_transition_progress: bool = False,
        include_montages: bool = True,
        include_curves: bool = False,
        curve_names: Optional[List[str]] = None,
        max_transitions_per_machine: int = 128,
        prefer_pie_world: bool = True,
        require_pie_world: bool = False
    ) -> Dict[str, Any]:
        """
        Set properties on the matched live AnimInstance for runtime probing only.

        This modifies the current runtime object, not the Animation Blueprint asset.

        Args:
            actor_label: Optional actor label filter
            actor_name: Optional actor object name filter
            actor_path: Optional actor path filter
            component_name: Optional SkeletalMeshComponent object name
            properties: Property map such as {"GroundSpeed": 250.0}
            property_name: Single property name alternative
            value: Single property value alternative
            tick_after_set: Force a narrow component animation tick after setting
            tick_count: Number of forced component animation ticks
            tick_delta_time: Delta time per forced tick
            refresh_bone_transforms: Refresh component bone transforms during/after forced ticks when tick_count > 0
            include_snapshot_after: Include state-machine/montage/curve snapshot after the set
            include_previous_values: Include previous property values in the response
            state_machine_name: Optional state-machine name substring filter for snapshots
            include_states: Include per-state metadata in snapshots
            include_state_weights: Include runtime state/machine weights in snapshots
            include_relevant_anim_times: Include relevant asset-player time/remaining values per state in snapshots
            include_transition_progress: Include active transition elapsed/crossfade progress in snapshots
            include_inactive_transition_progress: Include inactive transition progress entries in snapshots
            include_montages: Include active montage data in snapshots
            include_curves: Include curve values in snapshots
            curve_names: Optional curve names for snapshots
            max_transitions_per_machine: Maximum transitions per included machine for transition progress
            prefer_pie_world: Prefer active PIE/SIE/play world before editor world
            require_pie_world: Fail instead of falling back to the editor world when no PIE/SIE/play world matches

        Returns:
            Response containing applied runtime properties and optional post-set snapshot.
        """
        try:
            params: Dict[str, Any] = {
                "actor_label": actor_label,
                "actor_name": actor_name,
                "actor_path": actor_path,
                "component_name": component_name,
                "tick_after_set": tick_after_set,
                "tick_count": tick_count,
                "tick_delta_time": tick_delta_time,
                "refresh_bone_transforms": refresh_bone_transforms,
                "include_snapshot_after": include_snapshot_after,
                "include_previous_values": include_previous_values,
                "state_machine_name": state_machine_name,
                "include_states": include_states,
                "include_state_weights": include_state_weights,
                "include_relevant_anim_times": include_relevant_anim_times,
                "include_transition_progress": include_transition_progress,
                "include_inactive_transition_progress": include_inactive_transition_progress,
                "include_montages": include_montages,
                "include_curves": include_curves,
                "max_transitions_per_machine": max_transitions_per_machine,
                "prefer_pie_world": prefer_pie_world,
                "require_pie_world": require_pie_world,
            }
            if properties is not None:
                params["properties"] = properties
            if property_name:
                params["property_name"] = property_name
                params["value"] = value
            if curve_names is not None:
                params["curve_names"] = curve_names
            return send_node_command("set_anim_instance_runtime_property_for_probe", params)
        except Exception as e:
            error_msg = f"Error setting AnimInstance runtime property for probe: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def sample_anim_state_machine_runtime_response(
        ctx: Context,
        actor_label: str = "",
        actor_name: str = "",
        actor_path: str = "",
        component_name: str = "",
        cases: Optional[List[Dict[str, Any]]] = None,
        properties: Optional[Dict[str, Any]] = None,
        name: str = "case_0",
        tick_count: int = 1,
        tick_delta_time: float = 1.0 / 30.0,
        refresh_bone_transforms: bool = True,
        restore_after_case: bool = True,
        include_baseline: bool = True,
        state_machine_name: str = "",
        include_states: bool = True,
        include_state_weights: bool = True,
        include_relevant_anim_times: bool = False,
        include_transition_progress: bool = True,
        include_inactive_transition_progress: bool = False,
        include_montages: bool = True,
        include_curves: bool = False,
        curve_names: Optional[List[str]] = None,
        max_state_machines: int = 32,
        max_state_machine_instances_to_probe: int = 256,
        max_states_per_machine: int = 64,
        max_transitions_per_machine: int = 128,
        max_curves: int = 128,
        prefer_pie_world: bool = True,
        require_pie_world: bool = False
    ) -> Dict[str, Any]:
        """
        Apply runtime AnimInstance property cases, tick narrowly, and sample state-machine response.

        This is a runtime probe only. By default it restores each successfully
        changed property after the case and never saves Animation Blueprint assets.

        Args:
            actor_label: Optional actor label filter
            actor_name: Optional actor object name filter
            actor_path: Optional actor path filter
            component_name: Optional SkeletalMeshComponent object name
            cases: Case list. Each case can include name, properties, tick_count, tick_delta_time, restore_after_case.
            properties: Single-case property map used when cases is omitted
            name: Single-case name used when cases is omitted
            tick_count: Default forced component animation tick count per case
            tick_delta_time: Default delta time per forced tick
            refresh_bone_transforms: Refresh component bone transforms during/after forced ticks when tick_count > 0
            restore_after_case: Restore successful property changes after each case
            include_baseline: Capture state before cases
            state_machine_name: Optional state-machine name substring filter
            include_states: Include per-state metadata
            include_state_weights: Include runtime state/machine weights
            include_relevant_anim_times: Include relevant asset-player time/remaining values per state
            include_transition_progress: Include active transition elapsed/crossfade progress
            include_inactive_transition_progress: Include inactive transition progress entries
            include_montages: Include active montage data
            include_curves: Include curve values
            curve_names: Optional curve names
            max_state_machines: Maximum state machines to include
            max_state_machine_instances_to_probe: Maximum runtime AnimNode indexes to probe
            max_states_per_machine: Maximum states per included machine
            max_transitions_per_machine: Maximum transitions per included machine for transition progress
            max_curves: Maximum curves to include
            prefer_pie_world: Prefer active PIE/SIE/play world before editor world
            require_pie_world: Fail instead of falling back to the editor world when no PIE/SIE/play world matches

        Returns:
            Response containing baseline and per-case state-machine snapshots.
        """
        try:
            params: Dict[str, Any] = {
                "actor_label": actor_label,
                "actor_name": actor_name,
                "actor_path": actor_path,
                "component_name": component_name,
                "name": name,
                "tick_count": tick_count,
                "tick_delta_time": tick_delta_time,
                "refresh_bone_transforms": refresh_bone_transforms,
                "restore_after_case": restore_after_case,
                "include_baseline": include_baseline,
                "state_machine_name": state_machine_name,
                "include_states": include_states,
                "include_state_weights": include_state_weights,
                "include_relevant_anim_times": include_relevant_anim_times,
                "include_transition_progress": include_transition_progress,
                "include_inactive_transition_progress": include_inactive_transition_progress,
                "include_montages": include_montages,
                "include_curves": include_curves,
                "max_state_machines": max_state_machines,
                "max_state_machine_instances_to_probe": max_state_machine_instances_to_probe,
                "max_states_per_machine": max_states_per_machine,
                "max_transitions_per_machine": max_transitions_per_machine,
                "max_curves": max_curves,
                "prefer_pie_world": prefer_pie_world,
                "require_pie_world": require_pie_world,
            }
            if cases is not None:
                params["cases"] = cases
            if properties is not None:
                params["properties"] = properties
            if curve_names is not None:
                params["curve_names"] = curve_names
            return send_node_command("sample_anim_state_machine_runtime_response", params)
        except Exception as e:
            error_msg = f"Error sampling AnimInstance state-machine runtime response: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_anim_graph_rigidbody_settings(
        ctx: Context,
        blueprint_name: str,
        node_id: str = "",
        alpha = None,
        external_force = None,
        simulation_space: str = "",
        enable_world_geometry = None,
        allow_non_sample: bool = False,
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function"
    ) -> Dict[str, Any]:
        """
        Set a narrow set of RigidBody AnimGraph node settings.

        By default this refuses to modify Animation Blueprints outside `/Game/_MCP_Sample/`.

        Args:
            blueprint_name: Animation Blueprint name or path
            node_id: Optional RigidBody node GUID filter
            alpha: Optional RigidBody alpha value
            external_force: Optional [X, Y, Z] external force vector
            simulation_space: Optional ComponentSpace, WorldSpace, or BaseBoneSpace
            enable_world_geometry: Optional world geometry collision toggle
            allow_non_sample: Allow editing non-`/Game/_MCP_Sample/` assets
            graph_name: Target animation graph name, defaults to AnimGraph
            graph_id: Optional target graph GUID
            graph_type: Graph selector type, defaults to function

        Returns:
            Response containing updated RigidBody node metadata and reflected settings.
        """
        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_id": node_id,
                "allow_non_sample": allow_non_sample,
            }
            if alpha is not None:
                params["alpha"] = alpha
            if external_force is not None:
                params["external_force"] = external_force
            if simulation_space:
                params["simulation_space"] = simulation_space
            if enable_world_geometry is not None:
                params["enable_world_geometry"] = enable_world_geometry
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("set_anim_graph_rigidbody_settings", params)
        except Exception as e:
            error_msg = f"Error setting AnimGraph RigidBody settings: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_anim_graph_input_pose_passthrough(
        ctx: Context,
        blueprint_name: str,
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function",
        replace_existing: bool = False,
        input_node_position = None,
        allow_non_sample: bool = False
    ) -> Dict[str, Any]:
        """
        Ensure an Animation Blueprint AnimGraph passes the incoming input pose to the root output.

        This is intended for Post Process AnimBP learning/setup: it creates or reuses a
        Linked Input Pose node and connects its pose output to the AnimGraph root Result pin.

        Args:
            blueprint_name: Anim Blueprint name or path
            graph_name: Target animation graph name, defaults to AnimGraph
            graph_id: Optional target graph GUID
            graph_type: Graph selector type, defaults to function because AnimGraph is reported there
            replace_existing: Replace an existing root Result pose link when it is not the input pose
            input_node_position: Optional [X, Y] position for a newly created input pose node
            allow_non_sample: Allow editing non-`/Game/_MCP_Sample/` AnimBPs

        Returns:
            Response containing created/reused node metadata and connection status.
        """
        try:
            if input_node_position is None:
                input_node_position = [-320, 0]
            params = {
                "blueprint_name": blueprint_name,
                "replace_existing": replace_existing,
                "input_node_position": input_node_position,
                "allow_non_sample": allow_non_sample,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("ensure_anim_graph_input_pose_passthrough", params)
        except Exception as e:
            error_msg = f"Error ensuring AnimGraph input pose pass-through: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_anim_graph_modify_bone_demo(
        ctx: Context,
        blueprint_name: str,
        bone_name: str = "head",
        rotation = None,
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function",
        replace_existing: bool = False,
        allow_non_sample: bool = False
    ) -> Dict[str, Any]:
        """
        Ensure an AnimGraph contains a safe Modify Bone demonstration chain.

        The resulting chain is:
        Linked Input Pose -> Local To Component Space -> Transform (Modify) Bone ->
        Component To Local Space -> Root Result.

        Args:
            blueprint_name: Anim Blueprint name or path
            bone_name: Bone to modify, defaults to head
            rotation: Optional [Pitch, Yaw, Roll] additive rotation in bone space
            graph_name: Target animation graph name, defaults to AnimGraph
            graph_id: Optional target graph GUID
            graph_type: Graph selector type, defaults to function because AnimGraph is reported there
            replace_existing: Replace existing pose links on the target chain pins
            allow_non_sample: Allow editing non-`/Game/_MCP_Sample/` AnimBPs

        Returns:
            Response containing chain node metadata and connection status.
        """
        try:
            if rotation is None:
                rotation = [0, 0, 6]
            params = {
                "blueprint_name": blueprint_name,
                "bone_name": bone_name,
                "rotation": rotation,
                "replace_existing": replace_existing,
                "allow_non_sample": allow_non_sample,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("ensure_anim_graph_modify_bone_demo", params)
        except Exception as e:
            error_msg = f"Error ensuring AnimGraph Modify Bone demo chain: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_postprocess_anim_demo_variant(
        ctx: Context,
        source_blueprint_name: str,
        source_skeletal_mesh: str,
        variant_name: str = "Variant",
        target_root: str = "/Game/_MCP_Sample/AnimStudy",
        target_blueprint_name: str = "",
        target_skeletal_mesh: str = "",
        bone_name: str = "head",
        rotation = None,
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function",
        replace_existing: bool = True,
        overwrite_existing: bool = False,
        compile: bool = True,
        save: bool = True,
        dry_run: bool = False,
        allow_non_sample: bool = False
    ) -> Dict[str, Any]:
        """
        Create or reuse a sample Post Process AnimBP demo variant and assign it to a duplicated SkeletalMesh.

        The command duplicates the source AnimBP/SkeletalMesh to target sample paths, ensures the
        target AnimGraph contains the Modify Bone demo chain, compiles the AnimBP, and sets the
        target SkeletalMesh Post Process AnimBlueprint to the generated class.

        Args:
            source_blueprint_name: Source Anim Blueprint name or path to duplicate
            source_skeletal_mesh: Source SkeletalMesh path to duplicate
            variant_name: Name segment used for default target paths
            target_root: Target content folder, defaults to /Game/_MCP_Sample/AnimStudy
            target_blueprint_name: Optional explicit target AnimBP package path
            target_skeletal_mesh: Optional explicit target SkeletalMesh package path
            bone_name: Bone to modify in the Post Process AnimGraph
            rotation: Optional [Pitch, Yaw, Roll] additive rotation in bone space
            graph_name: Target animation graph name, defaults to AnimGraph
            graph_id: Optional target graph GUID
            graph_type: Graph selector type, defaults to function
            replace_existing: Replace existing pose links needed for the demo chain
            overwrite_existing: Delete and recreate existing target assets instead of reusing them
            compile: Compile the target AnimBP before assigning it
            save: Save target AnimBP and SkeletalMesh assets
            dry_run: Validate and report paths without editing assets
            allow_non_sample: Allow target paths outside /Game/_MCP_Sample

        Returns:
            Response with target asset paths, graph result, compile result, and save result.
        """
        try:
            if rotation is None:
                rotation = [0, 0, 6]
            params = {
                "source_blueprint_name": source_blueprint_name,
                "source_skeletal_mesh": source_skeletal_mesh,
                "variant_name": variant_name,
                "target_root": target_root,
                "bone_name": bone_name,
                "rotation": rotation,
                "replace_existing": replace_existing,
                "overwrite_existing": overwrite_existing,
                "compile": compile,
                "save": save,
                "dry_run": dry_run,
                "allow_non_sample": allow_non_sample,
            }
            if target_blueprint_name:
                params["target_blueprint_name"] = target_blueprint_name
            if target_skeletal_mesh:
                params["target_skeletal_mesh"] = target_skeletal_mesh
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("ensure_postprocess_anim_demo_variant", params)
        except Exception as e:
            error_msg = f"Error ensuring Post Process AnimBP demo variant: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_anim_graph_modify_curve_demo(
        ctx: Context,
        blueprint_name: str,
        curve_values: Optional[Dict[str, float]] = None,
        alpha: float = 1.0,
        apply_mode: str = "Add",
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function",
        replace_existing: bool = False,
        allow_non_sample: bool = False
    ) -> Dict[str, Any]:
        """
        Ensure an AnimGraph contains a safe Modify Curve demonstration chain.

        The resulting chain is:
        Linked Input Pose -> Modify Curve -> Root Result.

        By default this command only modifies `/Game/_MCP_Sample/` AnimBPs.
        The default curve values are tuned for the StackOBot ControlRig gate study:
        IK_blend_interact=1.0 and IKBlend_l=1.0.

        Args:
            blueprint_name: Anim Blueprint name or path
            curve_values: Optional curve name to value map
            alpha: Modify Curve alpha, clamped by Unreal to 0..1
            apply_mode: Add, Scale, Blend, WeightedMovingAverage, or RemapCurve
            graph_name: Target animation graph name, defaults to AnimGraph
            graph_id: Optional target graph GUID
            graph_type: Graph selector type, defaults to function because AnimGraph is reported there
            replace_existing: Replace existing pose links on the target chain pins
            allow_non_sample: Allow editing non-`/Game/_MCP_Sample/` AnimBPs

        Returns:
            Response containing chain node metadata and reflected Modify Curve settings.
        """
        try:
            if curve_values is None:
                curve_values = {
                    "IK_blend_interact": 1.0,
                    "IKBlend_l": 1.0,
                }
            params = {
                "blueprint_name": blueprint_name,
                "curve_values": curve_values,
                "alpha": alpha,
                "apply_mode": apply_mode,
                "replace_existing": replace_existing,
                "allow_non_sample": allow_non_sample,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("ensure_anim_graph_modify_curve_demo", params)
        except Exception as e:
            error_msg = f"Error ensuring AnimGraph Modify Curve demo chain: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_anim_graph_controlrig_input_defaults(
        ctx: Context,
        blueprint_name: str,
        input_defaults: Optional[Dict[str, Any]] = None,
        expose_input_pins: Optional[List[str]] = None,
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function",
        node_id: str = "",
        node_name: str = "",
        title_contains: str = "",
        control_rig_class: str = "",
        disconnect_existing_links: bool = False,
        allow_non_sample: bool = False
    ) -> Dict[str, Any]:
        """
        Expose ControlRig AnimGraph input pins and optionally set their default values.

        By default this command only modifies `/Game/_MCP_Sample/` AnimBPs.
        The default inputs are tuned for the StackOBot ControlRig forced-driver study:
        ShouldDoIKTrace=true and InteractionWorldLocation=[80, -40, 80].

        Args:
            blueprint_name: Anim Blueprint name or path
            input_defaults: Optional input/control name to value map
            expose_input_pins: Optional input/control names to expose without assigning defaults
            graph_name: Target animation graph name, defaults to AnimGraph
            graph_id: Optional target graph GUID
            graph_type: Graph selector type, defaults to function because AnimGraph is reported there
            node_id: Optional ControlRig node GUID or object name filter
            node_name: Optional ControlRig node object name filter
            title_contains: Optional ControlRig node title substring filter
            control_rig_class: Optional ControlRig generated class path or substring filter
            disconnect_existing_links: Disconnect existing links on target pins so defaults drive the sample
            allow_non_sample: Allow editing non-`/Game/_MCP_Sample/` AnimBPs

        Returns:
            Response containing node metadata, available input names, exposed pins, and applied defaults.
        """
        try:
            if input_defaults is None and not expose_input_pins:
                input_defaults = {
                    "ShouldDoIKTrace": True,
                    "InteractionWorldLocation": [80, -40, 80],
                }
            params = {
                "blueprint_name": blueprint_name,
                "disconnect_existing_links": disconnect_existing_links,
                "allow_non_sample": allow_non_sample,
            }
            if input_defaults is not None:
                params["input_defaults"] = input_defaults
            if expose_input_pins is not None:
                params["expose_input_pins"] = expose_input_pins
            if node_id:
                params["node_id"] = node_id
            if node_name:
                params["node_name"] = node_name
            if title_contains:
                params["title_contains"] = title_contains
            if control_rig_class:
                params["control_rig_class"] = control_rig_class
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("set_anim_graph_controlrig_input_defaults", params)
        except Exception as e:
            error_msg = f"Error setting ControlRig AnimGraph input defaults: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_controlrig_forced_driver_animbp(
        ctx: Context,
        blueprint_name: str,
        curve_values: Optional[Dict[str, float]] = None,
        input_defaults: Optional[Dict[str, Any]] = None,
        alpha: float = 1.0,
        apply_mode: str = "Add",
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function",
        node_id: str = "",
        node_name: str = "",
        title_contains: str = "",
        control_rig_class: str = "",
        replace_existing: bool = True,
        disconnect_existing_links: bool = True,
        allow_non_sample: bool = False
    ) -> Dict[str, Any]:
        """
        Ensure a sample AnimBP has a forced ControlRig driver path.

        The command inserts a Modify Curve node directly before the selected
        ControlRig node, sets forced curve values, exposes/sets ControlRig input
        defaults, and can disconnect existing input links so defaults drive the sample.

        By default this command only modifies `/Game/_MCP_Sample/` AnimBPs.

        Args:
            blueprint_name: Anim Blueprint name or path
            curve_values: Optional curve name to value map
            input_defaults: Optional ControlRig input/control name to value map
            alpha: Modify Curve alpha, clamped by Unreal to 0..1
            apply_mode: Add, Scale, Blend, WeightedMovingAverage, or RemapCurve
            graph_name: Target animation graph name, defaults to AnimGraph
            graph_id: Optional target graph GUID
            graph_type: Graph selector type, defaults to function because AnimGraph is reported there
            node_id: Optional ControlRig node GUID or object name filter
            node_name: Optional ControlRig node object name filter
            title_contains: Optional ControlRig node title substring filter
            control_rig_class: Optional ControlRig generated class path or substring filter
            replace_existing: Replace pose links needed to insert Modify Curve before ControlRig
            disconnect_existing_links: Disconnect target ControlRig input links so defaults drive the sample
            allow_non_sample: Allow editing non-`/Game/_MCP_Sample/` AnimBPs

        Returns:
            Response containing ControlRig/ModifyCurve node metadata, applied curves, and input defaults.
        """
        try:
            if curve_values is None:
                curve_values = {
                    "IK_blend_interact": 1.0,
                    "IKBlend_l": 1.0,
                }
            if input_defaults is None:
                input_defaults = {
                    "ShouldDoIKTrace": True,
                    "InteractionWorldLocation": [80, -40, 80],
                }
            params = {
                "blueprint_name": blueprint_name,
                "curve_values": curve_values,
                "input_defaults": input_defaults,
                "alpha": alpha,
                "apply_mode": apply_mode,
                "replace_existing": replace_existing,
                "disconnect_existing_links": disconnect_existing_links,
                "allow_non_sample": allow_non_sample,
            }
            if node_id:
                params["node_id"] = node_id
            if node_name:
                params["node_name"] = node_name
            if title_contains:
                params["title_contains"] = title_contains
            if control_rig_class:
                params["control_rig_class"] = control_rig_class
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("ensure_controlrig_forced_driver_animbp", params)
        except Exception as e:
            error_msg = f"Error ensuring ControlRig forced-driver AnimBP: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_anim_graph_trail_demo(
        ctx: Context,
        blueprint_name: str,
        trail_bone: str = "VB VBHead",
        base_joint: str = "head",
        chain_length: int = 2,
        chain_bone_axis: str = "X",
        alpha: float = 1.0,
        fake_velocity = None,
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function",
        replace_existing: bool = False,
        allow_non_sample: bool = False,
        invert_chain_bone_axis: bool = False,
        reorient_parent_to_child: bool = True,
        actor_space_fake_velocity: bool = False,
        relaxation_speed_scale: float = 1.0,
        limit_stretch: bool = False,
        stretch_limit: float = 0.0,
        max_delta_time: float = 0.0
    ) -> Dict[str, Any]:
        """
        Ensure an AnimGraph contains a Trail Controller demonstration chain.

        The resulting chain is:
        Linked Input Pose -> Local To Component Space -> Trail ->
        Component To Local Space -> Root Result.

        By default this command only modifies `/Game/_MCP_Sample/` AnimBPs.

        Args:
            blueprint_name: Anim Blueprint name or path
            trail_bone: Active trail bone, defaults to StackOBot's VB VBHead
            base_joint: Base joint for velocity, defaults to head
            chain_length: Number of bones in the trail chain, minimum 2
            chain_bone_axis: X, Y, or Z
            alpha: Trail node alpha
            fake_velocity: Optional [X, Y, Z] fake velocity for static preview tests
            graph_name: Target animation graph name, defaults to AnimGraph
            graph_id: Optional target graph GUID
            graph_type: Graph selector type, defaults to function because AnimGraph is reported there
            replace_existing: Replace existing pose links on the target chain pins
            allow_non_sample: Allow editing non-`/Game/_MCP_Sample/` AnimBPs
            invert_chain_bone_axis: Invert the selected chain bone axis
            reorient_parent_to_child: Reorient parent bones toward children
            actor_space_fake_velocity: Apply fake velocity in actor space instead of world space
            relaxation_speed_scale: Trail relaxation scale
            limit_stretch: Enable stretch limiting
            stretch_limit: Stretch limit when enabled
            max_delta_time: Optional timestep clamp

        Returns:
            Response containing chain node metadata and reflected Trail settings.
        """
        try:
            if fake_velocity is None:
                fake_velocity = [0, 0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "trail_bone": trail_bone,
                "base_joint": base_joint,
                "chain_length": chain_length,
                "chain_bone_axis": chain_bone_axis,
                "alpha": alpha,
                "fake_velocity": fake_velocity,
                "replace_existing": replace_existing,
                "allow_non_sample": allow_non_sample,
                "invert_chain_bone_axis": invert_chain_bone_axis,
                "reorient_parent_to_child": reorient_parent_to_child,
                "actor_space_fake_velocity": actor_space_fake_velocity,
                "relaxation_speed_scale": relaxation_speed_scale,
                "limit_stretch": limit_stretch,
                "stretch_limit": stretch_limit,
                "max_delta_time": max_delta_time,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("ensure_anim_graph_trail_demo", params)
        except Exception as e:
            error_msg = f"Error ensuring AnimGraph Trail demo chain: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def ensure_anim_graph_secondary_motion_chain(
        ctx: Context,
        blueprint_name: str,
        dynamics_type: str,
        root_bone: str,
        chain_end_bone: str,
        alpha: float = 1.0,
        gravity: Optional[List[float]] = None,
        angular_limit_degrees: float = 60.0,
        body_extents: Optional[List[float]] = None,
        linear_damping: float = 0.75,
        angular_damping: float = 0.75,
        kawaii_damping: float = 0.08,
        kawaii_stiffness: float = 0.03,
        graph_name: str = "AnimGraph",
        graph_id: str = "",
        graph_type: str = "function",
        replace_existing: bool = True,
        allow_non_sample: bool = False,
        compile: bool = True,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Ensure a reference-pose secondary-motion chain in an Animation Blueprint.

        The authored topology is Local Ref Pose -> Local To Component Space ->
        AnimDynamics or KawaiiPhysics -> Component To Local Space -> Result.
        AnimDynamics is authored with a world-space, gravity-driven constrained
        chain. KawaiiPhysics is an optional plugin and is discovered/configured
        reflectively so UnrealMCP itself does not depend on it.

        Args:
            blueprint_name: Animation Blueprint name or path
            dynamics_type: anim_dynamics or kawaii_physics
            root_bone: Top/anchored bone of the simulated chain
            chain_end_bone: Descendant end bone; used directly by AnimDynamics and as a topology guard for Kawaii
            alpha: Skeletal-control blend alpha
            gravity: World-space acceleration [x, y, z], defaults to [0, 0, -980]
            angular_limit_degrees: Cone/rotation limit, clamped to 0..90
            body_extents: AnimDynamics body extents [x, y, z]
            linear_damping: AnimDynamics linear damping override (minimum effective value 0.7)
            angular_damping: AnimDynamics angular damping override (minimum effective value 0.7)
            kawaii_damping: KawaiiPhysics damping
            kawaii_stiffness: KawaiiPhysics stiffness
            graph_name: Target graph, defaults to AnimGraph
            graph_id: Optional graph GUID selector
            graph_type: Graph selector type
            replace_existing: Replace conflicting pose links on the authored chain
            allow_non_sample: Explicitly allow intentional edits outside _MCP sample/temp folders
            compile: Compile after graph mutation
            save: Save only after a successful compile

        Returns:
            Node topology/settings, optional-plugin availability, compile/save readback, and MCP-dependency guard.
        """
        try:
            normalized_type = dynamics_type.strip().lower()
            if normalized_type not in {"anim_dynamics", "animdynamics", "kawaii_physics", "kawaii"}:
                return tool_error(
                    "dynamics_type must be anim_dynamics or kawaii_physics",
                    error_code="secondary_motion_invalid_dynamics_type",
                )
            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "dynamics_type": normalized_type,
                "root_bone": root_bone,
                "chain_end_bone": chain_end_bone,
                "alpha": alpha,
                "gravity": gravity if gravity is not None else [0.0, 0.0, -980.0],
                "angular_limit_degrees": angular_limit_degrees,
                "body_extents": body_extents if body_extents is not None else [5.0, 5.0, 30.0],
                "linear_damping": linear_damping,
                "angular_damping": angular_damping,
                "kawaii_damping": kawaii_damping,
                "kawaii_stiffness": kawaii_stiffness,
                "replace_existing": replace_existing,
                "allow_non_sample": allow_non_sample,
                "compile": compile,
                "save": save,
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("ensure_anim_graph_secondary_motion_chain", params)
        except Exception as e:
            error_msg = f"Error ensuring AnimGraph secondary-motion chain: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_branch_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """Add a Branch node to a Blueprint graph."""
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_branch_node", params)
        except Exception as e:
            error_msg = f"Error adding branch node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_sequence_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None,
        output_count: int = 2,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """Add a Sequence node to a Blueprint graph."""
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "node_position": node_position,
                "output_count": output_count
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_sequence_node", params)
        except Exception as e:
            error_msg = f"Error adding sequence node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_return_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "function",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """Add or resolve a Return node in a Blueprint function graph."""
        try:
            if node_position is None:
                node_position = [320, 0]
            params = {
                "blueprint_name": blueprint_name,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_return_node", params)
        except Exception as e:
            error_msg = f"Error adding return node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_dynamic_cast_node(
        ctx: Context,
        blueprint_name: str,
        target_class: str,
        node_position = None,
        pure: bool = False,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """Add a Dynamic Cast node to a Blueprint graph."""
        try:
            reject_mcp_dependency_reference("target_class", target_class)
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "target_class": target_class,
                "node_position": node_position,
                "pure": pure
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_dynamic_cast_node", params)
        except Exception as e:
            error_msg = f"Error adding dynamic cast node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_loop_node(
        ctx: Context,
        blueprint_name: str,
        loop_type: str = "for_loop",
        first_index: Optional[int] = None,
        last_index: Optional[int] = None,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a supported Blueprint loop macro node.

        Section 7 stable scope supports only loop_type="for_loop".
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "loop_type": loop_type,
                "node_position": node_position,
            }
            if first_index is not None:
                params["first_index"] = first_index
            if last_index is not None:
                params["last_index"] = last_index
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_loop_node", params)
        except Exception as e:
            error_msg = f"Error adding loop node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_array_function_node(
        ctx: Context,
        blueprint_name: str,
        operation: str,
        element_type: str,
        param_defaults: Optional[Dict[str, Any]] = None,
        type_object: str = "",
        enum_type: str = "",
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a typed Blueprint array function node.

        Supported operations: length, get, set, add, add_unique, insert, contains, remove, clear.
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            if param_defaults is None:
                param_defaults = {}
            params = {
                "blueprint_name": blueprint_name,
                "operation": operation,
                "element_type": element_type,
                "param_defaults": param_defaults,
                "node_position": node_position,
            }
            if type_object:
                params["type_object"] = type_object
            if enum_type:
                params["enum_type"] = enum_type
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_array_function_node", params)
        except Exception as e:
            error_msg = f"Error adding array function node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_make_array_node(
        ctx: Context,
        blueprint_name: str,
        element_type: str,
        input_count: Optional[int] = None,
        values: Optional[List[Any]] = None,
        type_object: str = "",
        enum_type: str = "",
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a typed Blueprint Make Array node.

        Creates a 1D array literal with explicit element pin typing.
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "element_type": element_type,
                "node_position": node_position,
            }
            if input_count is not None:
                params["input_count"] = input_count
            if values is not None:
                params["values"] = values
            if type_object:
                params["type_object"] = type_object
            if enum_type:
                params["enum_type"] = enum_type
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_make_array_node", params)
        except Exception as e:
            error_msg = f"Error adding make array node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_input_axis_event_node(
        ctx: Context,
        blueprint_name: str,
        axis_name: str,
        node_position = None,
        consume_input: bool = True,
        execute_when_paused: bool = False,
        override_parent_binding: bool = False,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a legacy input axis event node to a Blueprint event graph.

        Args:
            blueprint_name: Blueprint name or path
            axis_name: Project input axis mapping name
            node_position: Optional [X, Y] position in the graph
            consume_input: Whether this event consumes input
            execute_when_paused: Whether this event runs while paused
            override_parent_binding: Whether parent bindings are overridden

        Returns:
            Response containing the created node metadata
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "axis_name": axis_name,
                "node_position": node_position,
                "consume_input": consume_input,
                "execute_when_paused": execute_when_paused,
                "override_parent_binding": override_parent_binding
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_input_axis_event_node", params)
        except Exception as e:
            error_msg = f"Error adding input axis event node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_enhanced_input_action_node(
        ctx: Context,
        blueprint_name: str,
        input_action: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add an Enhanced Input action event node to a Blueprint event graph.

        Args:
            blueprint_name: Blueprint name or path
            input_action: UInputAction asset path
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            reject_mcp_dependency_reference("input_action", input_action)
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "input_action": input_action,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_enhanced_input_action_node", params)
        except Exception as e:
            error_msg = f"Error adding enhanced input action node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_variable_get_node(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        node_position = None,
        target_class: str = "",
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a variable getter node to a Blueprint event graph.

        Args:
            blueprint_name: Blueprint name or path
            variable_name: Variable or component member name
            node_position: Optional [X, Y] position in the graph
            target_class: Optional class path/name for external object member variables

        Returns:
            Response containing the created node metadata
        """
        try:
            reject_mcp_dependency_reference("target_class", target_class)
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
                "node_position": node_position
            }
            if target_class:
                params["target_class"] = target_class
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_variable_get_node", params)
        except Exception as e:
            error_msg = f"Error adding variable get node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_variable_set_node(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        node_position = None,
        target_class: str = "",
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a variable setter node to a Blueprint event graph.

        Args:
            blueprint_name: Blueprint name or path
            variable_name: Variable member name
            node_position: Optional [X, Y] position in the graph
            target_class: Optional class path/name for external object member variables

        Returns:
            Response containing the created node metadata
        """
        try:
            reject_mcp_dependency_reference("target_class", target_class)
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
                "node_position": node_position
            }
            if target_class:
                params["target_class"] = target_class
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_variable_set_node", params)
        except Exception as e:
            error_msg = f"Error adding variable set node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_math_node(
        ctx: Context,
        blueprint_name: str,
        operation: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a Kismet Math Library function node to a Blueprint event graph.

        Args:
            blueprint_name: Blueprint name or path
            operation: add, subtract, multiply, divide, clamp, or in_range
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "operation": operation,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_math_node", params)
        except Exception as e:
            error_msg = f"Error adding math node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_compare_node(
        ctx: Context,
        blueprint_name: str,
        operation: str,
        value_type: str = "double",
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a comparison expression node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            operation: less, greater, less_equal, greater_equal, equal, not_equal, or symbolic aliases
            value_type: int, int64, double, bool, object, class, or name
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "operation": operation,
                "value_type": value_type,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_compare_node", params)
        except Exception as e:
            error_msg = f"Error adding compare node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_boolean_node(
        ctx: Context,
        blueprint_name: str,
        operation: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a boolean expression node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            operation: not, and, nand, or, xor, nor, equal, not_equal, or symbolic aliases
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "operation": operation,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_boolean_node", params)
        except Exception as e:
            error_msg = f"Error adding boolean node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_select_node(
        ctx: Context,
        blueprint_name: str,
        value_type: str = "int",
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a typed Select expression node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            value_type: string, text, name, int, double, vector, rotator, color, transform, object, or class
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "value_type": value_type,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_select_node", params)
        except Exception as e:
            error_msg = f"Error adding select node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_literal_node(
        ctx: Context,
        blueprint_name: str,
        literal_type: str,
        value: Any = None,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a typed literal expression node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            literal_type: int, int64, float, double, bool, name, byte, string, or text
            value: Optional literal default value
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "literal_type": literal_type,
                "node_position": node_position
            }
            if value is not None:
                params["value"] = value
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_literal_node", params)
        except Exception as e:
            error_msg = f"Error adding literal node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_enum_literal_node(
        ctx: Context,
        blueprint_name: str,
        enum_type: str,
        value: Any = None,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a typed enum literal expression node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            enum_type: Native or asset enum path, preferably /Script/Module.EnumName
            value: Optional enum entry name or integer enum value
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            reject_mcp_dependency_reference("enum_type", enum_type)
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "enum_type": enum_type,
                "node_position": node_position
            }
            if value is not None:
                params["value"] = value
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_enum_literal_node", params)
        except Exception as e:
            error_msg = f"Error adding enum literal node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_is_valid_node(
        ctx: Context,
        blueprint_name: str,
        value_type: str = "object",
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add an Is Valid expression node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            value_type: object or class
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "value_type": value_type,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_is_valid_node", params)
        except Exception as e:
            error_msg = f"Error adding is_valid node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_make_struct_node(
        ctx: Context,
        blueprint_name: str,
        struct_type: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a native Make Struct helper node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            struct_type: vector, rotator, or transform
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "struct_type": struct_type,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_make_struct_node", params)
        except Exception as e:
            error_msg = f"Error adding make struct node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_break_struct_node(
        ctx: Context,
        blueprint_name: str,
        struct_type: str,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a native Break Struct helper node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            struct_type: vector, rotator, or transform
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "struct_type": struct_type,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_break_struct_node", params)
        except Exception as e:
            error_msg = f"Error adding break struct node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_switch_int_node(
        ctx: Context,
        blueprint_name: str,
        start_index: int = 0,
        case_count: int = 2,
        case_values: Optional[List[int]] = None,
        has_default_pin: bool = True,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a contiguous Switch on Int node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            start_index: First case value when case_values is omitted
            case_count: Number of contiguous case pins when case_values is omitted
            case_values: Optional ascending contiguous integer cases
            has_default_pin: Whether to include the Default exec output
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "start_index": start_index,
                "case_count": case_count,
                "has_default_pin": has_default_pin,
                "node_position": node_position
            }
            if case_values is not None:
                params["case_values"] = case_values
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_switch_int_node", params)
        except Exception as e:
            error_msg = f"Error adding switch int node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def add_blueprint_switch_enum_node(
        ctx: Context,
        blueprint_name: str,
        enum_type: str,
        has_default_pin: bool = False,
        node_position = None,
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = "",
        create_graph_if_missing: bool = False
    ) -> Dict[str, Any]:
        """
        Add a Switch on Enum node to a Blueprint graph.

        Args:
            blueprint_name: Blueprint name or path
            enum_type: Native or asset enum path, preferably /Script/Module.EnumName
            has_default_pin: Whether to include the Default exec output
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing the created node metadata
        """
        try:
            reject_mcp_dependency_reference("enum_type", enum_type)
            if node_position is None:
                node_position = [0, 0]
            params = {
                "blueprint_name": blueprint_name,
                "enum_type": enum_type,
                "has_default_pin": has_default_pin,
                "node_position": node_position
            }
            add_graph_selector(params, graph_name, graph_id, graph_type, create_graph_if_missing)
            return send_node_command("add_blueprint_switch_enum_node", params)
        except Exception as e:
            error_msg = f"Error adding switch enum node: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)

    @mcp.tool()
    def set_blueprint_pin_default(
        ctx: Context,
        blueprint_name: str,
        node_id: str,
        pin_name: str,
        value: Any,
        direction: str = "",
        graph_name: str = "",
        graph_id: str = "",
        graph_type: str = ""
    ) -> Dict[str, Any]:
        """
        Set the default value or default object on a Blueprint node pin.

        Args:
            blueprint_name: Blueprint name or path
            node_id: Node GUID
            pin_name: Pin name
            value: Default value, object path, or class path
            direction: Optional input/output pin filter

        Returns:
            Response containing the updated pin metadata
        """
        try:
            reject_mcp_dependency_references("value", value)
            params = {
                "blueprint_name": blueprint_name,
                "node_id": node_id,
                "pin_name": pin_name,
                "value": value,
                "direction": direction
            }
            add_graph_selector(params, graph_name, graph_id, graph_type)
            return send_node_command("set_blueprint_pin_default", params)
        except Exception as e:
            error_msg = f"Error setting pin default: {e}"
            logger.error(error_msg)
            return _node_exception_error(error_msg, e)
    
    logger.info("Blueprint node tools registered successfully")
