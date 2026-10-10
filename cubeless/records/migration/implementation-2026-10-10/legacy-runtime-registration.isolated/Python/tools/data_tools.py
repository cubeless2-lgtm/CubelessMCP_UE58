"""DataTable / DataAsset tools for Unreal MCP.

Editor-only data authoring through native UnrealMCP bridge commands.
Row values and asset properties are plain JSON matching the row struct /
class fields (verify field names with get_datatable_schema first).
"""

import logging
from typing import Any, Dict, Optional

from mcp.server.fastmcp import Context, FastMCP

from tools.dependency_guard import (
    reject_mcp_dependency_reference,
    reject_mcp_dependency_references,
)
from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")


def register_data_tools(mcp: FastMCP):
    """Register DataTable/DataAsset tools with the MCP server."""

    def send_data_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
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

    def data_exception_error(message: str, exc: Exception) -> Dict[str, Any]:
        return tool_error(
            message,
            error_code="data_tool_failed",
            details={"exception_type": type(exc).__name__},
        )

    @mcp.tool()
    def get_datatable_schema(ctx: Context, datatable_path: str) -> Dict[str, Any]:
        """
        Read a DataTable's row struct field list (name, authored name, C++ type) and row count.

        Args:
            datatable_path: DataTable asset name/path
        """
        try:
            return send_data_command("get_datatable_schema", {"datatable_path": datatable_path})
        except Exception as e:
            error_msg = f"Error reading DataTable schema: {e}"
            logger.error(error_msg)
            return data_exception_error(error_msg, e)

    @mcp.tool()
    def list_datatable_rows(
        ctx: Context,
        datatable_path: str,
        row_name_contains: str = "",
        max_rows: int = 200,
        include_values: bool = True,
    ) -> Dict[str, Any]:
        """
        List DataTable rows with optional name filter and value payloads.

        Args:
            datatable_path: DataTable asset name/path
            row_name_contains: Optional row-name substring filter
            max_rows: Maximum rows to return (1..5000)
            include_values: Include per-row JSON values
        """
        try:
            params = {
                "datatable_path": datatable_path,
                "row_name_contains": row_name_contains,
                "max_rows": max_rows,
                "include_values": include_values,
            }
            return send_data_command("list_datatable_rows", params)
        except Exception as e:
            error_msg = f"Error listing DataTable rows: {e}"
            logger.error(error_msg)
            return data_exception_error(error_msg, e)

    @mcp.tool()
    def get_datatable_row(ctx: Context, datatable_path: str, row_name: str) -> Dict[str, Any]:
        """
        Read one DataTable row as JSON.

        Args:
            datatable_path: DataTable asset name/path
            row_name: Row name
        """
        try:
            return send_data_command(
                "get_datatable_row",
                {"datatable_path": datatable_path, "row_name": row_name},
            )
        except Exception as e:
            error_msg = f"Error reading DataTable row: {e}"
            logger.error(error_msg)
            return data_exception_error(error_msg, e)

    @mcp.tool()
    def add_or_update_datatable_row(
        ctx: Context,
        datatable_path: str,
        row_name: str,
        row_values: Dict[str, Any],
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Add a new row or update an existing row from a JSON object.

        Field names/types must match the row struct (see get_datatable_schema).
        The edit is one editor transaction (Ctrl+Z undoable).

        Args:
            datatable_path: DataTable asset name/path
            row_name: Row name to create or update
            row_values: JSON object of row struct fields
            save: Save the DataTable asset afterwards
        """
        try:
            reject_mcp_dependency_references("row_values", row_values)
            params = {
                "datatable_path": datatable_path,
                "row_name": row_name,
                "row_values": row_values,
                "save": save,
            }
            return send_data_command("add_or_update_datatable_row", params)
        except Exception as e:
            error_msg = f"Error adding/updating DataTable row: {e}"
            logger.error(error_msg)
            return data_exception_error(error_msg, e)

    @mcp.tool()
    def remove_datatable_row(
        ctx: Context,
        datatable_path: str,
        row_name: str,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Remove one DataTable row.

        Args:
            datatable_path: DataTable asset name/path
            row_name: Row name to remove
            save: Save the DataTable asset afterwards
        """
        try:
            params = {
                "datatable_path": datatable_path,
                "row_name": row_name,
                "save": save,
            }
            return send_data_command("remove_datatable_row", params)
        except Exception as e:
            error_msg = f"Error removing DataTable row: {e}"
            logger.error(error_msg)
            return data_exception_error(error_msg, e)

    @mcp.tool()
    def create_datatable(
        ctx: Context,
        asset_path: str,
        row_struct_path: str,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Create a new DataTable asset for an existing row struct.

        Args:
            asset_path: Target asset path, e.g. /Game/Data/DT_Example
            row_struct_path: Row struct path, e.g. /Game/Data/S_ExampleRow or /Script/Module.StructName
            save: Save the new asset
        """
        try:
            reject_mcp_dependency_reference("asset_path", asset_path)
            reject_mcp_dependency_reference("row_struct_path", row_struct_path)
            params = {
                "asset_path": asset_path,
                "row_struct_path": row_struct_path,
                "save": save,
            }
            return send_data_command("create_datatable", params)
        except Exception as e:
            error_msg = f"Error creating DataTable: {e}"
            logger.error(error_msg)
            return data_exception_error(error_msg, e)

    @mcp.tool()
    def get_data_asset(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """
        Read a DataAsset's class and full property values as JSON.

        Args:
            asset_path: DataAsset name/path
        """
        try:
            return send_data_command("get_data_asset", {"asset_path": asset_path})
        except Exception as e:
            error_msg = f"Error reading DataAsset: {e}"
            logger.error(error_msg)
            return data_exception_error(error_msg, e)

    @mcp.tool()
    def set_data_asset_properties(
        ctx: Context,
        asset_path: str,
        properties: Dict[str, Any],
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Set one or more DataAsset properties from a JSON object.

        Property names/types must match the asset class (see get_data_asset).
        The edit is one editor transaction (Ctrl+Z undoable).

        Args:
            asset_path: DataAsset name/path
            properties: JSON object of property values
            save: Save the asset afterwards
        """
        try:
            reject_mcp_dependency_references("properties", properties)
            params = {
                "asset_path": asset_path,
                "properties": properties,
                "save": save,
            }
            return send_data_command("set_data_asset_properties", params)
        except Exception as e:
            error_msg = f"Error setting DataAsset properties: {e}"
            logger.error(error_msg)
            return data_exception_error(error_msg, e)

    @mcp.tool()
    def create_data_asset(
        ctx: Context,
        asset_path: str,
        class_path: str,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Create a new DataAsset instance of a UDataAsset subclass.

        Args:
            asset_path: Target asset path, e.g. /Game/Data/DA_Example
            class_path: UDataAsset subclass path, e.g. /Script/Module.MyDataAsset or a Blueprint class path
            save: Save the new asset
        """
        try:
            reject_mcp_dependency_reference("asset_path", asset_path)
            reject_mcp_dependency_reference("class_path", class_path)
            params = {
                "asset_path": asset_path,
                "class_path": class_path,
                "save": save,
            }
            return send_data_command("create_data_asset", params)
        except Exception as e:
            error_msg = f"Error creating DataAsset: {e}"
            logger.error(error_msg)
            return data_exception_error(error_msg, e)
