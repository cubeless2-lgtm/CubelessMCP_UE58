# Lyra Delegate / Latent / Async Readiness

- Generated UTC: `2026-06-07T04:14:25.483947+00:00`
- Read-only project root: `D:\Git\LyraStarterGame`
- Output dir: `D:\Git\unreal-mcp-cubeless\Docs\Analysis\Lyra`
- Minimum stable scope: `delegate_async_gap_classification`
- Current status: `not_ready_for_generic_delegate_or_async_proxy_authoring`

## Executive Result

Lyra has enough native delegate, arbitrary delegate target, latent, and async callback topology that generic C++ to BP conversion should stay blocked here. Blueprint Event Dispatcher lifecycle authoring is covered, but native lifecycle classification and async proxy node authoring are still required.

## Source Scan

- Source files scanned: `697`
- Files with delegate/async/latent matches: `154`

### Pattern Findings

| Pattern | Readiness | Risk | Hits | Files | Gap |
| --- | --- | --- | ---: | ---: | --- |
| Native delegate binding | `blocked_native` | `critical` | 98 | 48 | Native delegate binding depends on object lifetime and shutdown-safe unbinding. |
| Engine lifecycle delegates | `blocked_native` | `critical` | 43 | 19 | Engine lifecycle delegates need shutdown-safe C++/subsystem handling. |
| Custom K2 async nodes | `blocked_native` | `critical` | 28 | 2 | Custom K2 expansion behavior is native editor code, outside generic BP graph authoring. |
| Gameplay AbilityTasks | `blocked_native` | `critical` | 15 | 8 | AbilityTasks are GAS-specific async nodes with prediction and ability lifecycle constraints. |
| Native delegate declarations | `blocked_native` | `high` | 32 | 21 | Native-only delegate topology usually needs C++ lifecycle handling or wrapper APIs. |
| Delegate broadcasts | `partial_blocked` | `high` | 97 | 42 | Broadcast points define callback semantics and ordering that need explicit graph topology. |
| Explicit delegate unbinding | `partial_blocked` | `high` | 54 | 33 | Conversion must preserve cleanup paths and object lifetime. |
| BlueprintAssignable delegate properties | `partial_blocked` | `high` | 29 | 18 | Existing MCP can declare/call/bind/assign/unbind/clear Blueprint Event Dispatchers, but generic or arbitrary delegate target authoring remains blocked. |
| Dynamic delegate binding | `partial_blocked` | `high` | 23 | 15 | Blueprint Event Dispatcher lifecycle nodes are supported; arbitrary AddDynamic conversion still needs target classification and lifecycle policy. |
| Blueprint internal async factories | `partial_blocked` | `high` | 12 | 9 | Internal async factories normally spawn specialized K2 async nodes with delegate outputs. |
| Blueprint async action classes | `partial_blocked` | `high` | 9 | 9 | Async proxy nodes expose callback exec pins that generic function calls do not model. |
| Latent Blueprint functions | `partial_blocked` | `high` | 4 | 1 | MCP can optionally allow latent function calls, but latent continuation semantics still need validation. |
| Possible delegate handle removal or clear | `inventory_only` | `medium` | 60 | 35 | Needs context to distinguish delegate handle cleanup from ordinary container mutation. |
| Dynamic delegate declarations | `inventory_only` | `medium` | 34 | 23 | Can inventory signatures and author Blueprint Event Dispatcher lifecycle nodes, but cannot generically author native or arbitrary delegate topologies yet. |

### Readiness By File

- `inventory_only`: 21
- `blocked_native`: 83
- `partial_blocked`: 50

### Risk By File

- `medium`: 21
- `high`: 68
- `critical`: 65

### Module Hit Counts

- `LyraGame`: 276
- `CommonUser/CommonUser`: 72
- `GameSettings/GameSettings`: 64
- `CommonGame/CommonGame`: 40
- `GameplayMessageRouter/GameplayMessageNodes`: 28
- `CommonLoadingScreen/CommonLoadingScreen`: 11
- `LyraEditor`: 10
- `GameFeatures`: 8
- `UIExtension/UIExtension`: 7
- `GameplayMessageRouter/GameplayMessageRuntime`: 6
- `PocketWorlds/PocketWorlds`: 6
- `GameSubtitles/GameSubtitles`: 5
- `AsyncMixin/AsyncMixin`: 4
- `CommonLoadingScreen/CommonStartupLoadingScreen`: 1

## Delegate Lifecycle Classifier

