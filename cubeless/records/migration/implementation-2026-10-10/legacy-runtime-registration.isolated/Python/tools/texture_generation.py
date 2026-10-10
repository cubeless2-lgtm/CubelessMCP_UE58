"""AI texture generation tools for Unreal MCP."""

import logging
import os
import re
import tempfile
import threading
from functools import wraps
from pathlib import Path
from typing import Any, Dict

from mcp.server.fastmcp import Context, FastMCP
import numpy as np
from PIL import Image

from services.unreal_texture_importer import (
    apply_material_to_mesh as unreal_apply_material_to_mesh,
    create_material_instance_with_texture as unreal_create_material_instance_with_texture,
    create_material_with_texture_set as unreal_create_material_with_texture_set,
    export_static_mesh_uv_layout,
    import_texture_to_unreal as unreal_import_texture_to_unreal,
    resolve_target_static_mesh,
)
from tools.mutation_result_contract import (
    composite_failure_safety_fields,
    find_unknown_transport_marker,
)
from tools.response_schema import tool_error


logger = logging.getLogger("UnrealMCP")
_TEXTURE_COMPOSITE_LOCK = threading.RLock()


def _serialize_texture_composite(func: Any) -> Any:
    """Prevent multi-step texture workflows from interleaving editor mutations."""

    @wraps(func)
    def wrapped(*args: Any, **kwargs: Any) -> Dict[str, Any]:
        if not _TEXTURE_COMPOSITE_LOCK.acquire(blocking=False):
            return tool_error(
                "Another texture composite workflow is already running",
                error_code="texture_composite_already_running",
                mutation_attempted=False,
                mutation_performed=False,
                changed=False,
            )
        try:
            return func(*args, **kwargs)
        finally:
            _TEXTURE_COMPOSITE_LOCK.release()

    return wrapped


UV_GUIDED_PROMPT_SUFFIX = (
    "Follow the provided UV layout guide. Keep meaningful details inside UV islands. "
    "Avoid visible seams across UV island borders. Generate a BaseColor/albedo game texture only; "
    "do not claim to create a complete PBR material set."
)


def _default_output_dir() -> str:
    return os.path.join(tempfile.gettempdir(), "unreal_mcp_ai_textures")


def sanitize_output_name(output_name: str) -> str:
    """Return a filesystem-friendly PNG basename without an extension."""
    name = re.sub(r"[^A-Za-z0-9_.-]+", "_", output_name.strip())
    name = name.strip("._")
    return name or "AI_Texture"


def _png_path(output_dir: str, output_name: str) -> str:
    """Build an absolute PNG path without changing the filesystem."""
    safe_name = sanitize_output_name(output_name)
    output_path = Path(output_dir).expanduser().resolve() / f"{safe_name}.png"
    return str(output_path)


def ensure_png_path(output_dir: str, output_name: str) -> str:
    """Build an absolute PNG path and create its parent directory."""
    output_path = Path(_png_path(output_dir, output_name))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    return str(output_path)


def _material_name_from_output(output_name: str) -> str:
    safe_name = sanitize_output_name(output_name)
    if safe_name.startswith("T_"):
        return "M_" + safe_name[2:]
    if safe_name.startswith("MI_") or safe_name.startswith("M_"):
        return safe_name
    return "M_" + safe_name


def _clamp01(value: float, default: float) -> float:
    try:
        return max(0.0, min(1.0, float(value)))
    except (TypeError, ValueError):
        return default


def _nonnegative_float(value: float, default: float) -> float:
    try:
        return max(0.0, float(value))
    except (TypeError, ValueError):
        return default


def _texture_set_asset_name(asset_name: str, slot: str) -> str:
    safe_name = sanitize_output_name(asset_name)
    suffix_by_slot = {
        "base_color": "",
        "normal": "_N",
        "roughness": "_R",
        "ambient_occlusion": "_AO",
        "metallic": "_M",
    }
    suffix = suffix_by_slot.get(slot, f"_{slot}")
    return f"{safe_name}{suffix}"


