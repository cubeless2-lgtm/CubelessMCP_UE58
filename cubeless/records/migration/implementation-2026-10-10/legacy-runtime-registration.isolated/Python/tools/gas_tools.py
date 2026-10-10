"""UE 5.6.1 Gameplay Ability System authoring and inspection tools."""

from __future__ import annotations

import logging
from typing import Any, Dict

from mcp.server.fastmcp import FastMCP

from tools.dependency_guard import reject_mcp_dependency_reference
from tools.response_schema import tool_error, tool_pre_dispatch_error


logger = logging.getLogger("UnrealMCP")
_MAX_UNREAL_NAME_UNITS = 1023


def _utf16_units(value: str) -> int:
    return len(value.encode("utf-16-le", errors="surrogatepass")) // 2


def _validated_identifier(field_name: str, value: str) -> str:
    if not isinstance(value, str):
        raise ValueError(f"{field_name} must be a string")
    normalized = value.strip()
    if not normalized:
        raise ValueError(f"{field_name} must not be empty")
    if _utf16_units(normalized) > _MAX_UNREAL_NAME_UNITS:
        raise ValueError(
            f"{field_name} exceeds the Unreal name limit of {_MAX_UNREAL_NAME_UNITS} UTF-16 code units"
        )
    if normalized.casefold() == "none":
        raise ValueError(f"{field_name} must not resolve to NAME_None")
    if not (normalized[0] == "_" or normalized[0].isalpha()):
        raise ValueError(f"{field_name} must begin with a letter or underscore")
    if not all(character == "_" or character.isalnum() for character in normalized):
        raise ValueError(f"{field_name} may contain only letters, numbers, and underscores")
    reject_mcp_dependency_reference(field_name, normalized)
    return normalized


def _validated_package_path(value: str) -> str:
    if not isinstance(value, str):
        raise ValueError("package_path must be a string")
    normalized = value.strip().replace("\\", "/").rstrip("/")
    if normalized != "/Game" and not normalized.startswith("/Game/"):
        raise ValueError("package_path must be /Game or a child of /Game")
    if "." in normalized or "//" in normalized:
        raise ValueError("package_path must be a package directory, not an object path")
    if _utf16_units(normalized) > _MAX_UNREAL_NAME_UNITS:
        raise ValueError("package_path exceeds the Unreal name limit")
    reject_mcp_dependency_reference("package_path", normalized)
    return normalized


def _validated_blueprint_path(field_name: str, value: str) -> str:
    if not isinstance(value, str):
        raise ValueError(f"{field_name} must be a string")
    normalized = value.strip().strip("\"'").replace("\\", "/")
    if not normalized.startswith("/Game/") or "//" in normalized:
        raise ValueError(f"{field_name} must identify a Blueprint under /Game")
    if _utf16_units(normalized) > _MAX_UNREAL_NAME_UNITS:
        raise ValueError(f"{field_name} exceeds the Unreal name limit")
    reject_mcp_dependency_reference(field_name, normalized)
    return normalized


def _validated_bool(field_name: str, value: bool) -> bool:
    if not isinstance(value, bool):
        raise ValueError(f"{field_name} must be a boolean")
    return value


def _validation_error(exc: ValueError) -> Dict[str, Any]:
    if getattr(exc, "reason", "") == "mcp_only_reference":
        field_name = str(getattr(exc, "field_name", "value"))
        message = f"Rejected MCP-only reference in {field_name}"
        error_code = "mcp_dependency_reference_rejected"
    else:
        message = str(exc)
        error_code = "gas_invalid_request"
    return tool_error(
        message,
        error_code=error_code,
        mutation_attempted=False,
        mutation_performed=False,
        mutation_state_unknown=False,
        changed=False,
    )