- Classified delegate lifecycle sites: `263`

### Conversion Buckets

- `inventory_cleanup`: 97
- `requires_explicit_unbind_policy`: 8
- `requires_wrapper_api`: 76
- `native_required`: 60
- `bp_event_dispatcher_candidate`: 12
- `async_or_ability_task`: 10

### Lifecycle Classifications

- `explicit_cleanup_observed`: 97
- `dynamic_delegate_requires_unbind_policy`: 8
- `native_delegate_lifecycle_blocker`: 83
- `engine_lifecycle_native_blocker`: 53
- `bp_event_dispatcher_candidate_with_cleanup`: 12
- `async_or_ability_task_candidate`: 10

### Top Lifecycle Blockers

- `Source/LyraGame/Weapons/LyraGameplayAbility_RangedWeapon.cpp:470` `async_or_ability_task_candidate` / `async_or_ability_task` (remove_handle): Gameplay Ability or prediction-related delegate context requires GAS-aware async policy.
- `Source/LyraGame/Weapons/LyraGameplayAbility_RangedWeapon.cpp:446` `async_or_ability_task_candidate` / `async_or_ability_task` (add_uobject): Gameplay Ability or prediction-related delegate context requires GAS-aware async policy.
- `Source/LyraGame/UI/PerformanceStats/LyraPerfStatContainerBase.cpp:24` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/UI/LyraHUDLayout.cpp:47` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/UI/LyraHUDLayout.cpp:46` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/UI/IndicatorSystem/SActorCanvas.cpp:180` `native_delegate_lifecycle_blocker` / `native_required` (add_sp): Raw, shared-pointer, lambda, and static delegate bindings do not map safely to ordinary BP events.
- `Source/LyraGame/UI/IndicatorSystem/SActorCanvas.cpp:179` `native_delegate_lifecycle_blocker` / `native_required` (add_sp): Raw, shared-pointer, lambda, and static delegate bindings do not map safely to ordinary BP events.
- `Source/LyraGame/UI/Foundation/LyraControllerDisconnectedScreen.cpp:61` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/UI/Foundation/LyraConfirmationScreen.cpp:53` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/UI/Common/LyraBoundActionButton.cpp:18` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/Teams/LyraTeamCreationComponent.cpp:84` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/Teams/AsyncAction_ObserveTeamColors.cpp:95` `async_or_ability_task_candidate` / `async_or_ability_task` (add_dynamic): Async action or custom K2 async node context usually exposes callback exec topology.
- `Source/LyraGame/Teams/AsyncAction_ObserveTeamColors.cpp:85` `async_or_ability_task_candidate` / `async_or_ability_task` (remove_all): Async action or custom K2 async node context usually exposes callback exec topology.
- `Source/LyraGame/Teams/AsyncAction_ObserveTeamColors.cpp:59` `async_or_ability_task_candidate` / `async_or_ability_task` (add_dynamic): Async action or custom K2 async node context usually exposes callback exec topology.
- `Source/LyraGame/Teams/AsyncAction_ObserveTeamColors.cpp:40` `async_or_ability_task_candidate` / `async_or_ability_task` (remove_all): Async action or custom K2 async node context usually exposes callback exec topology.
- `Source/LyraGame/Teams/AsyncAction_ObserveTeam.cpp:53` `async_or_ability_task_candidate` / `async_or_ability_task` (add_dynamic): Async action or custom K2 async node context usually exposes callback exec topology.
- `Source/LyraGame/Teams/AsyncAction_ObserveTeam.cpp:40` `async_or_ability_task_candidate` / `async_or_ability_task` (remove_all): Async action or custom K2 async node context usually exposes callback exec topology.
- `Source/LyraGame/System/LyraReplicationGraph.cpp:577` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/System/LyraReplicationGraph.cpp:576` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/System/LyraReplicationGraph.cpp:517` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/System/LyraGameInstance.h:36` `engine_lifecycle_native_blocker` / `native_required` (engine_lifecycle): Engine/editor/Slate lifecycle delegates depend on subsystem or module shutdown order.
- `Source/LyraGame/System/LyraGameInstance.cpp:313` `engine_lifecycle_native_blocker` / `native_required` (engine_lifecycle): Engine/editor/Slate lifecycle delegates depend on subsystem or module shutdown order.
- `Source/LyraGame/System/LyraGameInstance.cpp:121` `engine_lifecycle_native_blocker` / `native_required` (remove_all, engine_lifecycle): Engine/editor/Slate lifecycle delegates depend on subsystem or module shutdown order.
- `Source/LyraGame/System/LyraGameInstance.cpp:113` `engine_lifecycle_native_blocker` / `native_required` (add_uobject, engine_lifecycle): Engine/editor/Slate lifecycle delegates depend on subsystem or module shutdown order.
- `Source/LyraGame/System/LyraAssetManager.h:79` `engine_lifecycle_native_blocker` / `native_required` (engine_lifecycle): Engine/editor/Slate lifecycle delegates depend on subsystem or module shutdown order.
- `Source/LyraGame/System/LyraAssetManager.cpp:267` `engine_lifecycle_native_blocker` / `native_required` (engine_lifecycle): Engine/editor/Slate lifecycle delegates depend on subsystem or module shutdown order.
- `Source/LyraGame/System/LyraAssetManager.cpp:256` `engine_lifecycle_native_blocker` / `native_required` (engine_lifecycle): Engine/editor/Slate lifecycle delegates depend on subsystem or module shutdown order.
- `Source/LyraGame/System/LyraAssetManager.cpp:254` `engine_lifecycle_native_blocker` / `native_required` (engine_lifecycle): Engine/editor/Slate lifecycle delegates depend on subsystem or module shutdown order.
- `Source/LyraGame/Settings/Widgets/LyraSettingsListEntrySetting_KeyboardInput.cpp:108` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.
- `Source/LyraGame/Settings/Widgets/LyraSettingsListEntrySetting_KeyboardInput.cpp:106` `native_delegate_lifecycle_blocker` / `requires_wrapper_api` (add_uobject): AddUObject binding depends on UObject lifetime and explicit unbinding or owner teardown.