def _texture_set_image_paths(
    base_color_image_path: str,
    normal_image_path: str = "",
    roughness_image_path: str = "",
    ambient_occlusion_image_path: str = "",
    metallic_image_path: str = "",
) -> Dict[str, str]:
    paths = {
        "base_color": base_color_image_path,
        "normal": normal_image_path,
        "roughness": roughness_image_path,
        "ambient_occlusion": ambient_occlusion_image_path,
        "metallic": metallic_image_path,
    }
    return {slot: path for slot, path in paths.items() if path}


def import_texture_set_files_to_unreal(
    image_paths: Dict[str, str],
    unreal_folder: str,
    asset_name: str,
) -> Dict[str, Any]:
    """Import BaseColor and optional companion texture files into Unreal."""

    if not image_paths.get("base_color"):
        return tool_error(
            "base_color image path is required",
            error_code="missing_base_color_image",
            stage="import_texture_set",
            image_paths=image_paths,
        )

    imported_textures: Dict[str, str] = {}
    imports: Dict[str, Any] = {}

    for slot, image_path in image_paths.items():
        slot_asset_name = _texture_set_asset_name(asset_name, slot)
        imported = unreal_import_texture_to_unreal(image_path, unreal_folder, slot_asset_name)
        imports[slot] = imported
        if not imported.get("success"):
            safety_fields = composite_failure_safety_fields(
                imported,
                failed_stage=f"import_texture:{slot}",
                mutation_relevant=True,
                completed_mutation_stages=[
                    f"import_texture:{completed_slot}"
                    for completed_slot in imported_textures
                ],
                failed_stage_mutation_performed=bool(
                    imported.get("mutation_performed") is True
                    or imported.get("partial_mutation") is True
                ),
            )
            return tool_error(
                f"Texture import failed for {slot}",
                error_code=imported.get("error_code", "texture_set_import_failed"),
                stage="import_texture_set",
                failed_slot=slot,
                imports=imports,
                **safety_fields,
            )
        imported_textures[slot] = imported["asset_path"]

    return {
        "success": True,
        "message": "Imported texture set",
        "unreal_folder": unreal_folder,
        "asset_name": sanitize_output_name(asset_name),
        "texture_asset_paths": imported_textures,
        "imports": imports,
    }


