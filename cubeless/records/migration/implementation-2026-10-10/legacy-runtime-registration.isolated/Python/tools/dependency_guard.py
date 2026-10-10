"""Guards against persisting MCP-only references into authored Unreal assets."""

from __future__ import annotations

from typing import Any

_FORBIDDEN_MCP_REFERENCE_TOKENS = (
    "/script/unrealmcp",
    "/unrealmcp",
    "unrealmcp",
    "mcpunreal",
    "mcp_unreal",
    "/script/cubelesscontrolrigtools",
    "cubelesscontrolrigtools",
    "cubelesscontrolrigtoolseditor",
)


def is_mcp_dependency_reference(value: Any) -> bool:
    if not isinstance(value, str):
        return False

    candidate = value.strip().strip("\"'").replace("\\", "/").lower()
    if not candidate:
        return False

    return any(token in candidate for token in _FORBIDDEN_MCP_REFERENCE_TOKENS)


def mcp_dependency_reference_error(field_name: str, value: Any) -> str:
    return (
        f"Refusing to persist MCP-only reference in '{field_name}': {value}. "
        "UnrealMCP is an editor authoring bridge; finished assets must not depend on it."
    )


def reject_mcp_dependency_reference(field_name: str, value: Any) -> None:
    if is_mcp_dependency_reference(value):
        error = ValueError(mcp_dependency_reference_error(field_name, value))
        # Keep ValueError compatibility while letting guarded tool boundaries
        # return stable metadata without echoing the rejected reference value.
        error.field_name = field_name
        error.reason = "mcp_only_reference"
        raise error


def reject_mcp_dependency_references(field_name: str, value: Any) -> None:
    if isinstance(value, str):
        reject_mcp_dependency_reference(field_name, value)
        return

    if isinstance(value, dict):
        for key, child in value.items():
            reject_mcp_dependency_references(f"{field_name}.{key}", child)
        return

    if isinstance(value, (list, tuple, set)):
        for index, child in enumerate(value):
            reject_mcp_dependency_references(f"{field_name}[{index}]", child)
