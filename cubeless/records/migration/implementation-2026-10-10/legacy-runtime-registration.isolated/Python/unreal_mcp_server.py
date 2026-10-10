"""
Unreal Engine MCP Server

A simple MCP server for interacting with Unreal Engine.
"""

import logging
import os
import sys
import threading
import time
from contextlib import asynccontextmanager
from logging.handlers import RotatingFileHandler
from typing import AsyncIterator, Dict, Any, Optional
from mcp.server.fastmcp import FastMCP

from services.unreal_bridge_connection import UnrealBridgeConnection
from tools.management_tools import request_telemetry, runtime_guard


def _get_int_env(name: str, default: int) -> int:
    try:
        return int(os.environ.get(name, str(default)))
    except (TypeError, ValueError):
        return default


class LockedSafeRotatingFileHandler(RotatingFileHandler):
    """RotatingFileHandler that tolerates Windows log-file locks."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._rollover_retry_after = 0.0

    def shouldRollover(self, record):
        if self._rollover_retry_after and time.monotonic() < self._rollover_retry_after:
            return False
        return super().shouldRollover(record)

    def doRollover(self):
        try:
            super().doRollover()
            self._rollover_retry_after = 0.0
        except OSError:
            self._rollover_retry_after = time.monotonic() + 60.0


# Configure logging with more detailed format
_log_max_bytes = max(1024, _get_int_env("UNREAL_MCP_LOG_MAX_BYTES", 5 * 1024 * 1024))
_log_backup_count = max(1, _get_int_env("UNREAL_MCP_LOG_BACKUP_COUNT", 3))
logging.basicConfig(
    level=os.environ.get("UNREAL_MCP_LOG_LEVEL", "WARNING").upper(),
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        LockedSafeRotatingFileHandler(
            'unreal_mcp.log',
            maxBytes=_log_max_bytes,
            backupCount=_log_backup_count,
            encoding='utf-8',
        ),
        # logging.StreamHandler(sys.stdout) # Remove this handler to unexpected non-whitespace characters in JSON
    ]
)
logger = logging.getLogger("UnrealMCP")


def _env_int(name: str, default: int) -> int:
    value = os.environ.get(name)
    if value is None or value.strip() == "":
        return default
    try:
        return int(value)
    except ValueError:
        logger.warning("Invalid integer for %s: %s", name, value)
        return default


def _env_bool(name: str, default: bool = False) -> bool:
    value = os.environ.get(name)
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "on"}


# Configuration
UNREAL_HOST = os.environ.get("UNREAL_MCP_HOST", "127.0.0.1")
UNREAL_PORT = _env_int("UNREAL_MCP_5_6_1_PORT", 55561)
UNREAL_RESPONSE_TIMEOUT_SECONDS = _env_int("UNREAL_MCP_RESPONSE_TIMEOUT_SECONDS", 120)
UNREAL_MCP_HEARTBEAT_TIMEOUT_SECONDS = max(1, _env_int("UNREAL_MCP_HEARTBEAT_TIMEOUT_SECONDS", 5))
UNREAL_MCP_LOG_PAYLOAD_CHARS = _env_int("UNREAL_MCP_LOG_PAYLOAD_CHARS", 1200)
# Default-off because per-stage clocks and byte facts are diagnostic overhead.
UNREAL_MCP_TRANSPORT_TELEMETRY = _env_bool("UNREAL_MCP_TRANSPORT_TELEMETRY", False)


class UnrealConnection(UnrealBridgeConnection):
    """Backward-compatible server export configured from current module globals."""

    def __init__(
        self,
        host: Optional[str] = None,
        port: Optional[int] = None,
        response_timeout_seconds: Optional[int] = None,
    ) -> None:
        super().__init__(
            host=host or UNREAL_HOST,
            port=port if port is not None else UNREAL_PORT,
            response_timeout_seconds=response_timeout_seconds or UNREAL_RESPONSE_TIMEOUT_SECONDS,
            log_payload_chars=UNREAL_MCP_LOG_PAYLOAD_CHARS,
            guard=runtime_guard,
            telemetry=request_telemetry,
            transport_telemetry=UNREAL_MCP_TRANSPORT_TELEMETRY,
        )

# Global connection state
_unreal_connection: Optional[UnrealConnection] = None
_unreal_connection_lock = threading.RLock()
_unreal_connection_accepting = True

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Get the connection to Unreal Engine."""
    global _unreal_connection
    try:
        with _unreal_connection_lock:
            if not _unreal_connection_accepting:
                logger.debug("Unreal bridge connection request rejected during server shutdown")
                return None
            if _unreal_connection is None:
                candidate = UnrealConnection()
                if not candidate.connect():
                    logger.warning("Could not connect to Unreal Engine")
                    return None
                _unreal_connection = candidate
            elif not _unreal_connection.connected or _unreal_connection.socket is None:
                if not _unreal_connection.connect():
                    logger.warning("Could not reconnect to Unreal Engine")
                    return None

            # Keep the object stable even while reconnecting so concurrent
            # callers cannot escape its per-instance lifecycle lock through
            # singleton replacement.
            return _unreal_connection
    except Exception as e:
        logger.error(f"Error getting Unreal connection: {e}")
        return None


def build_unreal_health(ping_unreal: bool = False) -> Dict[str, Any]:
    """Return structured bridge health without mutating Unreal assets."""
    guard_decision = runtime_guard.check_bridge_target(UNREAL_HOST, UNREAL_PORT)
    bridge = {
        "host": UNREAL_HOST,
        "port": UNREAL_PORT,
        "guard": guard_decision,
        "status": "not_checked",
        "connected": False,
    }
    if ping_unreal:
        started = time.perf_counter()
        attempts = []
        response = None
        for attempt_index in range(2):
            connection = UnrealConnection()
            response = connection.send_command(
                "ping",
                {},
                timeout_seconds=UNREAL_MCP_HEARTBEAT_TIMEOUT_SECONDS,
            )
            connection.disconnect()
            attempts.append(
                {
                    "attempt": attempt_index + 1,
                    "success": bool(response and response.get("status") != "error" and response.get("success") is not False),
                    "status": response.get("status") if response else "no_response",
                    "error": response.get("error") if response else "No response from Unreal Engine",
                }
            )
            if attempts[-1]["success"]:
                break
        elapsed_ms = round((time.perf_counter() - started) * 1000.0, 3)
        bridge["ping_elapsed_ms"] = elapsed_ms
        bridge["response"] = response
        bridge["attempts"] = attempts
        if response and response.get("status") != "error" and response.get("success") is not False:
            bridge["status"] = "connected"
            bridge["connected"] = True
        else:
            bridge["status"] = "not connected"
            bridge["connected"] = False
        bridge["success"] = bridge["connected"]
        return {"success": bridge["connected"], "unreal_bridge": bridge}
    bridge["success"] = True
    return {"success": True, "unreal_bridge": bridge}

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    """Handle server startup and shutdown."""
    global _unreal_connection, _unreal_connection_accepting
    logger.info("UnrealMCP server starting up")
    with _unreal_connection_lock:
        _unreal_connection_accepting = True
    try:
        connection = get_unreal_connection()
        if connection:
            logger.info("Connected to Unreal Engine on startup")
        else:
            logger.warning("Could not connect to Unreal Engine on startup")
    except Exception as e:
        logger.error(f"Error connecting to Unreal Engine on startup: {e}")
        with _unreal_connection_lock:
            _unreal_connection = None
    
    try:
        yield {}
    finally:
        with _unreal_connection_lock:
            # Close the global admission gate before detaching the singleton.
            # Otherwise a concurrent getter can publish a brand-new connection
            # between detach and the terminal close of the old instance.
            _unreal_connection_accepting = False
            connection = _unreal_connection
            _unreal_connection = None
        if connection:
            # Lifespan shutdown is terminal for this shared client.  ``close``
            # interrupts an active receive without waiting behind its request
            # lock, and prevents queued callers from reconnecting mid-shutdown.
            connection.close()
        logger.info("Unreal MCP server shut down")

# Initialize server
mcp = FastMCP(
    "UnrealMCP",
    description="Unreal Engine integration via Model Context Protocol",
    lifespan=server_lifespan
)

