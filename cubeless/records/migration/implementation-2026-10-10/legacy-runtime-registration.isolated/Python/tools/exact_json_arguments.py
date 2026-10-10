"""Targeted exact-JSON argument handling for safety boundary fields."""

from __future__ import annotations

from collections.abc import Iterable
from typing import Any

from mcp.server.fastmcp import FastMCP
from mcp.server.fastmcp.utilities.func_metadata import FuncMetadata


class _ExactJsonFuncMetadata(FuncMetadata):
    """Keep selected raw scalar values intact for strict Pydantic validation."""

    exact_json_fields: frozenset[str]

    def pre_parse_json(self, data: dict[str, Any]) -> dict[str, Any]:
        parsed = super().pre_parse_json(data)
        for field_name in self.exact_json_fields:
            if field_name in data:
                parsed[field_name] = data[field_name]
        return parsed


def enforce_exact_json_fields(
    mcp: FastMCP,
    tool_name: str,
    field_names: Iterable[str],
) -> None:
    """Disable FastMCP scalar-string coercion for selected tool arguments."""

    tool = mcp._tool_manager.get_tool(tool_name)
    if tool is None:
        raise RuntimeError(f"Cannot apply exact JSON policy to missing tool: {tool_name}")
    tool.fn_metadata = _ExactJsonFuncMetadata(
        arg_model=tool.fn_metadata.arg_model,
        exact_json_fields=frozenset(field_names),
    )