def generate_pbr_companion_texture_files(
    base_color_image_path: str,
    output_name: str = "",
    output_dir: str = "",
    normal_strength: float = 2.0,
    roughness: float = 0.75,
    roughness_detail: float = 0.15,
    ambient_occlusion_strength: float = 0.35,
    metallic: float = 0.0,
) -> Dict[str, Any]:
    """Generate heuristic companion PBR texture PNGs from a BaseColor image."""

    base_path = Path(base_color_image_path).expanduser().resolve()
    if not base_path.exists():
        return tool_error(
            f"BaseColor image does not exist: {base_path}",
            error_code="base_color_image_missing",
            stage="pbr_companion_generation",
            base_color_image_path=str(base_path),
        )

    local_output_dir = Path(output_dir).expanduser().resolve() if output_dir else base_path.parent
    safe_output_name = sanitize_output_name(output_name or base_path.stem)
    local_output_dir.mkdir(parents=True, exist_ok=True)

    normal_strength_value = _nonnegative_float(normal_strength, 2.0)
    roughness_value = _clamp01(roughness, 0.75)
    roughness_detail_value = _clamp01(roughness_detail, 0.15)
    ao_strength_value = _clamp01(ambient_occlusion_strength, 0.35)
    metallic_value = _clamp01(metallic, 0.0)

    with Image.open(base_path) as source_image:
        rgb_image = source_image.convert("RGB")
        width, height = rgb_image.size
        luma = np.asarray(rgb_image.convert("L"), dtype=np.float32) / 255.0

    padded_luma = np.pad(luma, 1, mode="edge")
    dx = padded_luma[1:-1, 2:] - padded_luma[1:-1, :-2]
    dy = padded_luma[2:, 1:-1] - padded_luma[:-2, 1:-1]

    nx = -dx * normal_strength_value
    ny = -dy * normal_strength_value
    nz = np.ones_like(luma)
    length = np.sqrt((nx * nx) + (ny * ny) + (nz * nz))
    normal = np.stack(
        (
            (nx / length * 0.5) + 0.5,
            (ny / length * 0.5) + 0.5,
            (nz / length * 0.5) + 0.5,
        ),
        axis=2,
    )

    roughness_map = np.clip(roughness_value + ((0.5 - luma) * roughness_detail_value), 0.0, 1.0)
    ao_map = np.clip(1.0 - ((1.0 - luma) * ao_strength_value), 0.0, 1.0)
    metallic_map = np.full_like(luma, metallic_value)

    outputs = {
        "normal": local_output_dir / f"{safe_output_name}_N.png",
        "roughness": local_output_dir / f"{safe_output_name}_R.png",
        "ambient_occlusion": local_output_dir / f"{safe_output_name}_AO.png",
        "metallic": local_output_dir / f"{safe_output_name}_M.png",
    }

    Image.fromarray(np.clip(normal * 255.0, 0, 255).astype(np.uint8)).save(outputs["normal"])
    Image.fromarray(np.clip(roughness_map * 255.0, 0, 255).astype(np.uint8)).save(outputs["roughness"])
    Image.fromarray(np.clip(ao_map * 255.0, 0, 255).astype(np.uint8)).save(outputs["ambient_occlusion"])
    Image.fromarray(np.clip(metallic_map * 255.0, 0, 255).astype(np.uint8)).save(outputs["metallic"])

    return {
        "success": True,
        "message": "Generated heuristic PBR companion textures",
        "base_color_image_path": str(base_path),
        "output_dir": str(local_output_dir),
        "output_name": safe_output_name,
        "image_size": {"width": width, "height": height},
        "map_paths": {name: str(path) for name, path in outputs.items()},
        "parameters": {
            "normal_strength": normal_strength_value,
            "roughness": roughness_value,
            "roughness_detail": roughness_detail_value,
            "ambient_occlusion_strength": ao_strength_value,
            "metallic": metallic_value,
        },
        "heuristic": True,
        "warnings": [
            "Companion maps are derived from BaseColor luminance and are suitable as a first-pass material aid, not a scanned or model-authored PBR set."
        ],
    }


def _builtin_image_generation_required(
    prompt: str,
    output_path: str,
    size: str,
    reference_image_path: str = "",
) -> Dict[str, Any]:
    message = (
        "OpenAI API-key based image generation is disabled for this project. "
        "Use Codex built-in image generation to create the PNG, then call "
        "generate_pbr_companion_textures and import_create_and_apply_texture_set."
    )
    return tool_error(
        message,
        error_code="builtin_image_generation_required",
        stage="builtin_image_generation_required",
        api_route_disabled=True,
        billing_route="none",
        image_generation_route="codex_builtin_image_generation",
        prompt=prompt,
        requested_size=size,
        expected_output_path=output_path,
        reference_image_path=reference_image_path,
    )