def register_gas_tools(mcp: FastMCP) -> None:
    """Register the minimal UE 5.6.1 GAS tool surface."""

    def send_gas_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        from unreal_mcp_server import get_unreal_connection

        unreal = get_unreal_connection()
        if not unreal:
            return tool_pre_dispatch_error(
                "Failed to connect to Unreal Engine",
                error_code="unreal_connection_failed",
            )
        response = unreal.send_command(command_name, params)
        if response is None:
            return tool_pre_dispatch_error(
                "No response from Unreal Engine",
                error_code="unreal_no_response",
            )
        return response

    def gas_exception_error(message: str, exc: Exception) -> Dict[str, Any]:
        logger.exception(message)
        return tool_pre_dispatch_error(
            message,
            error_code="gas_tool_failed",
            details={"exception_type": type(exc).__name__},
        )

    @mcp.tool()
    def inspect_gas_blueprint(blueprint_path: str) -> Dict[str, Any]:
        """Inspect GAS type, AbilitySystem components, and gameplay attributes on a Blueprint."""
        try:
            return send_gas_command(
                "gas_inspect_blueprint",
                {"blueprint_path": _validated_blueprint_path("blueprint_path", blueprint_path)},
            )
        except ValueError as exc:
            return _validation_error(exc)
        except Exception as exc:
            return gas_exception_error("Failed to inspect GAS Blueprint", exc)

    @mcp.tool()
    def create_gameplay_ability_blueprint(
        asset_name: str,
        package_path: str = "/Game/GAS/Abilities",
        save: bool = True,
    ) -> Dict[str, Any]:
        """Create an idempotent UE 5.6 GameplayAbility Blueprint in one editor transaction."""
        try:
            return send_gas_command(
                "gas_create_gameplay_ability_blueprint",
                {
                    "asset_name": _validated_identifier("asset_name", asset_name),
                    "package_path": _validated_package_path(package_path),
                    "save": _validated_bool("save", save),
                },
            )
        except ValueError as exc:
            return _validation_error(exc)
        except Exception as exc:
            return gas_exception_error("Failed to create GameplayAbility Blueprint", exc)

    @mcp.tool()
    def create_gameplay_effect_blueprint(
        asset_name: str,
        package_path: str = "/Game/GAS/Effects",
        save: bool = True,
    ) -> Dict[str, Any]:
        """Create an idempotent UE 5.6 GameplayEffect Blueprint in one editor transaction."""
        try:
            return send_gas_command(
                "gas_create_gameplay_effect_blueprint",
                {
                    "asset_name": _validated_identifier("asset_name", asset_name),
                    "package_path": _validated_package_path(package_path),
                    "save": _validated_bool("save", save),
                },
            )
        except ValueError as exc:
            return _validation_error(exc)
        except Exception as exc:
            return gas_exception_error("Failed to create GameplayEffect Blueprint", exc)

    @mcp.tool()
    def create_attribute_set_blueprint(
        asset_name: str,
        package_path: str = "/Game/GAS/Attributes",
        save: bool = True,
    ) -> Dict[str, Any]:
        """Create an idempotent UE 5.6 AttributeSet Blueprint in one editor transaction."""
        try:
            return send_gas_command(
                "gas_create_attribute_set_blueprint",
                {
                    "asset_name": _validated_identifier("asset_name", asset_name),
                    "package_path": _validated_package_path(package_path),
                    "save": _validated_bool("save", save),
                },
            )
        except ValueError as exc:
            return _validation_error(exc)
        except Exception as exc:
            return gas_exception_error("Failed to create AttributeSet Blueprint", exc)

    @mcp.tool()
    def add_ability_system_component_to_blueprint(
        blueprint_path: str,
        component_name: str = "AbilitySystem",
        save: bool = True,
    ) -> Dict[str, Any]:
        """Add one standard AbilitySystemComponent in one editor transaction."""
        try:
            return send_gas_command(
                "gas_add_ability_system_component_to_blueprint",
                {
                    "blueprint_path": _validated_blueprint_path("blueprint_path", blueprint_path),
                    "component_name": _validated_identifier("component_name", component_name),
                    "save": _validated_bool("save", save),
                },
            )
        except ValueError as exc:
            return _validation_error(exc)
        except Exception as exc:
            return gas_exception_error("Failed to add AbilitySystemComponent", exc)

    @mcp.tool()
    def add_gameplay_attribute_to_attribute_set(
        attribute_set_path: str,
        attribute_name: str,
        category: str = "GAS|Attributes",
        save: bool = True,
    ) -> Dict[str, Any]:
        """Add an FGameplayAttributeData member in one editor transaction."""
        try:
            normalized_category = category.strip() if isinstance(category, str) else ""
            if not normalized_category:
                raise ValueError("category must be a non-empty string")
            reject_mcp_dependency_reference("category", normalized_category)
            return send_gas_command(
                "gas_add_gameplay_attribute_to_attribute_set",
                {
                    "attribute_set_path": _validated_blueprint_path(
                        "attribute_set_path", attribute_set_path
                    ),
                    "attribute_name": _validated_identifier("attribute_name", attribute_name),
                    "category": normalized_category,
                    "save": _validated_bool("save", save),
                },
            )
        except ValueError as exc:
            return _validation_error(exc)
        except Exception as exc:
            return gas_exception_error("Failed to add gameplay attribute", exc)

    logger.info("UE 5.6 GAS tools registered successfully")
