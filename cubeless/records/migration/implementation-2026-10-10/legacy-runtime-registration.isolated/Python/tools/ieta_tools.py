"""
Ieta status tools for the Cubeless Unreal MCP workflow.

These tools surface connection state through the Unreal-side Slate status
window when the Cubeless UnrealMCP plugin exposes the `ieta_status` command.
"""

import logging
from typing import Any, Dict

from mcp.server.fastmcp import Context, FastMCP

from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")


def register_ieta_tools(mcp: FastMCP):
    """Register Ieta helper tools."""

    @mcp.tool()
    def show_ieta_connection_status(
        ctx: Context,
        text: str = "흥, Unreal MCP 연결은 확인했어.",
        command: str = "이에타",
        progress: float = 1.0,
    ) -> Dict[str, Any]:
        """
        Show the current MCP connection state in Unreal's Ieta Slate window.

        This is a UI/status operation only. It does not modify Unreal assets.

        Args:
            text: Status text to display in the Unreal Slate window.
            command: Label shown in the Slate window as the command/context.
            progress: Optional progress value from 0.0 to 1.0 for the Slate progress bar.

        Returns:
            Response from Unreal, or a not-connected result if the bridge cannot
            be reached.
        """
        from unreal_mcp_server import UNREAL_HOST, UNREAL_PORT, get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return tool_error(
                    f"Failed to connect to Unreal Engine bridge at {UNREAL_HOST}:{UNREAL_PORT}",
                    error_code="unreal_connection_failed",
                    connection_status="not_connected",
                    slate_call="failed",
                )

            response = unreal.send_command(
                "ieta_status",
                {
                    "text": text,
                    "command": command,
                    "progress": max(0.0, min(1.0, progress)),
                },
            )

            if not response:
                return tool_error(
                    "No response from Unreal Engine",
                    error_code="unreal_no_response",
                    connection_status="unknown",
                    slate_call="failed",
                )

            if response.get("status") == "success":
                return {
                    "success": True,
                    "connection_status": "connected",
                    "slate_call": "success",
                    "message": "Ieta connection status was shown in Unreal Slate. Slate call: success.",
                    "unreal_response": response,
                }

            return tool_error(
                response.get("error") or response.get("message") or "Unreal returned an error",
                error_code="ieta_status_failed",
                connection_status="unknown",
                slate_call="failed",
                unreal_response=response,
            )
        except Exception as exc:
            logger.exception("Error showing Ieta connection status")
            return tool_error(
                str(exc),
                error_code="ieta_status_failed",
                connection_status="unknown",
                slate_call="failed",
                details={"exception_type": type(exc).__name__},
            )

    logger.info("Ieta tools registered successfully")
