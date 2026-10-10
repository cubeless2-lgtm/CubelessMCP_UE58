# Lyra External Project Intake & Readiness Analysis

- Generated UTC: `2026-06-07T04:12:34.578663+00:00`
- Read-only project root: `D:\Git\LyraStarterGame`
- Requested project root: `auto-detected`
- Output dir: `D:\Git\unreal-mcp-cubeless\Docs\Analysis\Lyra`
- Unreal descriptor: `Lyra.uproject`
- Engine association: `5.7`

## Executive Result

- Minimum stable scope: `readiness_classification_only`
- BP conversion readiness: `partial_with_native_blockers`
- Lyra should remain a read-only corpus for this phase.
- The current UnrealMCP BP surface is useful for inventory, BP shell creation, variables, function calls, control-flow graph glue, Enhanced Input events, and compile validation.
- Full C++ to BP conversion is blocked for native-heavy Lyra systems such as replication, GAS internals, GameFeatures activation, CommonUI policy, Slate/editor modules, and custom K2 nodes.

## Project Shape

- Source/config files scanned: `734` source, `68` config
- Content root present: `True`

### Enabled Plugins

- `AESGCMHandlerComponent`: enabled
- `ActorPalette`: enabled
- `AnimationLocomotionLibrary`: enabled
- `AnimationWarping`: enabled
- `AssetReferenceRestrictions`: enabled
- `AssetSearch`: enabled
- `AsyncMixin`: enabled
- `AudioGameplay`: enabled
- `AudioGameplayVolume`: enabled
- `AudioModulation`: enabled
- `AutomatedPerfTesting`: enabled
- `CommonConversation`: enabled
- `CommonGame`: enabled
- `CommonLoadingScreen`: enabled
- `CommonUI`: enabled
- `CommonUser`: enabled
- `ContextualAnimation`: enabled
- `ControlFlows`: enabled
- `DTLSHandlerComponent`: enabled
- `DataRegistry`: enabled
- `EnhancedInput`: enabled
- `FunctionalTestingEditor`: enabled
- `GameFeatures`: enabled
- `GameSettings`: enabled
- `GameSubtitles`: enabled
- `GameplayAbilities`: enabled
- `GameplayBehaviorSmartObjects`: enabled
- `GameplayBehaviors`: enabled
- `GameplayInsights`: enabled
- `GameplayInteractions`: enabled
- `GameplayMessageRouter`: enabled
- `GameplayStateTree`: enabled
- `Gauntlet`: enabled
- `GeometryScripting`: enabled
- `Metasound`: enabled
- `ModelingToolsEditorMode`: enabled
- `ModularGameplay`: enabled
- `ModularGameplayActors`: enabled
- `MoviePipelineMaskRenderPass`: enabled
- `MovieRenderPipeline`: enabled
- ... 25 more

### Module File Counts

- `LyraGame`: 448
- `GameSettings/GameSettings`: 60
- `CommonGame/CommonGame`: 30
- `LyraEditor`: 26
- `ModularGameplayActors/ModularGameplayActors`: 16
- `CommonUser/CommonUser`: 13
- `PocketWorlds/PocketWorlds`: 12
- `GameSubtitles/GameSubtitles`: 11
- `CommonLoadingScreen/CommonLoadingScreen`: 9
- `UIExtension/UIExtension`: 8
- `GameplayMessageRouter/GameplayMessageRuntime`: 7
- `CommonLoadingScreen/CommonStartupLoadingScreen`: 6
- `LyraExtTool/LyraExtTool`: 5
- `AsyncMixin/AsyncMixin`: 4
- `GameplayMessageRouter/GameplayMessageNodes`: 4
- `LyraClient.Target.cs`: 1
- `LyraEditor.Target.cs`: 1
- `LyraGame.Target.cs`: 1
- `LyraGameEOS.Target.cs`: 1
- `LyraGameSteam.Target.cs`: 1
- `LyraGameSteamEOS.Target.cs`: 1
- `LyraServer.Target.cs`: 1
- `LyraServerEOS.Target.cs`: 1
- `LyraServerSteam.Target.cs`: 1
- `LyraServerSteamEOS.Target.cs`: 1

