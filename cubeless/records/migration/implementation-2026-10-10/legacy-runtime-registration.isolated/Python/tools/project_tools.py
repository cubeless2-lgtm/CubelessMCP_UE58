"""
Project Tools for Unreal MCP.

This module provides tools for managing project-wide settings and configuration.
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context

from tools.response_schema import tool_error

# Get logger
logger = logging.getLogger("UnrealMCP")


def _unreal_connection_error() -> Dict[str, Any]:
    return tool_error(
        "Failed to connect to Unreal Engine",
        error_code="unreal_connection_failed",
    )


def _unreal_no_response_error() -> Dict[str, Any]:
    return tool_error(
        "No response from Unreal Engine",
        error_code="unreal_no_response",
    )


def _project_exception_error(message: str, exc: Exception) -> Dict[str, Any]:
    return tool_error(
        message,
        error_code="project_tool_failed",
        details={"exception_type": type(exc).__name__},
    )


def register_project_tools(mcp: FastMCP):
    """Register project tools with the MCP server."""
    
    @mcp.tool()
    def create_input_mapping(
        ctx: Context,
        action_name: str,
        key: str,
        input_type: str = "Action"
    ) -> Dict[str, Any]:
        """
        Create an input mapping for the project.
        
        Args:
            action_name: Name of the input action
            key: Key to bind (SpaceBar, LeftMouseButton, etc.)
            input_type: Type of input mapping (Action or Axis)
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return _unreal_connection_error()
            
            params = {
                "action_name": action_name,
                "key": key,
                "input_type": input_type
            }
            
            logger.info(f"Creating input mapping '{action_name}' with key '{key}'")
            response = unreal.send_command("create_input_mapping", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return _unreal_no_response_error()
            
            logger.info(f"Input mapping creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error creating input mapping: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)

    @mcp.tool()
    def audit_content_root_mcp(
        ctx: Context,
        root_path: str,
        recursive: bool = True,
        scan_paths: bool = True,
        include_dependencies: bool = True,
        include_assets: bool = False,
        write_report: bool = False,
        forbidden_dependency_prefixes: Optional[List[str]] = None,
        required_asset_paths: Optional[List[str]] = None,
        expected_asset_count: Optional[int] = None,
        expected_class_counts: Optional[Dict[str, int]] = None,
        max_samples: int = 50,
        fail_on_dirty_packages: bool = False,
        fail_on_redirectors: bool = True,
        fail_on_forbidden_dependencies: bool = True,
    ) -> Dict[str, Any]:
        """
        Read-only audit for a /Game content root.

        Use this before and after reusable content promotion to check asset and
        class counts, redirectors, dirty packages, required assets, and forbidden
        dependency prefixes without compiling, saving, or mutating assets.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "root_path": root_path,
                "recursive": recursive,
                "scan_paths": scan_paths,
                "include_dependencies": include_dependencies,
                "include_assets": include_assets,
                "write_report": write_report,
                "max_samples": max_samples,
                "fail_on_dirty_packages": fail_on_dirty_packages,
                "fail_on_redirectors": fail_on_redirectors,
                "fail_on_forbidden_dependencies": fail_on_forbidden_dependencies,
            }
            if forbidden_dependency_prefixes is not None:
                params["forbidden_dependency_prefixes"] = forbidden_dependency_prefixes
            if required_asset_paths is not None:
                params["required_asset_paths"] = required_asset_paths
            if expected_asset_count is not None:
                params["expected_asset_count"] = expected_asset_count
            if expected_class_counts is not None:
                params["expected_class_counts"] = expected_class_counts

            response = unreal.send_command("audit_content_root_mcp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error auditing content root: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)

    @mcp.tool()
    def recreate_content_folder_mcp(
        ctx: Context,
        source_root: str = "/Game/UltraDynamicSky",
        target_root: str = "/Game/_MCP_Temp/UltraDynamicSky_MCP",
        suffix: str = "_MCP",
        allow_fallback_duplicate: bool = True,
        dry_run: bool = False,
        overwrite_existing: bool = False,
        delete_target_root_first: bool = False,
        remap_references: bool = True,
        compile_blueprints: bool = True,
        log_archive_remap_objects: bool = False,
        refresh_blueprint_nodes: bool = False,
        refresh_failed_blueprint_nodes: bool = True,
        save_assets: bool = True,
        repair_level_actors: bool = True,
        force_repair_level_actors: bool = True,
        compile_passes: int = 2,
        check_editor_log_health: bool = True,
        fail_on_editor_log_issues: bool = True,
        suppress_editor_prompts: bool = True,
    ) -> Dict[str, Any]:
        """
        Recreate a content folder under an MCP target path using a suffix rule.

        The Unreal-side command attempts exact-behavior recreation. When exact
        behavior cannot be safely produced from a fresh factory asset, it uses a
        duplicate fallback and records the reason in the report.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params = {
                "source_root": source_root,
                "target_root": target_root,
                "suffix": suffix,
                "allow_fallback_duplicate": allow_fallback_duplicate,
                "dry_run": dry_run,
                "overwrite_existing": overwrite_existing,
                "delete_target_root_first": delete_target_root_first,
                "remap_references": remap_references,
                "compile_blueprints": compile_blueprints,
                "log_archive_remap_objects": log_archive_remap_objects,
                "refresh_blueprint_nodes": refresh_blueprint_nodes,
                "refresh_failed_blueprint_nodes": refresh_failed_blueprint_nodes,
                "save_assets": save_assets,
                "repair_level_actors": repair_level_actors,
                "force_repair_level_actors": force_repair_level_actors,
                "compile_passes": compile_passes,
                "check_editor_log_health": check_editor_log_health,
                "fail_on_editor_log_issues": fail_on_editor_log_issues,
                "suppress_editor_prompts": suppress_editor_prompts,
            }
            response = unreal.send_command("recreate_content_folder_mcp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error recreating content folder: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)

    @mcp.tool()
    def postprocess_content_folder_mcp(
        ctx: Context,
        source_root: str = "/Game/UltraDynamicSky",
        target_root: str = "/Game/_MCP_Temp/UltraDynamicSky_MCP",
        suffix: str = "_MCP",
        dry_run: bool = False,
        remap_references: bool = True,
        compile_blueprints: bool = True,
        refresh_blueprint_nodes: bool = False,
        refresh_failed_blueprint_nodes: bool = True,
        save_assets: bool = True,
        repair_level_actors: bool = False,
        force_repair_level_actors: bool = False,
        compile_passes: int = 2,
        check_editor_log_health: bool = True,
        fail_on_editor_log_issues: bool = True,
        suppress_editor_prompts: bool = True,
    ) -> Dict[str, Any]:
        """
        Run the reusable MCP postprocess and verification pass for an existing target content folder.

        This does not create, duplicate, delete, or overwrite assets. It pairs
        source assets to existing target assets using the suffix rule, then fixes
        references, Blueprint internals, level instances, and verification data.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params = {
                "source_root": source_root,
                "target_root": target_root,
                "suffix": suffix,
                "dry_run": dry_run,
                "remap_references": remap_references,
                "compile_blueprints": compile_blueprints,
                "refresh_blueprint_nodes": refresh_blueprint_nodes,
                "refresh_failed_blueprint_nodes": refresh_failed_blueprint_nodes,
                "save_assets": save_assets,
                "repair_level_actors": repair_level_actors,
                "force_repair_level_actors": force_repair_level_actors,
                "compile_passes": compile_passes,
                "check_editor_log_health": check_editor_log_health,
                "fail_on_editor_log_issues": fail_on_editor_log_issues,
                "suppress_editor_prompts": suppress_editor_prompts,
            }
            response = unreal.send_command("postprocess_content_folder_mcp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error postprocessing content folder: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)

    @mcp.tool()
    def repair_world_actor_instances_mcp(
        ctx: Context,
        map_path: str,
        source_root: str = "/Game/UltraDynamicSky",
        target_root: str = "/Game/_MCP_Temp/UltraDynamicSky_MCP",
        suffix: str = "_MCP",
        dry_run: bool = False,
        save_map: bool = True,
        remap_level_blueprint: bool = False,
        repair_actor_classes: bool = True,
        repair_component_classes: bool = True,
        remap_actor_references: bool = True,
        remove_source_object_map_keys: bool = True,
        check_editor_log_health: bool = True,
        fail_on_editor_log_issues: bool = True,
        suppress_editor_prompts: bool = True,
    ) -> Dict[str, Any]:
        """
        Repair actor/component instances and object-key maps in one explicit target map.

        Use this after postprocess_content_folder_mcp when map-placed actors,
        components, or world instance references may still point at source assets.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params = {
                "map_path": map_path,
                "source_root": source_root,
                "target_root": target_root,
                "suffix": suffix,
                "dry_run": dry_run,
                "save_map": save_map,
                "remap_level_blueprint": remap_level_blueprint,
                "repair_actor_classes": repair_actor_classes,
                "repair_component_classes": repair_component_classes,
                "remap_actor_references": remap_actor_references,
                "remove_source_object_map_keys": remove_source_object_map_keys,
                "check_editor_log_health": check_editor_log_health,
                "fail_on_editor_log_issues": fail_on_editor_log_issues,
                "suppress_editor_prompts": suppress_editor_prompts,
            }
            response = unreal.send_command("repair_world_actor_instances_mcp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error repairing world actor instances: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)

    @mcp.tool()
    def replace_asset_references_mcp(
        ctx: Context,
        replacements: Optional[Dict[str, str]] = None,
        target_root: str = "",
        target_paths: Optional[List[str]] = None,
        source_root: str = "",
        string_replacements: Optional[Dict[str, str]] = None,
        name_replacements: Optional[Dict[str, str]] = None,
        dry_run: bool = True,
        save_assets: bool = False,
        scan_paths: bool = True,
        refresh_materials: bool = True,
        include_package_objects: bool = False,
        allow_non_game_targets: bool = False,
    ) -> Dict[str, Any]:
        """
        Replace asset/object and path-like references in target assets.

        Defaults are intentionally non-mutating. Pass dry_run=False and
        save_assets=True only after reviewing the reported replacements.

        Args:
            replacements: Optional source asset path to target asset path mapping
            target_root: Optional /Game root whose assets should be scanned
            target_paths: Optional explicit target asset package/object paths
            source_root: Optional source root used for path/text remap repair
            string_replacements: Optional raw string replacements
            name_replacements: Optional raw name/string replacements
            dry_run: Preview replacement work without mutating assets
            save_assets: Save modified target assets when dry_run=False
            scan_paths: Scan target paths before resolving assets
            refresh_materials: Refresh material references and caches after remap
            include_package_objects: Also remap loaded inner package objects
            allow_non_game_targets: Permit explicit target_paths outside /Game
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "target_root": target_root,
                "source_root": source_root,
                "dry_run": dry_run,
                "save_assets": save_assets,
                "scan_paths": scan_paths,
                "refresh_materials": refresh_materials,
                "include_package_objects": include_package_objects,
                "allow_non_game_targets": allow_non_game_targets,
            }
            if replacements is not None:
                params["replacements"] = replacements
            if target_paths is not None:
                params["target_paths"] = target_paths
            if string_replacements is not None:
                params["string_replacements"] = string_replacements
            if name_replacements is not None:
                params["name_replacements"] = name_replacements

            response = unreal.send_command("replace_asset_references_mcp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error replacing asset references: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)

    @mcp.tool()
    def analyze_blueprint_widget_fallbacks_mcp(
        ctx: Context,
        source_root: str,
        target_root: str,
        suffix: str = "_MCP",
    ) -> Dict[str, Any]:
        """
        Analyze whether recreated Blueprint/Widget assets still need duplicate fallback handling.

        Args:
            source_root: Source content folder under /Game
            target_root: Recreated/target content folder under /Game
            suffix: Target asset suffix used to pair source and target assets
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params = {
                "source_root": source_root,
                "target_root": target_root,
                "suffix": suffix,
            }
            response = unreal.send_command("analyze_blueprint_widget_fallbacks_mcp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error analyzing Blueprint/Widget fallback state: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)

    @mcp.tool()
    def create_volume_texture_from_2d_sheet_mcp(
        ctx: Context,
        source_texture_path: str,
        target_volume_texture_path: str,
        tile_size_x: int = 128,
        tile_size_y: int = 128,
        dry_run: bool = False,
        overwrite_existing: bool = False,
        save_asset: bool = True,
        srgb: Optional[bool] = None,
        compression_settings: str = "TC_VectorDisplacementmap",
        mip_gen_settings: str = "TMGS_NoMipmaps",
        never_stream: bool = True,
        virtual_texture_streaming: bool = False,
        address_mode: str = "TA_Wrap",
    ) -> Dict[str, Any]:
        """
        Create or update a VolumeTexture from a 2D tiled source texture.

        The Unreal-side command explicitly sets Source2D tile size before
        rebuilding the volume, which avoids VolumeTextureFactory's ambiguous
        auto tile inference for non-square sheets such as 2048x1024/128.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "source_texture_path": source_texture_path,
                "target_volume_texture_path": target_volume_texture_path,
                "tile_size_x": tile_size_x,
                "tile_size_y": tile_size_y,
                "dry_run": dry_run,
                "overwrite_existing": overwrite_existing,
                "save_asset": save_asset,
                "compression_settings": compression_settings,
                "mip_gen_settings": mip_gen_settings,
                "never_stream": never_stream,
                "virtual_texture_streaming": virtual_texture_streaming,
                "address_mode": address_mode,
            }
            if srgb is not None:
                params["srgb"] = srgb

            response = unreal.send_command("create_volume_texture_from_2d_sheet_mcp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error creating VolumeTexture from 2D sheet: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)

    @mcp.tool()
    def create_volume_texture_from_raw_rgba16_sheet_mcp(
        ctx: Context,
        raw_file_path: str,
        source_texture_path: str,
        target_volume_texture_path: str,
        sheet_size_x: int = 2048,
        sheet_size_y: int = 1024,
        tile_size_x: int = 128,
        tile_size_y: int = 128,
        dry_run: bool = False,
        overwrite_existing: bool = False,
        save_assets: bool = True,
        srgb: bool = False,
        source_compression_settings: str = "TC_Default",
        volume_compression_settings: str = "TC_VectorDisplacementmap",
        source_mip_gen_settings: str = "TMGS_FromTextureGroup",
        volume_mip_gen_settings: str = "TMGS_NoMipmaps",
        never_stream_volume: bool = True,
        virtual_texture_streaming: bool = False,
        address_mode: str = "TA_Wrap",
    ) -> Dict[str, Any]:
        """
        Create or update a Texture2D and VolumeTexture from a raw RGBA16 sheet.

        This bypasses Unreal's Interchange image importer and writes the
        Texture2D editor source data directly, which is useful for large
        procedural volume sheets generated outside Unreal.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "raw_file_path": raw_file_path,
                "source_texture_path": source_texture_path,
                "target_volume_texture_path": target_volume_texture_path,
                "sheet_size_x": sheet_size_x,
                "sheet_size_y": sheet_size_y,
                "tile_size_x": tile_size_x,
                "tile_size_y": tile_size_y,
                "dry_run": dry_run,
                "overwrite_existing": overwrite_existing,
                "save_assets": save_assets,
                "srgb": srgb,
                "source_compression_settings": source_compression_settings,
                "volume_compression_settings": volume_compression_settings,
                "source_mip_gen_settings": source_mip_gen_settings,
                "volume_mip_gen_settings": volume_mip_gen_settings,
                "never_stream_volume": never_stream_volume,
                "virtual_texture_streaming": virtual_texture_streaming,
                "address_mode": address_mode,
            }
            response = unreal.send_command("create_volume_texture_from_raw_rgba16_sheet_mcp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error creating VolumeTexture from raw RGBA16 sheet: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)

    @mcp.tool()
    def set_material_texture_sample_sampler_type_mcp(
        ctx: Context,
        material_path: str,
        texture_path: str,
        sampler_type: str = "SAMPLERTYPE_LinearColor",
        dry_run: bool = False,
        save: bool = True,
        compile: bool = True,
    ) -> Dict[str, Any]:
        """
        Change sampler type on TextureSample nodes that reference a texture.

        Useful after replacing or procedurally rebuilding a texture with
        linear data, where existing sample nodes still use Color samplers.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params: Dict[str, Any] = {
                "material_path": material_path,
                "texture_path": texture_path,
                "sampler_type": sampler_type,
                "dry_run": dry_run,
                "save": save,
                "compile": compile,
            }
            response = unreal.send_command("set_material_texture_sample_sampler_type_mcp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error setting material texture sample sampler type: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)

    @mcp.tool()
    def run_content_validation_pipeline_mcp(
        ctx: Context,
        source_root: str,
        target_root: str,
        suffix: str = "_MCP",
        map_path: str = "",
        continue_on_failure: bool = False,
        run_world_repair: bool = False,
        allow_fallback_duplicate: bool = True,
        dry_run: bool = False,
        overwrite_existing: bool = True,
        delete_target_root_first: bool = True,
        remap_references: bool = True,
        compile_blueprints: bool = True,
        refresh_blueprint_nodes: bool = False,
        refresh_failed_blueprint_nodes: bool = True,
        save_assets: bool = True,
        repair_level_actors: bool = True,
        force_repair_level_actors: bool = True,
        compile_passes: int = 3,
        check_editor_log_health: bool = True,
        fail_on_editor_log_issues: bool = True,
        suppress_editor_prompts: bool = True,
        save_map: bool = True,
        repair_actor_classes: bool = True,
        repair_component_classes: bool = True,
        remap_actor_references: bool = True,
        remove_source_object_map_keys: bool = True,
    ) -> Dict[str, Any]:
        """
        Run recreate, postprocess, optional world repair, and verification as one MCP pipeline.

        Args:
            source_root: Source content folder under /Game
            target_root: Generated/target content folder under /Game
            suffix: Target asset suffix used by recreation and validation
            map_path: Optional target map path for world actor repair
            continue_on_failure: Continue later pipeline steps after an earlier failure
            run_world_repair: Run map actor/component repair; requires map_path
            allow_fallback_duplicate: Permit duplicate fallback for assets that cannot be freshly recreated
            dry_run: Run without applying destructive content changes where supported
            overwrite_existing: Overwrite existing generated target assets
            delete_target_root_first: Delete target_root before recreation
            remap_references: Remap source asset references to generated targets
            compile_blueprints: Compile Blueprints during recreation/postprocess
            refresh_blueprint_nodes: Refresh all Blueprint nodes
            refresh_failed_blueprint_nodes: Refresh nodes only after compile failures
            save_assets: Save changed generated assets
            repair_level_actors: Repair level actor instances during recreation
            force_repair_level_actors: Force level actor repair even when heuristics are uncertain
            compile_passes: Blueprint compile/postprocess pass count
            check_editor_log_health: Include editor log health checks
            fail_on_editor_log_issues: Treat editor log issues as validation failure
            suppress_editor_prompts: Suppress editor prompts during batch work
            save_map: Save the map when world repair changes it
            repair_actor_classes: Remap actor classes during world repair
            repair_component_classes: Remap component classes during world repair
            remap_actor_references: Remap actor/object references during world repair
            remove_source_object_map_keys: Remove stale source keys from object maps
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return _unreal_connection_error()

            params = {
                "source_root": source_root,
                "target_root": target_root,
                "suffix": suffix,
                "map_path": map_path,
                "continue_on_failure": continue_on_failure,
                "run_world_repair": run_world_repair,
                "allow_fallback_duplicate": allow_fallback_duplicate,
                "dry_run": dry_run,
                "overwrite_existing": overwrite_existing,
                "delete_target_root_first": delete_target_root_first,
                "remap_references": remap_references,
                "compile_blueprints": compile_blueprints,
                "refresh_blueprint_nodes": refresh_blueprint_nodes,
                "refresh_failed_blueprint_nodes": refresh_failed_blueprint_nodes,
                "save_assets": save_assets,
                "repair_level_actors": repair_level_actors,
                "force_repair_level_actors": force_repair_level_actors,
                "compile_passes": compile_passes,
                "check_editor_log_health": check_editor_log_health,
                "fail_on_editor_log_issues": fail_on_editor_log_issues,
                "suppress_editor_prompts": suppress_editor_prompts,
                "save_map": save_map,
                "repair_actor_classes": repair_actor_classes,
                "repair_component_classes": repair_component_classes,
                "remap_actor_references": remap_actor_references,
                "remove_source_object_map_keys": remove_source_object_map_keys,
            }
            response = unreal.send_command("run_content_validation_pipeline_mcp", params)
            return response or _unreal_no_response_error()
        except Exception as e:
            error_msg = f"Error running content validation pipeline: {e}"
            logger.error(error_msg)
            return _project_exception_error(error_msg, e)
    
    logger.info("Project tools registered successfully")
