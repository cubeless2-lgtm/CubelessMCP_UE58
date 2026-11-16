"""Missing native authoring access on the official Toolset Registry."""
import unreal
import toolset_registry
from cubeless_mcp_ue58.operations import authoring_access as domain


@unreal.uclass()
class CubelessAuthoringAccessTools(unreal.ToolsetDefinition):
    """UE5.8 Editor-only authoring access. Explicit object paths; no selection, implicit save or uncertain replay. Optimus requires DeformerGraph."""

    @toolset_registry.tool_call
    @staticmethod
    def create_editor_preview_widget(owner_path: str, widget_class_path: str, widget_name: str, execute: bool = False) -> str:
        """Create a UserWidget only in an already-loaded transient WidgetTree or current Editor/null-world UserWidget's transient tree. Concrete class must already be loaded and pass runtime-reference independence audit. Default preflight; explicit execute; max64 retained instances and256 existing owner widgets. Initialize callbacks may execute and failure may leave a created object. No PIE/game world, viewport insertion, attachment, asset save or rollback/replay. Result path is retained by this Python adapter until release/reload/shutdown."""
        return domain.create_preview_widget(owner_path,widget_class_path,widget_name,execute)

    @toolset_registry.tool_call
    @staticmethod
    def release_editor_preview_widget(object_path: str, execute: bool = False) -> str:
        """Release only this adapter's retained reference to a previously created preview widget. Does not remove it from a panel or destroy objects held elsewhere; no save. Default preflight."""
        return domain.release_preview_widget(object_path,execute)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_editor_viewport_configuration() -> str:
        """Read Editor viewport and LocalPlayer config CDO aspect-axis constraints. No settings write, rendering toggle or resize."""
        return domain.global_inspection('inspect_editor_viewport_configuration')

    @toolset_registry.tool_call
    @staticmethod
    def inspect_rigvm_template_catalog(name_filter: str = '*', offset: int = 0, limit: int = 128) -> str:
        """Page actual registered RigVM template notations, '*' for all, otherwise case-insensitive substring<=256 chars. Limit1..256. Requires loaded RigVM; no registry population, node creation or compile proof. Use inspect_editor_authoring_details on a RigVMGraph to diagnose existing deprecated units before compilation."""
        return domain.global_inspection('inspect_rig_vm_template_catalog',(name_filter,offset,limit))

    @toolset_registry.tool_call
    @staticmethod
    def deactivate_editor_niagara_component(component_path: str, execute: bool = False) -> str:
        """DeactivateImmediate on an exact registered actor-owned component in the current Editor or EditorPreview world; no PIE/SIE/CDO. Default preflight, explicit execution, authored input, owner/world/registration guards, no save/replay. Engine callbacks may run; failure retains unknown/partial state. Not GPU completion. Requires Niagara loaded."""
        return domain.invoke('deactivate_editor_niagara_component',component_path,(execute,),mutation=execute,loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_editor_authoring_details(object_path: str) -> str:
        """Read a fixed stored-data profile: AnimBlueprint TargetSkeleton/ParentAssetOverrides, Landscape component sizes, NamedReroute declaration, StaticMesh platform minimum LOD, MeshComponent override materials, Font default typeface names, Optimus variables or Editor WidgetTree members. Reflected property values are native export-text strings. Max4096 entries, property text1M chars. No compile/save/widget construction or runtime execution. Requires relevant feature plugins loaded; asset path resolution may load."""
        return domain.invoke('inspect_editor_authoring_details', object_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_embedded_rich_curves(object_path: str) -> str:
        """Read direct reflected FRichCurve properties with full keys/tangents/default/infinity, including Niagara float/vector curve data interfaces. Max32 curves/16384 finite keys. No recursive traversal, curve mutation or save. Asset resolution may load; use an exact loaded subobject path for Niagara internals."""
        return domain.invoke('inspect_embedded_rich_curves', object_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_rigvm_library_functions(object_path: str) -> str:
        """Read an already-loaded RigVMFunctionLibrary's function paths and native public flags. Max4096 functions. Pass the library object path from the rig, not the graph or Blueprint path. No construction, graph mutation, compile or save."""
        return domain.invoke('inspect_rig_vm_library_functions', object_path, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_editor_actor_metadata(actor_path: str) -> str:
        """Read an exact already-loaded Actor's tick capability, Editor/game hidden flags and direct GroupActor member paths (max4096). No recursive subgroup expansion, actor load, tick or mutation. No PIE/SIE."""
        return domain.invoke('inspect_editor_actor_metadata', actor_path, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_component_registration_state(component_path: str) -> str:
        """Read an exact already-loaded ActorComponent's registration flag, owner/world paths and template flag. Does not register, tick or instantiate a component. No PIE/SIE."""
        return domain.invoke('inspect_component_registration_state', component_path, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_object_class_hierarchy(object_path: str) -> str:
        """Read an already-loaded object's exact class and superclass paths, or an already-loaded UClass itself and its superclasses. Max128 ready classes. No class/asset load, CDO creation or Blueprint compile."""
        return domain.invoke('inspect_object_class_hierarchy', object_path, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_preview_mesh_reference(asset_path: str) -> str:
        """Read the stored Skeleton/AnimationAsset preview mesh soft path without loading that mesh or searching for a fallback. Distinguish assigned and already loaded; source asset resolution may load."""
        return domain.invoke('inspect_preview_mesh_reference', asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_skeletal_mesh_authoring_data(asset_path: str) -> str:
        """Read ready SkeletalMesh LOD info as native authored text, raw bone names/parent indices, mesh-only sockets and post-process LOD threshold. Max32 LODs/2MiB LOD text,16384 raw bones,4096 sockets. No build/wait/save or evaluated-pose claim."""
        return domain.invoke('inspect_skeletal_mesh_authoring_data', asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_rigvm_unit_catalog(name_filter: str = '*', include_deprecated: bool = False, offset: int = 0, limit: int = 128) -> str:
        """Page loaded reflected RigVM unit and dispatch-factory declarations, including RigVMFunction structs. Filter '*' means all names; otherwise a case-insensitive substring<=256 chars. Deprecated structs excluded by default or explicitly marked when included. Limit1..256. Requires ControlRig/RigVM loaded. No module load/node creation or template-notation/constructibility proof. Prefer If dispatch to deprecated SelectBool: DISPATCH_RigVMDispatch_If(in Condition,in True,in False,out Result)."""
        return domain.rigvm_unit_catalog(name_filter, include_deprecated, offset, limit)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_asset_editing_state(asset_path: str) -> str:
        """Observe exact loaded asset package dirty state and all open asset editors, including other open objects in its package. Does not open/focus/close/save editors. Observation is not an exclusive lock or overwrite permission."""
        return domain.invoke('inspect_asset_editing_state', asset_path, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_float_curve(asset_path: str) -> str:
        """Read CurveFloat complete rich keys, tangents/weights, default value and infinity modes; max4096 keys. No sampling approximation or write."""
        return domain.invoke('inspect_float_curve', asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def set_float_curve_keys(asset_path: str, keys_json: str, allow_replace: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Replace standalone /Game CurveFloat keys from JSON [{time,value,interp_mode?,tangent_mode?,tangent_weight_mode?,arrive_tangent?,leave_tangent?,arrive_tangent_weight?,leave_tangent_weight?}]. Max4096 increasing unique float times. Defaults linear interp(0), auto tangent(0), no weights(0); modes interp/tangent0..2, weight0..3. Explicit tangents preserved, no auto tangent recompute. Existing keys/clear need allow_replace. Default preflight; no-op never saves. Default/infinity/event/authored settings preserved across callbacks. Exact package save only; dirty package requires additional approval. No rollback/retry."""
        return domain.invoke('set_float_curve_keys', asset_path, (keys_json, allow_replace, execute, save, allow_save_dirty_package), mutation=execute)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_blueprint_timelines(asset_path: str) -> str:
        """Read source-local Blueprint Timeline templates, lengths and float/vector/color/event tracks with exact curve references/external flags. Max1024 timelines and4096 tracks each. No compile/editor open/save; does not merge inherited templates."""
        return domain.invoke('inspect_blueprint_timelines', asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_asset_import_sources(asset_path: str) -> str:
        """Read AssetImportData source filenames, resolved paths, timestamps and recorded import hashes, without opening source files/reimporting. No import metadata returns has_import_data=false, not a guessed source. Max4096 files per import-data object."""
        return domain.invoke('inspect_asset_import_sources', asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_specialized_anim_graph(asset_path: str) -> str:
        """Read Blueprint or exact loaded EdGraph source nodes/comments/positions and stored compiler-message flag/type/text, ControlRig actual class accessor and PoseDriver DrivenName targets. Max4096 graphs/16384 nodes/4096 targets. Diagnostics reflect the last stored compiler result, not a fresh compile. Existing generic graph tools remain the editing route; no compile/save."""
        return domain.invoke('inspect_specialized_anim_graph', asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_animation_sampling_rate(asset_path: str) -> str:
        """Read an AnimSequence's native target sampling frame rate as numerator/denominator. This is not compressed-key count or playback speed. No edit, compression or save; asset resolution may load the sequence."""
        return domain.invoke('inspect_animation_native_data', asset_path, expected_classes=('AnimSequence',))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_pose_asset_additive(asset_path: str) -> str:
        """Read PoseAsset.IsValidAdditive. This engine flag is not proof of pose quality or compatibility. No mutation."""
        return domain.invoke('inspect_animation_native_data', asset_path, expected_classes=('PoseAsset',))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_camera_animation_binding(asset_path: str) -> str:
        """Read Camera/TemplateSequence BoundActorClass soft path without loading its class. Requires TemplateSequence for such assets. Does not change binding, evaluate or save."""
        return domain.invoke('inspect_animation_native_data', asset_path, expected_classes=('CameraAnimationSequence', 'TemplateSequence'))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_texture_source_pixels(asset_path: str, mip_index: int = 0, start_pixel: int = 0, count: int = 256) -> str:
        """Read original Texture2D source texel bytes in native source format, one block/layer, max256MiB mip. Page count0..4096;0 reads metadata. Returns format/bytes_per_pixel; never assumes RGBA8 or applies gamma. Source can be absent. No GPU readback/resource refresh/save."""
        return domain.invoke('inspect_texture_source_pixels', asset_path, (mip_index, start_pixel, count))

    @toolset_registry.tool_call
    @staticmethod
    def refresh_texture_editor_data(asset_path: str, execute: bool = False) -> str:
        """Dispatch PostEditChange for standalone /Game Texture2D after authored property edits; default preflight. Preserve authored properties/source identity across callbacks. Reports queued derived/resource work, never GPU completion. No save or automatic retry."""
        return domain.invoke('refresh_texture_editor_data', asset_path, (execute,), mutation=execute)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_skeletal_component_state(component_path: str) -> str:
        """Read exact loaded SkeletalMeshComponent effective PhysicsAsset and disable-post-process flag. No physics rebuild, tick or save."""
        return domain.invoke('inspect_skeletal_component_state', component_path, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def set_skeletal_post_process_disabled(component_path: str, disabled: bool, execute: bool = False) -> str:
        """Toggle PostProcess AnimBP only on exact registered placed Editor component; explicit execution, no CDO/PIE/SIE. Setter can reinitialize animation instances; re-resolve afterward. Preserve mesh/owner/other authored properties. No compile/save/rollback/retry."""
        return domain.invoke('set_skeletal_post_process_disabled', component_path, (disabled, execute), mutation=execute, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def tick_skeletal_animation(component_path: str, delta_seconds: float = 0.03333333333333333, refresh_bone_transforms: bool = True, execute: bool = False) -> str:
        """TickAnimation(delta0..1,false), optionally RefreshBoneTransforms, on an exact registered current Editor component, or DebugSkelMeshComponent owned by a live actor in an EditorPreview world. Explicit execution; no PIE/SIE/templates. AnimBP/Notify callbacks may run: inspect actual state after failure and do not replay. Not game-world advancement, physics simulation, GPU completion or asset save."""
        return domain.invoke('tick_skeletal_animation', component_path, (delta_seconds, refresh_bone_transforms, execute), mutation=execute, loaded_only=True)

    @toolset_registry.tool_call
    @staticmethod
    def remove_skeleton_bones(asset_path: str, bone_names_json: str, remove_children: bool = False, allow_remove: bool = False, allow_dependent_updates: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Remove named non-root raw bones from standalone /Game Skeleton. Default preflight returns exact removal list and loaded animations subject to engine validation. Parents need all descendants or remove_children=true; existing engine bool alone does not expand descendants. Max16384 raw bones/4096 requested names. Execution requires BOTH allow_remove and allow_dependent_updates: engine refreshes hierarchy GUID/remapping/retarget metadata/blend profiles, may remove invalid virtual bones and validates loaded animations. Mesh/animation source data are not broadly rewritten or auto-saved. Remaining names/parents/ref transforms checked after callbacks/save. Save targets only Skeleton package, dirty-save approval separate. Partial/unknown failure retained, no rollback/retry."""
        return domain.invoke('remove_skeleton_bones', asset_path, (bone_names_json, remove_children, allow_remove, allow_dependent_updates, execute, save, allow_save_dirty_package), mutation=execute)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_optimus_deformer(asset_path: str) -> str:
        """Read DeformerGraph Optimus graph compile status/graph inventory. Status is not asynchronous shader or GPU/resource readiness. Requires enabled/loaded DeformerGraph. No compile/save."""
        return domain.invoke('inspect_optimus_deformer', asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def compile_optimus_deformer(asset_path: str, execute: bool = False) -> str:
        """Compile Optimus authored graphs via public Compile; default preflight. Capture synchronous diagnostics, accept CompiledWithWarnings, verify authored inputs and object identity after callbacks. CompileEnd precedes async shader work: graph_compile_succeeded does NOT mean full_compile_complete/shader_completion_verified. Poll inspect for graph status; this tool does not certify eventual shaders. May clear/rebuild generated data even on failure; never auto-save/rollback/retry. Requires DeformerGraph."""
        return domain.invoke('compile_optimus_deformer', asset_path, (execute,), mutation=execute)