# Import and register tools
from tools.editor_tools import register_editor_tools
from tools.blueprint_tools import register_blueprint_tools
from tools.node_tools import register_blueprint_node_tools
from tools.project_tools import register_project_tools
from tools.umg_tools import register_umg_tools
from tools.python_tools import register_python_tools
from tools.batch_tools import register_batch_tools
from tools.data_tools import register_data_tools
from tools.sequencer_tools import register_sequencer_tools
from tools.skeletal_tools import register_skeletal_tools
from tools.landscape_tools import register_landscape_tools
from tools.pcg_tools import register_pcg_tools
from tools.controlrig_authoring_tools import register_controlrig_authoring_tools
from tools.material_tools import register_material_tools
from tools.niagara_tools import register_niagara_tools
from tools.niagara_planning_tools import register_niagara_planning_tools
from tools.texture_generation import register_texture_generation_tools
from tools.ai_tools import register_ai_tools
from tools.networking_tools import register_networking_tools
from tools.gas_tools import register_gas_tools
from tools.audio_tools import register_audio_tools
from tools.state_tree_tools import register_state_tree_tools
from tools.chaos_tools import register_chaos_tools
from tools.ieta_tools import register_ieta_tools
from tools.management_tools import (
    register_management_tools,
    set_health_provider,
    tool_registry,
)
from tools.tool_discovery import configure_tool_exposure

set_health_provider(build_unreal_health)

def register_tool_group(category, register_func):
    """Register one tool group while recording runtime policy metadata."""
    with tool_registry.capture_tools(mcp, category=category):
        register_func(mcp)

# Register tools. Runtime policy defaults to all tools enabled.
register_tool_group("editor", register_editor_tools)
register_tool_group("blueprint", register_blueprint_tools)
register_tool_group("blueprint_node", register_blueprint_node_tools)
register_tool_group("project", register_project_tools)
register_tool_group("umg", register_umg_tools)
register_tool_group("python", register_python_tools)
register_tool_group("batch", register_batch_tools)
register_tool_group("data", register_data_tools)
register_tool_group("sequencer", register_sequencer_tools)
register_tool_group("skeletal", register_skeletal_tools)
register_tool_group("landscape", register_landscape_tools)
register_tool_group("pcg", register_pcg_tools)
register_tool_group("controlrig_authoring", register_controlrig_authoring_tools)
register_tool_group("material", register_material_tools)
register_tool_group("niagara", register_niagara_tools)
register_tool_group("niagara_planning", register_niagara_planning_tools)
register_tool_group("texture_generation", register_texture_generation_tools)
register_tool_group("ai", register_ai_tools)
register_tool_group("networking", register_networking_tools)
register_tool_group("gas", register_gas_tools)
register_tool_group("audio", register_audio_tools)
register_tool_group("state_tree", register_state_tree_tools)
register_tool_group("chaos", register_chaos_tools)
register_tool_group("ieta", register_ieta_tools)
register_management_tools(mcp, tool_registry)

# Keep the complete FastMCP-validated catalog while exposing either the legacy
# eager surface or the compact three-tool search/dispatch surface.
tool_exposure = configure_tool_exposure(mcp, tool_registry)
TOOL_EXPOSURE_MODE = tool_exposure.mode


def get_catalog_tools():
    """Return the complete domain catalog for repository audits and tests."""

    return tool_exposure.catalog_tools()


