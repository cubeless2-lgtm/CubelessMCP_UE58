"""Chaos, Chaos Cloth, and Chaos Destruction tools for UE 5.6.1."""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP
from tools.dependency_guard import (
    reject_mcp_dependency_reference,
    reject_mcp_dependency_references,
)
from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")
LEARNING_ROOT = "/Game/_MCP_Temp/ChaosDestructionLearning"


def _send(command: str, params: Dict[str, Any], timeout_seconds: int = 120) -> Dict[str, Any]:
    from unreal_mcp_server import get_unreal_connection

    unreal = get_unreal_connection()
    if not unreal:
        return tool_error(
            "Failed to connect to Unreal Engine",
            error_code="unreal_connection_failed",
        )
    response = unreal.send_command(command, params, timeout_seconds=timeout_seconds)
    if not response:
        return tool_error(
            "No response from Unreal Engine",
            error_code="unreal_no_response",
        )
    return response


def _actor_selector_error(actor_name: str, actor_label: str) -> Dict[str, Any] | None:
    """Require one non-empty native actor selector for mutation routes."""

    if actor_name.strip() or actor_label.strip():
        return None
    return tool_error(
        "Either actor_name or actor_label must be non-empty",
        error_code="chaos_actor_selector_required",
        details={
            "fields": ["actor_name", "actor_label"],
            "reason": "at_least_one_non_empty",
        },
    )


def _dependency_reference_error(exc: ValueError) -> Dict[str, Any]:
    """Return safe dependency-guard evidence without echoing rejected values."""

    field_name = getattr(exc, "field_name", "<unknown>")
    if not isinstance(field_name, str) or not field_name:
        field_name = "<unknown>"
    if len(field_name) > 256:
        field_name = f"{field_name[:253]}..."
    reason = getattr(exc, "reason", "mcp_only_reference")
    if reason != "mcp_only_reference":
        reason = "mcp_only_reference"
    return tool_error(
        "MCP-only dependency reference rejected",
        error_code="mcp_dependency_reference_rejected",
        details={"field": field_name, "reason": reason},
    )


