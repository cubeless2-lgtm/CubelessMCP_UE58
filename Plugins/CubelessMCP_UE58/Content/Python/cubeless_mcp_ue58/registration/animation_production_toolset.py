"""Dedicated missing animation-production contracts for official Toolset Registry."""
import unreal
import toolset_registry
from cubeless_mcp_ue58.operations import animation_production as domain

@unreal.uclass()
class CubelessAnimationProductionTools(unreal.ToolsetDefinition):
    """Editor-only AnimGraph bindings, StateMachine editing, PoseSearch and Chooser production. Required enabled plugins: PoseSearch, Chooser. Explicit identifiers, execute=false/save=false for edits, separate async index request/status. Owned target only, serialized journal, callback identity/authored input preservation and direct single-package save. Chooser compile diagnostics gate save; internal loads retain the session Montage save guard and report pre-load uncertainty. No rollback/retry, runtime MCP dependency, independent server or external pipeline. Chooser supports class/struct contexts, dot property paths, Bool/FloatRange/byte Enum/GameplayTagContainer conditions and acyclic NestedChooser/EvaluateChooser results. Function/container-index bindings and proxy results are excluded. Index DDC build is not saved-asset/runtime packaging proof."""

    @toolset_registry.tool_call
    @staticmethod
    def create_pose_search_schema(asset_path: str, skeleton_path: str, execute: bool = False, save: bool = False) -> str:
        'Create a fresh Schema with a skeleton and no channels; add channels explicitly. No overwrite; execute=false preflight, save=false leaves new package dirty.'
        return domain.run('create','pose_search_schema',asset_path,reference_path=skeleton_path,execute=execute,save=save)

    @toolset_registry.tool_call
    @staticmethod
    def create_pose_search_database(asset_path: str, schema_path: str, execute: bool = False, save: bool = False) -> str:
        'Create a fresh Database referencing an existing Schema. No overwrite; execute=false preflight, save=false leaves new package dirty.'
        return domain.run('create','pose_search_database',asset_path,reference_path=schema_path,execute=execute,save=save)

    @toolset_registry.tool_call
    @staticmethod
    def create_chooser_table(asset_path: str, result_class_path: str, execute: bool = False, save: bool = False, parent_chooser_path: str | None = None, allow_save_dirty_package: bool = False) -> str:
        """Create fresh ObjectResult Chooser. With parent_chooser_path, asset_path is a fresh simple child name; result_class_path must be empty and root context/output are inherited. No replacement. Nested save covers the root package with explicit dirty approval."""
        if parent_chooser_path:
            if result_class_path:return domain.reject('create_nested_chooser','Nested output class is inherited; result_class_path must be empty')
            return domain.run('edit','create_nested_chooser',parent_chooser_path,{'value':asset_path},execute=execute,save=save,allow_dirty=allow_save_dirty_package)
        return domain.run('create','chooser_table',asset_path,reference_path=result_class_path,execute=execute,save=save)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_pose_search_schema(asset_path: str) -> str:
        'Read authored properties and collection/reference summary. Explicit resolution may load; input load uncertainty is reported.'
        return domain.run('inspect','inspect_pose_search_schema',asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_pose_search_database(asset_path: str) -> str:
        'Read authored properties and collection/reference summary. Explicit resolution may load; input load uncertainty is reported.'
        return domain.run('inspect','inspect_pose_search_database',asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_chooser_table(asset_path: str) -> str:
        'Read authored properties and collection/reference summary. Explicit resolution may load; input load uncertainty is reported.'
        return domain.run('inspect','inspect_chooser_table',asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def set_pose_search_sample_rate(asset_path: str, rate: int, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Set Schema sample rate1..240. execute=false validates only; save=false leaves dirty. Target package only; no atomic rollback/retry.'
        return domain.run('edit','set_pose_search_sample_rate',asset_path,{'rate':rate},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=False)

    @toolset_registry.tool_call
    @staticmethod
    def add_pose_search_channel(asset_path: str, channel_type: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Append owned Position/Velocity/Heading/Trajectory channel. Limits128. No replacement. execute=false validates only; save=false leaves dirty. Target package only; no atomic rollback/retry.'
        return domain.run('edit','add_pose_search_channel',asset_path,{'value':channel_type},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=False)

    @toolset_registry.tool_call
    @staticmethod
    def remove_pose_search_channel(asset_path: str, channel_index: int, allow_remove: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Remove one authored channel; explicit approval required. execute=false validates only; save=false leaves dirty. Target package only; no atomic rollback/retry.'
        return domain.run('edit','remove_pose_search_channel',asset_path,{'index':channel_index},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_remove)

    @toolset_registry.tool_call
    @staticmethod
    def add_pose_search_animation(asset_path: str, animation_path: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Append a skeleton-compatible animation entry; existing duplicate refused;4096 max. execute=false validates only; save=false leaves dirty. Target package only; no atomic rollback/retry.'
        return domain.run('edit','add_pose_search_animation',asset_path,{'value':animation_path},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=False)

    @toolset_registry.tool_call
    @staticmethod
    def remove_pose_search_animation(asset_path: str, animation_index: int, allow_remove: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Remove one Database entry with explicit approval. execute=false validates only; save=false leaves dirty. Target package only; no atomic rollback/retry.'
        return domain.run('edit','remove_pose_search_animation',asset_path,{'index':animation_index},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_remove)

    @toolset_registry.tool_call
    @staticmethod
    def set_pose_search_search_settings(asset_path: str, mode: str, neighbors: int = 200, principal_components: int = 8, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Set BruteForce/PCAKDTree/VPTree search settings; valid bounded integer settings required. execute=false validates only; save=false leaves dirty. Target package only; no atomic rollback/retry.'
        return domain.run('edit','set_pose_search_search_settings',asset_path,{'value':mode,'neighbors':neighbors,'principal_components':principal_components},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=False)

    @toolset_registry.tool_call
    @staticmethod
    def request_pose_search_index(asset_path: str) -> str:
        'Start a nonempty Database index request without game-thread WaitForCompletion. Returns job_id and in_progress/complete/failed; accepted is not completed. Does not save.'
        return domain.run('index','request_pose_search_index',asset_path,execute=True)

    @toolset_registry.tool_call
    @staticmethod
    def get_pose_search_index_status(asset_path: str, job_id: str) -> str:
        'Continue the exact owned engine index request; engine ContinueRequest can ensure its task exists. Success completion requires engine Success and positive pose count. Database, Schema, animation/model/Skeleton inputs are frozen; modification generations invalidate the original job instead of accepting a replacement build. Input invalidation retains a conservative unresolved edit guard. No cancellation/restart promised.'
        return domain.run('index','get_pose_search_index_status',asset_path,job_id=job_id)

    @toolset_registry.tool_call
    @staticmethod
    def set_chooser_context(asset_path: str, context_index: int, class_path: str, allow_replace: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Append or explicitly replace a root class or ScriptStruct context using the legacy class_path argument. Nested tables inherit root contexts. Replacing requires allow_replace; compile diagnostics gate optional target-package save.'
        return domain.run('edit','set_chooser_context',asset_path,{'index':context_index,'value':class_path},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def add_chooser_column(asset_path: str, column_type: str, property_name: str, context_index: int = 0, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False, options_json: str = "{}") -> str:
        """Append Bool/FloatRange/Enum/GameplayTag condition with a reflected dot property path. Enum requires a byte enum; GameplayTag requires GameplayTagContainer. Class/struct and nested struct/object properties supported; no function/array-index bindings. Optional GameplayTag options: match Any|All, direction RowValueInInput|InputInRowValue, exact/invert booleans. Engine defaults are retained. 128 columns max; no implicit save."""
        try:
            import json
            options=json.loads(options_json)
            if not isinstance(options,dict):raise ValueError('Options must be a JSON object')
        except Exception as error:return domain.reject('add_chooser_column',error)
        return domain.run('edit','add_chooser_column',asset_path,{'value':column_type,'property':property_name,'context':context_index,'options':options},execute=execute,save=save,allow_dirty=allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def add_chooser_row(asset_path: str, result_asset_path: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False, result_kind: str = "Asset") -> str:
        """Assign Asset (default), same-root embedded NestedChooser or top-level EvaluateChooser result. Chooser links require matching context/output and an acyclic graph. Existing result replacement needs allow_replace; other rows/references preserved. No automatic save/retry."""
        return domain.run('edit','add_chooser_row',asset_path,{'value':result_asset_path,'result_kind':result_kind},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=False)

    @toolset_registry.tool_call
    @staticmethod
    def set_chooser_result(asset_path: str, row_index: int, result_asset_path: str, allow_replace: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False, result_kind: str = "Asset") -> str:
        """Assign Asset (default), same-root embedded NestedChooser or top-level EvaluateChooser result. Chooser links require matching context/output and an acyclic graph. Existing result replacement needs allow_replace; other rows/references preserved. No automatic save/retry."""
        return domain.run('edit','set_chooser_result',asset_path,{'index':row_index,'value':result_asset_path,'result_kind':result_kind},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def set_chooser_fallback(asset_path: str, result_asset_path: str, allow_replace: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False, result_kind: str = "Asset") -> str:
        """Assign Asset (default), same-root embedded NestedChooser or top-level EvaluateChooser result. Chooser links require matching context/output and an acyclic graph. Existing result replacement needs allow_replace; other rows/references preserved. No automatic save/retry."""
        return domain.run('edit','set_chooser_fallback',asset_path,{'value':result_asset_path,'result_kind':result_kind},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def set_chooser_bool_cell(asset_path: str, column_index: int, row_index: int, value: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Set Bool cell to True/False/Any. Exact existing row/column required. execute=false validates only; save=false leaves dirty. Target package only; no atomic rollback/retry.'
        return domain.run('edit','set_chooser_cell',asset_path,{'index':column_index,'row':row_index,'value':value},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=False)

    @toolset_registry.tool_call
    @staticmethod
    def set_chooser_float_cell(asset_path: str, column_index: int, row_index: int, minimum: float, maximum: float, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Set finite FloatRange inclusive min<=max; bound endpoints replace infinite flags explicitly. execute=false validates only; save=false leaves dirty. Target package only; no atomic rollback/retry.'
        return domain.run('edit','set_chooser_cell',asset_path,{'index':column_index,'row':row_index,'min':minimum,'max':maximum},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=False)

    @toolset_registry.tool_call
    @staticmethod
    def remove_chooser_row(asset_path: str, row_index: int, allow_remove: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Remove row and matching condition cells together with explicit approval. execute=false validates only; save=false leaves dirty. Target package only; no atomic rollback/retry.'
        return domain.run('edit','remove_chooser_row',asset_path,{'index':row_index},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_remove)

    @toolset_registry.tool_call
    @staticmethod
    def remove_chooser_column(asset_path: str, column_index: int, allow_remove: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        'Remove one condition column with explicit approval. execute=false validates only; save=false leaves dirty. Target package only; no atomic rollback/retry.'
        return domain.run('edit','remove_chooser_column',asset_path,{'index':column_index},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_remove)

    @toolset_registry.tool_call
    @staticmethod
    def set_pose_search_channel_settings(asset_path: str, channel_index: int, settings_json: str, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Edit owned channel scalar settings: Weight, SampleTimeOffset, OriginTimeOffset, bDefaultWithRootBone, bNormalizeDisplacement, SampleRole, OriginRole where present. Existing channel/class/role preserved. Finite values only."""
        import json
        try:
            settings=json.loads(settings_json)
            if not isinstance(settings,dict):raise ValueError('Object required')
        except Exception as error:return domain.reject('set_pose_search_channel_settings',error)
        return domain.run('edit','set_pose_search_channel_settings',asset_path,{'index':channel_index,'settings':settings},execute=execute,save=save,allow_dirty=allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def set_pose_search_animation_settings(asset_path: str, animation_index: int, enabled: bool = True, mirror_option: str = 'UnmirroredOnly', sampling_start: float = 0.0, sampling_end: float = 0.0, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Edit one entry enable/mirror/range; [0,0] means full asset range. Mirroring requires compatible schema mirror data. Other entry/reference data preserved."""
        return domain.run('edit','set_pose_search_animation_settings',asset_path,{'index':animation_index,'enabled':enabled,'value':mirror_option,'start':sampling_start,'end':sampling_end},execute=execute,save=save,allow_dirty=allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def set_pose_search_database_schema(asset_path: str, schema_path: str, allow_replace: bool = False, execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Explicitly replace Schema only when every current animation entry is compatible. Refuses active owned index conflicts."""
        return domain.run('edit','set_pose_search_database_schema',asset_path,{'value':schema_path},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def bind_cached_pose(asset_path: str, graph_name: str, use_node_id: str, save_node_id: str, allow_replace: bool = False, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Rebind an existing UseCachedPose by GUID to exact SaveCachedPose in the same AnimGraph/layer scope. Preserve other nodes, authored defaults and semantic links; replacement explicit. Compile/save optional, single target package only; failed mutations must be inspected, never replayed automatically."""
        return domain.run('bind','bind_cached_pose',asset_path,{'graph':graph_name,'node_id':use_node_id,'source':save_node_id,'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def bind_linked_anim_layer(asset_path: str, graph_name: str, node_id: str, layer_name: str, allow_replace: bool = False, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Rebind an existing LinkedAnimLayer to an already implemented interface/self layer with the same compiled parameter signature. Layer, Interface, GUID and member reference are set together, pins reconstructed and existing semantic links checked. Preserves compatible existing instance class; self layer clears overrides. Changed port signatures may fail preservation after partial edits; no rollback. Does not create layer interfaces/functions."""
        return domain.run('bind','bind_linked_anim_layer',asset_path,{'graph':graph_name,'node_id':node_id,'source':layer_name,'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def bind_motion_matching_database(asset_path: str, graph_name: str, node_id: str, database_path: str, allow_replace: bool = False, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Bind exact MotionMatching node to Database with matching default-role Skeleton. Preserve other node settings/pins/graph. Explicit replacement, compile and target-only save. Required plugin: PoseSearch. Node construction and general wiring reuse official BlueprintTools."""
        return domain.run('bind','bind_motion_matching_database',asset_path,{'graph':graph_name,'node_id':node_id,'source':database_path,'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def evaluate_chooser_table(asset_path: str, context_object_path: str | None = None, context_struct_json: str | None = None, contexts_json: str | None = None) -> str:
        """Evaluate property-only Bool/FloatRange/Enum/GameplayTag conditions and acyclic Asset/NestedChooser/EvaluateChooser results. Supply one legacy context or contexts_json: a JSON array in context-index order, e.g. [{"object_path":"/Game/A.A"},{"struct":{"Added":{"tags":["Registered.Tag"]}}}]. Provide every slot (max 64); do not mix forms. Scratch struct fields support scalars, nested structs and GameplayTagContainer (no object refs or other containers). Returns selected asset or has_result=false; no asset save."""
        return domain.run(
            'evaluate', 'evaluate_chooser_table', asset_path,
            {'context_json': context_struct_json or '', 'contexts_json': contexts_json},
            reference_path=context_object_path or '',
        )

    @toolset_registry.tool_call
    @staticmethod
    def bind_motion_matching_chooser(asset_path: str, graph_name: str, node_id: str, chooser_path: str, function_name: str, allow_replace: bool = False, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Create a fresh thread-safe OnUpdate function evaluating a single AnimInstance class-context Chooser into matching-skeleton PoseSearch Databases. Wire standard engine conversion, validity branch and SetDatabasesToSearch; bind exact MotionMatching GUID. Existing function bodies are preserved, OnUpdate replacement explicit. Partial function may remain on failure: inspect, never replay. Compile/save explicit and target-package only. Requires PoseSearch and Chooser; generated content calls only their standard runtime APIs."""
        return domain.run('chooser_bind','bind_motion_matching_chooser',asset_path,{'graph':graph_name,'node_id':node_id,'chooser':chooser_path,'function':function_name,'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def create_anim_layer(asset_path: str, layer_name: str, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Create a fresh self animation-layer graph with the standard pose result in an owning root AnimBlueprint. Existing graphs/functions are never replaced. Compile/save explicit; single target package only; preserve Skeleton and other graph content. Interface creation and typed input signatures are outside this operation. Author its body and instantiate a LinkedAnimLayer through existing graph tools."""
        return domain.run('layer','create_anim_layer',asset_path,{'name':layer_name,'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_animation_state_graph(asset_path: str, machine_graph_name: str) -> str:
        """Inspect an exact, uniquely named nested StateMachine graph, State/Transition GUIDs, endpoints and bound graph paths. No graph creation/compile/save; resolves the Blueprint explicitly. Existing general AnimGraph inspection remains available."""
        return domain.run('state_inspect','inspect_animation_state_graph',asset_path,{'graph':machine_graph_name})

    @toolset_registry.tool_call
    @staticmethod
    def create_anim_state(asset_path: str, machine_graph_name: str, state_name: str, x: int = 0, y: int = 0, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Add a fresh named State with its standard pose-result graph in an existing StateMachine. Explicit bounded position follows engine grid snapping; preserve other graphs/links/Skeleton. Engine structural work may compile skeleton metadata; full compile/save optional. No implicit animation assignment or Entry replacement."""
        return domain.run('state_edit','create_state',asset_path,{'graph':machine_graph_name,'payload':{'name':state_name,'x':x,'y':y},'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def create_anim_transition(asset_path: str, machine_graph_name: str, from_state_id: str, to_state_id: str, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Create a unique directed transition and its rule graph using actual StateMachine schema between distinct State GUIDs. Verify actual endpoints/created graph. No duplicate/self transition, no implicit rule enable. Engine structural work may occur; compile/save target only when requested."""
        return domain.run('state_edit','create_transition',asset_path,{'graph':machine_graph_name,'payload':{'from_id':from_state_id,'to_id':to_state_id},'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def set_anim_entry_state(asset_path: str, machine_graph_name: str, state_id: str, allow_replace: bool = False, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Connect Entry to the exact existing State; replacing another Entry target requires allow_replace. Preserve unrelated transitions/graphs. Engine schema updates and callback preservation are checked; no automatic replay or atomic rollback."""
        return domain.run('state_edit','set_entry_state',asset_path,{'graph':machine_graph_name,'payload':{'node_id':state_id},'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def set_anim_transition_rule(asset_path: str, machine_graph_name: str, transition_id: str, value: bool, allow_replace: bool = False, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Set an unshared transition rule Result bool constant. Replacing a wired condition requires allow_replace; preserves other authored nodes rather than deleting the condition subgraph. Shared rules are refused. Variable/compare expression authoring is separate from this constant operation."""
        return domain.run('state_edit','set_transition_rule',asset_path,{'graph':machine_graph_name,'payload':{'node_id':transition_id,'value':value},'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def set_anim_transition_settings(asset_path: str, machine_graph_name: str, transition_id: str, settings_json: str, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Patch explicit unshared transition scalar settings: priority_order>=0, crossfade_duration0..60, automatic_rule bool, automatic_rule_trigger_time-1..60, bidirectional bool. Omitted fields preserved. Shared crossfade refused; referenced curves/profiles unchanged."""
        try:
            import json
            settings=json.loads(settings_json)
            if not isinstance(settings,dict) or 'node_id' in settings:raise ValueError('Settings object without node_id required')
        except Exception as error:return domain.reject('set_transition_settings',error)
        return domain.run('state_edit','set_transition_settings',asset_path,{'graph':machine_graph_name,'payload':dict(settings,node_id=transition_id),'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package)

    @toolset_registry.tool_call
    @staticmethod
    def delete_anim_state(asset_path: str, machine_graph_name: str, state_id: str, allow_delete: bool = False, allow_cascade: bool = False, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Delete exact State and owned graph only with allow_delete; attached transitions require allow_cascade. Entry target/last State/shared rule graph deletion refused. Engine RemoveGraph may implicitly compile even when compile_blueprint=false. Report actual retired objects/partial state; no rollback/replay. Save only target package explicitly."""
        return domain.run('state_edit','delete_state',asset_path,{'graph':machine_graph_name,'payload':{'node_id':state_id,'cascade':allow_cascade},'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_delete)

    @toolset_registry.tool_call
    @staticmethod
    def delete_anim_transition(asset_path: str, machine_graph_name: str, transition_id: str, allow_delete: bool = False, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Delete an exact unshared Transition and owned rule/custom graph with allow_delete; preserve surviving state/graph values and links. Engine cleanup may implicitly compile; no atomic rollback or automatic retry. Explicit single-package save."""
        return domain.run('state_edit','delete_transition',asset_path,{'graph':machine_graph_name,'payload':{'node_id':transition_id},'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_delete)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_animation_blueprint_bindings(asset_path: str) -> str:
        """Inspect exact AnimBlueprint node GUIDs and authored CachedPose, LinkedLayer/member/GUID and OnUpdate bindings across nested graphs. Read only after explicit asset resolution, no compile/save. Reports live Blueprint status and dirty state; those flags alone do not prove an earlier failed mutation made no change."""
        return domain.run('inspect','inspect_animation_blueprint_bindings',asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def set_anim_state_sequence(asset_path: str, machine_graph_name: str, state_id: str, animation_path: str, allow_replace: bool = False, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Create a standard default SequencePlayer in the selected nested State and connect its pose to that State Result. Canonical ordinary /Game AnimSequence (resolve redirectors separately), exact matching Skeleton and standard AnimGraph binding class required. Existing pose link replacement explicit; keep old nodes rather than deleting them. New player uses engine default playback settings. Preserve unrelated authored data, callbacks and source identity; compile/save target package explicitly, no rollback/replay."""
        return domain.run('state_edit','set_state_sequence',asset_path,{'graph':machine_graph_name,'payload':{'node_id':state_id,'animation_path':animation_path},'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def set_anim_transition_bool_variable(asset_path: str, machine_graph_name: str, transition_id: str, variable_name: str, allow_replace: bool = False, execute: bool = False, compile_blueprint: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Create a pure self bool-variable getter in an unshared transition rule and connect it to the rule Result. Existing runtime Blueprint-visible bool required; no implicit variable creation. Replacing an existing condition input requires allow_replace and retains previous nodes/default. Verify member identity, type and links after callbacks; optional compile and target-only save. Float/enum comparisons are outside this operation."""
        return domain.run('state_edit','set_transition_bool_variable',asset_path,{'graph':machine_graph_name,'payload':{'node_id':transition_id,'variable_name':variable_name},'compile':compile_blueprint},execute=execute,save=save,allow_dirty=allow_save_dirty_package,allow_remove=allow_replace)

    @toolset_registry.tool_call
    @staticmethod
    def set_chooser_enum_cell(asset_path: str, column_index: int, row_index: int, value: str | None = None, comparison: str = "Equal", execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Set an Enum cell by declared enum name, with Equal/NotEqual/Any comparison. Any ignores value. Reject unknown/hidden/out-of-byte enum entries before mutation. Preserve enum references and unrelated cells; optional explicit single-package save."""
        return domain.run('edit','set_chooser_enum_cell',asset_path,{'index':column_index,'row':row_index,'value':value or '', 'comparison':comparison},execute=execute,save=save,allow_dirty=allow_save_dirty_package)


    @toolset_registry.tool_call
    @staticmethod
    def set_chooser_gameplay_tag_cell(asset_path: str, column_index: int, row_index: int, tags: list[str], execute: bool = False, save: bool = False, allow_save_dirty_package: bool = False) -> str:
        """Set a GameplayTag cell from <=256 existing registered tag names; never registers project tags. Empty list follows the column's engine Any/All/invert semantics. Keep other cells and binding references; optional explicit single-package save."""
        return domain.run('edit','set_chooser_gameplay_tag_cell',asset_path,{'index':column_index,'row':row_index,'tags':list(tags)},execute=execute,save=save,allow_dirty=allow_save_dirty_package)
