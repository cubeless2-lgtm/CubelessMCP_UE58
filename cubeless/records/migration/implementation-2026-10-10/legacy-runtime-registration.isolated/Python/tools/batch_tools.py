"""Batch command tools for Unreal MCP.

Sends multiple bridge commands in ONE socket round trip. The C++ bridge
executes them sequentially on the game thread inside a single editor
transaction, so a whole batch is one Ctrl+Z entry in the editor.
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

from tools.dependency_guard import reject_mcp_dependency_references
from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")

MAX_BATCH_COMMANDS = 100
# The bridge-side wall clock covers the WHOLE batch. Scale the Python-side
# socket timeout with the item count, but never below the single-command
# default and never above 10 minutes.
MIN_BATCH_TIMEOUT_SECONDS = 120
MAX_BATCH_TIMEOUT_SECONDS = 600
PER_COMMAND_TIMEOUT_BUDGET_SECONDS = 10


def _default_batch_timeout(command_count: int) -> int:
    scaled = MIN_BATCH_TIMEOUT_SECONDS + PER_COMMAND_TIMEOUT_BUDGET_SECONDS * command_count
    return max(MIN_BATCH_TIMEOUT_SECONDS, min(MAX_BATCH_TIMEOUT_SECONDS, scaled))


def register_batch_tools(mcp: FastMCP):
    """Register batch execution tools with the MCP server."""

    @mcp.tool()
    def execute_batch(
        ctx: Context,
        commands: List[Dict[str, Any]],
        stop_on_error: bool = True,
        timeout_seconds: int = 0,
    ) -> Dict[str, Any]:
        """
        Execute up to 100 bridge commands in one round trip and one editor transaction.

        Each entry is {"type": "<bridge_command>", "params": {...}}. Nested
        "batch" entries are rejected. The bridge runs entries sequentially on
        the game thread; with stop_on_error=True (default) execution halts at
        the first failing entry. The whole batch is a single editor undo entry.

        Note: the bridge-side command timeout (UNREAL_MCP_COMMAND_TIMEOUT_SECONDS,
        default 120s) bounds the WHOLE batch. For long batches raise that env
        var or split the work into smaller batches.

        Args:
            commands: List of {"type": str, "params": dict} entries (1..100)
            stop_on_error: Stop at the first failing entry (default True)
            timeout_seconds: Python-side socket timeout override; 0 = auto-scale
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            if not isinstance(commands, list) or not commands:
                return tool_error(
                    "commands must be a non-empty list of {type, params} objects",
                    error_code="batch_invalid_commands",
                )
            if len(commands) > MAX_BATCH_COMMANDS:
                return tool_error(
                    f"commands exceeds the {MAX_BATCH_COMMANDS}-entry batch limit ({len(commands)})",
                    error_code="batch_too_large",
                )

            for index, entry in enumerate(commands):
                if not isinstance(entry, dict) or not isinstance(entry.get("type"), str) or not entry.get("type"):
                    return tool_error(
                        f"commands[{index}] must be an object with a non-empty 'type' string",
                        error_code="batch_invalid_entry",
                        details={"index": index},
                    )
                if entry["type"] == "batch":
                    return tool_error(
                        f"commands[{index}]: nested batch commands are not allowed",
                        error_code="batch_nested",
                        details={"index": index},
                    )
                params = entry.get("params")
                if params is not None and not isinstance(params, dict):
                    return tool_error(
                        f"commands[{index}].params must be an object when present",
                        error_code="batch_invalid_entry_params",
                        details={"index": index},
                    )

            reject_mcp_dependency_references("commands", commands)

            unreal = get_unreal_connection()
            if not unreal:
                return tool_error(
                    "Failed to connect to Unreal Engine",
                    error_code="unreal_connection_failed",
                )

            effective_timeout = timeout_seconds if timeout_seconds > 0 else _default_batch_timeout(len(commands))
            response = unreal.send_command(
                "batch",
                {"commands": commands, "stop_on_error": stop_on_error},
                timeout_seconds=effective_timeout,
            )
            if not response:
                return tool_error(
                    "No response from Unreal Engine",
                    error_code="unreal_no_response",
                )
            return response
        except Exception as e:
            error_msg = f"Error executing batch: {e}"
            logger.error(error_msg)
            return tool_error(
                error_msg,
                error_code="batch_tool_failed",
                details={"exception_type": type(e).__name__},
            )
