# Lyra Combined BP Authoring Readiness

- Generated UTC: `2026-06-07T05:17:08.775585+00:00`
- Project root: `D:\Git\LyraStarterGame`
- Minimum stable scope: `readiness_classification_and_candidate_selection`
- Current authoring ceiling: `temporary_smoke_only_bp_shells_allowlisted_actor_parent_component_hierarchy_property_graph_glue_validation_durable_read_only_asset_exists_preflight_overwrite_rename_decision_save_gate_rollback_draft_executor_readiness_checklist_and_disabled_executor_skeleton`
- Editor open required now: `False`
- Editor decision: Static class ancestry extraction found high-confidence ParentClass/NativeParentClass tags for Blueprint-like assets.

## Combined Verdict

The current UnrealMCP BP authoring surface is ready for candidate selection and narrow BP shell/graph-glue experiments, not for full Lyra C++ to BP conversion.

## C++ Intake Summary

- Source files: `734`
- Config files: `68`

### Supported C++ Pattern Categories

- `control_flow_expression_authoring`: 5390
- `basic_actor_component_authoring`: 743
- `enhanced_input`: 58
- `blueprint_exposed_api`: 1096
- `reflected_api`: 2195

### Partial C++ Pattern Categories

- `common_ui_umg`: 1080
- `gameplay_ability_system`: 430
- `delegates_async_latent`: 311
- `subsystems_lifecycle`: 142
- `gameplay_tags`: 871
- `data_assets_and_registry`: 67

### Native-Blocked C++ Pattern Categories

- `slate_editor_cpp`: 367
- `networking_replication`: 246
- `game_features_modular_gameplay`: 214
- `custom_k2_native_extensions`: 18

## Asset Ancestry Summary

- Assets scanned: `8683`
- Blueprint-like assets: `521`
- Supported partial candidates: `93`
- Blocked or partial-blocked Blueprint-like assets: `255`

### Supported Asset Categories

- `actor_blueprint`: 87
- `enhanced_input_asset`: 32
- `component_blueprint`: 4
- `blueprint_function_library`: 2

### Dedicated-Support Asset Categories

- `common_ui_umg`: 171
- `unknown_blueprint`: 84
- `gameplay_effect`: 45
- `gameplay_ability`: 37
- `animation_blueprint`: 27
- `gameplay_cue`: 24
- `framework_blueprint`: 12
- `data_asset`: 7
- `blueprint_interface`: 3

### Blocked Asset Categories

- `game_feature_or_experience`: 10
- `editor_utility_blueprint`: 7

## Delegate / Latent / Async Summary

- Status: `not_ready_for_generic_delegate_or_async_proxy_authoring`
- Matched source files: `154`

### Delegate Pattern Totals

- `possible_delegate_unbinding`: 60
- `native_delegate_declaration`: 32
- `delegate_broadcast`: 97
- `dynamic_binding`: 23
- `native_binding`: 98
- `engine_lifecycle_delegate`: 43
- `delegate_unbinding`: 54
- `dynamic_delegate_declaration`: 34
- `async_action_class`: 9
- `blueprint_internal_async_factory`: 12
- `blueprint_assignable_delegate`: 29
- `latent_function`: 4
- `custom_k2_async_node`: 28
- `ability_task`: 15

### Delegate Lifecycle Buckets

- `inventory_cleanup`: 97
- `requires_explicit_unbind_policy`: 8
- `requires_wrapper_api`: 76
- `native_required`: 60
- `bp_event_dispatcher_candidate`: 12
- `async_or_ability_task`: 10

### Async Proxy Inventory

- Async proxy classes: `13`
- Classes requiring callback exec/native policy: `13`
- Callback delegates: `11`
- Factory functions: `29`
- Broadcast sites: `13`
- Authoring status: `inventory_only_until_async_proxy_callback_exec_model_exists`

#### Async Class Kinds

- `cancellable_async_action`: 6
- `blueprint_async_action`: 3
- `custom_k2_async_node`: 1
- `ability_task`: 3

### Delegate MCP Gaps

- generic delegate lifecycle authoring for non-Event-Dispatcher targets
- async proxy node authoring with callback exec pins
- latent call validation exists only as opt-in function-call creation, not full continuation topology
- UMG event binding exists, but it is widget-specific and not a generic delegate solution

## Not Ready For

- whole-project C++ to Blueprint conversion
- replication/RPC/ReplicationGraph lowering
- Gameplay Ability System internals and ability-task flow
- CommonUI widget tree and activation-policy authoring
- Animation Blueprint graph/state-machine conversion
- GameFeature activation and experience architecture recreation
- custom K2/editor-only/Slate behavior recreation

## Next Reinforcement Candidates

- `1` Native/arbitrary delegate lifecycle and async callback primitives: Lyra source shows 106 delegate bind/assign/unbind triggers, 144 lifecycle sites that still require native retention, wrapper APIs, or explicit unbind policy, and 13 async proxy classes requiring callback exec or native policy; Blueprint Event Dispatcher lifecycle is now covered, but native delegates, arbitrary delegate targets, and async callback topology still need reinforcement.
- `2` CommonUI/UMG structure inventory: Asset ancestry found 171 CommonUI/UMG Blueprint-like assets.
- `3` GameplayAbility-specific classifier: Asset ancestry found 37 ability Blueprints plus GAS-heavy C++ patterns.
- `4` Animation Blueprint read-only graph inventory: Asset ancestry found 27 animation-related Blueprint assets; current MCP generic graph tools are not AnimGraph tools.
- `5` Optional Editor Asset Registry verification: Use only if exact Unreal AssetRegistry class names or Blueprint graph counts are needed beyond static package tag extraction.

## Decision

Do not open Lyra in Editor for this combined pass. Static package tag extraction already produced high-confidence class ancestry for the Blueprint-like set. Use an Editor Asset Registry pass only as the next optional verification step when exact graph counts or property payload details become necessary.