## Async Proxy Callback Inventory

- Async proxy classes: `13`
- Classes requiring callback exec/native policy: `13`
- BlueprintAssignable callback fields: `11`
- Factory functions: `29`
- Broadcast sites: `13`
- Authoring status: `inventory_only_until_async_proxy_callback_exec_model_exists`

### Async Class Kinds

- `cancellable_async_action`: 6
- `blueprint_async_action`: 3
- `custom_k2_async_node`: 1
- `ability_task`: 3

### Async Authoring Policy Counts

- `callback_exec_model_required`: 9
- `native_policy_required`: 4

### Top Async Proxy Classes

- `UAsyncAction_PushContentToLayerForPlayer` `cancellable_async_action` / `callback_exec_model_required` at `Plugins/CommonGame/Source/Public/Actions/AsyncAction_PushContentToLayerForPlayer.h:25` (callbacks `2`, factories `2`, activate `2`, broadcasts `2`, cleanup `8`)
- `UAsyncAction_ObserveTeam` `cancellable_async_action` / `callback_exec_model_required` at `Source/LyraGame/Teams/AsyncAction_ObserveTeam.h:20` (callbacks `1`, factories `3`, activate `2`, broadcasts `2`, cleanup `6`)
- `UAsyncAction_QueryReplays` `blueprint_async_action` / `callback_exec_model_required` at `Source/LyraGame/Replays/AsyncAction_QueryReplays.h:23` (callbacks `1`, factories `2`, activate `2`, broadcasts `2`, cleanup `0`)
- `UAsyncAction_ShowConfirmation` `blueprint_async_action` / `callback_exec_model_required` at `Plugins/CommonGame/Source/Public/Actions/AsyncAction_ShowConfirmation.h:23` (callbacks `1`, factories `6`, activate `2`, broadcasts `1`, cleanup `1`)
- `UAsyncAction_CommonUserInitialize` `cancellable_async_action` / `callback_exec_model_required` at `Plugins/CommonUser/Source/CommonUser/Public/AsyncAction_CommonUserInitialize.h:24` (callbacks `1`, factories `4`, activate `2`, broadcasts `1`, cleanup `4`)
- `UAsyncAction_ObserveTeamColors` `cancellable_async_action` / `callback_exec_model_required` at `Source/LyraGame/Teams/AsyncAction_ObserveTeamColors.h:20` (callbacks `1`, factories `2`, activate `2`, broadcasts `1`, cleanup `7`)
- `UAsyncAction_ListenForGameplayMessage` `cancellable_async_action` / `callback_exec_model_required` at `Plugins/GameplayMessageRouter/Source/GameplayMessageRuntime/Public/GameFramework/AsyncAction_ListenForGameplayMessage.h:25` (callbacks `1`, factories `2`, activate `2`, broadcasts `1`, cleanup `6`)
- `UAsyncAction_ExperienceReady` `blueprint_async_action` / `callback_exec_model_required` at `Source/LyraGame/GameModes/AsyncAction_ExperienceReady.h:21` (callbacks `1`, factories `2`, activate `2`, broadcasts `1`, cleanup `3`)
- `UAsyncAction_CreateWidgetAsync` `cancellable_async_action` / `callback_exec_model_required` at `Plugins/CommonGame/Source/Public/Actions/AsyncAction_CreateWidgetAsync.h:25` (callbacks `1`, factories `2`, activate `2`, broadcasts `1`, cleanup `5`)
- `UAbilityTask_WaitForInteractableTargets` `ability_task` / `native_policy_required` at `Source/LyraGame/Interaction/Tasks/AbilityTask_WaitForInteractableTargets.h:22` (callbacks `1`, factories `0`, activate `0`, broadcasts `1`, cleanup `0`)
- `UAbilityTask_WaitForInteractableTargets_SingleLineTrace` `ability_task` / `native_policy_required` at `Source/LyraGame/Interaction/Tasks/AbilityTask_WaitForInteractableTargets_SingleLineTrace.h:16` (callbacks `0`, factories `2`, activate `2`, broadcasts `0`, cleanup `4`)
- `UAbilityTask_GrantNearbyInteraction` `ability_task` / `native_policy_required` at `Source/LyraGame/Interaction/Tasks/AbilityTask_GrantNearbyInteraction.h:15` (callbacks `0`, factories `2`, activate `2`, broadcasts `0`, cleanup `4`)
- `UK2Node_AsyncAction_ListenForGameplayMessages` `custom_k2_async_node` / `native_policy_required` at `Plugins/GameplayMessageRouter/Source/GameplayMessageNodes/Public/K2Node_AsyncAction_ListenForGameplayMessages.h:22` (callbacks `0`, factories `0`, activate `0`, broadcasts `0`, cleanup `0`)