def register_chaos_tools(mcp: FastMCP) -> None:
    """Register guarded Chaos solver, cloth, and destruction tools."""

    @mcp.tool()
    def inspect_chaos_solver_settings(
        ctx: Context,
        actor_name: str = "",
        actor_label: str = "",
        world_context: str = "auto",
        include_runtime_state: bool = True,
        max_results: int = 16,
    ) -> Dict[str, Any]:
        """Inspect Chaos Solver actor settings and optional UE 5.6 runtime state.

        Runtime output includes read-only diagnostics for the six allowlisted
        global CVars that can override iteration and collision actor values
        after ``FPBDRigidsSolver::ApplyConfig``. ``max_results`` is clamped to
        1..256. Each actor includes copy-ready ``expected_current_fields``;
        the response also lists ``writable_setting_names``.
        """
        try:
            return _send(
                "inspect_chaos_solver_settings",
                {
                    "actor_name": actor_name,
                    "actor_label": actor_label,
                    "world_context": world_context,
                    "include_runtime_state": include_runtime_state,
                    "max_results": max_results,
                },
            )
        except Exception as exc:
            logger.exception("Chaos Solver inspection failed")
            return tool_error(
                "Chaos Solver inspection failed",
                error_code="chaos_solver_inspection_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def configure_chaos_solver_settings(
        ctx: Context,
        settings: Dict[str, Any],
        actor_name: str = "",
        actor_label: str = "",
        world_context: str = "editor",
        expected_current: Optional[Dict[str, Any]] = None,
        dry_run: bool = True,
        save_level: bool = False,
    ) -> Dict[str, Any]:
        """Plan or apply an allowlisted Chaos Solver patch with strict readback.

        ``settings`` accepts iteration counts (0..255), non-negative collision,
        cluster, and event-filter numbers, ``cluster_union_connection_type`` as
        an inspected enum name, generation/filter booleans, non-negative break
        limits, and throttle counts of -1 or non-negative integers. Initial
        overlap depenetration accepts -1 or a non-negative number. Collision
        margins, runtime determinism, and CVar writes are intentionally blocked.
        Use the inspector's flat ``expected_current_fields`` for optimistic
        concurrency. Apply requires ``dry_run=False``; persistence additionally
        requires ``save_level=True`` in the editor world.
        """
        selector_error = _actor_selector_error(actor_name, actor_label)
        if selector_error is not None:
            return selector_error
        params: Dict[str, Any] = {
            "actor_name": actor_name,
            "actor_label": actor_label,
            "settings": settings,
            "world_context": world_context,
            "dry_run": dry_run,
            "save_level": save_level,
        }
        if expected_current is not None:
            params["expected_current"] = expected_current
        try:
            return _send("configure_chaos_solver_settings", params)
        except Exception as exc:
            logger.exception("Chaos Solver configuration failed")
            return tool_error(
                "Chaos Solver configuration failed",
                error_code="chaos_solver_configuration_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def inspect_chaos_cloth_asset(
        ctx: Context,
        cloth_asset_path: str,
        lod_index: int = -1,
        property_name_filter: str = "",
        include_property_values: bool = True,
        include_property_flags: bool = True,
        include_dataflow_nodes: bool = True,
        max_properties: int = 512,
        max_nodes: int = 256,
    ) -> Dict[str, Any]:
        """Inspect native Chaos Cloth collections, properties, and Dataflow ownership.

        ``max_properties`` is clamped to 1..4096 and ``max_nodes`` to 1..2048.
        For durable edits, round-trip the returned ``dataflow_asset_path`` and
        a ``cloth_dataflow_nodes`` entry's ``name``/``type``. Supply only an
        ``editable_properties`` member marked ``supported_for_mcp_write`` and
        use its canonical ``export_text`` as the value/expected-current form.
        """
        try:
            reject_mcp_dependency_reference("cloth_asset_path", cloth_asset_path)
        except ValueError as exc:
            return _dependency_reference_error(exc)
        try:
            return _send(
                "inspect_chaos_cloth_asset",
                {
                    "cloth_asset_path": cloth_asset_path,
                    "lod_index": lod_index,
                    "property_name_filter": property_name_filter,
                    "include_property_values": include_property_values,
                    "include_property_flags": include_property_flags,
                    "include_dataflow_nodes": include_dataflow_nodes,
                    "max_properties": max_properties,
                    "max_nodes": max_nodes,
                },
            )
        except Exception as exc:
            logger.exception("Chaos Cloth asset inspection failed")
            return tool_error(
                "Chaos Cloth asset inspection failed",
                error_code="chaos_cloth_asset_inspection_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def inspect_chaos_cloth_simulation_mesh(
        ctx: Context,
        cloth_asset_path: str,
        lod_index: int = 0,
        weight_map_name: str = "",
        vertex_start: int = 0,
        max_vertices: int = 4096,
        face_start: int = 0,
        max_faces: int = 4096,
        include_positions: bool = True,
        include_normals: bool = False,
        include_indices: bool = True,
        include_weight_values: bool = True,
    ) -> Dict[str, Any]:
        """Inspect one bounded UE 5.6 Chaos Cloth simulation-mesh LOD.

        The native route exposes simulation positions, normals, triangle
        indices, and a selected weight map even though ``UChaosClothAsset``
        does not expose those collection arrays to Unreal Python. Vertex and
        face paging keep large cloth assets within one bounded MCP response;
        use the returned totals and page metadata to request later ranges.
        """
        try:
            reject_mcp_dependency_reference("cloth_asset_path", cloth_asset_path)
        except ValueError as exc:
            return _dependency_reference_error(exc)
        try:
            return _send(
                "inspect_chaos_cloth_simulation_mesh",
                {
                    "cloth_asset_path": cloth_asset_path,
                    "lod_index": lod_index,
                    "weight_map_name": weight_map_name,
                    "vertex_start": vertex_start,
                    "max_vertices": max_vertices,
                    "face_start": face_start,
                    "max_faces": max_faces,
                    "include_positions": include_positions,
                    "include_normals": include_normals,
                    "include_indices": include_indices,
                    "include_weight_values": include_weight_values,
                },
            )
        except Exception as exc:
            logger.exception("Chaos Cloth simulation mesh inspection failed")
            return tool_error(
                "Chaos Cloth simulation mesh inspection failed",
                error_code="chaos_cloth_simulation_mesh_inspection_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def inspect_chaos_cloth_component(
        ctx: Context,
        actor_name: str = "",
        actor_label: str = "",
        component_name: str = "",
        model_index: int = 0,
        lod_index: int = -1,
        world_context: str = "auto",
        include_runtime_properties: bool = True,
        max_properties: int = 512,
        max_results: int = 16,
    ) -> Dict[str, Any]:
        """Inspect bounded Chaos Cloth component settings, properties, and metrics.

        Runtime collection reads are emitted only after a valid initialized
        outfit interactor is available. ``max_results`` bounds components and
        is clamped to 1..256; ``max_properties`` is one response-wide
        runtime-property budget clamped to 1..4096. Each component returns
        copy-ready ``expected_current_fields`` for serialized settings.
        """
        try:
            return _send(
                "inspect_chaos_cloth_component",
                {
                    "actor_name": actor_name,
                    "actor_label": actor_label,
                    "component_name": component_name,
                    "model_index": model_index,
                    "lod_index": lod_index,
                    "world_context": world_context,
                    "include_runtime_properties": include_runtime_properties,
                    "max_properties": max_properties,
                    "max_results": max_results,
                },
            )
        except Exception as exc:
            logger.exception("Chaos Cloth component inspection failed")
            return tool_error(
                "Chaos Cloth component inspection failed",
                error_code="chaos_cloth_component_inspection_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def configure_chaos_cloth_component(
        ctx: Context,
        actor_name: str = "",
        property_updates: Optional[List[Dict[str, Any]]] = None,
        component_settings: Optional[Dict[str, Any]] = None,
        component_name: str = "",
        actor_label: str = "",
        model_index: int = 0,
        lod_index: int = -1,
        world_context: str = "auto",
        expected_current: Optional[Dict[str, Any]] = None,
        allow_intrinsic_recreate: bool = False,
        dry_run: bool = True,
        recreate_simulation: str = "auto",
        save_level: bool = False,
    ) -> Dict[str, Any]:
        """Plan or apply guarded Chaos Cloth component/runtime-property changes.

        ``component_settings`` accepts enable/suspend booleans, environment
        collision, geometry scale (0..10000), teleport distance (>=0), teleport
        rotation (0..180), and ``teleport_action``. Each ``property_updates``
        entry supplies ``name``, ``value_type`` (float, weighted_float, int,
        bool, vector, or string), its typed value, and optional LOD and
        ``expected_current``. At most 256 entries are accepted, and
        duplicate/overlapping LOD updates are rejected.
        Runtime collection updates plus one-shot ``teleport`` and
        ``teleport_and_reset`` actions are transient and cannot be persisted
        alone with ``save_level``; ``reset_mode`` and serialized component
        settings are durable. Intrinsic runtime-property changes require
        explicit simulation recreation approval. ``recreate_simulation`` is
        ``auto``, ``always``, or ``never``. Apply requires ``dry_run=False``
        and every mutation is verified by native readback.
        """
        selector_error = _actor_selector_error(actor_name, actor_label)
        if selector_error is not None:
            return selector_error
        updates = property_updates if property_updates is not None else []
        settings = component_settings if component_settings is not None else {}
        params: Dict[str, Any] = {
            "actor_name": actor_name,
            "actor_label": actor_label,
            "component_name": component_name,
            "property_updates": updates,
            "component_settings": settings,
            "model_index": model_index,
            "lod_index": lod_index,
            "world_context": world_context,
            "allow_intrinsic_recreate": allow_intrinsic_recreate,
            "dry_run": dry_run,
            "recreate_simulation": recreate_simulation,
            "save_level": save_level,
        }
        if expected_current is not None:
            params["expected_current"] = expected_current
        try:
            reject_mcp_dependency_references("property_updates", updates)
            reject_mcp_dependency_references("component_settings", settings)
        except ValueError as exc:
            return _dependency_reference_error(exc)
        try:
            return _send("configure_chaos_cloth_component", params)
        except Exception as exc:
            logger.exception("Chaos Cloth component configuration failed")
            return tool_error(
                "Chaos Cloth component configuration failed",
                error_code="chaos_cloth_component_configuration_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def configure_chaos_cloth_dataflow_node(
        ctx: Context,
        cloth_asset_path: str,
        node_name: str,
        property_values: Dict[str, str],
        expected_node_type: str = "",
        expected_dataflow_asset_path: str = "",
        expected_current: Optional[Dict[str, str]] = None,
        allow_connected_inputs: bool = False,
        regenerate_asset: bool = True,
        save_assets: bool = False,
        dry_run: bool = True,
    ) -> Dict[str, Any]:
        """Plan or apply typed-export-text changes to one reviewed Cloth Dataflow node.

        At most 64 values are accepted, and each must be an Unreal export-text
        string. Apply mode also requires the
        exact node type and Dataflow asset path returned by inspection, with
        regeneration enabled. Connected inputs require explicit opt-in, while a
        Dataflow graph referenced by another package is always blocked because
        only the selected Cloth Asset can be regenerated safely. After an apply,
        regeneration, readback, or save failure, the native handler attempts to
        restore the selected properties and regenerate the original Cloth Asset;
        inspect the returned ``rollback_*`` evidence before continuing.
        """
        params: Dict[str, Any] = {
            "cloth_asset_path": cloth_asset_path,
            "node_name": node_name,
            "property_values": property_values,
            "expected_node_type": expected_node_type,
            "expected_dataflow_asset_path": expected_dataflow_asset_path,
            "allow_connected_inputs": allow_connected_inputs,
            "regenerate_asset": regenerate_asset,
            "save_assets": save_assets,
            "dry_run": dry_run,
        }
        if expected_current is not None:
            params["expected_current"] = expected_current
        try:
            reject_mcp_dependency_reference("cloth_asset_path", cloth_asset_path)
            if expected_dataflow_asset_path:
                reject_mcp_dependency_reference(
                    "expected_dataflow_asset_path", expected_dataflow_asset_path
                )
            reject_mcp_dependency_references("property_values", property_values)
        except ValueError as exc:
            return _dependency_reference_error(exc)
        try:
            return _send(
                "configure_chaos_cloth_dataflow_node",
                params,
                timeout_seconds=300,
            )
        except Exception as exc:
            logger.exception("Chaos Cloth Dataflow node configuration failed")
            return tool_error(
                "Chaos Cloth Dataflow node configuration failed",
                error_code="chaos_cloth_dataflow_configuration_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def configure_chaos_cloth_weight_map(
        ctx: Context,
        cloth_asset_path: str,
        node_name: str,
        weight_map_name: str,
        weights: Optional[List[float]] = None,
        fill_value: Optional[float] = None,
        lod_index: int = 0,
        expected_node_type: str = "",
        expected_dataflow_asset_path: str = "",
        expected_vertex_count: int = -1,
        expected_topology_md5: str = "",
        expected_weight_map_md5: str = "",
        expected_node_state_md5: str = "",
        expected_path_md5: str = "",
        require_runtime_binding: bool = True,
        save_assets: bool = False,
        dry_run: bool = True,
    ) -> Dict[str, Any]:
        """Plan or apply one durable Chaos Cloth Dataflow weight-map update.

        Supply exactly one normalized ``[0,1]`` source: ``weights`` or
        ``fill_value``. Apply mode is guarded by exact Dataflow/node identity,
        vertex count, topology MD5, generated weight-map MD5, node-state MD5,
        and the exact downstream ``path_md5`` copied from a fresh dry-run. By default,
        the selected map must remain bound to the generated runtime cloth
        collection. Native regeneration, independent readback, rollback, and
        optional asset saves are reported with ``mutation_result_v1``.
        """
        params: Dict[str, Any] = {
            "cloth_asset_path": cloth_asset_path,
            "node_name": node_name,
            "weight_map_name": weight_map_name,
            "lod_index": lod_index,
            "expected_node_type": expected_node_type,
            "expected_dataflow_asset_path": expected_dataflow_asset_path,
            "expected_vertex_count": expected_vertex_count,
            "expected_topology_md5": expected_topology_md5,
            "expected_weight_map_md5": expected_weight_map_md5,
            "expected_node_state_md5": expected_node_state_md5,
            "expected_path_md5": expected_path_md5,
            "require_runtime_binding": require_runtime_binding,
            "save_assets": save_assets,
            "dry_run": dry_run,
        }
        if weights is not None:
            params["weights"] = weights
        if fill_value is not None:
            params["fill_value"] = fill_value
        try:
            reject_mcp_dependency_reference("cloth_asset_path", cloth_asset_path)
            if expected_dataflow_asset_path:
                reject_mcp_dependency_reference(
                    "expected_dataflow_asset_path", expected_dataflow_asset_path
                )
        except ValueError as exc:
            return _dependency_reference_error(exc)
        if not dry_run and not expected_topology_md5.strip():
            return tool_error(
                "expected_topology_md5 is required in apply mode",
                error_code="chaos_cloth_topology_precondition_required",
                details={
                    "field": "expected_topology_md5",
                    "reason": "non_empty_apply_precondition",
                },
            )
        if not dry_run:
            missing_preconditions = []
            if not expected_node_type.strip():
                missing_preconditions.append("expected_node_type")
            if not expected_dataflow_asset_path.strip():
                missing_preconditions.append("expected_dataflow_asset_path")
            if expected_vertex_count < 0:
                missing_preconditions.append("expected_vertex_count")
            if not expected_weight_map_md5.strip():
                missing_preconditions.append("expected_weight_map_md5")
            if not expected_node_state_md5.strip():
                missing_preconditions.append("expected_node_state_md5")
            if not expected_path_md5.strip():
                missing_preconditions.append("expected_path_md5")
            if missing_preconditions:
                return tool_error(
                    "Fresh inspection preconditions are required in apply mode",
                    error_code="chaos_cloth_apply_preconditions_required",
                    details={
                        "missing_fields": missing_preconditions,
                        "reason": "fresh_inspection_required",
                    },
                )
        try:
            return _send(
                "configure_chaos_cloth_weight_map",
                params,
                timeout_seconds=300,
            )
        except Exception as exc:
            logger.exception("Chaos Cloth weight map configuration failed")
            return tool_error(
                "Chaos Cloth weight map configuration failed",
                error_code="chaos_cloth_weight_map_configuration_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def create_legacy_chaos_clothing_from_skeletal_sections(
        ctx: Context,
        source_skeletal_mesh_path: str,
        output_skeletal_mesh_path: str,
        section_configs: List[Dict[str, Any]],
        dry_run: bool = True,
        save_assets: bool = True,
        use_source_physics_asset: bool = True,
        create_vroid_lower_body_cloth_physics_asset: bool = False,
    ) -> Dict[str, Any]:
        """Create a new Skeletal Mesh with legacy Chaos Cloth bound to named sections.

        Each ``section_configs`` item requires ``section_index``,
        ``expected_material_slot``, and ``max_distance_cm``. The command
        accepts ``material_path`` per section and requires it when the source
        material slot has no assigned material. The selected material must
        already support Skeletal Mesh and Clothing usage; it is assigned only
        on the duplicated output mesh, never the source mesh.
        The command duplicates the source to a new temporary-root Skeletal Mesh, creates
        and binds one engine clothing asset per section, then copies Blender's
        ``Cloth_PinGuide`` vertex-color red channel into the MaxDistance paint
        map (black = pinned, white = free). The source mesh is never modified.
        """
        params: Dict[str, Any] = {
            "source_skeletal_mesh_path": source_skeletal_mesh_path,
            "output_skeletal_mesh_path": output_skeletal_mesh_path,
            "section_configs": section_configs,
            "dry_run": dry_run,
            "save_assets": save_assets,
            "use_source_physics_asset": use_source_physics_asset,
            "create_vroid_lower_body_cloth_physics_asset": create_vroid_lower_body_cloth_physics_asset,
        }
        try:
            reject_mcp_dependency_reference(
                "source_skeletal_mesh_path", source_skeletal_mesh_path
            )
            reject_mcp_dependency_reference(
                "output_skeletal_mesh_path", output_skeletal_mesh_path
            )
            reject_mcp_dependency_references("section_configs", section_configs)
        except ValueError as exc:
            return _dependency_reference_error(exc)
        try:
            return _send(
                "create_legacy_chaos_clothing_from_skeletal_sections",
                params,
                timeout_seconds=300,
            )
        except Exception as exc:
            logger.exception("Legacy Chaos Cloth skeletal-section creation failed")
            return tool_error(
                "Legacy Chaos Cloth skeletal-section creation failed",
                error_code="chaos_cloth_asset_creation_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def create_chaos_destruction_learning_fixture(
        ctx: Context,
        dry_run: bool = True,
        force_rebuild: bool = False,
        save_assets: bool = True,
        configure_current_level: bool = False,
        asset_root: str = LEARNING_ROOT,
    ) -> Dict[str, Any]:
        """Create the isolated Chaos learning fixture; defaults to a non-mutating dry run."""
        try:
            return _send(
                "create_chaos_destruction_learning_fixture",
                {
                    "asset_root": asset_root,
                    "dry_run": dry_run,
                    "force_rebuild": force_rebuild,
                    "save_assets": save_assets,
                    "configure_current_level": configure_current_level,
                },
                timeout_seconds=300,
            )
        except Exception as exc:
            logger.exception("Chaos fixture creation failed")
            return tool_error(
                "Chaos fixture creation failed",
                error_code="chaos_fixture_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def inspect_geometry_collection_performance_guarded(
        ctx: Context,
        geometry_collection_path: str,
        actor_name: str = "",
        tiny_relative_threshold: float = 0.04,
    ) -> Dict[str, Any]:
        """Read hierarchy, Tiny Geo, removal, Root Mesh Proxy, and optional runtime state."""
        try:
            return _send(
                "inspect_geometry_collection_performance_guarded",
                {
                    "geometry_collection_path": geometry_collection_path,
                    "actor_name": actor_name,
                    "tiny_relative_threshold": tiny_relative_threshold,
                },
            )
        except Exception as exc:
            logger.exception("Geometry Collection inspection failed")
            return tool_error(
                "Geometry Collection inspection failed",
                error_code="geometry_collection_inspection_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def inspect_geometry_collection_runtime_state(
        ctx: Context,
        component_name: str,
        actor_name: str = "",
        actor_label: str = "",
        include_pieces: bool = True,
        max_pieces: int = 512,
        pie_instance: int = -1,
    ) -> Dict[str, Any]:
        """Read per-piece Geometry Collection state from the active begun-play PIE world.

        The native command reads DynamicCollection, removal decay, render-world
        transforms, PhysicsObject transforms, velocities, and sleep/disabled
        states without mutating or saving the world. ``component_name`` and
        either ``actor_name`` or ``actor_label`` are required. Pass
        ``pie_instance`` when multiple begun-play PIE worlds exist.
        """
        normalized_component_name = component_name.strip()
        if not normalized_component_name:
            return tool_error(
                "component_name must be non-empty",
                error_code="chaos_component_name_required",
            )
        selector_error = _actor_selector_error(actor_name, actor_label)
        if selector_error is not None:
            return selector_error
        try:
            params: Dict[str, Any] = {
                "actor_name": actor_name,
                "actor_label": actor_label,
                "component_name": normalized_component_name,
                "include_pieces": bool(include_pieces),
                "max_pieces": max(1, min(int(max_pieces), 512)),
            }
            if pie_instance >= 0:
                params["pie_instance"] = int(pie_instance)
            return _send(
                "inspect_geometry_collection_runtime_state",
                params,
                timeout_seconds=30,
            )
        except Exception as exc:
            logger.exception("Geometry Collection runtime-state inspection failed")
            return tool_error(
                "Geometry Collection runtime-state inspection failed",
                error_code="geometry_collection_runtime_inspection_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def run_chaos_destruction_performance_smoke(
        ctx: Context,
        operation: str = "sample",
        actor_name: str = "",
        solver_actor_name: str = "",
        start: Optional[List[float]] = None,
        end: Optional[List[float]] = None,
        expected_hit: bool = True,
        trace_count: int = 100,
        benchmark_path: str = "both",
        strain: Optional[float] = None,
        radius: float = 100.0,
        breaking_velocity: float = 600.0,
        breaking_velocity_vector: Optional[List[float]] = None,
        item_index: int = -1,
        expected_direction: Optional[List[float]] = None,
        dispatch_world_time_seconds: float = -1.0,
        witness_actor_name: str = "",
        floor_actor_name: str = "Chaos_StaticFloor",
        one_way_level: int = -1,
        initial_witness_location: Optional[List[float]] = None,
        initial_witness_velocity: Optional[List[float]] = None,
        per_advance_breaks_allowed: int = 2,
        per_advance_breaks_reschedule_limit: int = 64,
        particle_release_throttling_min_count: int = 64,
        particle_release_throttling_max_count: int = 128,
        optimize_for_runtime_memory: bool = True,
        performance_scenario: str = "",
        capture_limit_frames: int = 90,
        require_play_session: bool = True,
    ) -> Dict[str, Any]:
        """Operate on an existing PIE/SIE Chaos fixture without controlling editor lifecycle."""
        params: Dict[str, Any] = {
            "operation": operation,
            "actor_name": actor_name,
            "solver_actor_name": solver_actor_name,
            "radius": radius,
            "breaking_velocity": breaking_velocity,
            "expected_hit": expected_hit,
            "trace_count": trace_count,
            "benchmark_path": benchmark_path,
            "item_index": item_index,
            "dispatch_world_time_seconds": dispatch_world_time_seconds,
            "witness_actor_name": witness_actor_name,
            "floor_actor_name": floor_actor_name,
            "one_way_level": one_way_level,
            "per_advance_breaks_allowed": per_advance_breaks_allowed,
            "per_advance_breaks_reschedule_limit": per_advance_breaks_reschedule_limit,
            "particle_release_throttling_min_count": particle_release_throttling_min_count,
            "particle_release_throttling_max_count": particle_release_throttling_max_count,
            "optimize_for_runtime_memory": optimize_for_runtime_memory,
            "performance_scenario": performance_scenario,
            "capture_limit_frames": capture_limit_frames,
            "require_play_session": require_play_session,
        }
        if start is not None:
            params["start"] = start
        if strain is not None:
            params["strain"] = strain
        if end is not None:
            params["end"] = end
        if breaking_velocity_vector is not None:
            params["breaking_velocity_vector"] = breaking_velocity_vector
        if expected_direction is not None:
            params["expected_direction"] = expected_direction
        if initial_witness_location is not None:
            params["initial_witness_location"] = initial_witness_location
        if initial_witness_velocity is not None:
            params["initial_witness_velocity"] = initial_witness_velocity
        try:
            return _send("run_chaos_destruction_performance_smoke", params)
        except Exception as exc:
            logger.exception("Chaos destruction smoke operation failed")
            return tool_error(
                "Chaos destruction smoke operation failed",
                error_code="chaos_destruction_smoke_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def run_chaos_cache_smoke(
        ctx: Context,
        operation: str = "inspect",
        cache_collection_path: str = f"{LEARNING_ROOT}/CC_ChaosLearning",
        cache_manager_actor_name: str = "",
        component_actor_name: str = "",
        cache_name: str = "ChaosLearning",
    ) -> Dict[str, Any]:
        """Configure, trigger, or inspect an existing PIE/SIE Chaos Cache fixture."""
        try:
            return _send(
                "run_chaos_cache_smoke",
                {
                    "operation": operation,
                    "cache_collection_path": cache_collection_path,
                    "cache_manager_actor_name": cache_manager_actor_name,
                    "component_actor_name": component_actor_name,
                    "cache_name": cache_name,
                },
            )
        except Exception as exc:
            logger.exception("Chaos cache smoke operation failed")
            return tool_error(
                "Chaos cache smoke operation failed",
                error_code="chaos_cache_smoke_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def create_geometry_collection_from_mesh(
        ctx: Context,
        source_static_mesh_paths: List[str],
        output_geometry_collection_path: str,
        source_transforms: Optional[List[Dict[str, Any]]] = None,
        root_mesh_proxy_static_mesh_path: str = "",
        material_reindex: bool = True,
        overwrite_existing: bool = False,
    ) -> Dict[str, Any]:
        """Create a native Geometry Collection while preserving every source Static Mesh."""
        try:
            if not source_static_mesh_paths:
                return tool_error(
                    "source_static_mesh_paths must not be empty",
                    error_code="chaos_cinematic_invalid_source_meshes",
                )
            reject_mcp_dependency_reference(
                "output_geometry_collection_path", output_geometry_collection_path
            )
            for index, path in enumerate(source_static_mesh_paths):
                reject_mcp_dependency_reference(f"source_static_mesh_paths[{index}]", path)
            if root_mesh_proxy_static_mesh_path:
                reject_mcp_dependency_reference(
                    "root_mesh_proxy_static_mesh_path", root_mesh_proxy_static_mesh_path
                )
            return _send(
                "create_geometry_collection_from_mesh",
                {
                    "source_static_mesh_paths": source_static_mesh_paths,
                    "source_transforms": source_transforms if source_transforms is not None else [],
                    "output_geometry_collection_path": output_geometry_collection_path,
                    "root_mesh_proxy_static_mesh_path": root_mesh_proxy_static_mesh_path,
                    "material_reindex": material_reindex,
                    "overwrite_existing": overwrite_existing,
                },
                timeout_seconds=300,
            )
        except Exception as exc:
            logger.exception("Geometry Collection conversion failed")
            return tool_error(
                "Geometry Collection conversion failed",
                error_code="chaos_cinematic_conversion_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def configure_building_fracture(
        ctx: Context,
        source_geometry_collection_path: str,
        output_geometry_collection_path: str,
        fracture_method: str = "uniform_voronoi",
        site_count: int = 64,
        random_seed: int = 1337,
        cluster_levels: int = 2,
        cluster_size: int = 8,
        damage_thresholds: Optional[List[float]] = None,
        merge_tiny_geometry: bool = True,
        tiny_relative_threshold: float = 0.01,
        collision_object_reduction_percentage: float = 20.0,
        maximum_collision_particles: int = 60,
        collision_implicit_type: str = "convex",
        remove_on_sleep: bool = True,
        remove_on_break: bool = True,
        sleep_time_seconds: float = 5.0,
        removal_duration_seconds: float = 1.0,
        break_timer_seconds: float = 1.0,
        root_mesh_proxy_static_mesh_path: str = "",
        overwrite_existing: bool = False,
    ) -> Dict[str, Any]:
        """Create a separate hierarchically fractured wall/building Geometry Collection."""
        try:
            for label, path in (
                ("source_geometry_collection_path", source_geometry_collection_path),
                ("output_geometry_collection_path", output_geometry_collection_path),
            ):
                reject_mcp_dependency_reference(label, path)
            if source_geometry_collection_path == output_geometry_collection_path:
                return tool_error(
                    "source and output Geometry Collection paths must differ",
                    error_code="chaos_cinematic_source_overwrite_blocked",
                )
            if root_mesh_proxy_static_mesh_path:
                reject_mcp_dependency_reference(
                    "root_mesh_proxy_static_mesh_path", root_mesh_proxy_static_mesh_path
                )
            return _send(
                "configure_building_fracture",
                {
                    "source_geometry_collection_path": source_geometry_collection_path,
                    "output_geometry_collection_path": output_geometry_collection_path,
                    "fracture_method": fracture_method,
                    "site_count": site_count,
                    "random_seed": random_seed,
                    "cluster_levels": cluster_levels,
                    "cluster_size": cluster_size,
                    "damage_thresholds": damage_thresholds
                    if damage_thresholds is not None
                    else [500000.0, 250000.0, 100000.0],
                    "merge_tiny_geometry": merge_tiny_geometry,
                    "tiny_relative_threshold": tiny_relative_threshold,
                    "collision_object_reduction_percentage": collision_object_reduction_percentage,
                    "maximum_collision_particles": maximum_collision_particles,
                    "collision_implicit_type": collision_implicit_type,
                    "remove_on_sleep": remove_on_sleep,
                    "remove_on_break": remove_on_break,
                    "sleep_time_seconds": sleep_time_seconds,
                    "removal_duration_seconds": removal_duration_seconds,
                    "break_timer_seconds": break_timer_seconds,
                    "root_mesh_proxy_static_mesh_path": root_mesh_proxy_static_mesh_path,
                    "overwrite_existing": overwrite_existing,
                },
                timeout_seconds=300,
            )
        except Exception as exc:
            logger.exception("Building fracture configuration failed")
            return tool_error(
                "Building fracture configuration failed",
                error_code="chaos_cinematic_fracture_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def configure_anchor_field(
        ctx: Context,
        geometry_collection_actor_name: str,
        field_actor_name: str = "Chaos_AnchorField",
        box_center: Optional[List[float]] = None,
        box_extent: Optional[List[float]] = None,
        field_magnitude: int = 1,
        save_level: bool = True,
    ) -> Dict[str, Any]:
        """Create a native serialized Anchor Field and assign it as an initialization field."""
        try:
            return _send(
                "configure_anchor_field",
                {
                    "geometry_collection_actor_name": geometry_collection_actor_name,
                    "field_actor_name": field_actor_name,
                    "box_center": box_center if box_center is not None else [0.0, 0.0, 0.0],
                    "box_extent": box_extent if box_extent is not None else [100.0, 100.0, 100.0],
                    "field_magnitude": field_magnitude,
                    "save_level": save_level,
                },
            )
        except Exception as exc:
            logger.exception("Anchor Field configuration failed")
            return tool_error(
                "Anchor Field configuration failed",
                error_code="chaos_cinematic_anchor_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def trigger_trace_external_strain(
        ctx: Context,
        operation: str = "begin",
        actor_name: str = "",
        start: Optional[List[float]] = None,
        end: Optional[List[float]] = None,
        expected_hit: bool = True,
        trace_complex: bool = True,
        radius: float = 100.0,
        strain: float = 100000.0,
        propagation_depth: int = 1,
        propagation_factor: float = 0.5,
        breaking_velocity: float = 800.0,
    ) -> Dict[str, Any]:
        """Trace an exact Geometry Collection item and dispatch real external strain."""
        try:
            return _send(
                "trigger_trace_external_strain",
                {
                    "operation": operation,
                    "actor_name": actor_name,
                    "start": start if start is not None else [],
                    "end": end if end is not None else [],
                    "expected_hit": expected_hit,
                    "trace_complex": trace_complex,
                    "radius": radius,
                    "strain": strain,
                    "propagation_depth": propagation_depth,
                    "propagation_factor": propagation_factor,
                    "breaking_velocity": breaking_velocity,
                },
            )
        except Exception as exc:
            logger.exception("Trace + External Strain failed")
            return tool_error(
                "Trace + External Strain failed",
                error_code="chaos_cinematic_trace_strain_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def record_chaos_cache(
        ctx: Context,
        operation: str,
        cache_manager_actor_name: str,
        geometry_collection_actor_names: List[str],
        cache_collection_path: str,
        cache_names: Optional[List[str]] = None,
        solver_actor_name: str = "",
        duration_seconds: float = 2.0,
        frame_rate: float = 30.0,
        timeout_seconds: float = 10.0,
        initial_trigger_mode: str = "strain_velocity",
        motion_velocity: Optional[List[float]] = None,
        per_advance_breaks_allowed: int = 8,
        per_advance_breaks_reschedule_limit: int = 64,
        particle_release_throttling_min_count: int = 64,
        particle_release_throttling_max_count: int = 128,
    ) -> Dict[str, Any]:
        """Begin, sample, or finish a bounded real Chaos Cache recording session."""
        try:
            reject_mcp_dependency_reference("cache_collection_path", cache_collection_path)
            return _send(
                "record_chaos_cache",
                {
                    "operation": operation,
                    "cache_manager_actor_name": cache_manager_actor_name,
                    "geometry_collection_actor_names": geometry_collection_actor_names,
                    "cache_collection_path": cache_collection_path,
                    "cache_names": cache_names if cache_names is not None else [],
                    "solver_actor_name": solver_actor_name,
                    "duration_seconds": duration_seconds,
                    "frame_rate": frame_rate,
                    "timeout_seconds": timeout_seconds,
                    "initial_trigger_mode": initial_trigger_mode,
                    "motion_velocity": motion_velocity
                    if motion_velocity is not None
                    else [300.0, 0.0, 600.0],
                    "per_advance_breaks_allowed": per_advance_breaks_allowed,
                    "per_advance_breaks_reschedule_limit": per_advance_breaks_reschedule_limit,
                    "particle_release_throttling_min_count": particle_release_throttling_min_count,
                    "particle_release_throttling_max_count": particle_release_throttling_max_count,
                },
                timeout_seconds=max(30, int(timeout_seconds) + 10),
            )
        except Exception as exc:
            logger.exception("Chaos Cache recording failed")
            return tool_error(
                "Chaos Cache recording failed",
                error_code="chaos_cinematic_cache_record_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def validate_chaos_cache(
        ctx: Context,
        operation: str,
        cache_manager_actor_name: str,
        cache_collection_path: str,
        sequence_path: str,
        geometry_collection_actor_names: Optional[List[str]] = None,
        sample_times: Optional[List[float]] = None,
        scrub_frames: Optional[List[int]] = None,
        repeat_count: int = 2,
        timeout_seconds: float = 10.0,
    ) -> Dict[str, Any]:
        """Validate repeated and reverse-order cache scrubbing plus live-simulation suppression."""
        try:
            reject_mcp_dependency_reference("cache_collection_path", cache_collection_path)
            reject_mcp_dependency_reference("sequence_path", sequence_path)
            return _send(
                "validate_chaos_cache",
                {
                    "operation": operation,
                    "cache_manager_actor_name": cache_manager_actor_name,
                    "cache_collection_path": cache_collection_path,
                    "geometry_collection_actor_names": geometry_collection_actor_names
                    if geometry_collection_actor_names is not None
                    else [],
                    "sequence_path": sequence_path,
                    "sample_times": sample_times
                    if sample_times is not None
                    else [0.25, 0.5, 1.0],
                    "scrub_frames": scrub_frames
                    if scrub_frames is not None
                    else [0, 15, 30, 45, 60],
                    "validate_forward_reverse": True,
                    "repeat_count": repeat_count,
                    "timeout_seconds": timeout_seconds,
                },
                timeout_seconds=max(30, int(timeout_seconds) + 10),
            )
        except Exception as exc:
            logger.exception("Chaos Cache validation failed")
            return tool_error(
                "Chaos Cache validation failed",
                error_code="chaos_cinematic_cache_validation_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def create_chaos_level_sequence(
        ctx: Context,
        output_sequence_path: str,
        camera_actor_name: str = "",
        display_rate: float = 30.0,
        tick_resolution: int = 24000,
        start_frame: int = 0,
        end_frame: int = 60,
        overwrite_existing: bool = False,
    ) -> Dict[str, Any]:
        """Create a native Level Sequence prepared for deterministic Chaos cache playback."""
        try:
            reject_mcp_dependency_reference("output_sequence_path", output_sequence_path)
            return _send(
                "create_chaos_level_sequence",
                {
                    "output_sequence_path": output_sequence_path,
                    "camera_actor_name": camera_actor_name,
                    "display_rate": display_rate,
                    "tick_resolution": tick_resolution,
                    "start_frame": start_frame,
                    "end_frame": end_frame,
                    "overwrite_existing": overwrite_existing,
                },
            )
        except Exception as exc:
            logger.exception("Chaos Level Sequence creation failed")
            return tool_error(
                "Chaos Level Sequence creation failed",
                error_code="chaos_cinematic_sequence_create_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def bind_chaos_cache_to_sequence(
        ctx: Context,
        sequence_path: str,
        cache_manager_actor_name: str,
        cache_collection_path: str,
        section_start_frame: int = 0,
        section_end_frame: int = 60,
        cache_play_rate: float = 1.0,
        cache_start_offset_seconds: float = 0.0,
    ) -> Dict[str, Any]:
        """Bind a native Chaos Cache Manager and cache track/section to a Level Sequence."""
        try:
            reject_mcp_dependency_reference("sequence_path", sequence_path)
            reject_mcp_dependency_reference("cache_collection_path", cache_collection_path)
            return _send(
                "bind_chaos_cache_to_sequence",
                {
                    "sequence_path": sequence_path,
                    "cache_manager_actor_name": cache_manager_actor_name,
                    "cache_collection_path": cache_collection_path,
                    "section_start_frame": section_start_frame,
                    "section_end_frame": section_end_frame,
                    "cache_play_rate": cache_play_rate,
                    "cache_start_offset_seconds": cache_start_offset_seconds,
                },
            )
        except Exception as exc:
            logger.exception("Chaos Cache sequence binding failed")
            return tool_error(
                "Chaos Cache sequence binding failed",
                error_code="chaos_cinematic_sequence_binding_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def configure_chaos_sequence_timing(
        ctx: Context,
        sequence_path: str,
        display_rate: float = 30.0,
        tick_resolution: int = 24000,
        start_frame: int = 0,
        end_frame: int = 60,
        section_start_frame: int = 0,
        section_end_frame: int = 60,
        cache_play_rate: float = 1.0,
        cache_start_offset_seconds: float = 0.0,
        reverse: bool = False,
    ) -> Dict[str, Any]:
        """Set sequence and Chaos cache section timing, offsets, rate, and reverse state."""
        try:
            reject_mcp_dependency_reference("sequence_path", sequence_path)
            return _send(
                "configure_chaos_sequence_timing",
                {
                    "sequence_path": sequence_path,
                    "display_rate": display_rate,
                    "tick_resolution": tick_resolution,
                    "start_frame": start_frame,
                    "end_frame": end_frame,
                    "section_start_frame": section_start_frame,
                    "section_end_frame": section_end_frame,
                    "cache_play_rate": cache_play_rate,
                    "cache_start_offset_seconds": cache_start_offset_seconds,
                    "reverse": reverse,
                },
            )
        except Exception as exc:
            logger.exception("Chaos sequence timing configuration failed")
            return tool_error(
                "Chaos sequence timing configuration failed",
                error_code="chaos_cinematic_sequence_timing_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def inspect_chaos_cinematic_pipeline(
        ctx: Context,
        geometry_collection_path: str,
        sequence_path: str,
        cache_manager_actor_name: str,
        map_path: str,
        chaos_state_sha256: str,
        field_actor_name: str = "Chaos_AnchorField",
        require_movie_render_pipeline: bool = True,
        mrq_output_directory: str = "",
        mrq_manifest_path: str = "",
        expected_display_rate: int = 30,
        expected_tick_resolution: int = 24000,
        expected_playback_start_frame: int = 0,
        expected_playback_end_frame: int = 60,
    ) -> Dict[str, Any]:
        """Inspect native hierarchy, proxy, anchor, cache, Sequencer, MRQ, and MCP independence."""
        try:
            reject_mcp_dependency_reference("geometry_collection_path", geometry_collection_path)
            reject_mcp_dependency_reference("sequence_path", sequence_path)
            return _send(
                "inspect_chaos_cinematic_pipeline",
                {
                    "geometry_collection_path": geometry_collection_path,
                    "sequence_path": sequence_path,
                    "cache_manager_actor_name": cache_manager_actor_name,
                    "map_path": map_path,
                    "chaos_state_sha256": chaos_state_sha256,
                    "field_actor_name": field_actor_name,
                    "require_movie_render_pipeline": require_movie_render_pipeline,
                    "mrq_output_directory": mrq_output_directory,
                    "mrq_manifest_path": mrq_manifest_path,
                    "expected_display_rate": expected_display_rate,
                    "expected_tick_resolution": expected_tick_resolution,
                    "expected_playback_start_frame": expected_playback_start_frame,
                    "expected_playback_end_frame": expected_playback_end_frame,
                },
            )
        except Exception as exc:
            logger.exception("Chaos cinematic pipeline inspection failed")
            return tool_error(
                "Chaos cinematic pipeline inspection failed",
                error_code="chaos_cinematic_inspection_failed",
                details={"exception_type": type(exc).__name__},
            )
