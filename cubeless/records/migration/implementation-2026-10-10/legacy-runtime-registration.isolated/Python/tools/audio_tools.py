"""Focused UE 5.6 audio and MetaSound authoring tools."""

from __future__ import annotations

import logging
from typing import Any

from mcp.server.fastmcp import FastMCP
from tools.dependency_guard import reject_mcp_dependency_reference
from tools.response_schema import tool_pre_dispatch_error

logger = logging.getLogger("UnrealMCP")


def register_audio_tools(mcp: FastMCP) -> None:
    """Register the deliberately small UE 5.6 audio toolset."""

    def send_audio_command(command_name: str, params: dict[str, Any]) -> dict[str, Any]:
        from unreal_mcp_server import get_unreal_connection

        unreal = get_unreal_connection()
        if unreal is None:
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

    def audio_failure(
        message: str,
        exc: Exception,
        error_code: str = "audio_tool_failed",
    ) -> dict[str, Any]:
        if error_code == "audio_tool_failed":
            logger.exception(message)
        return tool_pre_dispatch_error(
            message,
            error_code=error_code,
            details={"exception_type": type(exc).__name__},
        )

    @mcp.tool()
    def list_audio_assets(
        directory: str = "/Game",
        recursive: bool = True,
        max_results: int = 200,
        offset: int = 0,
    ) -> dict[str, Any]:
        """List SoundWave, SoundCue, and MetaSoundSource assets with bounded pagination."""
        try:
            reject_mcp_dependency_reference("directory", directory)
            return send_audio_command(
                "audio_list_assets",
                {
                    "directory": directory,
                    "recursive": bool(recursive),
                    "max_results": int(max_results),
                    "offset": int(offset),
                },
            )
        except ValueError as exc:
            return audio_failure(
                f"Error listing audio assets: {exc}", exc, "audio_validation_failed"
            )
        except Exception as exc:  # noqa: BLE001 - bridge transports raise backend-specific errors
            return audio_failure(f"Error listing audio assets: {exc}", exc)

    @mcp.tool()
    def inspect_audio_asset(asset_path: str) -> dict[str, Any]:
        """Inspect one SoundWave, SoundCue, or MetaSoundSource without changing it."""
        try:
            reject_mcp_dependency_reference("asset_path", asset_path)
            return send_audio_command("audio_inspect_asset", {"asset_path": asset_path})
        except ValueError as exc:
            return audio_failure(
                f"Error inspecting audio asset: {exc}", exc, "audio_validation_failed"
            )
        except Exception as exc:  # noqa: BLE001 - bridge transports raise backend-specific errors
            return audio_failure(f"Error inspecting audio asset: {exc}", exc)

    @mcp.tool()
    def create_sound_cue_from_wave(
        name: str,
        sound_wave_path: str,
        package_path: str = "/Game/Audio/SoundCues",
    ) -> dict[str, Any]:
        """Create a SoundCue, or no-op only when its saved Wave-player graph matches exactly."""
        try:
            reject_mcp_dependency_reference("name", name)
            reject_mcp_dependency_reference("sound_wave_path", sound_wave_path)
            reject_mcp_dependency_reference("package_path", package_path)
            return send_audio_command(
                "audio_create_sound_cue_from_wave",
                {
                    "name": name,
                    "sound_wave_path": sound_wave_path,
                    "package_path": package_path,
                },
            )
        except ValueError as exc:
            return audio_failure(
                f"Error creating SoundCue: {exc}", exc, "audio_validation_failed"
            )
        except Exception as exc:  # noqa: BLE001 - bridge transports raise backend-specific errors
            return audio_failure(f"Error creating SoundCue: {exc}", exc)

    @mcp.tool()
    def create_basic_metasound_source(
        name: str,
        package_path: str = "/Game/Audio/MetaSounds",
        oscillator: str = "sine",
        frequency: float = 440.0,
        output_format: str = "mono",
    ) -> dict[str, Any]:
        """
        Create a basic continuous source through the UE 5.6 MetaSound editor builder API.

        An existing asset is never overwritten. It is a no-op only when saved graph
        readback proves the requested generator, frequency, format, and outputs match.

        Args:
            name: New asset name
            package_path: Destination package directory under /Game
            oscillator: sine, saw, square, triangle, or noise
            frequency: Oscillator frequency in Hz; ignored by noise
            output_format: mono or stereo
        """
        try:
            reject_mcp_dependency_reference("name", name)
            reject_mcp_dependency_reference("package_path", package_path)
            return send_audio_command(
                "audio_create_basic_metasound_source",
                {
                    "name": name,
                    "package_path": package_path,
                    "oscillator": oscillator,
                    "frequency": float(frequency),
                    "output_format": output_format,
                },
            )
        except ValueError as exc:
            return audio_failure(
                f"Error creating MetaSound source: {exc}",
                exc,
                "audio_validation_failed",
            )
        except Exception as exc:  # noqa: BLE001 - bridge transports raise backend-specific errors
            return audio_failure(f"Error creating MetaSound source: {exc}", exc)