## MCP Capability Match

- `can_call_functions`: True
- `can_optionally_allow_latent_function_calls`: True
- `can_list_delegate_graphs`: True
- `can_create_event_nodes`: True
- `can_connect_nodes`: True
- `can_bind_umg_widget_events`: True
- `has_event_dispatcher_declaration`: True
- `has_event_dispatcher_call_node`: True
- `has_custom_event_node`: True
- `has_event_dispatcher_bind_node`: True
- `has_event_dispatcher_unbind_node`: True
- `has_event_dispatcher_clear_node`: True
- `has_event_dispatcher_assign_node`: True
- `has_generic_delegate_bind_node`: False
- `has_async_proxy_node_authoring`: False

### Identified Gaps

- generic delegate lifecycle authoring for non-Event-Dispatcher targets
- async proxy node authoring with callback exec pins
- latent call validation exists only as opt-in function-call creation, not full continuation topology
- UMG event binding exists, but it is widget-specific and not a generic delegate solution

## Top Risk Files

- `Plugins/GameplayMessageRouter/Source/GameplayMessageNodes/Private/K2Node_AsyncAction_ListenForGameplayMessages.cpp`: `critical` / `blocked_native`, hits `24` (custom_k2_async_node)
- `Plugins/CommonUser/Source/CommonUser/Public/CommonSessionSubsystem.h`: `critical` / `blocked_native`, hits `22` (blueprint_assignable_delegate, dynamic_delegate_declaration, engine_lifecycle_delegate, native_delegate_declaration)
- `Plugins/CommonUser/Source/CommonUser/Private/CommonSessionSubsystem.cpp`: `critical` / `blocked_native`, hits `19` (delegate_broadcast, delegate_unbinding, engine_lifecycle_delegate, native_binding)
- `Plugins/CommonUser/Source/CommonUser/Private/CommonUserSubsystem.cpp`: `critical` / `blocked_native`, hits `17` (delegate_broadcast, delegate_unbinding, native_binding, possible_delegate_unbinding)
- `Source/LyraGame/Settings/Widgets/LyraSettingsListEntrySetting_KeyboardInput.cpp`: `critical` / `blocked_native`, hits `17` (delegate_unbinding, native_binding)
- `Plugins/GameSettings/Source/Private/Widgets/GameSettingListEntry.cpp`: `critical` / `blocked_native`, hits `12` (delegate_unbinding, dynamic_binding, native_binding)
- `Source/LyraGame/Character/LyraHealthComponent.cpp`: `critical` / `blocked_native`, hits `12` (delegate_broadcast, delegate_unbinding, native_binding)
- `Source/LyraGame/Settings/LyraSettingsLocal.cpp`: `critical` / `blocked_native`, hits `12` (delegate_broadcast, engine_lifecycle_delegate, native_binding, possible_delegate_unbinding)
- `Plugins/GameSettings/Source/Private/Widgets/Misc/GameSettingPressAnyKey.cpp`: `critical` / `blocked_native`, hits `11` (delegate_broadcast, engine_lifecycle_delegate, native_binding, native_delegate_declaration)
- `Source/LyraGame/GameFeatures/GameFeatureAction_AddInputContextMapping.cpp`: `critical` / `blocked_native`, hits `11` (delegate_unbinding, engine_lifecycle_delegate, native_binding, possible_delegate_unbinding)
- `Plugins/CommonLoadingScreen/Source/CommonLoadingScreen/Private/LoadingScreenManager.cpp`: `critical` / `blocked_native`, hits `10` (delegate_broadcast, delegate_unbinding, engine_lifecycle_delegate, native_binding, possible_delegate_unbinding)
- `Plugins/GameSettings/Source/Private/Widgets/GameSettingPanel.cpp`: `critical` / `blocked_native`, hits `10` (delegate_broadcast, delegate_unbinding, native_binding)
- `Source/LyraEditor/LyraEditor.cpp`: `critical` / `blocked_native`, hits `10` (delegate_unbinding, engine_lifecycle_delegate, native_binding)
- `Plugins/GameSettings/Source/Private/Registry/GameSettingRegistry.cpp`: `critical` / `blocked_native`, hits `9` (delegate_broadcast, native_binding)
- `Source/LyraGame/AbilitySystem/LyraGameplayCueManager.cpp`: `critical` / `blocked_native`, hits `9` (delegate_unbinding, native_binding, possible_delegate_unbinding)
- `Plugins/GameSettings/Source/Private/GameSetting.cpp`: `critical` / `blocked_native`, hits `7` (delegate_broadcast, native_binding)
- `Source/LyraGame/Hotfix/LyraHotfixManager.cpp`: `critical` / `blocked_native`, hits `7` (delegate_broadcast, engine_lifecycle_delegate, native_binding, possible_delegate_unbinding)
- `Source/LyraGame/GameFeatures/GameFeatureAction_WorldActionBase.cpp`: `critical` / `blocked_native`, hits `6` (engine_lifecycle_delegate, native_binding, possible_delegate_unbinding)
- `Source/LyraGame/System/LyraGameInstance.cpp`: `critical` / `blocked_native`, hits `6` (delegate_unbinding, engine_lifecycle_delegate, native_binding)
- `Plugins/CommonGame/Source/Private/CommonGameInstance.cpp`: `critical` / `blocked_native`, hits `5` (dynamic_binding, native_binding)

