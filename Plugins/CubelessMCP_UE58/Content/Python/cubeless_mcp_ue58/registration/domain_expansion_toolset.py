"""The 28 document domain tools on the official registry; no external MCP/TCP transport."""
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from ..operations import domain_expansion as domain


@unreal.uclass()
class CubelessDomainExpansionTools(unreal.ToolsetDefinition):
    """UE58 domain inspection and guarded authoring; native receipts retain the original operation contract."""

    @toolset_registry.tool_call
    @staticmethod
    def configure_pcg_custom_hlsl_elements(graph_path: str, node_name: str, element_counts_json: str, expected_current_json: str = 'null', dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        'Set UE5.8 Custom HLSL element counts on one temp node. Dry-run first; apply needs all before values. No compile, generate or save. Structured dict/list parameters use JSON text carriers named element_counts_json, expected_current_json. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed.'
        return reflected_result(domain.invoke_tool('configure_pcg_custom_hlsl_elements', {'graph_path': graph_path, 'node_name': node_name, 'element_counts_json': element_counts_json, 'expected_current_json': expected_current_json, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_emitter_execution_modes(system_path: str, max_results: int = 128) -> CubelessPythonOperationResult:
        'Read standard/stateless emitter modes and lightweight emitter module classes. Includes disabled handles; no compile or save. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_niagara_emitter_execution_modes', {'system_path': system_path, 'max_results': max_results}, None))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_controlrig_physics_units(control_rig_path: str, max_results: int = 128) -> CubelessPythonOperationResult:
        'Inspect Physics/Dynamics-named local graph nodes in legacy/runtime/editor ControlRig assets; external function bodies are not expanded. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_controlrig_physics_units', {'control_rig_path': control_rig_path, 'max_results': max_results}, None))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_anim_blueprint_compiled_layout(blueprint_path: str, max_results: int = 128) -> CubelessPythonOperationResult:
        'Read generated anim-node property/runtime indices and baked state-machine indices without compiling the Blueprint. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_anim_blueprint_compiled_layout', {'blueprint_path': blueprint_path, 'max_results': max_results}, None))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_body_solver_overrides(actor_path: str, component_name: str) -> CubelessPythonOperationResult:
        'Read raw/effective body iterations, partial island sleep and concurrency hash. Editor world, single unwelded non-skinned/non-instanced body only. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_chaos_body_solver_overrides', {'actor_path': actor_path, 'component_name': component_name}, None))

    @toolset_registry.tool_call
    @staticmethod
    def configure_chaos_body_solver_overrides(actor_path: str, component_name: str, settings_json: str, expected_state_hash: str = '', dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        "Guarded native body solver setters. Apply only in editor temp maps with matching hash; never saves. Partial island sleep affects the body's island; runtime delivery is not measured. Structured dict/list parameters use JSON text carriers named settings_json. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed."
        return reflected_result(domain.invoke_tool('configure_chaos_body_solver_overrides', {'actor_path': actor_path, 'component_name': component_name, 'settings_json': settings_json, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_cloth_solver_properties(actor_path: str, component_name: str, max_results: int = 128) -> CubelessPythonOperationResult:
        'Read UE5.8 solver-level cloth property collections, separately from per-asset/model properties. Empty collections are reported as empty. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_chaos_cloth_solver_properties', {'actor_path': actor_path, 'component_name': component_name, 'max_results': max_results}, None))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_cloth_snapshot_history(dataflow_path: str, node_name: str, max_results: int = 128) -> CubelessPythonOperationResult:
        'Read native editable-node snapshot history: active selection, lock, dates, weight/topology counts. Does not select, delete, regenerate or save. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_chaos_cloth_snapshot_history', {'dataflow_path': dataflow_path, 'node_name': node_name, 'max_results': max_results}, None))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_controlrig_physics_unit_catalog() -> CubelessPythonOperationResult:
        'Read installed nondeprecated Physics/Dynamics RigVM units and pin defaults. The relevant engine plugins must be enabled independently of MCP. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_controlrig_physics_unit_catalog', {}, None))

    @toolset_registry.tool_call
    @staticmethod
    def build_controlrig_physics_graph_variant(source_rig_path: str, target_rig_path: str, nodes_json: str, links_json: str, dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        "Clone a saved independent runtime/legacy rig into an unused temp path; add current Physics/Dynamics units, defaults and reviewed links through the existing authoring module's controller. Compile/hierarchy/node checks; no source mutation/save or unsafe factory creation. Structured dict/list parameters use JSON text carriers named nodes_json, links_json. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed."
        return reflected_result(domain.invoke_tool('build_controlrig_physics_graph_variant', {'source_rig_path': source_rig_path, 'target_rig_path': target_rig_path, 'nodes_json': nodes_json, 'links_json': links_json, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_anim_blueprint_authoring_nodes() -> CubelessPythonOperationResult:
        'Read the installed allowlisted AnimGraph node classes and typed runtime property defaults for native recipe authoring. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_anim_blueprint_authoring_nodes', {}, None))

    @toolset_registry.tool_call
    @staticmethod
    def build_anim_blueprint_graph_variant(source_blueprint_path: str, target_blueprint_path: str, nodes_json: str, links_json: str, graph_name: str = 'AnimGraph', replace_root: bool = False, dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        'Build a native AnimGraph recipe on an unused temp copy: inertial/dead blending, sequence/blendspace, skeletal controls, dynamics and space converters. Callback ownership guards, schema links, cycle checks, compile and authored settings/asset/link readback; no source writer or save. source_unchanged is null because complete in-memory source equivalence is not proven; source_identity_unchanged, source_package_remained_clean and source_preservation_scope describe observed preservation. Structured dict/list parameters use JSON text carriers named nodes_json, links_json. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed.'
        return reflected_result(domain.invoke_tool('build_anim_blueprint_graph_variant', {'source_blueprint_path': source_blueprint_path, 'target_blueprint_path': target_blueprint_path, 'nodes_json': nodes_json, 'links_json': links_json, 'graph_name': graph_name, 'replace_root': replace_root, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_stateless_authoring(system_path: str) -> CubelessPythonOperationResult:
        'Read stateless emitter spawn-info and engine module names/export-text properties plus authoring_status for an existing create/variant continuation; no compile/save is requested by this inspector. Poll this read-only tool until authoring_status.operation_complete is true; inspect nested success/error for the final outcome. A successful inspection alone is not authoring completion. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_niagara_stateless_authoring', {'system_path': system_path}, None))

    @toolset_registry.tool_call
    @staticmethod
    def create_niagara_stateless_system(target_system_path: str, dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        "Create an unused temp system through the stock editor's combined standard/stateless setup. Engine compilation uses frame-based continuation with a 10-second phase deadline and no waiting loop. accepted=true with operation_complete=false means initiation only, not compilation/publication completion; inspect the target with inspect_niagara_stateless_authoring and never replay creation. Retains pending candidates with do_not_retry, never saves. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed."
        return reflected_result(domain.invoke_tool('create_niagara_stateless_system', {'target_system_path': target_system_path, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def build_niagara_stateless_variant(source_system_path: str, target_system_path: str, emitters_json: str, dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        'Duplicate a saved independent system into an unused temp path and edit stateless spawn_infos/module fields. Exact emitter/module names, source unchanged and no save. Engine compilation uses frame-based continuation with a 10-second phase deadline and no waiting loop. accepted=true with operation_complete=false means initiation only, not settings/compilation/publication completion; inspect the target with inspect_niagara_stateless_authoring and never replay the variant. Dry-run is path/source preflight only. Structured dict/list parameters use JSON text carriers named emitters_json. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed.'
        return reflected_result(domain.invoke_tool('build_niagara_stateless_variant', {'source_system_path': source_system_path, 'target_system_path': target_system_path, 'emitters_json': emitters_json, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def configure_pcg_manual_inspection(actor_path: str, component_name: str, enabled: bool = True, dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        "Acquire/release this subsystem's transient PCG inspection reference in a temp map. Does not remove another editor's reference or generate/save content. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed."
        return reflected_result(domain.invoke_tool('configure_pcg_manual_inspection', {'actor_path': actor_path, 'component_name': component_name, 'enabled': enabled, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_pcg_manual_override_targets(actor_path: str, component_name: str) -> CubelessPythonOperationResult:
        'Read exact executed stack/pin storage keys, point samples, existing engine deltas and concurrency hashes. Enable inspection and generate first. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_pcg_manual_override_targets', {'actor_path': actor_path, 'component_name': component_name}, None))

    @toolset_registry.tool_call
    @staticmethod
    def edit_pcg_manual_point_override(actor_path: str, component_name: str, target_key: str, action: str, data_index: int = 0, point_index: int = -1, transform: str = '', delta_key: str = '', expected_state_hash: str = '', dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        'Author engine transform/offset/insert deltas or hide a point with the stock deletion delta (zero scale, count preserved); remove/clear deltas. Hash-bound idle temp component, explicit regeneration, no save. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed.'
        return reflected_result(domain.invoke_tool('edit_pcg_manual_point_override', {'actor_path': actor_path, 'component_name': component_name, 'target_key': target_key, 'action': action, 'data_index': data_index, 'point_index': point_index, 'transform': transform, 'delta_key': delta_key, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_cloth_dataflow_node_catalog() -> CubelessPythonOperationResult:
        'Read installed current Cloth Dataflow node types, exact pin names/types and property export text. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_chaos_cloth_dataflow_node_catalog', {}, None))

    @toolset_registry.tool_call
    @staticmethod
    def build_chaos_cloth_dataflow_graph(target_dataflow_path: str, nodes_json: str, links_json: str, source_dataflow_path: str = '', dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        'Build a bounded native Cloth Dataflow graph at an unused temp path. Current node types, export-text settings, saved mesh asset_bindings and exact links. Optional clean source clone; no source mutation, evaluation or save. Dry-run is path/source/type preflight. Structured dict/list parameters use JSON text carriers named nodes_json, links_json. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed.'
        return reflected_result(domain.invoke_tool('build_chaos_cloth_dataflow_graph', {'target_dataflow_path': target_dataflow_path, 'nodes_json': nodes_json, 'links_json': links_json, 'source_dataflow_path': source_dataflow_path, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def create_chaos_cloth_asset_from_template(target_cloth_path: str, template_dataflow_path: str, evaluate: bool = False, dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        'Use stock UE5.8 non-modal Cloth factory for an unused temp ClothAsset with owned embedded Dataflow. Optional native evaluation returns LOD mesh counts; no save or simulation success claim. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed.'
        return reflected_result(domain.invoke_tool('create_chaos_cloth_asset_from_template', {'target_cloth_path': target_cloth_path, 'template_dataflow_path': template_dataflow_path, 'evaluate': evaluate, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_cloth_snapshot_state(dataflow_path: str, node_name: str) -> CubelessPythonOperationResult:
        'Read bounded Cloth snapshot metadata and a full serialized-data state hash for guarded history edits. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_chaos_cloth_snapshot_state', {'dataflow_path': dataflow_path, 'node_name': node_name}, None))

    @toolset_registry.tool_call
    @staticmethod
    def edit_chaos_cloth_snapshot_history(dataflow_path: str, node_name: str, action: str, index: int, name: str = '', locked: bool = False, expected_state_hash: str = '', dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        'Select/delete/rename/lock/duplicate Cloth snapshots on a temp Dataflow node. Dry-run exposes eviction; locked deletion/history growth refused; native rollback and no save. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed.'
        return reflected_result(domain.invoke_tool('edit_chaos_cloth_snapshot_history', {'dataflow_path': dataflow_path, 'node_name': node_name, 'action': action, 'index': index, 'name': name, 'locked': locked, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_body_collision(actor_path: str, component_name: str) -> CubelessPythonOperationResult:
        "Read one unwelded editor body's collision, CCD, gravity, damping, mass scale and state hash. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success."
        return reflected_result(domain.invoke_tool('inspect_chaos_body_collision', {'actor_path': actor_path, 'component_name': component_name}, None))

    @toolset_registry.tool_call
    @staticmethod
    def configure_chaos_body_collision(actor_path: str, component_name: str, settings_json: str, expected_state_hash: str = '', dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        'Native collision/CCD/gravity/damping/mass setters. Exact hash and temp map required for apply; transaction/readback/rollback, no save or simulate-physics toggles. Structured dict/list parameters use JSON text carriers named settings_json. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed.'
        return reflected_result(domain.invoke_tool('configure_chaos_body_collision', {'actor_path': actor_path, 'component_name': component_name, 'settings_json': settings_json, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_constraint_profile(actor_path: str = '', component_name: str = '', physics_asset_path: str = '', joint_name: str = '') -> CubelessPythonOperationResult:
        'Read constraint profile export-text fields and full profile state hash. Select exactly one editor component or PhysicsAsset/joint. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success.'
        return reflected_result(domain.invoke_tool('inspect_chaos_constraint_profile', {'actor_path': actor_path, 'component_name': component_name, 'physics_asset_path': physics_asset_path, 'joint_name': joint_name}, None))

    @toolset_registry.tool_call
    @staticmethod
    def configure_chaos_constraint_profile(actor_path: str, component_name: str, settings_json: str, expected_state_hash: str = '', dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        'Patch constraint limits, drives, break/plasticity/projection fields using native CopyProfilePropertiesFrom. Export-text values, hash-bound temp map only; no save. Structured dict/list parameters use JSON text carriers named settings_json. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed.'
        return reflected_result(domain.invoke_tool('configure_chaos_constraint_profile', {'actor_path': actor_path, 'component_name': component_name, 'settings_json': settings_json, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def configure_physics_asset_constraint_profile(physics_asset_path: str, joint_name: str, settings_json: str, expected_state_hash: str = '', dry_run: bool = True, operation_id: str = "") -> CubelessPythonOperationResult:
        'Patch one exactly named, asset-owned PhysicsAsset constraint template. Temp asset only; never edits an external shared template or saves. Structured dict/list parameters use JSON text carriers named settings_json. Original native receipt is preserved in parsed data_json as native_result and native_raw_result; compile/save/rollback/candidate/pending fields are not inferred from success. operation_id identifies the existing durable journal; uncertain requests are never automatically replayed.'
        return reflected_result(domain.invoke_tool('configure_physics_asset_constraint_profile', {'physics_asset_path': physics_asset_path, 'joint_name': joint_name, 'settings_json': settings_json, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id))