### Content Asset Counts

- `.uasset`: 2835
- `.umap`: 3

## Readiness Categories

### Supported Or Partially Supported

| Category | Readiness | Risk | Hits | Files | Confirmed MCP Mapping | Related Tool Groups |
| --- | --- | --- | ---: | ---: | --- | --- |
| Blueprint graph control flow and expressions | `supported_partial` | `medium` | 5390 | 437 | `add_blueprint_branch_node`, `add_blueprint_switch_int_node`, `add_blueprint_switch_enum_node`, `add_blueprint_loop_node`, `add_blueprint_array_function_node`, `add_blueprint_make_array_node` | none |
| Actor and component composition | `supported_partial` | `medium` | 743 | 180 | `create_blueprint`, `add_component_to_blueprint`, `set_component_property`, `add_blueprint_event_node` | none |
| Enhanced Input | `supported_partial` | `medium` | 58 | 21 | `create_input_mapping`, `add_blueprint_enhanced_input_action_node`, `add_blueprint_input_action_node` | none |
| Blueprint-exposed native API | `supported_partial` | `low` | 1096 | 206 | `add_blueprint_call_function_node`, `set_blueprint_pin_default`, `connect_blueprint_nodes` | none |
| Reflected Unreal API surface | `supported_inventory` | `low` | 2195 | 311 | `create_blueprint`, `add_blueprint_variable`, `add_blueprint_function_parameter`, `compile_and_validate_blueprint` | none |

### Partial / Needs Dedicated Support

| Category | Readiness | Risk | Hits | Files | Confirmed MCP Mapping | Related Tool Groups |
| --- | --- | --- | ---: | ---: | --- | --- |
| CommonUI and UMG | `partial_blocked` | `high` | 1080 | 142 | `add_blueprint_event_node`, `add_blueprint_call_function_node` | `umg_tools` |
| Gameplay Ability System | `partial_blocked` | `high` | 430 | 73 | `add_blueprint_call_function_node`, `add_blueprint_event_node`, `compile_and_validate_blueprint` | none |
| Delegates, async actions, and latent flow | `partial_blocked` | `high` | 311 | 104 | `add_blueprint_call_function_node`, `connect_blueprint_nodes` | none |
| Subsystem and lifecycle orchestration | `partial_blocked` | `high` | 142 | 63 | `add_blueprint_call_function_node`, `add_blueprint_event_node` | none |
| Gameplay Tags | `partial` | `medium` | 871 | 180 | `add_blueprint_variable`, `add_blueprint_call_function_node`, `add_blueprint_enum_literal_node` | none |
| Data assets and data registries | `partial` | `medium` | 67 | 37 | `create_blueprint`, `set_blueprint_property`, `compile_and_validate_blueprint` | none |

### Native Blockers

| Category | Readiness | Risk | Hits | Files | Confirmed MCP Mapping | Related Tool Groups |
| --- | --- | --- | ---: | ---: | --- | --- |
| Slate and editor-only C++ | `blocked_native` | `critical` | 367 | 81 | none confirmed | `editor_tools` |
| Networking and replication | `blocked_native` | `critical` | 246 | 62 | `compile_and_validate_blueprint` | none |
| Game Features and Modular Gameplay | `blocked_native` | `critical` | 214 | 47 | `compile_and_validate_blueprint` | `project_tools` |
| Custom K2 and native graph extensions | `blocked_native` | `critical` | 18 | 3 | `list_blueprint_nodes`, `compile_and_validate_blueprint` | none |

## Top Risk Files