## Recommendations

- `1` Native and arbitrary delegate lifecycle classification: Lyra includes delegate sites that require native retention, wrapper APIs, or explicit unbind policy even though Blueprint Event Dispatcher lifecycle nodes are now covered. Trigger count: `144`.
- `2` Async proxy node inventory and authoring: Lyra uses async action, cancellable async action, custom K2 async, and AbilityTask classes whose callback delegates and broadcasts need explicit exec-pin topology. Trigger count: `55`.
- `3` Lifecycle delegate safety classifier: Native AddUObject/RemoveAll and engine lifecycle delegates are shutdown-sensitive and should remain native unless explicitly wrapped. Trigger count: `141`.
- `4` GAS AbilityTask-specific async policy: AbilityTasks are async Blueprint nodes tied to prediction, ability ownership, and activation lifecycle. Trigger count: `15`.

## Safe Now

- inventory delegate/async/latent sites
- create ordinary event/function/call nodes
- declare and call Blueprint Event Dispatchers with signature graphs
- bind Blueprint Event Dispatchers to signature-compatible custom events
- assign, unbind, and clear Blueprint Event Dispatcher lifecycle nodes
- optionally create latent function call nodes when explicitly allowed
- bind some UMG widget events through existing UMG-specific command

## Unsafe Without Reinforcement

- generic delegate lifecycle authoring for non-Event-Dispatcher targets
- native or arbitrary delegate target binding without lifecycle classification
- async action proxy nodes with callback exec outputs
- native AddUObject/AddLambda lifecycle conversion
- AbilityTask conversion
- custom K2 async node recreation