@mcp.prompt()
def info():
    """Information about available Unreal MCP tools and best practices."""
    return """
    # Unreal MCP Server Tools and Best Practices

    The default search exposure advertises `list_toolsets`, `describe_toolset`,
    and `call_tool`. Discover a domain and its exact schema before dispatching;
    high-risk calls require `confirm=true`. Legacy clients can start the server
    with `UNREAL_MCP_TOOL_EXPOSURE=eager` to advertise every tool directly.
    
    ## UMG (Widget Blueprint) Tools
    - `create_umg_widget_blueprint(widget_name, parent_class="UserWidget", path="/Game/UI")` 
      Create a new UMG Widget Blueprint
    - `add_text_block_to_widget(widget_name, text_block_name, text="", position=[0,0], size=[200,50], font_size=12, color=[1,1,1,1])`
      Add a Text Block widget with customizable properties
    - `add_button_to_widget(widget_name, button_name, text="", position=[0,0], size=[200,50], font_size=12, color=[1,1,1,1], background_color=[0.1,0.1,0.1,1])`
      Add a Button widget with text and styling
    - `bind_widget_event(widget_name, widget_component_name, event_name, function_name="")`
      Bind events like OnClicked to functions
    - `add_widget_to_viewport(widget_name, z_order=0)`
      Add widget instance to game viewport
    - `set_text_block_binding(widget_name, text_block_name, binding_property, binding_type="Text")`
      Set up dynamic property binding for text blocks

    ## Editor Tools
    ### Viewport and Screenshots
    - `focus_viewport(target, location, distance, orientation)` - Focus viewport
    - `take_screenshot(filepath)` - Capture the active editor viewport to a PNG through the legacy bridge command
    - `set_active_viewport_realtime(realtime=True, realtime_frame_count=300)` - Enable a temporary realtime override on the active editor viewport for Niagara/animated visual review
    - `capture_viewport_bookmark_screenshot(filepath, bookmark_index=-1, clean_game_view=False, view_location=None, view_rotation=None, view_fov=None)` - Capture the active editor viewport with bookmark-aware framing, optional clean game-view readback, and temporary camera override
    - `open_niagara_preview_player(system_path="")` - Open the level-independent Niagara Preview Player Slate drop surface and optionally load a Niagara System
    - `get_niagara_preview_player_state()` - Inspect Niagara Preview Player window and latest dropped asset/actor state
    - `get_niagara_preview_lab_state()` - Inspect Niagara Preview Lab map, dirty, and preview actor safety state
    - `cleanup_niagara_preview_lab()` - Delete Niagara Preview Lab preview actors without saving or reloading the map
    - `capture_niagara_preview_lab_view(filepath, view=1, auto_frame=True, camera_location=None, camera_rotation=None, camera_fov=None, allow_current_map_preview=False)` - Capture a clean Niagara Preview Lab PNG; auto-frames preview actors by default, supports manual weather/volume camera overrides, and can explicitly use the current editor map
    - `preview_niagara_system_in_preview_lab(system_path, filepath, view=1, label="", warmup_time=0.35, cleanup_after=True, auto_frame=True, camera_location=None, camera_rotation=None, camera_fov=None, user_parameter_overrides=None, allow_current_map_preview=False)` - One-call optimized Niagara preview: spawn, apply transient component-only User overrides, warm up, auto-frame or manual-camera capture, and optional cleanup; can explicitly use the current editor map when the dedicated Preview Lab map is unavailable
    - `sample_niagara_system_in_preview_lab(system_path, output_dir="", label="", warmup_times=[...], views=[...], auto_frame=True, camera_location=None, camera_rotation=None, camera_fov=None, user_parameter_overrides=None, allow_current_map_preview=False)` - Multi-candidate optimized Niagara preview sampling in one MCP round trip; can explicitly use the current editor map
    - `inspect_blueprint_niagara_component_template(blueprint_path, component_name="", expected_system_path="", require_exactly_one=False, require_expected_system_match=False)` - Read-only Blueprint SCS Niagara component-template inspection before any durable BP template mutation
    - `dry_run_blueprint_niagara_component_template_asset_swap(blueprint_path, component_name, source_system_path, replacement_system_path, confirmation_token, dry_run=True)` - Guarded no-write BP SCS Niagara component-template asset-swap preflight; validates the reviewed target and reports the planned replacement without SetAsset/compile/save
    - `execute_blueprint_niagara_component_template_asset_swap_canary(source_blueprint_path, target_blueprint_path, component_name, source_system_path, replacement_system_path, confirmation_token, dry_run=True)` - Temp-only BP SCS Niagara component-template asset-swap canary; duplicates to `/Game/_MCP_Temp`, mutates only the temp target when `dry_run=False`, and never writes the original Blueprint
    - `execute_blueprint_niagara_component_template_asset_swap_durable(blueprint_path, component_name, source_system_path, replacement_system_path, confirmation_token, dry_run=True)` - Guarded durable original Blueprint component-template asset-swap executor; defaults dry-run, requires `ALLOW_DURABLE_BP_COMPONENT_TEMPLATE_EXECUTE`, compile/save flags, and post-save marker scan for real execution
    - `inspect_niagara_component_runtime_state(actor_name="", actor_label="", component_name="", system_path="", parameter_name_contains="", include_instance_parameters=False, ...)` - Read-only live Niagara component runtime state, override parameter store, optional instance parameter store, and emitter counters
    - `inspect_niagara_component_parameter_store(actor_name="", actor_label="", component_name="", system_path="", parameter_name_contains="", max_components=8, max_parameters=64)` - Narrow runtime propagation probe comparing component OverrideParameters and SystemInstance InstanceParameters
    - `inspect_niagara_component_grid2d_runtime(actor_name="", actor_label="", component_name="", system_path="", max_components=16)` - Read-only live Niagara Grid2D runtime NumCells inspector
    - `sample_render_target2d_region_stats(render_target_path, x0=0, x1=-1, y0=0, y1=-1, channel="max", threshold=0.01)` - Read-only C++ render-target region stats for smoke-test readback
    - `preview_niagara_component_asset_swap(replacement_system_path, actor_name="", actor_label="", component_name="", system_path="", restore_original_asset=True, preserve_override_parameters=True, activate_after_swap=True, advance_time=0.35)` - BP-manager parity check: temporarily assign a replacement Niagara System to exactly one live component, read override/runtime state, and restore the original asset without saving
    - `clear_dirty_package_flags(package_paths=None, package_path="", clear_all_dirty=False, dry_run=True, allow_non_temp_path=False, required_root="/Game/_MCP_Temp")` - Clear dirty flags for already-loaded editor packages without loading, saving, reloading, or running during PIE/SIE; dry-run and temp-root scoped by default

    ## Niagara Authoring
    - `analyze_niagara_system(system_path)` - Aggregate read-only Niagara renderer, user parameter, stack, graph, module-input, and compile inspection
    - `inspect_niagara_factory_bootstrap_defaults(kind="all")` - Inspect UE Niagara factory default transient system/emitter bootstrap structures without saving content
    - `audit_niagara_promotion_candidate(system_path, proposed_target_path="", forbidden_dependency_prefixes=None, forbidden_source_roots=None)` - Read-only preflight for generated Niagara promotion; rejects MCP/plugin and temp dependencies by default
    - `promote_niagara_system_candidate(source_system_path, target_system_path, dry_run=True, forbidden_dependency_prefixes=None, forbidden_source_roots=None)` - Guarded generated-temp Niagara System promotion by duplication; dry-run by default and refuses overwrite
    - `plan_niagara_promotion_manifest(source_system_path="", source_system_paths=None, target_root="/Game/MCPWeather/Generated", target_system_paths=None, write_manifest=False)` - Read-only Python promotion planner that dry-runs guarded Niagara promotion candidates and optionally writes a Saved/MCP JSON manifest
    - `execute_niagara_promotion_manifest(manifest=None, manifest_path="", confirmation_token="", require_confirmation=True)` - Confirmed Python promotion executor that revalidates every manifest entry before calling guarded Niagara promotion with dry_run=False; can load saved manifests from Saved/MCP/NiagaraPromotionManifests
    - `create_empty_niagara_system(target_system_path, save=True, wait_for_completion=True)` - Create a saved empty Niagara System, generated temp path only by default
    - `create_empty_niagara_emitter(target_emitter_path, add_default_modules_and_renderers=False, save=True)` - Create a saved empty Niagara Emitter, generated temp path only by default
    - `create_basic_sprite_niagara_system(target_system_path, emitter_name="Emitter", material_path="", save=True, wait_for_completion=True)` - Create a saved one-emitter sprite Niagara System, generated temp path only by default
    - `create_sprite_niagara_preset_system(target_system_path, preset="basic", emitter_name="Emitter", material_path="", spawn_rate=None, velocity=None, sprite_size=None, lifetime=None, use_project_weather_materials=False, add_spawn_box_location=None, spawn_box_size=None, spawn_box_offset=None, add_weather_detail_modules=None, save=True, wait_for_completion=True)` - Create a saved sprite preset Niagara System (`basic`, `rain`, `snow`, `dust`, `sparks`); rain/snow set InitializeParticle size/lifetime, add BoxLocation, and add conservative force/noise/color/size detail modules by default; optionally apply project-owned `/Game/MCPWeather` rain/snow materials; generated temp path only by default
    - `create_niagara_visible_scratch_pad_snow_seed(target_system_path, preset="snow", material_path="", scratch_pad_name="MCP_VisibleSizeScratch", parameter_name="Particles.SpriteSize", user_parameter_name="User.MCP_VisibleScratchPad_SpriteSize", save=True, request_compile=True, wait_for_completion=True)` - Composite temp-only visible seed recipe: create a snow preset with stock motion/detail modules, then add a Scratch Pad User-driven sprite-size module
    - `cleanup_generated_niagara_assets_offline(package_paths=None, name_prefix="", older_than_minutes=None, dry_run=True, require_editor_closed=True, allow_all=False)` - Offline filesystem cleanup for generated Niagara temp assets after closing the editor; dry-run and editor-closed guards enabled by default
    - `inspect_niagara_renderers(system_path)` - Inspect enabled Niagara renderers and material slots
    - `set_niagara_renderer_material(system_path, material_path, emitter_index=None, emitter_name="", renderer_index=None, material_slot_index=0, allow_source_edit=False, save=True)` - Set renderer material, temp/generated assets only by default
    - `add_niagara_sprite_renderer(system_path, material_path, emitter_index=None, emitter_name="", alignment="Automatic", facing_mode="FaceCamera", sub_image_size=None, skip_if_existing=True, allow_source_edit=False, save=True, request_compile=True)` - Add a Sprite renderer to one emitter
    - `add_niagara_mesh_renderer(system_path, emitter_index=None, emitter_name="", meshes_binding_parameter="User.MeshList", mesh_index_binding="Particles.MeshIndex", mesh_paths=None, skip_if_existing=True, allow_source_edit=False, save=True, request_compile=True)` - Add a Mesh renderer, optionally bound to a User Mesh Array
    - `set_niagara_renderer_enabled(system_path, enabled, emitter_index=None, emitter_name="", renderer_index=None, allow_source_edit=False, save=True, request_compile=True)` - Enable or disable one renderer without disabling its emitter
    - `set_niagara_emitter_enabled(system_path, enabled, emitter_index=None, emitter_name="", allow_source_edit=False, save=True, request_compile=True)` - Enable or disable one emitter handle
    - `remove_niagara_emitter(system_path, emitter_index=None, emitter_name="", allow_source_edit=False, save=True, request_compile=True)` - Remove one emitter handle from an authored Niagara System
    - `remap_niagara_parameter_collection_references(system_path, new_collection_path, old_collection_path="", old_namespace="", allow_source_edit=False, save=True, request_compile=True, replace_object_references=False)` - Remap Niagara System graph/script NPC references to a replacement collection
    - `remap_niagara_script_parameter_collection_references(new_collection_path, old_collection_path="", old_namespace="", script_path="", script_paths=None, root_path="", allow_source_edit=False, save=True, request_compile=True, wait_for_completion=True)` - Remap standalone Niagara Script assets to a replacement collection
    - `recompile_niagara_scripts_in_root(root_path="", script_paths=None, allow_source_compile=False, force=True, save=True)` - Recompile Niagara Script assets, temp/generated roots only by default
    - `repair_niagara_parameter_collection(collection_path, allow_source_edit=False, save=True)` - Repair missing Niagara Parameter Collection default store entries
    - `inspect_niagara_parameter_collection_values(collection_path, parameter_names=None, namespace="", max_parameters=256)` - Read NPC default values and missing-store indicators
    - `set_niagara_parameter_collection_values(collection_path, values, namespace="", allow_source_edit=False, save=True)` - Set NPC default values, temp/generated collections only by default
    - `inspect_niagara_user_parameters(system_path)` - Inspect exposed User parameters and current values
    - `set_niagara_user_parameter(system_path, parameter_name, value, allow_source_edit=False, save=True)` - Set supported User parameter values
    - `upsert_niagara_user_parameters(system_path, parameters, continue_on_error=False, allow_source_edit=False, save=True, request_compile=True)` - Create or update exposed User parameters, including supported Array Data Interfaces
    - `remove_niagara_user_parameters(system_path, parameter_names=None, parameter_name="", dry_run=True, remove_graph_references=True, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=True, timeout_seconds=10.0, poll_interval_seconds=0.1)` - Remove exposed User parameters with dry-run, compile wait, and post-compile absence readback guards
    - `remove_niagara_unused_module_input_path(system_path, scratch_pad_name, module_name, module_node_guid, input_name, linked_user_parameter_name, custom_hlsl_signature_name, expected_hlsl_identifier_absent, custom_hlsl_input_name="", scratch_pad_script_path="", expected_hlsl_body_hash="", expected_custom_hlsl_node_guid="", expected_override_node_guid="", expected_input_type="", expected_user_source_node_guid="", expected_scratch_map_get_node_guids=None, dry_run=True, allow_source_edit=False, save=False, request_compile=True, wait_for_completion=True, timeout_seconds=10.0, poll_interval_seconds=0.1)` - Dry-run-first removal of one exact unused User -> Module -> Scratch Custom HLSL input path; apply requires the returned hash, type, and GUID contract
    - `remove_niagara_parameter_map_set_nodes(system_path, parameter_pin_names=None, parameter_pin_name="", dry_run=True, allow_multiple=False, allow_source_edit=False)` - Remove selected ParameterMapSet nodes only when map flow can be rewired safely
    - `remove_niagara_orphan_parameter_map_get_nodes(system_path, parameter_pin_names=None, parameter_pin_name="", dry_run=True, allow_multiple=False, only_if_unlinked=True, allow_source_edit=False)` - Remove selected ParameterMapGet nodes only when matched outputs are unlinked
    - `inspect_niagara_custom_hlsl_nodes(system_path, node_guid="", signature_name="", contains_text="", max_hlsl_chars=4000)` - Inspect Custom HLSL nodes and current HLSL strings
    - `replace_niagara_custom_hlsl(system_path, replacement_hlsl, node_guid="", signature_name="", contains_text="", scratch_pad_name="", source_path_contains="", expected_contains="", dry_run=True, allow_source_edit=False)` - Replace exactly one Custom HLSL node after selector and expected-text checks
    - `inspect_niagara_stack(system_path, include_pins=False, max_function_calls=200)` - Inspect system/emitter/scratch-pad function calls
    - `inspect_niagara_graph(system_path, include_pins=True, include_links=True)` - Inspect full Niagara graph nodes, pins, links, and scratch-pad graphs
    - `inspect_niagara_graph_actions(system_path, scratch_pad_name="", safe_only=True)` - Inspect UE 5.6 native Niagara graph actions and stable action ids for one Scratch Pad
    - `apply_niagara_scratch_pad_graph_recipe(system_path, custom_hlsl_node_guid, nodes, links, output_mappings, dry_run=True)` - Apply an allowlisted native-node recipe to a generated ghost Scratch Pad with non-force compile/ID guards and fail-closed dirty recovery
    - `inspect_niagara_scratch_pad_interface(system_path, include_graph_summary=True, include_parent_scratch_pads=True)` - Inspect Scratch Pad inputs/outputs/usages as a read-only authoring interface
    - `duplicate_or_attach_emitter_from_source(target_system_path, source_emitter_path="", source_system_path="", source_emitter_index=None, source_emitter_name="", new_emitter_name="", enabled=None, copy_source_user_parameters=False, allow_source_edit=False, save=True, request_compile=True)` - Add a source emitter into a generated temp Niagara System; can localize private Scratch Pads and copy serializable source User defaults
    - `create_niagara_system_from_source_template(target_system_path, source_system_path, source_emitter_indices=None, copy_source_user_parameters=True, save=True, request_compile=True, wait_for_completion=True)` - One-call source-template creation from a source Niagara System into a generated temp System; preferred for project-authored weather/FX seeds
    - `create_niagara_scratch_pad_vec2_user_parameter_module(target_system_path, scratch_pad_name="MCP_BlankUpdateScratch", parameter_name="Particles.SpriteSize", parameter_value=None, user_parameter_name="User.MCP_BlankScratchPad_SpriteSize", user_parameter_default_value=None, target_usage="ParticleUpdateScript", target_stack_emitter_index=0, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=True, timeout_seconds=180)` - Composite temp-only recipe that creates a blank Scratch Pad module, adds one Vector2D ParameterMapSet write, links it to a User parameter, inserts it into the stack, and waits for compile
    - `create_niagara_scratch_pad_bool_user_parameter_module(target_system_path, scratch_pad_name="MCP_BoolUserScratch", parameter_name="Particles.MCPBoolProbe", module_input_name="MCPBoolInput", user_parameter_name="User.MCP_ScratchPad_Bool", user_parameter_default_value=None, target_usage="ParticleUpdateScript", target_stack_emitter_index=0, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=True, timeout_seconds=180)` - Composite temp-only recipe that creates a blank Scratch Pad bool write, exposes a reachable Module bool input, binds that input to a User bool parameter, inserts it into the stack, and waits for compile
    - `create_niagara_scratch_pad_bool_visible_toggle_module(target_system_path, scratch_pad_name="MCP_BoolVisibleScratch", parameter_name="Particles.SpriteSize", base_module_input_name="MCPBaseSpriteSize", user_parameter_name="User.MCP_VisibleToggle", custom_hlsl_input_name="MCPVisibleToggle", save=True, request_compile=True, wait_for_completion=True, timeout_seconds=180)` - Composite temp-only recipe that writes SpriteSize, preserves a vec2 Module BaseValue, inserts Custom HLSL fed by a bool User parameter, and stacks the module so the bool toggles visible sprite size
    - `create_or_duplicate_scratch_pad_module(target_system_path, source_script_path="", source_system_path="", source_owner_kind="system", source_script_index=None, source_scratch_pad_name="", source_emitter_index=None, source_emitter_name="", target_owner_kind="system", target_emitter_index=None, target_emitter_name="", new_script_name="", target_supported_usage="ParticleUpdateScript", allow_source_edit=False, save=True, request_compile=True)` - Create a blank Scratch Pad module from the engine default module template or duplicate an existing Scratch Pad script into a generated temp Niagara System or emitter
    - `add_scratch_pad_module_to_stack(target_system_path, scratch_pad_script_path="", scratch_pad_owner_kind="system", scratch_pad_script_index=None, scratch_pad_name="", scratch_pad_emitter_index=None, scratch_pad_emitter_name="", target_usage="ParticleUpdateScript", target_usage_id="", target_emitter_index=None, target_emitter_name="", target_index=None, suggested_name="", allow_source_edit=False, save=True, request_compile=True, skip_if_duplicate=True)` - Insert a target-local Scratch Pad module into a Niagara stack
    - `add_niagara_scratch_pad_assignment_module_to_stack(target_system_path, parameter_name="Particles.SpriteSize", parameter_type="vec2", default_value="50.000000,50.000000", target_usage="ParticleUpdateScript", allow_assignment_module_route="")` - Token-gated temp-only assignment-module stack insertion for reviewed SpriteSize vec2, Color Linear Color, Lifetime float, and Velocity vec3 shapes; defaults refuse without `ALLOW_SCRATCH_PAD_ASSIGNMENT_MODULE_ROUTE`
    - `add_niagara_scratch_pad_parameter_write(system_path, parameter_name, value=None, parameter_type="vec2", default_value="", scratch_pad_script_path="", scratch_pad_owner_kind="system", scratch_pad_script_index=None, scratch_pad_name="", scratch_pad_emitter_index=None, scratch_pad_emitter_name="", skip_if_exists=True, allow_source_edit=False, save=True, request_compile=True)` - Add one ParameterMapSet write inside a target-local Scratch Pad module graph
    - `add_niagara_stock_module_to_stack(target_system_path, module_script_path, target_usage="ParticleSpawnScriptInterpolated", target_emitter_index=None, target_emitter_name="", target_index=None, suggested_name="", allow_source_edit=False, save=True, request_compile=True, skip_if_duplicate=True)` - Insert an existing Niagara Module script asset such as `/Niagara/Modules/Spawn/Location/BoxLocation.BoxLocation` into a generated/temp system stack
    - `set_niagara_scratch_pad_function_input_default(system_path, function_name="", input_pin_name="", value=None, default_value="", function_node_guid="", function_node_index=None, function_call_index=None, scratch_pad_script_path="", scratch_pad_owner_kind="system", scratch_pad_script_index=None, scratch_pad_name="", scratch_pad_emitter_index=None, scratch_pad_emitter_name="", break_links=True, allow_multi_link_break=False, allow_source_edit=False, save=True, request_compile=True)` - Set a Scratch Pad internal function input default and optionally break existing links
    - `link_niagara_scratch_pad_pin_to_user_parameter(system_path, target_pin_name, user_parameter_name, scratch_pad_name="", overwrite_existing=False, allow_source_edit=False, save=True, request_compile=True)` - Link a Scratch Pad ParameterMapSet input pin to a Vector2D User parameter
    - `wrap_niagara_scratch_pad_output_with_stack_context(system_path, target_pin_name="StackContext.RGBA", scratch_pad_name="RenderCircleToGrid", previous_stack_value_pin_name="PreviousStackValue", local_value_pin_name="LocalStampValue", expression="max(PreviousStackValue, LocalStampValue)", skip_if_already_wrapped=True, replace_existing_custom=True, allow_source_edit=False, save=True, request_compile=True)` - Wrap a Scratch Pad output pin with a Custom HLSL accumulator that combines incoming StackContext with the existing local value
    - `insert_niagara_scratch_pad_custom_hlsl_for_pin(system_path, target_pin_name, expression, scratch_pad_name="RenderGrid", custom_hlsl_script_usage="DynamicInput", inputs=None, user_parameter_inputs=None, replace_existing_custom=False, rebuild_existing_custom=False, delete_unlinked_custom_input_source_nodes=False, allow_source_edit=False, save=True, request_compile=True)` - Wrap a Scratch Pad input pin with Custom HLSL, including optional User.* ParameterMapGet inputs; generated/temp diagnostics may use Function script usage for statement-style probes
    - `rewire_niagara_custom_hlsl_input_to_module_parameter(system_path, input_name, scratch_pad_name="", custom_node_signature_name="", module_parameter_name="", delete_orphaned_source_node=True, allow_source_edit=False, save=True, request_compile=True)` - Rewire a Custom HLSL runtime DI input to a Module.* ParameterMapGet pin for the proven User parameter -> Module input -> HLSL input chain
    - `inspect_niagara_compile_status(system_path, request_compile=False, force=False, allow_source_compile=False, wait_for_completion=False)` - Inspect Niagara script compile status and outstanding requests
    - `inspect_niagara_simulation_stages(system_path, include_compile_data=True, include_script_compile_status=True, max_stages=128)` - Inspect protected Niagara SimulationStage settings and compilation data
    - `add_niagara_grid_simulation_stage(system_path, emitter_index=None, emitter_name="", stage_name="MCP_GridSimulationStage", data_interface_name="Emitter.Grid2D Collection", insert_index=None, skip_if_duplicate=True, enabled=True, iteration_source="DataInterface", execute_behavior="Always", num_iterations=1, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False)` - Create a generic Grid2D DataInterface SimulationStage and return its ParticleSimulationStage usage id
    - `remove_niagara_simulation_stage(system_path, emitter_index=None, emitter_name="", stage_index=None, stage_name="", allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False)` - Remove one SimulationStage and its associated stage graph nodes, temp/generated assets only by default
    - `set_niagara_simulation_stage_settings(system_path, emitter_index=None, emitter_name="", stage_index=None, stage_name="", enabled=None, iteration_source="", direct_dispatch_type="", direct_dispatch_element_type="", execute_behavior="", element_count=None, element_count_x=None, element_count_y=None, element_count_z=None, num_iterations=None, num_iterations_binding="", gpu_dispatch_force_linear=None, override_gpu_dispatch_num_threads=None, allow_source_edit=False, save=True, request_compile=True)` - Edit one generic SimulationStage, including an optional existing User.* int iteration binding; temp/generated assets only by default
    - `inspect_niagara_module_inputs(system_path, include_linked_sources=True, include_resolved_stack_inputs=False)` - Inspect module input candidates for generation planning
    - `inspect_niagara_data_interface_overrides(system_path, input_name="", emitter_index=None, emitter_name="", module_index=None, module_name="", module_node_guid="", max_modules=200, max_inputs_per_module=64, include_data_interface_properties=False, max_data_interface_properties=120, max_data_interface_property_value_length=512)` - Read Niagara Data Interface module input override graph nodes and User object bindings
    - `create_niagara_module_input_override(system_path, input_name, value, emitter_index=None, emitter_name="", module_index=None, module_name="", module_node_guid="", overwrite_existing=False, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False)` - Create a missing RapidIteration module input override, temp/generated assets only by default
    - `set_niagara_module_input_literal_override(system_path, input_name, value, emitter_index=None, emitter_name="", module_index=None, module_name="", module_node_guid="", overwrite_existing=False, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False)` - Set a scalar/vector/color/bool module input through a typed override-pin DefaultValue string, temp/generated assets only by default
    - `set_niagara_render_target2d_module_input(system_path, input_name, user_parameter_name="User.RT_IF_Deform", render_target_asset_path="", emitter_index=None, emitter_name="", module_index=None, module_name="", module_node_guid="", inherit_user_parameter_settings=True, overwrite_existing=False, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False)` - Bind a RenderTarget2D data-interface module input to a User render target parameter
    - `set_niagara_module_input_user_parameter(system_path, input_name, user_parameter_name, default_value=None, emitter_index=None, emitter_name="", module_index=None, module_name="", module_node_guid="", overwrite_existing=False, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False)` - Bind a scalar/vector/color/bool module input to a User parameter
    - `set_niagara_array_data_interface_module_input_user_parameter(system_path, input_name, user_parameter_name, emitter_index=None, emitter_name="", module_index=None, module_name="", module_node_guid="", overwrite_existing=False, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False)` - Bind a supported Array Data Interface module input to a User array parameter
    - `set_niagara_module_input_linked_parameter(system_path, input_name, linked_parameter_name, emitter_index=None, emitter_name="", module_index=None, module_name="", module_node_guid="", overwrite_existing=False, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False)` - Link a module input to an existing namespaced Niagara parameter such as `Emitter.Grid2D Collection`
    - `set_niagara_module_inputs_batch(system_path, edits, operation="set_existing", overwrite_existing=False, continue_on_error=False, allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False)` - Apply multiple RapidIteration module input edits, compile once, and save once
    - `set_niagara_module_input_value(system_path, input_name, value, emitter_index=None, emitter_name="", module_index=None, module_name="", module_node_guid="", allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False)` - Set an existing RapidIteration module input override
    - `inspect_niagara_rapid_iteration_parameters(system_path, contains=None, max_parameters_per_script=256)` - Read RapidIteration parameter values with script ownership
    - `set_niagara_rapid_iteration_parameter(system_path, parameter_name, value, owner_kind="", owner_name="", script_usage="", emitter_index=None, allow_source_edit=False, save=True, request_compile=True)` - Set one exact RapidIteration parameter; fails on ambiguous matches

    ### Actor Management
    - `get_actors_in_level()` - List all actors in current level
    - `find_actors_by_name(pattern)` - Find actors by name pattern
    - `spawn_actor(name, type, location=[0,0,0], rotation=[0,0,0], scale=[1,1,1])` - Create actors
    - `delete_actor(name)` - Remove actors
    - `set_actor_transform(name, location, rotation, scale)` - Modify actor transform
    - `get_actor_properties(name)` - Get actor properties
    
    ## Blueprint Management
    - `create_blueprint(name, parent_class)` - Create new Blueprint classes
    - `add_component_to_blueprint(blueprint_name, component_type, component_name, parent_component_name?)` - Add components
    - `list_blueprint_components(blueprint_name, component_name="")` - List Blueprint SCS components and defaults
    - `duplicate_blueprint_to_temp(blueprint_name, target_path, required_root="/Game/_MCP_Temp", dry_run=True, overwrite_existing=False)` - Duplicate a Blueprint to a temp path for canary/preflight edits
    - `set_blueprint_actor_tick_settings(blueprint_name, tick_hz=60.0, tick_interval=-1.0, dry_run=True, compile=True, save=True)` - Set Actor CDO tick defaults without adding runtime dependencies
    - `replace_blueprint_component_class(blueprint_name, component_name, new_component_class, sphere_radius=-1.0, dry_run=True, compile=True, save=True)` - Replace one SCS component template class while preserving native-only saved content
    - `get_component_property(blueprint_name, component_name, property_name)` - Read component template properties
    - `set_static_mesh_properties(blueprint_name, component_name, static_mesh)` - Configure meshes
    - `set_skeletal_mesh_component_anim_defaults(blueprint_name, component_name, skeletal_mesh="", anim_class="")` - Set SkeletalMeshComponent mesh and AnimBP defaults through native component APIs
    - `set_physics_properties(blueprint_name, component_name)` - Configure physics
    - `compile_blueprint(blueprint_name)` - Compile Blueprint changes
    - `compile_and_validate_blueprint(blueprint_name, save=False, refresh_nodes=True)` - Compile and return validation status
    - `set_blueprint_property(blueprint_name, property_name, property_value)` - Set properties
    - `set_pawn_properties(blueprint_name)` - Configure Pawn settings
    - `spawn_blueprint_actor(blueprint_name, actor_name)` - Spawn Blueprint actors
    
    ## Blueprint Node Management
    - `list_blueprint_graphs(blueprint_name, graph_type="")` - List Blueprint graphs with graph_id metadata
    - `resolve_blueprint_graph(blueprint_name, graph_name="", graph_id="", graph_type="", create_if_missing=False)` - Resolve or create a Blueprint graph
    - `inspect_blueprint_graph_call_topology(blueprint_name, graph_type="", reference_contains="")` - Read static Blueprint graph call/reference/link topology
    - `inspect_anim_graph_protected_topology(blueprint_name, node_type="", graph_name="AnimGraph")` - Read protected AnimGraph nodes, pins, and pose links without modifying assets
    - `inspect_anim_graph_node_settings(blueprint_name, node_type="", graph_name="AnimGraph")` - Read reflected FAnimNode settings for AnimGraph nodes
    - `inspect_anim_state_machine_transitions(blueprint_name, state_machine_name="")` - Read AnimBP state-machine transition source/target/rule graph topology
    - `inspect_animation_asset_metadata_guarded(asset_path, include_curve_keys=False)` - Read source animation asset notifies, curves, sync markers, Montage sections, slots, and segments without mutating assets
    - `inspect_physics_asset_constraints_guarded(physics_asset_path="", skeletal_mesh_path="")` - Read PhysicsAsset bodies, constraints, profiles, collision summaries, and RBAN solver settings without simulation or mutation
    - `create_skeletal_bone_constraint_fixture(skeletal_mesh_path, actor_name="MCP_SkeletalBoneConstraint_Ghost", bone_name1="hand_l", bone_name2="hand_r", world_context="editor", dry_run=True)` - Validate or spawn a temporary native PhysicsConstraintComponent between two SkeletalMesh PhysicsAsset bodies in the editor or an explicitly selected play world; never saves project content
    - `inspect_skeletal_bone_constraint_fixture(actor_name, world_context="auto", constraint_component_name="")` - Read back constrained components/bones, forces, break state, angular state, and per-bone simulation state in editor or PIE/SIE
    - `inspect_controlrig_rigvm_topology(control_rig_path="", control_rig_class_path="")` - Read ControlRig Blueprint default RigVM graph nodes, pins, links, event metadata, and dirty-state guards without mutation
    - `validate_controlrig_rigvm_links(control_rig_path="", links=[])` - Read-only validation of proposed ControlRig RigVM source/target pin links through Unreal CanLink without adding links, compiling, or saving
    - `validate_controlrig_rigvm_pin_defaults(control_rig_path="", pin_defaults=[])` - Read-only validation of ControlRig RigVM pin default capability and proposed default strings without setting values, resizing arrays, compiling, or saving
    - `add_controlrig_rigvm_links(control_rig_path="", links=[], dry_run=False)` - Sample/temp-only ControlRig RigVM link writer with full prevalidation, no cast-node creation, and optional compile/save of the target sample asset only
    - `set_controlrig_rigvm_pin_defaults(control_rig_path="", pin_defaults=[], dry_run=False)` - Sample/temp-only ControlRig RigVM pin-default writer with full prevalidation, no array resize or linked-pin propagation by default, and optional compile/save of the target sample asset only
    - `controlrig_direct_gate_probe(control_rig_path="", control_rig_class="", cases?)` - Run transient ControlRig property/curve gate probes and return hierarchy transform deltas without saving assets
    - `sample_controlrig_pre_post_runtime_pose(control_rig_path="", control_rig_class="", input_defaults?, curve_values?)` - Sample transient ControlRig hierarchy pre/post execute transforms and deltas
    - `sample_anim_node_pre_post_runtime_pose(blueprint_name="", node_id="", node_type="", dry_run=True)` - Resolve an AnimGraph node target or run compiled mapping, PoseWatch, active-component, or isolated-temp pose probes on main or Post Process AnimInstances, with mapping_support diagnostics for node attribution
    - `sample_skeletal_bones_in_sie(actor_label="", actor_name="", component_name="", bones?, sockets?, require_pie_world=False)` - Sample live PIE/SIE/play SkeletalMeshComponent bone/socket transforms without modifying assets
    - `sample_blendspace_runtime_pose_grid(skeletal_mesh, blendspace_path="", blendspaces?, samples?, require_pie_world=True)` - Evaluate BlendSpace inputs on a transient runtime SkeletalMeshActor and return sampled bone/socket pose deltas
    - `ensure_blendspace_sample_variant(source_blendspace, axis_edits?, sample_edits?, add_samples?, force_resample=False)` - Duplicate/reuse a sample BlendSpace variant, edit axes/sample coordinates safely, rebuild runtime sampling data, and save only the target asset unless deliberate in-place resample is requested
    - `ensure_anim_state_machine_sample_variant(source_anim_bp_path, states?, transitions?)` - Duplicate/reuse a sample AnimBP, add a state machine with states/transitions, bounded bool/float/enum variable rules, and BlendSpacePlayer input bindings through Unreal schemas, compile/save only the target sample, and read back topology
    - `ensure_anim_bp_event_graph_locomotion_driver(source_anim_bp_path, speed_variable="Speed", include_direction=False, include_is_in_air=False, include_acceleration=False, include_is_accelerating=False)` - Duplicate/reuse a sample AnimBP, author Event Blueprint Update Animation -> TryGetPawnOwner -> IsValid Branch -> GetVelocity -> VSizeXY -> Set Speed plus optional CalculateDirection/IsFalling/CharacterMovement acceleration setters, compile/save only the target sample, and read back EventGraph topology
    - `create_anim_blueprint_sample(target_anim_bp_path, skeleton_path="", skeletal_mesh_path="")` - Create a sample AnimBP under _MCP_Sample/_MCP_Temp from a Skeleton or SkeletalMesh with compile/readback evidence
    - `ensure_anim_graph_controlrig_node_sample(source_anim_bp_path, control_rig_asset_path="", control_rig_class_path="")` - Duplicate/reuse a sample AnimBP, add a LinkedInputPose -> ControlRig -> Result chain, compile/save only the target sample, and read back topology
    - `ensure_controlrig_sample_asset(target_control_rig_path, skeletal_mesh_path="", setup_mode="empty_with_preview", curves?, nulls?, controls?, unit_nodes?)` - Create/reuse a sample ControlRig Blueprint under _MCP_Sample/_MCP_Temp with optional preview mesh, bounded preview hierarchy import, direct hierarchy curves, local/global transform nulls, float/bool/euler transform controls, named direct parents, direct control shape metadata, allowlisted single-instance ControlRig event RigVM unit node evidence, compile/init, and readback evidence
    - `profile_controlrig_skeleton(request)` / `compile_controlrig_profile_recipe(profile, request)` / `validate_controlrig_solver_intents(request)` / `make_controlrig_preset_recipe(preset)` / `validate_controlrig_recipe(recipe)` / `dry_run_controlrig_recipe(recipe)` / `build_controlrig_recipe(recipe, options=None)` / `build_controlrig_recipe_with_animbp_binding(recipe, options=None)` - Read-only skeleton profile evidence, deterministic profile-to-recipe planning, UnrealMCP-native validation-only solver intent preflight, preset-to-recipe generation, recipe-transaction ControlRig authoring, and sample AnimBP binding orchestration through Python/reflection or native validation, without a static UnrealMCP C++ dependency
    - `inspect_anim_instance_runtime_state(actor_label="", actor_name="", component_name="", state_machine_name="", require_pie_world=False)` - Read live PIE/SIE/play AnimInstance state-machine, montage, and curve state
    - `set_anim_instance_runtime_property_for_probe(actor_label="", properties?, require_pie_world=False)` - Set live AnimInstance runtime properties for probing without saving assets
    - `sample_anim_state_machine_runtime_response(actor_label="", cases?, require_pie_world=False)` - Apply runtime property cases, tick narrowly, and sample state-machine response
    - `set_anim_graph_rigidbody_settings(blueprint_name, alpha?, external_force?, simulation_space?)` - Modify a sample RigidBody AnimGraph node
    - `ensure_anim_graph_input_pose_passthrough(blueprint_name, graph_name="AnimGraph", replace_existing=False)` - Create/reuse a Linked Input Pose node and connect it to the AnimGraph root Result pin
    - `ensure_postprocess_anim_demo_variant(source_blueprint_name, source_skeletal_mesh, variant_name, bone_name="head", rotation=[0,0,6])` - Duplicate/reuse a sample Post Process AnimBP and SkeletalMesh variant for safe late ModifyBone proof
    - `ensure_anim_graph_modify_bone_demo(blueprint_name, bone_name="head", rotation=[0,0,6], replace_existing=False)` - Create/reuse a safe Post Process AnimBP Modify Bone demo chain
    - `ensure_anim_graph_modify_curve_demo(blueprint_name, curve_values?)` - Create/reuse a sample Modify Curve demo chain, defaulting to StackOBot ControlRig gate curves
    - `set_anim_graph_controlrig_input_defaults(blueprint_name, input_defaults?, expose_input_pins?)` - Expose sample ControlRig AnimGraph input pins and optionally set defaults
    - `ensure_controlrig_forced_driver_animbp(blueprint_name, curve_values?, input_defaults?)` - Insert sample Modify Curve before ControlRig and force input defaults
    - `ensure_linked_anim_graph_sample_variant(source_anim_bp_path, linked_anim_bp_path)` - Duplicate/reuse a sample AnimBP and add a Linked Anim Graph node targeting an existing compatible AnimBP class
    - `ensure_anim_graph_trail_demo(blueprint_name, trail_bone="VB VBHead", base_joint="head", replace_existing=False)` - Create/reuse a sample Trail Controller demo chain
    - `add_blueprint_event_node(blueprint_name, event_type)` - Add event nodes
    - `add_blueprint_input_action_node(blueprint_name, action_name)` - Add input nodes
    - `add_blueprint_function_node(blueprint_name, target, function_name)` - Add function nodes
    - `add_blueprint_comment_box(blueprint_name, comment_text)` - Add a Blueprint graph comment box
    - `set_blueprint_node_comment(blueprint_name, node_id, node_comment)` - Set one Blueprint node comment bubble
    - `set_blueprint_node_positions(blueprint_name, graph_name, updates, dry_run=True)` - Guardedly move existing nodes without changing graph topology
    - `collapse_blueprint_nodes_to_function(blueprint_name, graph_name, function_name, node_ids, dry_run=True)` - Guardedly extract a Blueprint selection through Unreal's native Collapse to Function path
    - `set_blueprint_function_description(blueprint_name, description, graph_name="")` - Set a Blueprint function's Details panel description/tooltip
    - `add_blueprint_get_editor_subsystem_node(blueprint_name, subsystem_class)` - Add a Get Editor Subsystem node
    - `add_blueprint_construct_object_node(blueprint_name, object_class)` - Add a Construct Object node
    - `add_blueprint_branch_node(blueprint_name)` - Add Branch control-flow node
    - `add_blueprint_sequence_node(blueprint_name, output_count=2)` - Add Sequence control-flow node
    - `add_blueprint_return_node(blueprint_name, graph_type="function")` - Add or resolve function Return node
    - `add_blueprint_dynamic_cast_node(blueprint_name, target_class)` - Add Dynamic Cast node
    - `connect_blueprint_nodes(blueprint_name, source_node_id, source_pin, target_node_id, target_pin)` - Connect nodes
    - `delete_blueprint_node(blueprint_name, node_id, graph_name="", expected_node_class="", allow_non_exec_linked_delete=False)` - Delete one guarded Blueprint graph node
    - `add_blueprint_variable(blueprint_name, variable_name, variable_type, default_value=None)` - Add member variables with defaults/metadata
    - `remove_blueprint_variable(blueprint_name, variable_name)` - Remove one declared member variable
    - `inspect_blueprint_variable_metadata(blueprint_name, variable_name)` - Read exact stored metadata for one Blueprint member variable without compiling, saving, or dirtying it
    - `set_blueprint_variable_metadata(blueprint_name, variable_name, metadata)` - Update metadata on existing member variables
    - `set_blueprint_category_sorting(blueprint_name, category_order?, category_to_front?)` - Set Blueprint Details/My Blueprint category order
    - `bind_pcg_generation_settings_to_blueprint(blueprint_name, component_variable="ForestPCG", function_class="/Script/YourRuntimeModule.YourPCGBlueprintLibrary")` - Wire Blueprint PCG generation variables to a PCG component in Construction Script
    - `add_blueprint_function_parameter(blueprint_name, parameter_name, parameter_type, direction="input")` - Add function input/output pins
    - `add_blueprint_custom_event_node(blueprint_name, custom_event_name, signature_source_class="", signature_source_delegate_name="")` - Add custom events, optionally copying Blueprint dispatcher or native delegate signatures
    - `add_blueprint_native_delegate_bind_node(blueprint_name, delegate_owner_class, delegate_name)` - Add bind nodes for native BlueprintAssignable delegates
    - `add_blueprint_local_variable(blueprint_name, variable_name, variable_type)` - Add function-scoped local variables
    - `add_blueprint_get_self_component_reference(blueprint_name, component_name)` - Add component refs
    - `add_blueprint_self_reference(blueprint_name)` - Add self references
    - `find_blueprint_nodes(blueprint_name, node_type, event_type)` - Find nodes
    
    ## Project Tools
    - `create_input_mapping(action_name, key, input_type)` - Create input mappings
    - `audit_content_root_mcp(root_path)` - Read-only audit for asset counts, dependencies, redirectors, dirty packages, and expected content checks
    - `replace_asset_references_mcp(replacements=None, target_root="", target_paths=None, string_replacements=None, name_replacements=None, dry_run=True, save_assets=False, include_package_objects=False)` - Replace object references or scoped text/name references, dry-run and /Game-scoped by default
    - `analyze_blueprint_widget_fallbacks_mcp(source_root, target_root)` - Diagnose Blueprint/Widget duplicate fallback safety for MCP-generated content
    - `run_content_validation_pipeline_mcp(source_root, target_root, map_path="")` - Run the MCP recreate/postprocess/world-repair validation pipeline

    ## PCG Graph Tools
    - `list_pcg_assets(root_path="/Game")` - List PCG-related assets under a content path
    - `refresh_pcg_components(actor_name="", selected_only=False, cleanup=True, generate=True)` - Refresh or generate PCG components in the current editor level
    - `inspect_pcg_component_outputs(...)` - Native read-only generated output and point sample readback for current-level PCG components
    - `create_pcg_graph_from_spec(graph_spec, overwrite_existing=False)` - Create a PCG graph from a deterministic JSON-style spec
    - `audit_pcg_graph_contract(graph_path)` - Read-only PCG graph audit with nodes, pins, spawners, actor selectors, edges, and dependencies
    - `validate_pcg_source_independence(graph_path)` - Check that a recreated graph avoids forbidden source dependencies such as `/Game/Cubeless/PCG/PCGStudy`
    - `set_pcg_static_mesh_spawner_entries(graph_path, node_id, mesh_entries)` - Replace weighted Static Mesh Spawner entries on a PCG graph node
    - `read_pcg_node_contract(graph_path, node_id="")` - Read stable node contract data: title, class, position, pins, spawner selector, and actor selector
    - `promote_pcg_temp_graph(source_graph_path, target_graph_path, dry_run=True)` - Guarded temp-to-library PCG graph promotion after source-independence validation
    - `pcg_actor_smoke_test(graph_path, dry_run=True)` - Dry-run or run a guarded current-level PCG component smoke test without map switching
    - `resolve_pcg_graph(graph_path)` - Resolve a PCG graph by short name, package path, or object path
    - `list_pcg_graph_nodes(graph_path)` - List native PCG graph nodes and pins
    - `inspect_pcg_custom_hlsl_nodes(graph_path, node_id="", ...)` - Native read-only PCG Custom HLSL source/declaration inspector
    - `set_pcg_attribute_selector(graph_path, node_id, selector_property_name, attribute_name, ...)` - Safely set PCG attribute/property selector structs through the native bridge, without Unreal Python selector mutation
    - `add_pcg_node(graph_path, settings_class)` / `connect_pcg_nodes(...)` / `set_pcg_node_setting(...)` / `save_pcg_graph(graph_path)` - Low-level PCG graph editing helpers

    ## Runtime Tool Management
    - `manage_tools(action="status", tool_name="", category="", enabled=None, include_tools=True, include_recent=True, ping_unreal=False)` - Inspect or adjust runtime-only MCP tool/category enablement. Use `action="health"` for registry, telemetry, guard, and optional Unreal bridge ping. Use `action="heartbeat"` for a required Unreal bridge ping. Defaults to all tools enabled, resets on server restart, cannot disable itself, and requires `enabled` for `set_tool`/`set_category`.

    ## Material Graph Management
    - `resolve_material_graph(material_path, graph_type="auto")` - Resolve a Material or Material Function
    - `list_material_nodes(material_path, graph_type="auto")` - List material expression nodes, pins, links, and root material property connections
    - `analyze_material_graph(material_path, graph_type="auto")` - Read-only Material/Function/Instance analysis with semantics, function/reroute metadata, cost hints, and usage hints
    - `add_material_node(material_path, expression_class)` - Add a MaterialExpression node
    - `add_custom_material_node(material_path, code, output_type="CMOT_Float3", inputs=[])` - Add a validated MaterialExpressionCustom node for Custom HLSL islands
    - `set_material_node_property(material_path, node_id, property_name, value)` - Set an expression property using node_key/node_id from list/analyze output
    - `connect_material_nodes(material_path, from_node_id, to_node_id, to_input, from_output="")` - Connect expression nodes using node_key/node_id values
    - `connect_material_property(material_path, from_node_id, property, from_output="")` - Connect an expression to BaseColor/Roughness/Normal/etc.
    - `delete_material_node(material_path, node_id)` - Delete an expression node using node_key/node_id
    - `layout_material_nodes(material_path)` - Layout expression nodes
    - `compile_and_save_material(material_path)` - Recompile/update and save a Material or Material Function
    - `list_material_collection_parameter_nodes(material_path)` - Inspect MPC collection parameter nodes and stale ParameterId mismatches
    - `mirror_material_parameter_collection(source_collection_path, target_collection_path)` - Copy same-name MPC parameters/defaults/ids into another MPC
    - `replace_material_collection_references(material_path, target_collection_path)` - Retarget MPC collection parameter nodes to another collection
    - `replace_material_collection_parameters(material_path)` - Bake MPC collection parameter nodes into normal scalar/vector parameter nodes
    - `replace_material_texture_references(material_path, replacements)` - Replace texture references in material texture sample nodes
    - `refresh_material_cached_expression_data(material_path)` - Refresh cached material dependencies after graph surgery
    - `expand_material_function_calls(material_path)` - Expand Material Function Call nodes into the owning Material graph; partial saves require allow_partial_save=True
    - `get_material_parameter_collection_values(collection_path)` - Read MPC asset defaults and current editor-world runtime values
    - `set_material_parameter_collection_values(collection_path, scalars, vectors)` - Set MPC asset defaults and/or runtime values
    - `set_material_parameter_collection_sync(action, source_collection_path, target_collection_path)` - Start/stop/status runtime MPC value sync

    ## AI Texture Generation
    - `get_static_mesh_uv_layout(mesh_path, uv_channel, output_path)` - Export a Static Mesh UV wireframe PNG
    - `generate_texture_from_prompt(prompt, output_name, output_dir, size)` - Prepare a BaseColor PNG request for Codex built-in image generation; does not call API-key image services
    - `generate_pbr_companion_textures(base_color_image_path, output_name, output_dir, normal_strength, roughness, roughness_detail, ambient_occlusion_strength, metallic)` - Generate heuristic Normal/Roughness/AO/Metallic PNGs from an existing BaseColor image
    - `generate_texture_for_mesh_uv(mesh_path, prompt, uv_channel, output_name, output_dir, size)` - Export a UV guide and prepare a built-in image generation handoff; does not call API-key image services
    - `import_texture_to_unreal(image_path, unreal_folder, asset_name)` - Import a PNG as Texture2D
    - `import_texture_set_to_unreal(base_color_image_path, unreal_folder, asset_name, normal_image_path, roughness_image_path, ambient_occlusion_image_path, metallic_image_path)` - Import BaseColor plus optional companion PNG files as Texture2D assets
    - `create_material_instance_with_texture(texture_asset_path, material_name, unreal_folder, base_material_path)` - Create a BaseColor material/material instance
    - `create_material_with_texture_set(base_color_texture_asset_path, material_name, unreal_folder, normal_texture_asset_path, roughness_texture_asset_path, ambient_occlusion_texture_asset_path, metallic_texture_asset_path, base_material_path)` - Create a material/material instance from BaseColor plus optional PBR maps
    - `apply_material_to_mesh(target, material_asset_path, material_slot)` - Apply a material to selected/named mesh targets
    - `import_create_and_apply_texture_set(base_color_image_path, output_name, unreal_folder, normal_image_path, roughness_image_path, ambient_occlusion_image_path, metallic_image_path, target, material_slot, base_material_path)` - Import a local texture set, create a material, and optionally apply it
    - `generate_and_apply_ai_texture(target, prompt, output_name, unreal_folder, uv_channel, size)` - Prepare the full texture pipeline handoff; import/apply steps run after the built-in image PNG exists

    ## Niagara Effect Planning and Diagnosis
    - `plan_niagara_effect(intent, working_path="", source_system_path="", draft_spec=None, material_candidates=None, constraints=None)` - Create or normalize a grounded, mutation-free ParticleEffectSpec
    - `validate_niagara_effect_spec(effect_spec)` - Validate emitter dependencies, objectives, temp scope, promotion, rollback, and plugin-independence contracts
    - `diagnose_niagara_effect_issue(observation, compile_feedback="", system_summary=None, previous_patch_history=None, max_results=5)` - Retrieve repository-grounded symptom/cause/fix records and an objective-patch template
    - `validate_niagara_effect_patch(patch, effect_spec=None, allow_non_temp_target=False)` - Validate objective operations and emit a non-dispatched execute_batch manifest
    - `create_niagara_refinement_ledger(system_path, objectives, baseline_state, run_id="", max_iterations=4, effect_spec_hash="")` - Create a compile-gated objective ledger from a generated-temp baseline
    - `evaluate_niagara_refinement_iteration(ledger, patch_id, candidate_state, objective_ids=None, base_state_id="")` - Evaluate selected objectives plus global non-regression guards
    - `select_niagara_best_refinement_state(ledger)` - Rank accepted compile-healthy evidence-complete states
    - `critique_niagara_preview_evidence(candidate_frame_paths, criteria, baseline_frame_paths=None, roi=None)` - Convert bounded preview frames into deterministic objective metrics

    ## Niagara Inspection and Validation
    - `list_niagara_assets(root_path="/Game", include_scripts=False, include_parameter_collections=False)` - List Niagara Systems/Emitters and optional related assets
    - `inspect_niagara_system(system_path)` - Read-only Niagara System summary with emitters, user parameters, scripts, and compile hints where Python reflection exposes them
    - `list_niagara_components(selected_only=False)` - List Niagara components in the current editor level
    - `duplicate_niagara_system_to_temp(system_path, temp_folder="/Game/_MCP_Temp")` - Duplicate a System into disposable MCP temp content for safe experiments
    - `compile_and_save_niagara_system(system_path, save=False)` - Best-effort Niagara compile/wait/poll validation; save is limited to /Game/_MCP_Temp assets
    
    ## Best Practices
    
    ### UMG Widget Development
    - Create widgets with descriptive names that reflect their purpose
    - Use consistent naming conventions for widget components
    - Organize widget hierarchy logically
    - Set appropriate anchors and alignment for responsive layouts
    - Use property bindings for dynamic updates instead of direct setting
    - Handle widget events appropriately with meaningful function names
    - Clean up widgets when no longer needed
    - Test widget layouts at different resolutions
    
    ### Editor and Actor Management
    - Use unique names for actors to avoid conflicts
    - Clean up temporary actors
    - Validate transforms before applying
    - Check actor existence before modifications
    - Take regular viewport screenshots during development
    - Keep the viewport focused on relevant actors during operations
    
    ### Blueprint Development
    - Compile Blueprints after changes
    - Use meaningful names for variables and functions
    - Organize nodes logically
    - Test functionality in isolation
    - Consider performance implications
    - Document complex setups
    
    ### Error Handling
    - Check command responses for success
    - Handle errors gracefully
    - Log important operations
    - Validate parameters
    - Clean up resources on errors
    """

# Run the server
if __name__ == "__main__":
    logger.info("Starting MCP server with stdio transport")
    mcp.run(transport='stdio') 
