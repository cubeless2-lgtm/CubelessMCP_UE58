"""Level Sequence (Sequencer) tools for Unreal MCP.

First-pass Sequencer authoring: create sequences, inspect bindings/tracks,
bind level actors, add transform keyframes, and add camera cuts. Frames
are in the sequence's display rate (set at creation).
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

from tools.dependency_guard import reject_mcp_dependency_reference
from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")


def register_sequencer_tools(mcp: FastMCP):
    """Register Sequencer tools with the MCP server."""

    def send_sequencer_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
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

    def sequencer_exception_error(message: str, exc: Exception) -> Dict[str, Any]:
        return tool_error(
            message,
            error_code="sequencer_tool_failed",
            details={"exception_type": type(exc).__name__},
        )

    @mcp.tool()
    def create_level_sequence(
        ctx: Context,
        sequence_path: str,
        frame_rate: float = 30.0,
        playback_start_frame: float = 0,
        playback_end_frame: float = 150,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Create a new Level Sequence asset with display rate and playback range.

        Args:
            sequence_path: Target asset path, e.g. /Game/Cinematics/LS_Shot01
            frame_rate: Integer display rate in fps; non-integral rates are rejected
            playback_start_frame: Playback range start (display frames)
            playback_end_frame: Playback range end (display frames)
            save: Save the new asset
        """
        try:
            reject_mcp_dependency_reference("sequence_path", sequence_path)
            params = {
                "sequence_path": sequence_path,
                "frame_rate": frame_rate,
                "playback_start_frame": playback_start_frame,
                "playback_end_frame": playback_end_frame,
                "save": save,
            }
            return send_sequencer_command("create_level_sequence", params)
        except Exception as e:
            error_msg = f"Error creating Level Sequence: {e}"
            logger.error(error_msg)
            return sequencer_exception_error(error_msg, e)

    @mcp.tool()
    def get_level_sequence_info(ctx: Context, sequence_path: str) -> Dict[str, Any]:
        """
        Read a Level Sequence's display rate, playback range, bindings, and tracks.

        Args:
            sequence_path: Level Sequence name/path
        """
        try:
            return send_sequencer_command("get_level_sequence_info", {"sequence_path": sequence_path})
        except Exception as e:
            error_msg = f"Error reading Level Sequence info: {e}"
            logger.error(error_msg)
            return sequencer_exception_error(error_msg, e)

    @mcp.tool()
    def add_level_sequence_actor_binding(
        ctx: Context,
        sequence_path: str,
        actor_label: str,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Bind a current-level actor into a Level Sequence as a possessable.

        Reuses an existing same-name binding instead of duplicating. Returns
        the binding_id used by transform-key and camera-cut tools.

        Args:
            sequence_path: Level Sequence name/path
            actor_label: Editor actor label (or object name) in the open level
            save: Save the sequence afterwards
        """
        try:
            params = {
                "sequence_path": sequence_path,
                "actor_label": actor_label,
                "save": save,
            }
            return send_sequencer_command("add_level_sequence_actor_binding", params)
        except Exception as e:
            error_msg = f"Error adding sequence actor binding: {e}"
            logger.error(error_msg)
            return sequencer_exception_error(error_msg, e)

    @mcp.tool()
    def add_level_sequence_transform_keys(
        ctx: Context,
        sequence_path: str,
        keys: List[Dict[str, Any]],
        binding_id: str = "",
        actor_label: str = "",
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Add cubic transform keyframes to a bound actor's transform track.

        Each key: {"frame": <display frame>, "location": [x,y,z], "rotation": [roll,pitch,yaw], "scale": [x,y,z]}.
        location/rotation/scale are each optional per key. Creates the track/section on demand.

        Args:
            sequence_path: Level Sequence name/path
            keys: Keyframe list (see above)
            binding_id: Binding guid from add_level_sequence_actor_binding (preferred)
            actor_label: Alternative lookup by binding name
            save: Save the sequence afterwards
        """
        try:
            if not keys:
                return tool_error("keys must be a non-empty list", error_code="sequencer_invalid_keys")
            params: Dict[str, Any] = {
                "sequence_path": sequence_path,
                "keys": keys,
                "save": save,
            }
            if binding_id:
                params["binding_id"] = binding_id
            if actor_label:
                params["actor_label"] = actor_label
            return send_sequencer_command("add_level_sequence_transform_keys", params)
        except Exception as e:
            error_msg = f"Error adding sequence transform keys: {e}"
            logger.error(error_msg)
            return sequencer_exception_error(error_msg, e)

    @mcp.tool()
    def set_level_sequence_playback_range(
        ctx: Context,
        sequence_path: str,
        playback_start_frame: float,
        playback_end_frame: float,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Set a Level Sequence's playback range in display frames.

        Args:
            sequence_path: Level Sequence name/path
            playback_start_frame: New playback start (display frames)
            playback_end_frame: New playback end (display frames)
            save: Save the sequence afterwards
        """
        try:
            params = {
                "sequence_path": sequence_path,
                "playback_start_frame": playback_start_frame,
                "playback_end_frame": playback_end_frame,
                "save": save,
            }
            return send_sequencer_command("set_level_sequence_playback_range", params)
        except Exception as e:
            error_msg = f"Error setting sequence playback range: {e}"
            logger.error(error_msg)
            return sequencer_exception_error(error_msg, e)

    @mcp.tool()
    def remove_level_sequence_binding(
        ctx: Context,
        sequence_path: str,
        binding_id: str = "",
        actor_label: str = "",
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Remove a possessable binding (and its tracks) from a Level Sequence.

        Args:
            sequence_path: Level Sequence name/path
            binding_id: Binding guid (preferred)
            actor_label: Alternative lookup by binding name
            save: Save the sequence afterwards
        """
        try:
            params: Dict[str, Any] = {"sequence_path": sequence_path, "save": save}
            if binding_id:
                params["binding_id"] = binding_id
            if actor_label:
                params["actor_label"] = actor_label
            return send_sequencer_command("remove_level_sequence_binding", params)
        except Exception as e:
            error_msg = f"Error removing sequence binding: {e}"
            logger.error(error_msg)
            return sequencer_exception_error(error_msg, e)

    @mcp.tool()
    def add_level_sequence_float_property_keys(
        ctx: Context,
        sequence_path: str,
        property_path: str,
        keys: List[Dict[str, Any]],
        binding_id: str = "",
        actor_label: str = "",
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Add cubic keys to a float property track on a bound actor.

        Each key: {"frame": <display frame>, "value": <float>}. Reuses an
        existing track for the same property path or creates one on demand.

        Args:
            sequence_path: Level Sequence name/path
            property_path: Property path on the bound object, e.g. 'CurrentFocalLength' or 'CameraComponent.FieldOfView'
            keys: Keyframe list (see above)
            binding_id: Binding guid (preferred)
            actor_label: Alternative lookup by binding name
            save: Save the sequence afterwards
        """
        try:
            if not keys:
                return tool_error("keys must be a non-empty list", error_code="sequencer_invalid_keys")
            params: Dict[str, Any] = {
                "sequence_path": sequence_path,
                "property_path": property_path,
                "keys": keys,
                "save": save,
            }
            if binding_id:
                params["binding_id"] = binding_id
            if actor_label:
                params["actor_label"] = actor_label
            return send_sequencer_command("add_level_sequence_float_property_keys", params)
        except Exception as e:
            error_msg = f"Error adding float property keys: {e}"
            logger.error(error_msg)
            return sequencer_exception_error(error_msg, e)

    @mcp.tool()
    def add_level_sequence_subsequence(
        ctx: Context,
        sequence_path: str,
        subsequence_path: str,
        start_frame: float,
        duration_frames: float,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Add another Level Sequence as a subsequence section (shot structure).

        Args:
            sequence_path: Parent Level Sequence name/path
            subsequence_path: Child Level Sequence name/path
            start_frame: Section start in the parent's display frames
            duration_frames: Section duration in the parent's display frames
            save: Save the parent sequence afterwards
        """
        try:
            params = {
                "sequence_path": sequence_path,
                "subsequence_path": subsequence_path,
                "start_frame": start_frame,
                "duration_frames": duration_frames,
                "save": save,
            }
            return send_sequencer_command("add_level_sequence_subsequence", params)
        except Exception as e:
            error_msg = f"Error adding subsequence: {e}"
            logger.error(error_msg)
            return sequencer_exception_error(error_msg, e)

    @mcp.tool()
    def open_level_sequence_in_editor(ctx: Context, sequence_path: str) -> Dict[str, Any]:
        """
        Open a Level Sequence in the Sequencer editor UI for visual review.

        Args:
            sequence_path: Level Sequence name/path
        """
        try:
            return send_sequencer_command("open_level_sequence_in_editor", {"sequence_path": sequence_path})
        except Exception as e:
            error_msg = f"Error opening sequence in editor: {e}"
            logger.error(error_msg)
            return sequencer_exception_error(error_msg, e)

    @mcp.tool()
    def add_level_sequence_camera_cut(
        ctx: Context,
        sequence_path: str,
        start_frame: float,
        end_frame: float,
        binding_id: str = "",
        actor_label: str = "",
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Add a camera cut section bound to a camera actor binding.

        Bind the camera actor first with add_level_sequence_actor_binding.

        Args:
            sequence_path: Level Sequence name/path
            start_frame: Cut start (display frames)
            end_frame: Cut end (display frames)
            binding_id: Camera binding guid (preferred)
            actor_label: Alternative lookup by binding name
            save: Save the sequence afterwards
        """
        try:
            params: Dict[str, Any] = {
                "sequence_path": sequence_path,
                "start_frame": start_frame,
                "end_frame": end_frame,
                "save": save,
            }
            if binding_id:
                params["binding_id"] = binding_id
            if actor_label:
                params["actor_label"] = actor_label
            return send_sequencer_command("add_level_sequence_camera_cut", params)
        except Exception as e:
            error_msg = f"Error adding sequence camera cut: {e}"
            logger.error(error_msg)
            return sequencer_exception_error(error_msg, e)