- `Source/LyraGame/Settings/LyraSettingsLocal.cpp`: risk `critical`, readiness `blocked_native`, patterns common_ui_umg, control_flow_expression_authoring, delegates_async_latent, gameplay_tags, slate_editor_cpp
- `Source/LyraGame/System/LyraReplicationGraph.cpp`: risk `critical`, readiness `blocked_native`, patterns basic_actor_component_authoring, control_flow_expression_authoring, delegates_async_latent, networking_replication
- `Source/LyraEditor/Validation/EditorValidator.cpp`: risk `critical`, readiness `blocked_native`, patterns control_flow_expression_authoring, slate_editor_cpp
- `Source/LyraGame/GameModes/LyraExperienceManagerComponent.cpp`: risk `critical`, readiness `blocked_native`, patterns control_flow_expression_authoring, data_assets_and_registry, delegates_async_latent, game_features_modular_gameplay, networking_replication
- `Plugins/CommonGame/Source/Private/CommonPlayerInputKey.cpp`: risk `critical`, readiness `blocked_native`, patterns common_ui_umg, control_flow_expression_authoring, delegates_async_latent, slate_editor_cpp
- `Plugins/CommonLoadingScreen/Source/CommonLoadingScreen/Private/LoadingScreenManager.cpp`: risk `critical`, readiness `blocked_native`, patterns basic_actor_component_authoring, common_ui_umg, control_flow_expression_authoring, delegates_async_latent, slate_editor_cpp
- `Source/LyraGame/UI/IndicatorSystem/IndicatorDescriptor.h`: risk `critical`, readiness `blocked_native`, patterns basic_actor_component_authoring, blueprint_exposed_api, common_ui_umg, reflected_api, slate_editor_cpp
- `Source/LyraGame/Character/LyraCharacter.cpp`: risk `critical`, readiness `blocked_native`, patterns basic_actor_component_authoring, control_flow_expression_authoring, delegates_async_latent, gameplay_ability_system, gameplay_tags
- `Source/LyraGame/Player/LyraPlayerController.cpp`: risk `critical`, readiness `blocked_native`, patterns basic_actor_component_authoring, common_ui_umg, control_flow_expression_authoring, delegates_async_latent, gameplay_ability_system
- `Source/LyraGame/GameModes/LyraGameMode.cpp`: risk `critical`, readiness `blocked_native`, patterns basic_actor_component_authoring, common_ui_umg, control_flow_expression_authoring, data_assets_and_registry, delegates_async_latent
- `Source/LyraEditor/Commandlets/ContentValidationCommandlet.cpp`: risk `critical`, readiness `blocked_native`, patterns control_flow_expression_authoring, slate_editor_cpp
- `Plugins/GameFeatures/ShooterCore/Source/ShooterCoreRuntime/Public/Input/AimAssistInputModifier.h`: risk `critical`, readiness `blocked_native`, patterns blueprint_exposed_api, control_flow_expression_authoring, enhanced_input, gameplay_tags, networking_replication

## Current MCP Capability Snapshot

- Python MCP tool functions: `118`
- UnrealMCP plugin command names observed: `75`

## Minimum Stable Range

- Inventory reflected C++ API and Blueprint-exposed surface.
- Classify native-heavy systems before attempting any BP graph authoring.
- Generate BP shells or graph glue only for low/medium risk classes with existing native parent types.
- Keep GAS, replication, GameFeatures, Slate/editor modules, and custom K2 as native-blocked until dedicated support exists.

## Next Reinforcement Candidates

- Add a dedicated asset-registry intake mode to inspect Blueprint class ancestry without saving assets.
- Add delegate/event binding graph primitives before attempting async or callback-heavy conversion.
- Add CommonUI/UMG structure inventory before converting UI classes.
- Add GameplayAbility-specific classification for ability tasks, costs, cooldowns, target data, and prediction boundaries.
- Keep replication graph, RPC authority policy, custom movement, Slate/editor modules, and custom K2 nodes native-blocked unless explicit C++ support is added.

## Read-only Verification

- Analyzer uses filesystem reads only for the external project.
- Binary assets are counted by path only.
- Output is guarded so it cannot be written inside the inspected project root.
