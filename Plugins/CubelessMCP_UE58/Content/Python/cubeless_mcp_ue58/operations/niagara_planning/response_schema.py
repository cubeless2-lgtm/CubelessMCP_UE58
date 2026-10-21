"""Shared response helpers for Python MCP tool boundaries."""

from __future__ import annotations

from typing import Any


def tool_success(**fields: Any) -> dict[str, Any]:
    """Return a canonical Python tool success response."""
    response: dict[str, Any] = {"success": True}
    response.update(fields)
    return response


def tool_error(
    message: str,
    *,
    error_code: str = "error",
    details: dict[str, Any] | None = None,
    **fields: Any,
) -> dict[str, Any]:
    """Return a canonical Python tool error response with legacy error alias."""
    response: dict[str, Any] = {
        "success": False,
        "message": message,
        "error": message,
        "error_code": error_code,
    }
    if details is not None:
        response["details"] = details
    response.update(fields)
    return response


def tool_pre_dispatch_error(
    message: str,
    *,
    error_code: str = "error",
    details: dict[str, Any] | None = None,
    **fields: Any,
) -> dict[str, Any]:
    """Return an error known to have occurred before an Unreal mutation was sent."""

    fields.update(
        mutation_attempted=False,
        mutation_performed=False,
        mutation_state_unknown=False,
        changed=False,
    )
    return tool_error(message, error_code=error_code, details=details, **fields)
