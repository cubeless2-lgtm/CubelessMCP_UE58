"""Instance vertex color tools on the official Toolset Registry."""
import unreal
import toolset_registry
from cubeless_mcp_ue58.operations import instance_vertex_colors as domain


@unreal.uclass()
class CubelessInstanceVertexColorTools(unreal.ToolsetDefinition):
    """Placed StaticMeshComponent render-LOD color overrides. Requires MeshPainting; Editor only. No source mesh editing, per-ISM/HISM coloring or implicit save."""

    @toolset_registry.tool_call
    @staticmethod
    def inspect_instance_vertex_colors(component_path: str, lod_index: int = 0,
            start_index: int = 0, count: int = 256, include_positions: bool = False) -> str:
        """Read exact loaded Editor component's effective RGBA8 colors and optional mesh-local positions. Render vertex indices include seam duplicates. Page count0..4096;0 reads metadata. Reports component override vs source mesh/default white. CPU data required, no loading/compile/save."""
        return domain.invoke("inspect_instance_vertex_colors", component_path,
            lod_index, start_index=start_index, count=count,
            include_positions=include_positions)

    @toolset_registry.tool_call
    @staticmethod
    def set_instance_vertex_colors(component_path: str, colors_json: str,
            lod_index: int = 0, execute: bool = False, allow_replace: bool = False,
            save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Set one placed StaticMeshComponent render LOD from JSON [[r,g,b,a],...], exactly one RGBA8 entry per render vertex. No gamma conversion. Default preflight; execute=true writes; existing overrides need allow_replace=true (same-value no-op accepted). Refuse active Nanite proxy/disabled vertex painting, templates and ISM/HISM. Source mesh and other LOD colors preserved. MeshPainting required. Explicit save targets owning package (may include other objects); preexisting dirty package needs allow_save_dirty_package=true. No-op never saves. CPU readback/cache verified; render update queued, not GPU completion. Partial/unknown failure is not rolled back or retried. Max1048576 colors/snapshot."""
        return domain.invoke("set_instance_vertex_colors", component_path,
            lod_index, colors_json=colors_json, execute=execute,
            allow_replace=allow_replace, save=save,
            allow_save_dirty_package=allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def clear_instance_vertex_colors(component_path: str, lod_index: int = 0,
            execute: bool = False, allow_remove: bool = False, save: bool = False,
            allow_save_dirty_package: bool = False) -> str:
        """Remove only selected render-LOD component override/cache, restoring mesh/default effective colors. Default preflight; existing data requires allow_remove=true. No-op does not save. Same exact-target/package-save and partial-state policy as set. Does not remove source mesh colors or other LOD data. Nanite policy is not changed."""
        return domain.invoke("clear_instance_vertex_colors", component_path,
            lod_index, execute=execute, allow_replace=allow_remove, save=save,
            allow_save_dirty_package=allow_save_dirty_package)
