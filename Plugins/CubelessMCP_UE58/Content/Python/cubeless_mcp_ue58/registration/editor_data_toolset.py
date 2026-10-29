"""Editor-only data access and explicit refresh operations."""
import unreal
import toolset_registry
from cubeless_mcp_ue58.operations import editor_data


@unreal.uclass()
class CubelessEditorDataTools(unreal.ToolsetDefinition):
    """Explicit asset/component/world data without UI selection. JSON results preserve partial mutation, save and retry state. No runtime MCP helpers."""

    @toolset_registry.tool_call
    @staticmethod
    def inspect_blueprint_definition(asset_path: str) -> str:
        """Read Blueprint parent/generated classes, local/inherited declared variables, SCS templates and already-created Actor CDO components without opening/compiling an editor. Bounded64 ancestors/16384 entries; missing CDO is unavailable rather than no native components. Does not create a CDO itself. Loading may have engine effects; no explicit compile/save."""
        return editor_data.invoke("inspect_blueprint_definition", asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_editor_world_configuration() -> str:
        """Read current Editor world's streaming levels, actual/desired visibility, loaded levels, WorldPartition configuration and public WorldSettings bookmark slots. No cell loading, viewport/UI jump, memory/triangle estimate or RX plugin dependency."""
        return editor_data.invoke("inspect_editor_world_configuration")

    @toolset_registry.tool_call
    @staticmethod
    def inspect_static_mesh_metadata(asset_path: str) -> str:
        """Read StaticMesh render LOD UV counts, source-model build/reduction settings and socket names/transforms in one call. Max64 LODs/4096 sockets. Refuse currently compiling meshes without waiting; missing render/source LODs remain distinct. No compile/save."""
        return editor_data.invoke("inspect_static_mesh_metadata", asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_component_authoring_data(component_path: str) -> str:
        """Read exact loaded StaticMesh component LOD paint/override-color counts, SkeletalMesh SingleAnimationPlayData defaults or ExponentialHeightFog volumetric settings. Template reads allowed; counts do not prove color contents and saved animation defaults are not live playback. No actor-label fallback, writes or save."""
        return editor_data.invoke("inspect_component_authoring_data", component_path, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_widget_animation_data(asset_path: str) -> str:
        """Read source-local WidgetBlueprint tree and animations including display/tick frame rates and each2D-transform section bitmask. Works with custom WidgetBlueprint parents without creating runtime widgets. Bounded tree/sections16384 and4096 animations. No compile/save."""
        return editor_data.invoke("inspect_widget_animation_data", asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_pcg_extraction_settings(settings_object_path: str) -> str:
        """Read exact GetActorProperty or Polygon2DOperation PCG settings. Returns current extractor behavior enums and deprecated5.8 force-extraction flag, or Operation/MetadataMode rather than nonexistent mode. Resolve settings path with existing PCG graph inspection. Requires enabled PCG; no writes."""
        return editor_data.invoke("inspect_pcg_extraction_settings", settings_object_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_editor_performance_settings() -> str:
        """Read effective EditorPerformanceSettings config properties as bounded exported text. Does not expose GEngine or change config, telemetry, accounts or performance options."""
        return editor_data.invoke("inspect_editor_performance_settings")

    @toolset_registry.tool_call
    @staticmethod
    def rerun_actor_construction(actor_path: str, execute: bool = False) -> str:
        """Rerun construction of exact live Editor-world actor only when execute=true. Default is admission dry-run. ChildActors redirecting to parents, suspended/global reconstruction, compile-error/being-compiled hierarchies and locked levels are refused before mutation. SDK return does not verify arbitrary construction side effects. Construction can replace components or execute user callbacks; result reports actor/context survival, dirty state and actual partial/unknown state. Re-resolve component references afterward. No Blueprint compiler/save, rollback or uncertain replay."""
        return editor_data.invoke("rerun_actor_construction", actor_path, (execute,), mutation=execute, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def refresh_component_render_state(component_path: str, execute: bool = False) -> str:
        """Queue MarkRenderStateDirty on exact registered Editor-world component, including Decal/PostProcess. Default dry-run; execute=true checks callback survival and owner/world. Reports queued state, never render-thread/GPU completion. Does not save or intentionally dirty authored packages."""
        return editor_data.invoke("refresh_component_render_state", component_path, (execute,), mutation=execute, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def set_asset_package_dirty(asset_path: str, dirty: bool, allow_clear: bool = False) -> str:
        """Explicitly set one /Game non-map asset package dirty flag. Clearing requires allow_clear=true and can suppress save prompts for unsaved edits; it NEVER saves or reverts data. No actor/world/config/Engine target. Callback changes and actual observed dirty state are reported; no retry/rollback."""
        return editor_data.invoke("set_asset_package_dirty", asset_path, (dirty, allow_clear), mutation=True)

    @toolset_registry.tool_call
    @staticmethod
    def spawn_editor_actor_deferred(class_path: str, actor_name: str, transform_json: str = "{}", execute: bool = False) -> str:
        """Create and FinishSpawning one explicitly named actor in current Editor persistent level with construction deferred; never a gameplay/runtime spawn API. Default dry-run. transform_json has location[x,y,z], rotation[pitch,yaw,roll], scale[x,y,z], defaults0/0/1. Refuse reserved/duplicate names and invalid/nonplaceable classes. Callbacks may modify/delete the actor; partial actors are retained and reported, no automatic retry/cleanup/save or generic crash-prevention guarantee."""
        return editor_data.spawn_editor_actor_deferred(class_path, actor_name, transform_json, execute)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_particle_positions(component_path: str, emitter_index: int = -1, max_particles: int = 1024) -> str:
        """Read last completed CPU particle positions of an exact loaded registered NiagaraComponent in current Editor/PIE world. Requires Niagara and an already-solo, already-paused, already-ticked system; never forces solo/tick/wait or GPU readback. Emitter index-1 means all. Max1..65536 total world-space LWC-converted samples; GPU/non-solo/stateless/unavailable data differs from observed empty CPU buffer. Truncation/mixed support is partial, never complete. No writes/save."""
        return editor_data.invoke("inspect_niagara_particle_positions", args=(component_path, emitter_index, max_particles))
