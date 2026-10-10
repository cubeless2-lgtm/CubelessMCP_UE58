"""
Python execution tools for Unreal MCP.

These tools intentionally keep the Unreal bridge generic. TA-specific workflows
can be implemented as Python scripts without adding a new C++ command each time.
"""

import logging
from typing import Any, Dict, Literal

from mcp.server.fastmcp import Context, FastMCP

from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")

InterchangePolicy = Literal["auto", "none", "legacy", "preserve", "interchange"]
INTERCHANGE_POLICIES = frozenset({"auto", "none", "legacy", "preserve", "interchange"})


def register_python_tools(mcp: FastMCP):
    """Register generic Unreal Python execution tools."""

    @mcp.tool()
    def execute_unreal_python(
        ctx: Context,
        code: str,
        mode: str = "ExecuteFile",
        defer_to_ticker: bool = False,
        allow_unsafe_editor_scripting_during_pie: bool = False,
        interchange_policy: InterchangePolicy = "auto",
    ) -> Dict[str, Any]:
        """
        Execute Python inside the running Unreal Editor.

        Source-code text is not used to decide import safety because aliases,
        getattr, helpers, and persistent Unreal Python globals can hide the
        eventual engine call. ``auto`` therefore conservatively behaves as
        ``legacy``: it runs standalone on the next editor tick, temporarily
        bypasses Interchange, and verifies restoration. This supports
        AssetImportTask/import_asset_tasks as well as
        AutomatedAssetImportData/import_assets_automated.

        Use ``preserve`` (or its ``interchange`` alias) for synchronous direct
        UInterchangeManager ``import_asset``, ``reimport_asset``, or
        ``import_scene`` calls and formats that require Interchange. A scripted
        async import must wait for completion and perform independent readback
        before the Python call returns; otherwise the bridge reports an unknown
        in-flight mutation and refuses a success result. Use ``none`` only when
        the script cannot import; it is a trusted caller declaration, not a
        sandbox, and avoids the import ticker/CVar scope. Declared import
        policies cannot run inside a synchronous batch/re-entrant command.

        Live Python asset Force Delete, Python-driven editor shutdown, map
        transitions, and unsafe PCG selector mutation remain rejected by focused
        accident guards. Generic Python execution is not a security sandbox.

        Args:
            code: Python code to execute in Unreal.
            mode: Python execution mode. ExecuteFile supports multiline TA scripts.
            defer_to_ticker: Run through Unreal ticker instead of directly on the game thread.
            allow_unsafe_editor_scripting_during_pie: Permit known unsafe editor actor cleanup calls during PIE/SIE.
            interchange_policy: Structured import execution policy. ``auto`` is the conservative legacy-compatible default; ``none`` declares no import; ``legacy`` selects legacy factories; ``preserve``/``interchange`` keep Interchange enabled for synchronous direct imports. Import-capable policies force standalone next-tick dispatch.

        Returns:
            Response from Unreal, including Python logs and command_result.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            normalized_interchange_policy = (
                interchange_policy.strip().lower()
                if isinstance(interchange_policy, str)
                else ""
            )
            if normalized_interchange_policy not in INTERCHANGE_POLICIES:
                return tool_error(
                    "interchange_policy must be one of: auto, none, legacy, preserve, interchange",
                    error_code="invalid_interchange_policy",
                    mutation_attempted=False,
                    mutation_performed=False,
                    allowed_interchange_policies=sorted(INTERCHANGE_POLICIES),
                )

            unreal = get_unreal_connection()
            if not unreal:
                return tool_error(
                    "Failed to connect to Unreal Engine",
                    error_code="unreal_connection_failed",
                )

            response = unreal.send_command(
                "execute_python",
                {
                    "code": code,
                    "mode": mode,
                    "defer_to_ticker": defer_to_ticker,
                    "interchange_policy": normalized_interchange_policy,
                    "allow_unsafe_editor_scripting_during_pie": allow_unsafe_editor_scripting_during_pie,
                },
            )
            return response or tool_error(
                "No response from Unreal Engine",
                error_code="unreal_no_response",
            )
        except Exception as exc:
            logger.exception("Error executing Unreal Python")
            return tool_error(
                str(exc),
                error_code="execute_unreal_python_failed",
                details={"exception_type": type(exc).__name__},
            )

    logger.info("Python tools registered successfully")