def register_texture_generation_tools(mcp: FastMCP):
    """Register texture helper tools that avoid API-key image generation."""

    @mcp.tool()
    def get_static_mesh_uv_layout(
        ctx: Context,
        mesh_path: str,
        uv_channel: int = 0,
        output_path: str = "",
    ) -> Dict[str, Any]:
        """
        Export a Static Mesh UV layout as a PNG.

        Args:
            mesh_path: Static Mesh asset path, for example /Game/Meshes/SM_Rock.
            uv_channel: UV channel index to export.
            output_path: Optional PNG output path. Defaults to the temp directory.
        """
        try:
            result = export_static_mesh_uv_layout(mesh_path, uv_channel, output_path or None)
            if find_unknown_transport_marker(result) is not None:
                return tool_error(
                    "Static Mesh UV read outcome is unknown; PNG export was not started",
                    error_code="uv_layout_read_transport_unknown",
                    stage="uv_layout_read",
                    **composite_failure_safety_fields(
                        result,
                        failed_stage="get_static_mesh_uv_data",
                        mutation_relevant=False,
                    ),
                )
            return result
        except Exception as exc:
            logger.exception("Failed to export Static Mesh UV layout")
            return tool_error(
                str(exc),
                error_code="uv_layout_export_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def generate_texture_from_prompt(
        ctx: Context,
        prompt: str,
        output_name: str,
        output_dir: str,
        size: str = "1024x1024",
    ) -> Dict[str, Any]:
        """
        Prepare a BaseColor texture request for Codex built-in image generation.

        This tool does not call API-key based or external image services.

        Args:
            prompt: Text prompt for the image model.
            output_name: Output PNG basename.
            output_dir: Local directory where the generated PNG will be saved.
            size: Image size, for example 1024x1024.
        """
        output_path = ensure_png_path(output_dir, output_name)
        return _builtin_image_generation_required(prompt, output_path, size)

    @mcp.tool()
    def generate_pbr_companion_textures(
        ctx: Context,
        base_color_image_path: str,
        output_name: str = "",
        output_dir: str = "",
        normal_strength: float = 2.0,
        roughness: float = 0.75,
        roughness_detail: float = 0.15,
        ambient_occlusion_strength: float = 0.35,
        metallic: float = 0.0,
    ) -> Dict[str, Any]:
        """
        Generate heuristic Normal, Roughness, AO, and Metallic PNGs from BaseColor.

        Args:
            base_color_image_path: Local BaseColor PNG path.
            output_name: Output basename. Defaults to the BaseColor stem.
            output_dir: Output directory. Defaults to the BaseColor directory.
            normal_strength: Luminance-gradient normal strength.
            roughness: Base grayscale roughness value in 0..1.
            roughness_detail: Luminance detail mixed into roughness in 0..1.
            ambient_occlusion_strength: Luminance-derived AO strength in 0..1.
            metallic: Constant grayscale metallic value in 0..1.
        """
        try:
            return generate_pbr_companion_texture_files(
                base_color_image_path=base_color_image_path,
                output_name=output_name,
                output_dir=output_dir,
                normal_strength=normal_strength,
                roughness=roughness,
                roughness_detail=roughness_detail,
                ambient_occlusion_strength=ambient_occlusion_strength,
                metallic=metallic,
            )
        except Exception as exc:
            logger.exception("Failed to generate PBR companion textures")
            return tool_error(
                str(exc),
                error_code="pbr_companion_generation_failed",
                stage="pbr_companion_generation",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    @_serialize_texture_composite
    def generate_texture_for_mesh_uv(
        ctx: Context,
        mesh_path: str,
        prompt: str,
        uv_channel: int = 0,
        output_name: str = "T_AI_Texture",
        output_dir: str = "",
        size: str = "1024x1024",
    ) -> Dict[str, Any]:
        """
        Export a mesh UV layout and prepare a Codex built-in image generation request.

        This tool does not call API-key based or external image services.

        Args:
            mesh_path: Static Mesh asset path.
            prompt: Texture prompt.
            uv_channel: UV channel index.
            output_name: Output PNG basename.
            output_dir: Local output directory.
            size: Image size, for example 1024x1024.
        """
        local_output_dir = output_dir or _default_output_dir()
        # Keep the read-before-write contract honest: merely preparing the UV
        # read must not create a local directory. The exporter creates the
        # parent only when it reaches the PNG-render stage.
        uv_path = _png_path(
            local_output_dir,
            f"{sanitize_output_name(output_name)}_UV{uv_channel}",
        )
        uv_result = export_static_mesh_uv_layout(mesh_path, uv_channel, uv_path)
        if not uv_result.get("success"):
            return tool_error(
                "UV layout export failed",
                error_code="uv_export_failed",
                stage="uv_export",
                uv_result=uv_result,
                **composite_failure_safety_fields(
                    uv_result,
                    failed_stage="get_static_mesh_uv_data",
                    mutation_relevant=False,
                ),
            )

        output_path = ensure_png_path(local_output_dir, output_name)
        guided_prompt = f"{prompt.strip()}\n\n{UV_GUIDED_PROMPT_SUFFIX}"
        result = _builtin_image_generation_required(
            guided_prompt,
            output_path,
            size,
            reference_image_path=uv_result.get("output_path", uv_path),
        )
        result.update(
            {
                "uv_layout_path": uv_result.get("output_path"),
                "mesh_path": mesh_path,
                "uv_channel": uv_channel,
                "uv_result": uv_result,
            }
        )
        return result

    @mcp.tool()
    def import_texture_to_unreal(
        ctx: Context,
        image_path: str,
        unreal_folder: str,
        asset_name: str,
    ) -> Dict[str, Any]:
        """
        Import a PNG file as a Texture2D asset into Unreal.

        Args:
            image_path: Local PNG path.
            unreal_folder: Destination Content Browser folder, for example /Game/AI_Generated/Textures.
            asset_name: Texture asset name.
        """
        return unreal_import_texture_to_unreal(image_path, unreal_folder, asset_name)

    @mcp.tool()
    @_serialize_texture_composite
    def import_texture_set_to_unreal(
        ctx: Context,
        base_color_image_path: str,
        unreal_folder: str,
        asset_name: str,
        normal_image_path: str = "",
        roughness_image_path: str = "",
        ambient_occlusion_image_path: str = "",
        metallic_image_path: str = "",
    ) -> Dict[str, Any]:
        """
        Import BaseColor and optional companion PNG files as Texture2D assets.

        Args:
            base_color_image_path: Required local BaseColor PNG path.
            unreal_folder: Destination Content Browser texture folder.
            asset_name: Base Texture2D asset name; companion maps use _N/_R/_AO/_M suffixes.
            normal_image_path: Optional local Normal PNG path.
            roughness_image_path: Optional local Roughness PNG path.
            ambient_occlusion_image_path: Optional local AO PNG path.
            metallic_image_path: Optional local Metallic PNG path.
        """
        return import_texture_set_files_to_unreal(
            _texture_set_image_paths(
                base_color_image_path,
                normal_image_path,
                roughness_image_path,
                ambient_occlusion_image_path,
                metallic_image_path,
            ),
            unreal_folder,
            asset_name,
        )

    @mcp.tool()
    def create_material_instance_with_texture(
        ctx: Context,
        texture_asset_path: str,
        material_name: str,
        unreal_folder: str,
        base_material_path: str = "",
    ) -> Dict[str, Any]:
        """
        Create a BaseColor material or a material instance using an imported texture.

        Args:
            texture_asset_path: Texture2D asset path.
            material_name: Material or material instance asset name.
            unreal_folder: Destination Content Browser folder.
            base_material_path: Optional base material for a Material Instance.
        """
        return unreal_create_material_instance_with_texture(
            texture_asset_path,
            material_name,
            unreal_folder,
            base_material_path,
        )

    @mcp.tool()
    def create_material_with_texture_set(
        ctx: Context,
        base_color_texture_asset_path: str,
        material_name: str,
        unreal_folder: str,
        normal_texture_asset_path: str = "",
        roughness_texture_asset_path: str = "",
        ambient_occlusion_texture_asset_path: str = "",
        metallic_texture_asset_path: str = "",
        base_material_path: str = "",
    ) -> Dict[str, Any]:
        """
        Create a Material or Material Instance from BaseColor and optional PBR maps.

        Args:
            base_color_texture_asset_path: Required BaseColor Texture2D asset path.
            material_name: Material or material instance asset name.
            unreal_folder: Destination Content Browser folder.
            normal_texture_asset_path: Optional Normal Texture2D asset path.
            roughness_texture_asset_path: Optional Roughness Texture2D asset path.
            ambient_occlusion_texture_asset_path: Optional AO Texture2D asset path.
            metallic_texture_asset_path: Optional Metallic Texture2D asset path.
            base_material_path: Optional base material for a Material Instance.
        """
        return unreal_create_material_with_texture_set(
            {
                "base_color": base_color_texture_asset_path,
                "normal": normal_texture_asset_path,
                "roughness": roughness_texture_asset_path,
                "ambient_occlusion": ambient_occlusion_texture_asset_path,
                "metallic": metallic_texture_asset_path,
            },
            material_name,
            unreal_folder,
            base_material_path,
        )

    @mcp.tool()
    @_serialize_texture_composite
    def import_create_and_apply_texture_set(
        ctx: Context,
        base_color_image_path: str,
        output_name: str,
        unreal_folder: str = "/Game/AI_Generated",
        normal_image_path: str = "",
        roughness_image_path: str = "",
        ambient_occlusion_image_path: str = "",
        metallic_image_path: str = "",
        target: str = "",
        material_slot: int = 0,
        base_material_path: str = "",
    ) -> Dict[str, Any]:
        """
        Import a local texture set, create a PBR material, and optionally apply it.

        Args:
            base_color_image_path: Required local BaseColor PNG path.
            output_name: Base output name for textures and material.
            unreal_folder: Root destination folder; Textures and Materials subfolders are used.
            normal_image_path: Optional local Normal PNG path.
            roughness_image_path: Optional local Roughness PNG path.
            ambient_occlusion_image_path: Optional local AO PNG path.
            metallic_image_path: Optional local Metallic PNG path.
            target: Optional selected/actor/StaticMesh target to apply the material to.
            material_slot: Material slot index used when target is provided.
            base_material_path: Optional base material for Material Instance creation.
        """
        logs = []
        completed_mutation_stages: list[str] = []
        texture_folder = f"{unreal_folder.rstrip('/')}/Textures"
        material_folder = f"{unreal_folder.rstrip('/')}/Materials"
        texture_asset_name = sanitize_output_name(output_name)
        material_name = _material_name_from_output(output_name)

        imported = import_texture_set_files_to_unreal(
            _texture_set_image_paths(
                base_color_image_path,
                normal_image_path,
                roughness_image_path,
                ambient_occlusion_image_path,
                metallic_image_path,
            ),
            texture_folder,
            texture_asset_name,
        )
        logs.append({"stage": "import_texture_set", "result": imported})
        if not imported.get("success"):
            inner_completed = imported.get("completed_mutation_stages")
            if not isinstance(inner_completed, list):
                inner_completed = []
            return tool_error(
                imported.get("message", "Texture set import failed"),
                error_code=imported.get("error_code", "texture_set_import_failed"),
                stage="import_texture_set",
                logs=logs,
                **composite_failure_safety_fields(
                    imported,
                    failed_stage="import_texture_set",
                    mutation_relevant=True,
                    completed_mutation_stages=inner_completed,
                    failed_stage_mutation_performed=bool(
                        imported.get("mutation_performed") is True
                        or imported.get("partial_mutation") is True
                    ),
                ),
            )
        completed_mutation_stages.append("import_texture_set")

        material = unreal_create_material_with_texture_set(
            imported["texture_asset_paths"],
            material_name,
            material_folder,
            base_material_path,
        )
        logs.append({"stage": "create_material", "result": material})
        if not material.get("success"):
            return tool_error(
                material.get("message", "Material creation failed"),
                error_code="create_material_failed",
                stage="create_material",
                logs=logs,
                **composite_failure_safety_fields(
                    material,
                    failed_stage="create_material",
                    mutation_relevant=True,
                    completed_mutation_stages=completed_mutation_stages,
                    failed_stage_mutation_performed=bool(
                        material.get("mutation_performed") is True
                        or material.get("partial_mutation") is True
                    ),
                ),
            )
        completed_mutation_stages.append("create_material")

        applied = None
        if target:
            applied = unreal_apply_material_to_mesh(target, material["asset_path"], material_slot)
            logs.append({"stage": "apply_material", "result": applied})
            if not applied.get("success"):
                return tool_error(
                    applied.get("message", "Material application failed"),
                    error_code="apply_material_failed",
                    stage="apply_material",
                    logs=logs,
                    **composite_failure_safety_fields(
                        applied,
                        failed_stage="apply_material",
                        mutation_relevant=True,
                        completed_mutation_stages=completed_mutation_stages,
                    ),
                )

        return {
            "success": True,
            "message": "Imported texture set and created material",
            "texture_asset_paths": imported["texture_asset_paths"],
            "material_asset_path": material.get("asset_path"),
            "applied": applied,
            "logs": logs,
        }

    @mcp.tool()
    def apply_material_to_mesh(
        ctx: Context,
        target: str,
        material_asset_path: str,
        material_slot: int = 0,
    ) -> Dict[str, Any]:
        """
        Apply a material to selected actors, a named actor, or a Static Mesh asset.

        Args:
            target: "selected", an actor label/path/name, or a Static Mesh asset path.
            material_asset_path: Material or Material Instance asset path.
            material_slot: Material slot index.
        """
        return unreal_apply_material_to_mesh(target, material_asset_path, material_slot)

    @mcp.tool()
    @_serialize_texture_composite
    def generate_and_apply_ai_texture(
        ctx: Context,
        target: str,
        prompt: str,
        output_name: str,
        unreal_folder: str = "/Game/AI_Generated",
        uv_channel: int = 0,
        size: str = "1024x1024",
    ) -> Dict[str, Any]:
        """
        Prepare the UV-guided texture workflow without calling API-key image generation.

        This returns a built-in image-generation handoff. After the PNG exists,
        call import_texture_to_unreal, create_material_instance_with_texture, and
        apply_material_to_mesh.

        Args:
            target: "selected", an actor label/path/name, or a Static Mesh asset path.
            prompt: Texture prompt.
            output_name: Texture asset/output basename.
            unreal_folder: Root destination folder in Unreal.
            uv_channel: UV channel index.
            size: Image size, for example 1024x1024.
        """
        logs = []
        completed_mutation_stages: list[str] = []

        resolved = resolve_target_static_mesh(target)
        logs.append({"stage": "resolve_target", "result": resolved})
        if not resolved.get("success"):
            return tool_error(
                "Failed to resolve target Static Mesh",
                error_code="resolve_target_failed",
                stage="resolve_target",
                logs=logs,
                **composite_failure_safety_fields(
                    resolved,
                    failed_stage="resolve_target",
                    mutation_relevant=False,
                ),
            )

        mesh_path = resolved["mesh_path"]
        local_output_dir = os.path.join(_default_output_dir(), sanitize_output_name(output_name))
        texture_generation = generate_texture_for_mesh_uv(
            ctx,
            mesh_path=mesh_path,
            prompt=prompt,
            uv_channel=uv_channel,
            output_name=output_name,
            output_dir=local_output_dir,
            size=size,
        )
        logs.append({"stage": "generate_texture", "result": texture_generation})
        if not texture_generation.get("success"):
            classified_read_response = texture_generation.get("read_unreal_response")
            if not isinstance(classified_read_response, dict):
                classified_read_response = texture_generation
            return tool_error(
                texture_generation.get(
                    "message",
                    "Codex built-in image generation is required before Unreal import/apply can continue",
                ),
                error_code=texture_generation.get("error_code", "builtin_image_generation_required"),
                stage="builtin_image_generation_required",
                logs=logs,
                next_steps=[
                    "Generate the PNG with Codex built-in image generation using the returned prompt and UV layout.",
                    "Save it to expected_output_path.",
                    "Then call generate_pbr_companion_textures and import_create_and_apply_texture_set.",
                ],
                prompt=texture_generation.get("prompt"),
                expected_output_path=texture_generation.get("expected_output_path"),
                reference_image_path=texture_generation.get("reference_image_path"),
                image_generation_route=texture_generation.get("image_generation_route"),
                api_route_disabled=texture_generation.get("api_route_disabled"),
                billing_route=texture_generation.get("billing_route"),
                **composite_failure_safety_fields(
                    classified_read_response,
                    failed_stage=str(texture_generation.get("failed_stage") or "generate_texture"),
                    mutation_relevant=False,
                ),
            )

        texture_folder = f"{unreal_folder.rstrip('/')}/Textures"
        material_folder = f"{unreal_folder.rstrip('/')}/Materials"
        texture_asset_name = sanitize_output_name(output_name)
        material_name = _material_name_from_output(output_name)

        imported_texture = unreal_import_texture_to_unreal(
            texture_generation["image_path"],
            texture_folder,
            texture_asset_name,
        )
        logs.append({"stage": "import_texture", "result": imported_texture})
        if not imported_texture.get("success"):
            return tool_error(
                "Texture import failed",
                error_code="import_texture_failed",
                stage="import_texture",
                logs=logs,
                **composite_failure_safety_fields(
                    imported_texture,
                    failed_stage="import_texture",
                    mutation_relevant=True,
                    completed_mutation_stages=completed_mutation_stages,
                    failed_stage_mutation_performed=bool(
                        imported_texture.get("mutation_performed") is True
                        or imported_texture.get("partial_mutation") is True
                    ),
                ),
            )
        completed_mutation_stages.append("import_texture")

        material = unreal_create_material_instance_with_texture(
            imported_texture["asset_path"],
            material_name,
            material_folder,
        )
        logs.append({"stage": "create_material", "result": material})
        if not material.get("success"):
            return tool_error(
                "Material creation failed",
                error_code="create_material_failed",
                stage="create_material",
                logs=logs,
                **composite_failure_safety_fields(
                    material,
                    failed_stage="create_material",
                    mutation_relevant=True,
                    completed_mutation_stages=completed_mutation_stages,
                    failed_stage_mutation_performed=bool(
                        material.get("mutation_performed") is True
                        or material.get("partial_mutation") is True
                    ),
                ),
            )
        completed_mutation_stages.append("create_material")

        applied = unreal_apply_material_to_mesh(target, material["asset_path"], 0)
        logs.append({"stage": "apply_material", "result": applied})
        if not applied.get("success"):
            return tool_error(
                "Material application failed",
                error_code="apply_material_failed",
                stage="apply_material",
                logs=logs,
                **composite_failure_safety_fields(
                    applied,
                    failed_stage="apply_material",
                    mutation_relevant=True,
                    completed_mutation_stages=completed_mutation_stages,
                ),
            )

        return {
            "success": True,
            "target": target,
            "mesh_path": mesh_path,
            "uv_layout_path": texture_generation.get("uv_layout_path"),
            "generated_image_path": texture_generation.get("image_path"),
            "texture_asset_path": imported_texture.get("asset_path"),
            "material_asset_path": material.get("asset_path"),
            "logs": logs,
            "note": "This pipeline imports and applies BaseColor. Use import_create_and_apply_texture_set when companion Normal/Roughness/AO/Metallic PNGs are available.",
        }

    logger.info("AI texture generation tools registered successfully")
