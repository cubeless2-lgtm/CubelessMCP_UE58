# PCG Dungeon Geometry Script MVP

This MVP lives under `/Game/Cubeless/PCG/Dungeon` and is intentionally built without project C++.

## Handoff Snapshot

Use this as the current PCG-dungeon-generation handoff:

- Open level: `/Game/Cubeless/PCG/Dungeon/Maps/LVL_Cubeless_PCG_Dungeon_MVP`
- Review output actor: `MCP_Cubeless_Dungeon_MVP_NativeOutput`
- Source/control actor: `MCP_Cubeless_Dungeon_MVP_PCGBridge`
- Production graph candidate: `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration`
- Current live preset: `default`
- Current live output: `65` native components / `816` instances
- Current final gate: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGeneration_FinalGate.json`, `pass=true`
- Current visual runner summary: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGeneration_VisualGateQA_Report.json`, `success=true`, `exposure_review_pass=true`
- Current native evidence: primary refresh, smoke test, and native preview all pass with `65` native components / `816` instances
- Current native evidence summary: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeEvidenceRefresh_Report.json`, `success=true`, `mode=summarize_existing_reports`
- Current handoff readiness: `Saved/MCP_Dungeon/CubelessDungeonMVP_HandoffReadiness.json`, `pass=true`
- Current delivery closeout: `Saved/MCP_Dungeon/CubelessDungeonMVP_DeliveryCloseout.json`, `pass=true`
- Current asset manifest audit: `Saved/MCP_Dungeon/CubelessDungeonMVP_AssetManifestAudit.json`, `pass=true`
- Current live dirty state: `Saved/MCP_Dungeon/CubelessDungeonMVP_LiveDirtyState.json`, `pass=true`
- Current delivery preflight: `Saved/MCP_Dungeon/CubelessDungeonMVP_DeliveryPreflight.json`, `pass=true`
- Current screenshots: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGenerationNativeOutputOnly_active_viewport_visual_qa.png` and `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGenerationNativeOutputOnly_Oblique_active_viewport_visual_qa.png`
- Current screenshot exposure: top near-white `0.0%`, oblique near-white `0.0%`, manual review exposure volume enabled
- Delivery manifest: `docs/pcg-dungeon-delivery-manifest.md`
- Operator guide: `docs/pcg-dungeon-operator-guide.md`
- V2 roadmap: `docs/pcg-dungeon-v2-roadmap.md`
- Review checklist: `docs/pcg-dungeon-review-checklist.md`

The delivery preflight also checks latest editor log health. Older exploratory Python/API errors can remain in `Saved/Logs/StylizedCubeless.log`, but they do not block delivery when there are no matching `Error/Fatal/Assertion/Ensure` lines after the latest delivery evidence checkpoint.

The generated content folder currently contains:

- `Blueprints`: `8` placeholder Blueprint assets
- `Graphs`: `6` PCG graph assets
- `Maps`: `1` validation map
- `Materials`: `22` material assets
- `Meshes`: `34` Geometry Script-baked Static Mesh modules

Fast review command from the repository root:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --redraw-count 2
```

Native evidence refresh command after point-contract or ceiling changes:

```powershell
python Tools\Unreal\run_pcg_dungeon_native_evidence_refresh.py --redraw-count 2 --mcp-response-timeout-seconds 900 --refresh-timeout-seconds 900
```

When primary/smoke/preview evidence is already current and only the combined summary needs to be rebuilt, use the report-only path. It reads existing JSON/PNG evidence and does not touch Unreal:

```powershell
python Tools\Unreal\run_pcg_dungeon_native_evidence_refresh.py --summarize-existing
```

Preset validation command pattern:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --preset compact_branching --archive-label compact_branching_postprocess --redraw-count 2
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --preset default --archive-label default_restored_after_postprocess_preset_suite --redraw-count 2
```

Read-only V1 handoff readiness check:

```powershell
python Tools\Unreal\check_pcg_dungeon_handoff_readiness.py
```

One-command local closeout for the current PCG-dungeon V1 delivery:

```powershell
python Tools\Unreal\run_pcg_dungeon_delivery_closeout.py
```

The closeout refreshes live dirty state, the read-only asset manifest audit, native evidence summary, handoff readiness, delivery preflight, and `git diff --check`.

For day-to-day use, follow `docs/pcg-dungeon-operator-guide.md`. It covers opening the level, reviewing the NativeOutput actor, applying presets, restoring default, and checking failures.

For later alternate dungeon work, use `docs/pcg-dungeon-v2-roadmap.md` as planning context. It is not part of the current V1 completion gate.

Before staging or reviewing the V1 work, use `docs/pcg-dungeon-review-checklist.md` to confirm the source scope and generated-evidence exclusions.

Do not treat gameplay placeholder reports as the active completion gate. The active scope is PCG dungeon generation only; player flow, reward items, shop UI, enemy AI, boss combat, and exit travel are deliberately outside this gate.

## Main Assets

- Level: `/Game/Cubeless/PCG/Dungeon/Maps/LVL_Cubeless_PCG_Dungeon_MVP`
- PCG graph: `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_Bridge`
- PCG native point-source graph: `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativePointSource`
- PCG native skeleton graph: `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeSkeleton`
- PCG native integration graph: `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration`
- PCG native preview point-source graph: `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativePointSource_PreviewOffset`
- PCG native preview integration graph: `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration_PreviewOffset`
- Bridge actor in the level: `MCP_Cubeless_Dungeon_MVP_PCGBridge`
- Native integration test actor in the level: `MCP_Cubeless_Dungeon_MVP_NativeIntegrationTest`
- Native integration output actor in the level: `MCP_Cubeless_Dungeon_MVP_NativeOutput`
- Native integration preview actor in the level: `MCP_Cubeless_Dungeon_MVP_NativeIntegrationPreview`
- Geometry Script modules: `/Game/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_*` (`34` current module meshes)
- Materials: `/Game/Cubeless/PCG/Dungeon/Materials/M_Dungeon_*` including room theme materials `M_Dungeon_Theme_*`
- Authoring script: `Plugins/CustomTools/Content/Python/ArtScripts/CubelessDungeonPCG.py`
- PCG entrypoint: `Plugins/CustomTools/Content/Python/ArtScripts/CubelessDungeonPCGEntrypoint.py`
- Gameplay placeholder Blueprints: `/Game/Cubeless/PCG/Dungeon/Blueprints/BP_DungeonGameplay_*Placeholder`
- Gameplay data export: `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayData.json`
- Gameplay placeholder placement report: `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayPlaceholder_Report.json`
- Gameplay placeholder visual validation report: `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayPlaceholderVisual_Report.json`
- Gameplay interaction contract: `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayInteractionContract.json`
- Gameplay content outcome contract: `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayContentOutcomeContract.json`
- Gameplay flow simulation report: `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayFlowSimulation_Report.json`
- Gameplay Blueprint state event validation report: `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayStateEventValidation_Report.json`
- PCG spawner contract export: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGSpawnerContract.json`
- PCG native graph handoff export: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGraphHandoff.json`
- PCG native point-source readiness report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePointSource_Report.json`
- PCG native point-source graph report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePointSourceGraph_Report.json`
- PCG native skeleton graph report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeSkeletonGraph_Report.json`
- PCG native skeleton audit report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeSkeletonAudit_Report.json`
- PCG native integration graph report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeIntegrationGraph_Report.json`
- PCG native integration audit report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeIntegrationAudit_Report.json`
- PCG native integration test actor report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeIntegrationTestActor_Report.json`
- PCG native integration smoke test report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeIntegrationTest_Report.json`
- PCG native integration output report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeIntegrationOutput_Report.json`
- PCG native output-only review report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeOutputOnlyReview_Report.json`
- PCG native primary refresh report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePrimaryRefresh_Report.json`
- PCG native primary refresh final gate report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePrimaryRefresh_FinalGate.json`
- PCG structure/orientation audit report: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGStructureAudit_Report.json`
- PCG generation final gate report: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGeneration_FinalGate.json`
- PCG generation refresh report: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGeneration_Refresh_Report.json`
- PCG generation parameter-scale report: `Saved/MCP_Dungeon/CubelessDungeonMVP_GenerationParameterScale_Report.json`
- PCG authoring surface report: `Saved/MCP_Dungeon/CubelessDungeonMVP_AuthoringSurface_Report.json`
- PCG authoring preset smoke report: `Saved/MCP_Dungeon/CubelessDungeonMVP_AuthoringPresetSmoke_Report.json`
- PCG native preview point-source graph report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePointSourcePreviewGraph_Report.json`
- PCG native preview integration graph report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeIntegrationPreviewGraph_Report.json`
- PCG native integration preview report: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeIntegrationPreview_Report.json`
- PCG native preview active-viewport screenshot: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePreview_active_viewport_visual_qa.png`
- PCG native preview native-only window screenshot: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePreview_NativeOnly_Window.png`
- PCG native preview side-by-side screenshot: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePreview_SideBySide_active_viewport_visual_qa.png`
- PCG native preview review screenshot: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePreview_ReviewReloadedLit_active_viewport_visual_qa.png`
- PCG native output-only review screenshot: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeOutputOnly_active_viewport_visual_qa.png`
- PCG native primary refresh screenshot: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePrimaryRefresh_active_viewport_visual_qa.png`
- Text minimap export: `Saved/MCP_Dungeon/CubelessDungeonMVP_Minimap.txt`

## Regeneration

Select or locate `MCP_Cubeless_Dungeon_MVP_PCGBridge`, then refresh its PCG component. The bridge graph executes `CubelessDungeonPCGEntrypoint.py`, which reads the bridge actor tags and regenerates the current validation dungeon.

The script always clears actors whose label starts with `MCP_Dungeon_MVP_` before spawning the new dungeon. It does not delete the PCG bridge actor.

## Gameplay Placeholder Actors

Gameplay implementation is currently paused. These assets remain as metadata/placeholders so dungeon anchors stay stable, but the active goal is PCG dungeon generation rather than playable reward, shop, enemy, boss, or exit behavior.

`create_or_update_gameplay_placeholders()` creates or refreshes the first playable placeholder layer from `CubelessDungeonMVP_GameplayData.json`.

It creates the Blueprint assets under `/Game/Cubeless/PCG/Dungeon/Blueprints` if they do not exist, then deletes and respawns only actors tagged `DungeonGameplayPlaceholder` or labeled with `MCP_Dungeon_Gameplay_*`. It does not delete PCG validation actors, native PCG output, the bridge actor, or preview/test PCG actors.

Current placeholder Blueprint assets:

- `BP_DungeonGameplay_PlayerStartPlaceholder`
- `BP_DungeonGameplay_ExitPlaceholder`
- `BP_DungeonGameplay_KeyPickupPlaceholder`
- `BP_DungeonGameplay_LockedGatePlaceholder`
- `BP_DungeonGameplay_RewardPlaceholder`
- `BP_DungeonGameplay_EnemySpawnPlaceholder`
- `BP_DungeonGameplay_BossSpawnPlaceholder`
- `BP_DungeonGameplay_ShopPlaceholder`

The latest placeholder placement passed and spawned `22` actors: player start `1`, exit `1`, shop `1`, key `1`, reward `4`, locked gate `1`, enemy `12`, and boss `1`. The locked gate placeholder carries `DungeonLockId` and `DungeonRequiredKeyId`; the key placeholder carries `DungeonKeyId` and `DungeonUnlocksLockIds`.

Run this helper after a dungeon refresh when gameplay placeholders should match the latest anchors.

Each gameplay placeholder Blueprint now has a visible StaticMeshComponent using existing Geometry Script Static Mesh assets:

- player start: `SM_GS_Dungeon_SpawnMarker`
- exit: `SM_GS_Dungeon_Detail_Arch`
- key: `SM_GS_Dungeon_RoomVariant_ProgressionRune`
- locked gate: `SM_GS_Dungeon_LockedDoorSeal`
- reward: `SM_GS_Dungeon_Detail_Pedestal`
- shop: `SM_GS_Dungeon_Detail_Counter`
- enemy spawn: `SM_GS_Dungeon_RoomVariant_CombatPartition`
- boss spawn: `SM_GS_Dungeon_Detail_BossFocus`

`validate_gameplay_placeholder_visual_components()` writes `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayPlaceholderVisual_Report.json`. The latest validation passed with actor count `22`, component failures `0`, and mesh failures `0`.

`build_gameplay_interaction_contract()` tags the live placeholder actors and writes `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayInteractionContract.json`. This is the C++-free handoff for later Blueprint, UI, AI, and interaction implementation.

It records these interaction kinds:

- `player_spawn`
- `exit_activation`
- `shop_open`
- `key_pickup`
- `reward_chest`
- `exit_unlock_reward`
- `locked_gate`
- `enemy_spawn`
- `boss_spawn`

The helper adds `DungeonInteractionContractId`, `DungeonInteractionKind`, and `DungeonInteractionState` to each live placeholder actor. Gate/key actors also carry `DungeonInteractionLockId`, `DungeonInteractionRequiredKeyId`, and `DungeonInteractionUnlocksLockIds` where applicable.

The latest interaction contract passed with `22` contract entries, key-gate linkage pass `true`, placeholder coverage pass `true`, actor update errors `0`, and dirty package count `0`.

Each placeholder Blueprint now has the common runtime handoff variables `InteractionContractId`, `InteractionKind`, `InteractionState`, `bInteractionReady`, and `bInteractionConsumed`. `BP_DungeonGameplay_KeyPickupPlaceholder` also has an instance-editable `LinkedGateActor` reference. `build_gameplay_interaction_contract()` writes those values onto the live placeholder actor instances and assigns each key to its matching locked gate actor. The latest instance variable update error count is `0`, and the latest key linked-gate reference pass is `true`.

`simulate_gameplay_interaction_flow()` writes `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayFlowSimulation_Report.json`. It validates the contract order without running PIE: player spawn ready, key pickup, locked gate unlock, reward availability, encounter spawn availability, and exit activation readiness. The latest flow simulation passed and opened `D142857_Lock_000` with `D142857_Key_000`.

`validate_gameplay_blueprint_state_events()` writes `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayStateEventValidation_Report.json`. It calls live placeholder Blueprint custom events in the editor, validates the resulting instance state variables and visual component visibility, then rebuilds the interaction contract and restores the initial placeholder visual state. The latest validation passed with key event results `1`, non-key event results `21`, validation failures `0`, visual validation failures `0`, reset failures `0`, and visual reset failures `0`.

`build_gameplay_content_outcome_contract()` writes `Saved/MCP_Dungeon/CubelessDungeonMVP_GameplayContentOutcomeContract.json`. This is the next C++-free handoff layer for replacing placeholders with real content later: player start, inventory key token, gate unlock token, loot proxy, shop service proxy, exit flow proxy, enemy spawn proxy, and boss spawn proxy. The latest contract passed with outcome count `22`, state-event coverage count `22`, failure count `0`, and outcome kind counts: player start `1`, exit flow `1`, shop service `1`, inventory key `1`, loot `4`, gate unlock `1`, enemy spawn `12`, and boss spawn `1`.

Gameplay placeholder Blueprints now have minimal callable runtime graph entry points:

- `BP_DungeonGameplay_KeyPickupPlaceholder`: custom event `DungeonInteract` sets `bInteractionConsumed=true`, `bInteractionReady=false`, and `InteractionState=consumed`.
- `BP_DungeonGameplay_LockedGatePlaceholder`: custom event `DungeonUnlockGate` sets `InteractionState=unlocked`, `bInteractionReady=true`, and `bInteractionConsumed=false`.
- `BP_DungeonGameplay_RewardPlaceholder`: custom event `DungeonOpenReward` sets `bInteractionConsumed=true`, `bInteractionReady=false`, and `InteractionState=opened`.
- `BP_DungeonGameplay_ShopPlaceholder`: custom event `DungeonOpenShop` sets `bInteractionConsumed=false`, `bInteractionReady=true`, and `InteractionState=open`.
- `BP_DungeonGameplay_ExitPlaceholder`: custom event `DungeonActivateExit` sets `bInteractionConsumed=false`, `bInteractionReady=true`, and `InteractionState=active`; `DungeonUseExit` sets `bInteractionConsumed=true`, `bInteractionReady=false`, and `InteractionState=completed`.
- `BP_DungeonGameplay_EnemySpawnPlaceholder`: custom event `DungeonSpawnEnemy` sets `bInteractionConsumed=true`, `bInteractionReady=false`, and `InteractionState=spawned`.
- `BP_DungeonGameplay_BossSpawnPlaceholder`: custom event `DungeonSpawnBoss` sets `bInteractionConsumed=true`, `bInteractionReady=false`, and `InteractionState=boss_spawned`.

`BP_DungeonGameplay_KeyPickupPlaceholder` also routes `ActorBeginOverlap` into the same consumed-state setter chain, so the key placeholder now has a minimal overlap-driven pickup state transition. Both `ActorBeginOverlap` and direct `DungeonInteract` then call `DungeonUnlockGate` on `LinkedGateActor`. Both Key and LockedGate Blueprints compile with `compile_error_count=0` and `compile_warning_count=0`.

## Native PCG Smoke Test

`MCP_Cubeless_Dungeon_MVP_NativeIntegrationTest` is a separate PCGVolume that references `PCG_Cubeless_Dungeon_MVP_NativeIntegration`. It is used only for native PCG smoke testing; the bridge actor remains the normal validation/regeneration path for now.

Native PCG generation and cleanup are asynchronous across editor ticks. Use the staged helper flow instead of checking results in the same Python call:

- `begin_native_integration_smoke_test()` creates/refreshes the test actor, cleans previous PCG output, and requests generation.
- After an editor tick, `verify_native_integration_smoke_generation(request_cleanup=True)` verifies the native output and requests cleanup.
- After another editor tick, `verify_native_integration_smoke_cleanup()` verifies that no native PCG generated Static Mesh components or instances remain on the test actor.

The expected smoke result is `65` generated Instanced Static Mesh components and `816` instances during generation, then `0` residual components and `0` residual instances after cleanup.

## Native PCG Output Candidate

`MCP_Cubeless_Dungeon_MVP_NativeOutput` is the current production candidate PCGVolume. It references the unoffset `PCG_Cubeless_Dungeon_MVP_NativeIntegration` graph and keeps generated PCG output visible in the validation level.

Use the staged output helper flow:

- `begin_native_integration_output(keep_existing=False)` creates or refreshes the output actor, cleans previous output, and requests native PCG generation.
- After an editor tick, `verify_native_integration_output_generation()` verifies that the production native output exists and records the runtime component/instance summary.
- `cleanup_native_integration_output(destroy_actor=False)` requests cleanup when the native production candidate should be hidden; pass `destroy_actor=True` only when the output PCGVolume should also be removed.

The latest checked output generated `65` Instanced Static Mesh components and `816` instances at the origin, with bounds min `[-4220.74, -3414.0, -1000.0]` and max `[5020.74, 4283.0, 1000.0]`. The bridge StaticMeshActor output remains in the same level as the current data/contract source, so this native output intentionally overlaps the bridge output for now.

For native-output-only review, use:

- `set_native_output_only_review_mode(True)` hides the bridge-generated `MCP_Dungeon_MVP_*` StaticMeshActor validation output and the offset native preview PCG actor, leaving the production `MCP_Cubeless_Dungeon_MVP_NativeOutput` generated output visible.
- `setup_native_output_only_review_camera(camera_height=14500.0, y_backoff=2600.0)` frames that output for active viewport capture.
- `restore_native_output_only_review_mode()` restores bridge StaticMeshActor and preview PCG visibility when side-by-side or contract review is needed again.
- `verify_native_output_only_review_restore_roundtrip()` verifies restore and re-enable behavior, records the result in the native output-only review report, and ends back in native-output-only mode.

The latest native-output-only review passed: bridge visible Static Mesh components `0`, hidden bridge Static Mesh components `816`, visible preview Static Mesh components `0`, hidden preview Static Mesh components `65`, native output generation `65` components / `816` instances, and active viewport screenshot QA `true`.

Restore roundtrip was also checked in the previous native-output-only review workflow. Current saved review state is NativeOutput-only: bridge validation StaticMeshActors and the offset preview actor are hidden, while `MCP_Cubeless_Dungeon_MVP_NativeOutput` remains generated.

For a full native primary refresh from the bridge config, use:

- `begin_native_primary_output_refresh(keep_existing_output=False)` refreshes the bridge validation dungeon/contract, rebuilds native point-source/skeleton/integration graph reports and audits, then requests production native output generation.
- After an editor tick, `verify_native_primary_output_refresh(enable_output_only_review=True, save_dirty_packages=True)` verifies native output generation, enables native-output-only review mode, frames the output camera, saves dirty packages, and writes `CubelessDungeonMVP_NativePrimaryRefresh_Report.json`.
- `record_native_primary_refresh_artifacts()` attaches the latest primary-refresh screenshot QA and staged native smoke-test result to the same primary refresh report.
- `record_native_primary_refresh_final_gate()` writes `CubelessDungeonMVP_NativePrimaryRefresh_FinalGate.json` and confirms the primary refresh report, screenshot QA, staged native smoke test, output-only review report, gameplay Blueprint state event validation report, gameplay placeholder visual validation report, live native output actor, and current dirty package count agree.

The latest staged native primary refresh passed: bridge validation dungeon `998` actors, PCG spawn points `816`, native point-source graph pass `true`, native integration graph pass `true`, native integration audit pass `true`, native output `65` components / `816` instances, output-only review pass `true`, camera success `true`, dirty package count `0`, screenshot QA `true`, smoke test pass `true`, and final gate pass `true`.

The latest closed-ceiling native evidence refresh supersedes the old `630`-instance primary route: primary refresh, staged native smoke test, primary final gate context, and native preview all pass with `65` native components / `816` instances. The current primary screenshot QA and preview side-by-side screenshot QA both pass, and live dirty package count is `0`.

## Current PCG Generation Gate

The current goal is PCG dungeon generation, not gameplay implementation. `audit_pcg_dungeon_structure_and_orientation()` writes `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGStructureAudit_Report.json`, and `record_pcg_generation_final_gate()` writes `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGeneration_FinalGate.json`.

The latest PCG generation gate passed with:

- production graph `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration`
- production output actor `MCP_Cubeless_Dungeon_MVP_NativeOutput`
- bridge actor role `validation_and_point_source_export`
- rooms `11`
- room graph edges `12`
- branch/loop edges `2`
- PCG spawn points `816`
- native output components `65`
- native output instances `816`
- review post-process volumes `1`
- seed suite `5/5`
- PCG generation refresh report `true`
- generation parameter-scale smoke test `true` (`DungeonGridCellSize=520`, `DungeonCorridorWidth=280` sample)
- authoring surface validation `true`
- authoring preset apply/restore smoke `true`
- direction yaw mismatches `0`
- tagged yaw mismatches `0`
- missing wall directions `0`
- single-mesh spawner group failures `0`
- material split group failures `0`
- native-output-only review light components hidden `17`
- top screenshot QA `true`
- oblique screenshot QA `true`
- visual runner exposure review `true` (`bright_percent=0.0`, `near_white_percent=0.0` for both current screenshots)
- top/oblique screenshot QA reports newer than the latest PCG generation refresh `true`
- dirty package count `0`

This PCG generation gate intentionally ignores gameplay implementation readiness. Gameplay placeholder and contract reports can remain useful metadata, but reward items, shop UI, enemy AI, boss combat, and exit travel are not part of the current gate.

For current PCG generation screenshots, native-output-only review mode hides the bridge StaticMeshActor validation output, the offset native preview output, and bridge-generated review PointLight/ThemeLight actors. The intentionally kept review DirectionalLight/SkyLight remain available, and the generated `MCP_Dungeon_MVP_ReviewExposureVolume` fixes manual exposure and reduces bloom so closed-ceiling captures do not turn into white clipped blocks against the black editor background.

For a PCG-generation-only refresh from the bridge config, use:

- `begin_pcg_generation_refresh_from_bridge(keep_existing_output=False)` validates the bridge tag authoring surface, runs preset apply/restore smoke and scale-parameter smoke checks, regenerates the validation dungeon and point contracts, rebuilds native point-source/skeleton/integration graph reports and audits, then requests production native output generation.
- `begin_pcg_generation_refresh_with_authoring_preset("default", keep_existing_output=False)` first applies one documented bridge authoring preset, then runs the same staged PCG-generation refresh begin step. Use this route when changing from `default` to `compact_branching`, `wide_looped`, or `open_cutaway`.
- After an editor tick, `verify_pcg_generation_refresh(enable_output_only_review=True, save_dirty_packages=True)` verifies native output generation, runs the structure/orientation audit, enables native-output-only review mode, frames the output camera, saves dirty packages, and writes `CubelessDungeonMVP_PCGGeneration_Refresh_Report.json`.
- `setup_pcg_generation_oblique_review_camera()` frames the current NativeOutput from the standard oblique review angle after native-output-only review mode is enabled.
- `record_pcg_generation_final_gate()` then requires the refresh report, structure audit, scale report, authoring reports, screenshot QA reports, live native output, and dirty package count to agree. The top and oblique screenshot QA reports must also be newer than the latest PCG generation refresh report. Screenshot QA is still captured by the separate top/oblique screenshot helpers before the final gate is claimed.

The local runner `Tools/Unreal/run_pcg_dungeon_generation_visual_gate_qa.py` wraps the visual gate closeout:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --redraw-count 2
```

It enables NativeOutput-only review mode, clears the current editor actor selection before each capture, captures the top active viewport screenshot QA, records PNG exposure statistics, sets the standard oblique camera, captures the oblique active viewport screenshot QA, and records the final PCG generation gate. It does not create or overwrite viewport bookmarks.

To apply a documented preset, wait for the PCG output refresh, recapture screenshots, and close the gate in one command:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --preset default --redraw-count 2
```

Replace `default` with `compact_branching`, `wide_looped`, or `open_cutaway` when that preset should become the live bridge configuration. Use `--refresh-current` instead of `--preset` when the bridge tags have already been edited manually. The runner polls `verify_pcg_generation_refresh()` because native PCG output generation can complete several editor ticks after the refresh request.
The runner defaults to `600s` for both refresh polling and long UnrealMCP `execute_python` socket responses because native output generation has been observed taking over seven minutes on `open_cutaway` and default restore runs.

Use `--archive-label <label>` when a preset run should keep its reports and PNG screenshots before another preset overwrites the fixed report paths:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --preset wide_looped --archive-label wide_looped_postprocess --redraw-count 2
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --preset default --archive-label default_restored_after_postprocess_preset_suite --redraw-count 2
```

Archive output is written under `Saved/MCP_Dungeon/PresetQA/<label>/`.
The archived summary report is copied after the runner writes the `archive` metadata block, so the copied JSON should include `archive.pass`.

## Native PCG Preview

`MCP_Cubeless_Dungeon_MVP_NativeIntegrationPreview` is a separate PCGVolume that references the preview-only `PCG_Cubeless_Dungeon_MVP_NativeIntegration_PreviewOffset` graph and intentionally keeps generated output visible for editor review. The production `PCG_Cubeless_Dungeon_MVP_NativePointSource` and `PCG_Cubeless_Dungeon_MVP_NativeIntegration` graphs stay unoffset for smoke/audit validation.

Use the staged preview helper flow:

- `begin_native_integration_preview(preview_offset=[14000.0, 0.0, 0.0], keep_existing=False)` creates/refreshes the preview-only point-source and integration graphs, applies the offset to point transforms, creates/refreshes the preview actor, cleans previous preview output, and requests native PCG generation.
- After an editor tick, `verify_native_integration_preview_generation()` verifies that the native output exists and records the runtime component/instance summary.
- `setup_native_preview_side_by_side_review_camera(camera_height=27000.0, y_backoff=3600.0)` frames the bridge output and native preview output for the current review screenshot.
- `cleanup_native_integration_preview(destroy_actor=False)` requests cleanup when the preview should be hidden; pass `destroy_actor=True` only when the preview PCGVolume should also be removed.

The latest checked preview generated `65` Instanced Static Mesh components and `816` instances. The clean side-by-side screenshot route wrote `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePreview_SideBySide_active_viewport_visual_qa.png` and passed capture QA with dirty package count `0`. Current delivery preflight treats this as the active native preview freshness evidence.

Visual review result: the bridge output is visible on the left and the native preview output is visible on the right. The two layouts are separated and non-empty, using the same dungeon structure. Prototype exposure is still high, so this is a generation/comparison screenshot, not an art-quality lighting pass.

## Tag Controls

The bridge actor uses actor tags as the current no-C++ authoring surface:

- `DungeonSeed=142857`
- `DungeonRoomCount=11`
- `DungeonCeilingStride=1`
- `DungeonChestCount=3`
- `DungeonEnemyCount=4`
- `DungeonKeyCount=1`
- `DungeonShopCount=1`
- `DungeonLockedDoorCount=1`
- `DungeonBossEnabled=1`
- `DungeonBranchChancePercent=100`
- `DungeonMaxLoopEdges=2`
- `DungeonGridCellSize=400`
- `DungeonCorridorWidth=400`
- `DungeonUseCeiling=1`
- `DungeonUseThemeMaterials=1`
- `DungeonPreviewMode=1`

Notes:

- `DungeonSeed` controls deterministic layout.
- `DungeonRoomCount` is clamped to `2..32`.
- `DungeonCeilingStride=1` covers every generated cell with a ceiling module. `DungeonCeilingStride=0` disables ceiling sample tiles, and higher values create a more open review cutaway.
- `DungeonChestCount` and `DungeonEnemyCount` now consume rooms after start, exit, key, shop, and boss roles are assigned.
- `DungeonKeyCount`, `DungeonShopCount`, and `DungeonLockedDoorCount` add progression roles without C++.
- `DungeonBossEnabled=0` keeps the exit marker but disables the boss role marker.
- `DungeonBranchChancePercent` and `DungeonMaxLoopEdges` are applied to room graph loop/branch edges.
- `DungeonUseCeiling=0` disables generated ceiling modules. The `open_cutaway` preset keeps this off for top-down structural review.
- `DungeonUseThemeMaterials=0` disables theme material overrides and keeps baked mesh materials.
- `DungeonGridCellSize` is applied to world spacing and base Static Mesh actor XY scale.
- `DungeonCorridorWidth` is applied to corridor module width, door frame width, connector width, locked seal width, and related corridor detail width scale.
- `DungeonPreviewMode` is currently a metadata/review-policy tag.

`validate_authoring_surface()` writes `Saved/MCP_Dungeon/CubelessDungeonMVP_AuthoringSurface_Report.json`. It documents the supported tag names, aliases, min/max ranges, default values, and example presets, then validates the live bridge actor tags. The current report passes with no missing required config tags, no unknown `Dungeon*=` config tags, no duplicate canonical fields, no clamped/invalid values, and all `4` example presets passing layout validation.

Current documented presets:

- `default`: the saved bridge defaults, `11` rooms, `2` loop edges, and full ceiling coverage with `CeilingStride=1`.
- `compact_branching`: `8` rooms, tighter `GridCellSize=340`, `CorridorWidth=260`, `1` loop edge, and full ceiling coverage.
- `wide_looped`: `11` rooms, wider `GridCellSize=520`, `CorridorWidth=360`, `4` loop edges, and full ceiling coverage. This was reduced from an earlier 14-room draft because the 14-room version passed its selected seed but failed the 5-seed suite.
- `open_cutaway`: default room count with ceiling disabled for clearer review.
- `small_route`: `7` rooms, compact spacing, and `1` loop edge for fast layout iteration.
- `long_route`: `11` rooms, limited loop budget, and longer route-distance seed window.
- `loop_dense`: `11` rooms and `5` loop edges for branch/connector stress checks.
- `boss_focus`: `10` rooms, combat-heavy role budget, and prominent boss/exit route.

Run the layout-only authoring preset matrix before spending time on visual QA for new presets:

```powershell
python Tools\Unreal\run_pcg_dungeon_authoring_preset_matrix.py --seed-count 5
```

The current preset matrix covers all `8` documented presets with `5` seeds each and passes with no failed seeds.

Ceiling generation now uses Geometry Script-baked Static Mesh modules split by PCG mesh key: `ceiling_room`, `ceiling_corridor`, and `ceiling_corner`. The latest default output spawns `172` room ceiling points, `33` corridor ceiling points, and `4` corner ceiling points, so each ceiling mesh key remains grouped into its own native PCG Static Mesh Spawner branch.

Preset helper workflow:

- `apply_authoring_preset_to_bridge("wide_looped")` replaces the bridge actor's supported dungeon config tags with canonical tags from the preset and saves dirty packages by default.
- After applying a preset, refresh the bridge PCG component to regenerate the validation dungeon from the new tags.
- `begin_pcg_generation_refresh_with_authoring_preset("wide_looped")` is the staged one-call begin route when the intended result is a regenerated PCG dungeon, native point-source handoff, and NativeOutput generation request from that preset.
- `validate_authoring_preset_apply_restore_smoke("wide_looped")` is the non-destructive safety check used by the gate. It applies the preset, validates the parsed config and layout summary, restores the exact original bridge tags, validates the authoring surface again, saves dirty packages, and writes `Saved/MCP_Dungeon/CubelessDungeonMVP_AuthoringPresetSmoke_Report.json`.
- The current saved bridge tags remain the `default` preset after the smoke test.

## Progression Roles

After layout generation, the script analyzes the room graph and assigns a deterministic dungeon progression layer:

- main route from start room to exit room
- side rooms branching from that route
- locked progression doors near the later route
- key rooms reachable before the first locked gate
- shop, treasure, combat, boss, start, and exit roles

The locked-door visual uses `SM_GS_Dungeon_LockedDoorSeal`, which is also baked from Geometry Script. The role data is written to the `progression` block in `Saved/MCP_Dungeon/CubelessDungeonMVP_Report.json`.

## Gameplay Data Layer

Every generated actor now receives stable tags for downstream Blueprint, AI, or validation work:

- `DungeonGenerated`
- `DungeonModule=<cell|ceiling|wall|door|column|stair|marker|locked_door_seal|connector_detail|corridor_detail|room_variant_detail|light|theme_light|review_postprocess|volume|door_anchor|spawn_anchor|route_anchor|encounter_spawn|reward_anchor|detail_anchor|detail_mesh|player_start|nav_bounds|nav_waypoint>`
- `DungeonMeshKey=<module_mesh_key>` and `DungeonStaticMeshPath=<asset_path>` on generated StaticMesh actors; this is the current contract for grouping future PCG Static Mesh Spawner nodes
- `DungeonMaterialMode=<baked|override>` and `DungeonMaterialName=<material_name>` on generated StaticMesh actors where a material override is applied
- `DungeonSeed=<seed>`
- `DungeonCell=<x,y>` where a grid cell applies
- `DungeonRoomId=<id>` where a room applies
- `DungeonCellKind=<room|corridor|room_center|room_corner|room_corridor_edge|exit_room>`
- `DungeonRole=<start|exit|boss|key|shop|treasure|combat|locked_after>`
- `DungeonGameplayRole=<role>` on gameplay marker actors
- `DungeonDoorKind=<normal|locked>` on room-corridor doors and locked-door seals
- `DungeonVolumeKind=<room|door|gate>` on TriggerBox gameplay volumes
- `DungeonRoomArchetype=<start_chamber|main_combat_room|side_combat_room|key_room|shop_room|treasure_vault|boss_exit_chamber>` on room-aware generated actors including room volumes, route anchors, spawn/reward anchors, room detail anchors/meshes, room variant details, and theme lights
- `DungeonRoomTheme=<entry|combat|progression|utility|reward|finale>` on room volumes, room detail anchors, and room detail meshes
- `DungeonRoomSizeClass=<small|medium|large>` on room volumes, room detail anchors, and room detail meshes
- `DungeonRoomAreaCells=<count>` and `DungeonRoomGraphDegree=<count>` on room volumes, room detail anchors, and room detail meshes
- `DungeonRoomShape=<archetype_shape>`, `DungeonRoomShapeFamily=<balanced|large_square|wide|tall|compact|vault|arena>`, and `DungeonRoomShapeAxis=<balanced|wide|tall>` on room-aware gameplay and detail actors
- `DungeonThemeName=<entry|combat|progression|utility|reward|finale|corridor>` on themed floor/corridor, ceiling, wall, door, column, stair, and room detail mesh actors
- `DungeonThemeMaterial=<material_name>` and `DungeonThemeRole=<archetype_or_connector>` on themed StaticMesh actors
- `DungeonLightKind=<theme_room|room|review_directional|review_sky>` on generated light actors
- `DungeonLightProfile=<profile>`, `DungeonLightIndex=<index>`, `DungeonLightIntensity=<value>`, and `DungeonLightRadius=<value>` on room theme light actors
- `DungeonReviewExposure=manual`, `DungeonReviewExposureBias=<value>`, and `DungeonReviewBloomIntensity=<value>` on the generated review PostProcessVolume
- `DungeonConnectorKind=<threshold|locked_threshold>`, `DungeonConnectorIndex=<index>`, and `DungeonConnectorMeshKey=<connector_threshold|connector_locked>` on generated room-corridor connector detail actors
- `DungeonDoorAnchorLabel=<actor_label>` on connector detail actors
- `DungeonCorridorDetailKind=<straight|corner|junction|endcap|doorway>`, `DungeonCorridorDetailIndex=<index>`, `DungeonCorridorDetailMeshKey=<mesh_key>`, and `DungeonCorridorDetailYaw=<degrees>` on generated corridor detail actors
- `DungeonCorridorNeighborDirs=<dir_list>` and `DungeonCorridorRoomDirs=<dir_list>` on generated corridor detail actors
- `DungeonRoomVariantKind=<entry_focus_inlay|combat_partition|progression_rune|utility_market|reward_border|finale_ring|ambient_rubble>`, `DungeonRoomVariantIndex=<index>`, `DungeonRoomVariantMeshKey=<mesh_key>`, and `DungeonRoomVariantYaw=<degrees>` on generated room variant detail actors
- `DungeonDoorIndex=<index>` on TargetPoint door anchors
- `DungeonRoomCell=<x,y>` and `DungeonCorridorCell=<x,y>` on TargetPoint door anchors
- `DungeonDoorInteraction=entry` on TargetPoint door anchors
- `DungeonSpawnKind=<start|exit|boss|key|shop|treasure|combat>` on TargetPoint spawn anchors
- `DungeonEncounterId=<seed_room_id>` on gameplay-facing actors that belong to an encounter room
- `DungeonEncounterKind=<safe|combat|utility|reward|boss>`
- `DungeonEncounterTier=<start|light|standard|elite|key_reward|shop|treasure|final>`
- `DungeonRewardKind=<none|key|shop|treasure|exit_unlock>`
- `DungeonLockState=<none|pre_gate|after_gate>`
- `DungeonSpawnBudget=<integer>`
- `DungeonLockId=<id>` and `DungeonRequiredKeyId=<id>` on locked door-facing actors
- `DungeonLockIndex`, `DungeonLockPathIndex`, `DungeonLockBeforeRoomId`, and `DungeonLockAfterRoomId` on locked door-facing actors
- `DungeonKeyId=<id>`, `DungeonUnlocksLockIds=<id_list>`, and `DungeonUnlockCount=<count>` on key marker and key spawn anchor actors
- `DungeonEncounterSpawnKind=<enemy|boss>` on TargetPoint encounter spawn slots
- `DungeonEncounterSlotIndex=<index>` and `DungeonEncounterSlotCount=<count>` on TargetPoint encounter spawn slots
- `DungeonEncounterSpawnIndex=<global_index>` on TargetPoint encounter spawn slots
- `DungeonRewardAnchorKind=<key|shop|treasure|exit_unlock>` on TargetPoint reward anchors
- `DungeonRewardId=<id>` and `DungeonRewardIndex=<index>` on TargetPoint reward anchors
- `DungeonInteractionKind=<pickup|shop|chest|exit_unlock>` on TargetPoint reward anchors
- `DungeonRewardSourceEncounterId=<encounter_id>` on TargetPoint reward anchors
- `DungeonDetailKind=<kind>`, `DungeonDetailIndex=<global_index>`, `DungeonDetailLocalIndex=<room_index>`, and `DungeonDetailSocket=<center|north_wall|south_wall|east_wall|west_wall>` on TargetPoint room detail anchors and StaticMeshActor room detail meshes
- `DungeonDetailPlacement=pcg_room_detail` on TargetPoint room detail anchors and StaticMeshActor room detail meshes
- `DungeonDetailMeshKey=<detail_module_key>`, `DungeonDetailMeshLabel=<actor_label>`, and `DungeonDetailMeshIndex=<index>` on TargetPoint room detail anchors
- `DungeonDetailMeshKey=<detail_module_key>`, `DungeonDetailMeshIndex=<index>`, and `DungeonDetailAnchorLabel=<actor_label>` on StaticMeshActor room detail meshes
- `DungeonPlaytestRole=<player_start|nav_bounds>` on generated playtest actors
- `DungeonPlaytestIndex=<index>` on generated playtest actors
- `DungeonFacesRoomId=<room_id>` on the generated PlayerStart
- `DungeonNavBoundsExtent=<x,y,z>`, `DungeonNavBoundsPadding=<cm>`, and `DungeonCoveredCellCount=<count>` on the generated NavMeshBoundsVolume
- `DungeonWaypointIndex=<index>` on generated TargetPoint nav waypoints
- `DungeonWaypointKind=<room|corridor>` on generated TargetPoint nav waypoints
- `DungeonWaypointDegree=<count>` and `DungeonWaypointNeighbors=<direction:cell|...>` on generated TargetPoint nav waypoints
- `DungeonRouteKind=<main|side>` on TargetPoint route anchors
- `DungeonRouteIndex=<index>` on TargetPoint route anchors
- `DungeonMainPathRoom=<0|1>` and `DungeonMainPathIndex=<index>` on TargetPoint route anchors
- `DungeonRouteDistanceFromStart=<rooms>` and `DungeonRouteDistanceToExit=<rooms>` on TargetPoint route anchors

The current default level has `1173` generated actors tagged with `DungeonGenerated`, `12` gameplay marker actors, `24` TargetPoint door anchors, `24` StaticMeshActor connector detail meshes, `37` StaticMeshActor corridor detail meshes, `11` StaticMeshActor room variant detail meshes, `209` generated ceiling actors, `1` generated review PostProcessVolume, `12` TargetPoint spawn anchors, `11` TargetPoint route anchors, `13` TargetPoint encounter spawn slots, `6` TargetPoint reward anchors, `24` TargetPoint room detail anchors, `24` StaticMeshActor room detail meshes, `2` generated playtest actors, `209` TargetPoint nav waypoints, `11` PointLight room theme lights, `36` TriggerBox gameplay volumes, `11` room archetype records, `11` room theme records, `11` room shape records, and `11` room encounter records:

- room volumes `11`
- door volumes `24`
- locked gate volumes `1`
- normal door anchors `23`
- locked door anchors `1`
- connector detail meshes `24`
- connector threshold meshes `23`
- connector locked threshold meshes `1`
- corridor detail meshes `37`
- corridor detail straight meshes `10`
- corridor detail corner meshes `3`
- corridor detail junction meshes `4`
- corridor detail endcap/doorway meshes `20`
- ceiling meshes `209`
- ceiling room meshes `172`
- ceiling corridor meshes `33`
- ceiling corner meshes `4`
- review post-process volumes `1`
- room variant detail meshes `11`
- room variant entry focus inlay meshes `1`
- room variant combat partition meshes `4`
- room variant utility market meshes `1`
- room variant progression rune meshes `1`
- room variant reward border meshes `3`
- room variant finale ring meshes `1`
- main route anchors `5`
- side route anchors `6`
- enemy encounter spawn slots `12`
- boss encounter spawn slots `1`
- reward anchors `6`
- room detail anchors `24`
- room detail meshes `24`
- room archetype-tagged room volumes `11`
- room shape records `11`
- themed StaticMesh actors `816`
- PCG static mesh spawn points `816`
- PCG static mesh spawner groups by `DungeonMeshKey` `32`
- PCG spawner groups with material variants `9`
- PCG native graph handoff point streams `32`
- PCG native graph material-safe spawner branches `65`
- PCG native point-source readiness points `816`
- PCG native point-source readiness groups `32`
- PCG native point-source output connection allowed `false`
- PCG native point-source graph points `816`
- PCG native point-source graph material-safe branches `65`
- PCG native point-source graph Static Mesh Spawner nodes `0`
- PCG native skeleton graph nodes `140`
- PCG native skeleton Static Mesh Spawner nodes `65`
- PCG native integration graph nodes `141`
- PCG native integration graph Static Mesh Spawner nodes `65`
- PCG native integration graph output connected `true`
- PCG native integration smoke generated Instanced Static Mesh components `65`
- PCG native integration smoke generated instances `816`
- PCG native integration smoke residual components after cleanup `0`
- PCG native integration smoke residual instances after cleanup `0`
- PCG native preview point-source graph points `816`
- PCG native preview point-source graph location offset `[14000, 0, 0]`
- PCG native preview integration graph output connected `true`
- PCG native integration preview generated Instanced Static Mesh components `65`
- PCG native integration preview generated instances `816`
- PCG native integration preview screenshot QA pass `true`
- PCG native integration preview review screenshot QA pass `true`
- room theme lights `11`
- legacy review lights `8`
- generated PlayerStart `1`
- generated NavMeshBoundsVolume `1`
- nav waypoints `206`
- lock-key links `1`

The locked-door tags are deliberately narrow: one sealed door and one gate volume carry `DungeonDoorKind=locked`.

`CubelessDungeonMVP_GameplayData.json` uses schema `cubeless_pcg_dungeon_gameplay_data_v1` and records config, rooms, room archetypes, room themes, room shapes, cells, room graph edges, progression, lock-key links, encounters, markers, TriggerBox volume records, TargetPoint door records, StaticMeshActor connector detail records, StaticMeshActor corridor detail records, StaticMeshActor room variant detail records, TargetPoint spawn records, TargetPoint route records, TargetPoint encounter spawn records, TargetPoint reward records, TargetPoint room detail anchor records, StaticMeshActor room detail mesh records, PointLight theme light records, playtest records, TargetPoint navigation waypoint records, PCG spawner contract data, PCG graph handoff data, native point-source readiness summary, module counts, and connectivity.

`CubelessDungeonMVP_PCGSpawnerContract.json` uses schema `cubeless_pcg_spawner_contract_v1`. It exports the current StaticMeshActor validation result as future PCG point data: one point per generated StaticMesh actor, grouped by `DungeonMeshKey`. TargetPoint, TriggerBox, PlayerStart, NavMeshBoundsVolume, and Light actors remain gameplay/validation actors outside the static mesh spawner groups.

`CubelessDungeonMVP_PCGGraphHandoff.json` uses schema `cubeless_pcg_graph_handoff_v1`. It turns the spawner contract into native PCG graph authoring metadata: one point stream/filter candidate per `DungeonMeshKey`, the target Static Mesh path for each future PCG Static Mesh Spawner, material split requirements, and an explicit exclusion policy for gameplay actor outputs.

`CubelessDungeonMVP_NativePointSource_Report.json` uses schema `cubeless_pcg_dungeon_native_point_source_v1`. It normalizes the spawner contract into the point data that a future native PCG point source should provide: point transform, `DungeonMeshKey`, `DungeonStaticMeshPath`, `DungeonMaterialMode`, optional `DungeonMaterialName`, diagnostic source label, and room/theme attributes. Its `promotion_policy.output_connection_allowed` is `false`, because the native graph still needs a real point-source node or data-source actor before the skeleton merge can connect to Output.

`PCG_Cubeless_Dungeon_MVP_NativePointSource` is a native PCG point-source candidate generated from the readiness report. It uses material-safe `PCGCreatePointsSettings` branches and branch-level `PCGAddAttributeSettings` nodes for `DungeonMeshKey`, `DungeonStaticMeshPath`, `DynamicMeshPath`, `DungeonMaterialMode`, and `DungeonMaterialName`. It outputs PCG points only, has no Static Mesh Spawner nodes, and is not connected to the native Static Mesh Spawner skeleton yet.

`PCG_Cubeless_Dungeon_MVP_NativeSkeleton` is a native PCG graph skeleton generated from the handoff. It contains `DungeonMeshKey` attribute filters, material split filters, and material-safe Static Mesh Spawner nodes. Its merge branch is intentionally not connected to graph Output yet, so it does not replace the bridge or generate production output until a native point source is promoted.

`CubelessDungeonMVP_NativeSkeletonAudit_Report.json` uses schema `cubeless_pcg_dungeon_native_skeleton_audit_v1`. It compares the saved graph handoff against the actual native skeleton graph node titles, node descriptions, class counts, Static Mesh Spawner mesh entries, and material override entries. This catches drift before the skeleton is promoted to production output.

`PCG_Cubeless_Dungeon_MVP_NativeIntegration` is the current native spawning candidate. It uses one `PCGSubgraphSettings` node with `subgraph_override` pointing at `PCG_Cubeless_Dungeon_MVP_NativePointSource`, then filters the emitted points by `DungeonMeshKey` and `DungeonMaterialName`, spawns the Geometry Script-baked Static Meshes, merges the branches, and connects to graph Output. The older `PCG_Cubeless_Dungeon_MVP_NativeSkeleton` stays output-disconnected as a safe diagnostic reference.

`CubelessDungeonMVP_NativeIntegrationAudit_Report.json` uses schema `cubeless_pcg_dungeon_native_integration_audit_v1`. It verifies the integration graph's subgraph override, expected filter/spawner titles, class counts, Static Mesh Spawner mesh entries, material overrides, node descriptions, and the saved output-connected policy.

`CubelessDungeonMVP_NativeIntegrationTest_Report.json` uses schema `cubeless_pcg_dungeon_native_integration_smoke_v1`. It records the staged native PCG smoke test: generation request, generation verification, cleanup request, and cleanup verification. The latest passing run generated `65` Instanced Static Mesh components with `816` total instances, then cleaned the test actor back to `0` residual generated components and `0` residual instances.

`CubelessDungeonMVP_NativeIntegrationOutput_Report.json` uses schema `cubeless_pcg_dungeon_native_integration_output_v1`. It records the kept production candidate output actor, graph reference, generation request, runtime component/instance summary, bounds, cleanup policy, and bridge overlap policy. The latest passing run generated `65` Instanced Static Mesh components with `816` total instances on `MCP_Cubeless_Dungeon_MVP_NativeOutput`.

`CubelessDungeonMVP_NativePrimaryRefresh_Report.json` uses schema `cubeless_pcg_dungeon_native_primary_refresh_v1`. It records the staged production refresh path: bridge validation dungeon/contract refresh, native point-source graph refresh, native skeleton graph/audit refresh, native integration graph/audit refresh, native output generation request, native output verification, native-output-only review activation, camera setup, dirty package save result, screenshot QA evidence, and staged native smoke-test evidence.

`CubelessDungeonMVP_PCGStructureAudit_Report.json` uses schema `cubeless_pcg_dungeon_structure_orientation_audit_v1`. It records the current PCG-generation-focused structure audit: native target graph/output actor, exposed generation parameters, room graph summary, spawner grouping validation, wall/door/connector orientation checks, and config coverage.

`CubelessDungeonMVP_GenerationParameterScale_Report.json` uses schema `cubeless_pcg_dungeon_generation_parameter_scale_v1`. It is a C++-free smoke test that samples non-default `DungeonGridCellSize` and `DungeonCorridorWidth` values without modifying level actors, then verifies world spacing, wall offsets, base module scale, corridor width scale, door/connector width scale, and scaled detail offsets.

`CubelessDungeonMVP_AuthoringSurface_Report.json` uses schema `cubeless_pcg_dungeon_authoring_surface_v1`. It records the current no-C++ tag authoring contract, including supported tags, aliases, ranges, defaults, presets, live bridge tag health, and preset layout smoke results.

`CubelessDungeonMVP_AuthoringPresetSmoke_Report.json` uses schema `cubeless_pcg_dungeon_authoring_preset_smoke_v1`. It records the preset apply/restore safety check for the current live bridge actor. The current smoke uses `wide_looped`, applies `11` rooms and `4` loop edges, then restores the exact original default bridge tags.

`CubelessDungeonMVP_AuthoringPresetMatrix_Report.json` uses schema `cubeless_pcg_dungeon_authoring_preset_matrix_v1`. It records layout-only QA for all documented presets across a seed window before any NativeOutput or screenshot work is run. The runner wrapper writes `CubelessDungeonMVP_AuthoringPresetMatrixRunner_Report.json`.

`CubelessDungeonMVP_PCGGeneration_Refresh_Report.json` uses schema `cubeless_pcg_dungeon_generation_refresh_v1`. It records the PCG-generation-only refresh path: bridge validation dungeon/contract refresh, authoring surface validation, preset apply/restore smoke, scale-parameter smoke, native point-source/skeleton/integration graph refresh, native output generation request, native output verification, structure/orientation audit, native-output-only review activation, camera setup, and dirty package save result. It intentionally excludes gameplay implementation validation.

`CubelessDungeonMVP_PCGGeneration_FinalGate.json` uses schema `cubeless_pcg_dungeon_generation_final_gate_v1`. It is the current goal gate for PCG dungeon generation and intentionally does not require gameplay implementation readiness. It also requires the PCG generation refresh report, generation parameter-scale smoke report, authoring surface report, authoring preset smoke report, and the current top and oblique native-output-only screenshot QA reports to load, pass, point at non-empty PNG files, be newer than the latest PCG generation refresh report, and add no dirty packages during capture. The current native-output-only review mode hides bridge validation StaticMeshActors, the offset native preview output, and bridge-generated review PointLight/ThemeLight actors before screenshot capture. The visual runner additionally clears selected actors before each capture and requires the current PNG exposure review to pass, so closed-ceiling captures with white clipping do not count as a clean visual closeout.

`CubelessDungeonMVP_GameplayPlaceholder_Report.json` uses schema `cubeless_pcg_dungeon_gameplay_placeholder_v1`. It records the generated placeholder Blueprint assets, cleanup result, spawned gameplay placeholder actors, role counts, lock-key tag linkage, and save/dirty package result.

`CubelessDungeonMVP_GameplayInteractionContract.json` uses schema `cubeless_pcg_dungeon_gameplay_interaction_contract_v1`. It records one interaction contract per gameplay placeholder actor, updates live placeholder tags with contract ids/kinds/states, validates key-to-gate linkage, checks placeholder coverage, and records save/dirty package result.

`CubelessDungeonMVP_GameplayContentOutcomeContract.json` uses schema `cubeless_pcg_dungeon_gameplay_content_outcome_contract_v1`. It records the C++-free content handoff for every interaction contract: player start, key inventory token, gate unlock token, loot proxy, shop service proxy, exit flow proxy, enemy spawn proxy, and boss spawn proxy. It is a contract-only layer; it does not claim final reward items, shop UI, AI, boss combat, or exit travel are implemented.

## Current Validation

For the current PCG-dungeon-generation goal, use `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGeneration_FinalGate.json` as the active gate. Current native primary, smoke, preview, preset archive, and screenshot evidence now matches the refreshed closed-ceiling output unless a report is explicitly archived as historical evidence.

Current delivery preflight now also checks native evidence freshness for the refreshed closed-ceiling contract and the V1 handoff readiness report. The combined native evidence summary must exist and pass, native primary refresh, native smoke test, and native preview must all match the active gate's `65` components / `816` instances, and the handoff report must confirm that the production NativeIntegration graph, NativeOutput actor, screenshots, asset audit, and dirty-state evidence all agree. Historical work-log entries and archived screenshots may still record earlier `630` or `642` instance passes as development history.

Current PCG generation gate expected result:

- PCG generation final gate pass `true`
- structure/orientation audit pass `true`
- gameplay implementation required `false`
- room count `11`
- room graph edges `12`
- branch/loop edges `2`
- PCG spawn points `816`
- native output components `65`
- native output instances `816`
- V1 handoff readiness pass `true`
- seed suite `5/5`
- PCG generation refresh report `true`
- generation parameter-scale smoke test `true`
- authoring surface validation `true`
- authoring preset apply/restore smoke `true`
- direction yaw mismatches `0`
- tagged yaw mismatches `0`
- missing wall directions `0`
- single-mesh spawner group failures `0`
- material split group failures `0`
- native-output-only review light components hidden `17`
- top screenshot QA `true`
- oblique screenshot QA `true`
- top/oblique screenshot QA reports newer than latest PCG generation refresh `true`
- visual gate QA runner pass `true`
- preset-backed visual gate runner pass `true` with `default`
- authoring preset matrix pass `true` for `8` presets x `5` seeds
- archived `wide_looped` preset visual gate runner pass `true` with `64` native components / `764` instances
- archived `compact_branching` preset visual gate runner pass `true` with `64` native components / `624` instances
- archived `open_cutaway` preset visual gate runner pass `true` with `61` native components / `573` instances
- archived `small_route` preset visual gate runner pass `true` with `64` native components / `529` instances
- archived `long_route` preset visual gate runner pass `true` with `65` native components / `757` instances
- archived `loop_dense` preset visual gate runner pass `true` with `64` native components / `854` instances
- archived `boss_focus` preset visual gate runner pass `true` with `65` native components / `701` instances
- default restore after non-default preset validation pass `true` with `65` native components / `816` instances
- archived summary metadata and exposure review pass `true` for `wide_looped_postprocess`, `compact_branching_postprocess`, `open_cutaway_postprocess`, and `default_restored_after_postprocess_preset_suite`
- dirty package count `0`

The current default report is written to `Saved/MCP_Dungeon/CubelessDungeonMVP_Report.json`.
Native PCG graph generation and audit checks are written to the separate `CubelessDungeonMVP_Native*` reports listed above.

Legacy default bridge / primary-refresh expected result:

- `source=pcg_bridge`
- `room_count=11`
- `edge_count=12`
- `cell_count=206`
- `door_count=23`
- `locked_door_count=1`
- `locked_door_spawn_count=1`
- `actor_count=998`
- `connectivity.connected=true`
- `progression.pass=true`
- gameplay export schema `cubeless_pcg_dungeon_gameplay_data_v1`
- gameplay rooms `11`
- gameplay cells `206`
- gameplay markers `12`
- gameplay volumes `35`
- gameplay door points `23`
- gameplay connector details `23`
- gameplay corridor details `34`
- gameplay room variant details `11`
- gameplay spawn points `12`
- gameplay route points `11`
- gameplay encounter spawn points `13`
- gameplay reward points `6`
- gameplay room detail points `24`
- gameplay room detail meshes `24`
- gameplay room archetypes `11`
- gameplay room themes `11`
- gameplay room shapes `11`
- gameplay PCG spawner contract pass `true`
- gameplay PCG graph handoff pass `true`
- gameplay PCG static mesh spawn points `816`
- gameplay PCG spawner groups `32`
- gameplay PCG spawner material-variant groups `9`
- gameplay PCG graph handoff streams `32`
- gameplay PCG graph material-safe spawner branches `65`
- native point-source readiness pass `true`
- native point-source readiness points `816`
- native point-source readiness groups `32`
- native point-source missing required attributes `0`
- native point-source invalid transforms `0`
- native point-source stream count mismatches `0`
- native point-source material split count mismatches `0`
- native point-source output connection allowed `false`
- native point-source graph pass `true`
- native point-source graph branches `65`
- native point-source graph points `816`
- native point-source graph nodes `391`
- native point-source graph CreatePoints nodes `65`
- native point-source graph AddAttribute nodes `325`
- native point-source graph StaticMeshSpawner nodes `0`
- native point-source graph failed edges `0`
- native point-source graph setup errors `0`
- native point-source graph output connected `true`
- native skeleton graph pass `true`
- native skeleton graph nodes `140`
- native skeleton graph AttributeFilter nodes `74`
- native skeleton graph StaticMeshSpawner nodes `65`
- native skeleton graph failed edges `0`
- native skeleton graph output connected `false`
- native skeleton audit pass `true`
- native skeleton audit missing filters `0`
- native skeleton audit missing spawners `0`
- native skeleton audit spawner mismatches `0`
- native skeleton audit class-count mismatches `0`
- native skeleton audit duplicate titles `0`
- native skeleton audit missing descriptions `0`
- native integration graph pass `true`
- native integration graph nodes `141`
- native integration graph Subgraph nodes `1`
- native integration graph AttributeFilter nodes `74`
- native integration graph StaticMeshSpawner nodes `65`
- native integration graph failed edges `0`
- native integration graph setup errors `0`
- native integration graph output connected `true`
- native integration audit pass `true`
- native integration audit subgraph override mismatches `0`
- native integration audit missing filters `0`
- native integration audit missing spawners `0`
- native integration audit spawner mismatches `0`
- native integration audit class-count mismatches `0`
- native integration audit duplicate titles `0`
- native integration audit missing descriptions `0`
- native integration test actor pass `true`
- native integration smoke test pass `true`
- native integration smoke status `passed`
- native integration smoke generated components `65`
- native integration smoke generated instances `816`
- native integration smoke cleanup residual components `0`
- native integration smoke cleanup residual instances `0`
- native integration output pass `true`
- native integration output status `generated`
- native integration output generated components `65`
- native integration output generated instances `816`
- native output-only review pass `true`
- native output-only review bridge visible components `0`
- native output-only review bridge hidden components `816`
- native output-only review preview visible components `0`
- native output-only review preview hidden components `65`
- native output-only review screenshot QA pass `true`
- native primary refresh pass `true`
- native primary refresh status `passed`
- native primary refresh dungeon actors `998`
- native primary refresh PCG spawn points `816`
- native primary refresh native output components `65`
- native primary refresh native output instances `816`
- native primary refresh output-only review pass `true`
- native primary refresh camera success `true`
- native primary refresh screenshot QA pass `true`
- native primary refresh smoke test pass `true`
- native primary refresh smoke generated components `65`
- native primary refresh smoke generated instances `816`
- native primary refresh smoke cleanup residual components `0`
- native primary refresh smoke cleanup residual instances `0`
- native primary refresh final gate pass `true`
- native primary refresh final gate live output components `65`
- native primary refresh final gate live output instances `816`
- native primary refresh final gate dirty package count `0`
- native primary refresh final gate gameplay state event validation pass `true`
- native primary refresh final gate gameplay state event failure count `0`
- native primary refresh final gate gameplay state event visual failure count `0`
- native primary refresh final gate gameplay state event reset failure count `0`
- native primary refresh final gate gameplay state event visual reset failure count `0`
- native primary refresh final gate gameplay content outcome contract pass `true`
- native primary refresh final gate gameplay content outcome count `22`
- native primary refresh final gate gameplay content outcome failure count `0`
- native integration preview pass `true`
- native integration preview status `generated`
- native integration preview generated components `65`
- native integration preview generated instances `816`
- native integration preview active screenshot QA pass `true`
- native integration preview side-by-side screenshot QA pass `true`
- native integration preview review screenshot QA pass `true`
- gameplay theme lights `11`
- gameplay playtest points `2`
- gameplay navigation waypoints `206`
- gameplay lock-key links `1`
- gameplay encounters `11`
- gameplay placeholder report schema `cubeless_pcg_dungeon_gameplay_placeholder_v1`
- gameplay placeholder pass `true`
- gameplay placeholder spawned actors `22`
- gameplay placeholder player start `1`
- gameplay placeholder exit `1`
- gameplay placeholder shop `1`
- gameplay placeholder key `1`
- gameplay placeholder reward `4`
- gameplay placeholder locked gate `1`
- gameplay placeholder enemy `12`
- gameplay placeholder boss `1`
- gameplay placeholder lock-key linkage pass `true`
- gameplay placeholder dirty package count after save `0`
- gameplay placeholder visual validation pass `true`
- gameplay placeholder visual actor count `22`
- gameplay placeholder visual component failures `0`
- gameplay placeholder visual mesh failures `0`
- gameplay interaction contract schema `cubeless_pcg_dungeon_gameplay_interaction_contract_v1`
- gameplay interaction contract pass `true`
- gameplay interaction contract entries `22`
- gameplay interaction key-gate linkage pass `true`
- gameplay interaction placeholder coverage pass `true`
- gameplay interaction actor update errors `0`
- gameplay interaction instance variable update errors `0`
- gameplay interaction key linked-gate reference pass `true`
- gameplay interaction key linked-gate reference count `1`
- gameplay interaction dirty package count after save `0`
- gameplay flow simulation schema `cubeless_pcg_dungeon_gameplay_flow_simulation_v1`
- gameplay flow simulation pass `true`
- gameplay flow simulation opened locks `1`
- gameplay flow simulation inventory keys `1`
- gameplay Blueprint state event validation pass `true`
- gameplay Blueprint state event validation key event results `1`
- gameplay Blueprint state event validation event results `21`
- gameplay Blueprint state event validation failures `0`
- gameplay Blueprint state event validation visual failures `0`
- gameplay Blueprint state event validation reset failures `0`
- gameplay Blueprint state event validation visual reset failures `0`
- key pickup Blueprint `DungeonInteract` graph compile errors `0`
- key pickup Blueprint `DungeonInteract` graph compile warnings `0`
- key pickup Blueprint `ActorBeginOverlap` route compile errors `0`
- key pickup Blueprint `ActorBeginOverlap` route compile warnings `0`
- key pickup Blueprint `LinkedGateActor -> DungeonUnlockGate` dispatch nodes `2`
- locked gate Blueprint `DungeonUnlockGate` graph compile errors `0`
- locked gate Blueprint `DungeonUnlockGate` graph compile warnings `0`
- reward Blueprint `DungeonOpenReward` graph compile errors `0`
- reward Blueprint `DungeonOpenReward` graph compile warnings `0`
- shop Blueprint `DungeonOpenShop` graph compile errors `0`
- shop Blueprint `DungeonOpenShop` graph compile warnings `0`
- exit Blueprint `DungeonActivateExit` / `DungeonUseExit` graph compile errors `0`
- exit Blueprint `DungeonActivateExit` / `DungeonUseExit` graph compile warnings `0`
- enemy Blueprint `DungeonSpawnEnemy` graph compile errors `0`
- enemy Blueprint `DungeonSpawnEnemy` graph compile warnings `0`
- boss Blueprint `DungeonSpawnBoss` graph compile errors `0`
- boss Blueprint `DungeonSpawnBoss` graph compile warnings `0`

Expected gameplay volume counts:

- room_volume `11`
- door_volume `23`
- gate_volume `1`

Expected TargetPoint door anchor counts for the default seed:

- normal `22`
- locked `1`

Expected StaticMeshActor connector detail counts for the default seed:

- connector detail actors `23`
- connector detail JSON records `23`
- threshold `22`
- locked_threshold `1`
- connector_threshold mesh `22`
- connector_locked mesh `1`

Expected StaticMeshActor corridor detail counts for the default seed:

- corridor detail actors `34`
- corridor detail JSON records `34`
- straight `10`
- corner `2`
- junction `3`
- endcap `17`
- doorway `2`
- corridor_detail_straight mesh `10`
- corridor_detail_corner mesh `2`
- corridor_detail_junction mesh `3`
- corridor_detail_endcap mesh `19`

Expected StaticMeshActor room variant detail counts for the default seed:

- room variant detail actors `11`
- room variant detail JSON records `11`
- entry_focus_inlay `1`
- combat_partition `4`
- utility_market `1`
- progression_rune `1`
- reward_border `3`
- finale_ring `1`
- room_variant_entry_inlay mesh `1`
- room_variant_combat_partition mesh `4`
- room_variant_utility_market mesh `1`
- room_variant_progression_rune mesh `1`
- room_variant_reward_border mesh `3`
- room_variant_finale_ring mesh `1`
- missing room variant required tags `0`

Expected TargetPoint spawn anchor counts:

- start `1`
- exit `1`
- boss `1`
- key `1`
- shop `1`
- treasure `3`
- combat `4`

Expected TargetPoint route anchor counts:

- main `5`
- side `6`

Expected TargetPoint encounter spawn slot counts:

- enemy `12`
- boss `1`
- tier standard `6`
- tier elite `4`
- tier light `2`
- tier final `1`

Expected TargetPoint reward anchor counts:

- key `1`
- shop `1`
- treasure `3`
- exit_unlock `1`
- interaction pickup `1`
- interaction shop `1`
- interaction chest `3`
- interaction exit_unlock `1`

Expected room archetype counts:

- start_chamber `1`
- main_combat_room `3`
- side_combat_room `1`
- key_room `1`
- shop_room `1`
- treasure_vault `3`
- boss_exit_chamber `1`

Expected room theme counts:

- entry `1`
- combat `4`
- progression `1`
- utility `1`
- reward `3`
- finale `1`
- themed StaticMesh actors `816`
- missing theme tags `0`

Expected PCG static mesh spawner contract counts:

- contract schema `cubeless_pcg_spawner_contract_v1`
- contract pass `true`
- static mesh spawn points `816`
- spawner groups by `DungeonMeshKey` `32`
- material-variant spawner groups `9`
- missing `DungeonMeshKey` tags `0`
- missing `DungeonStaticMeshPath` tags `0`
- unknown mesh keys `0`
- static mesh path conflict groups `0`
- largest groups: wall `220`, floor `172`, ceiling_room `172`, column `44`, ceiling_corridor `33`, corridor `33`, door `24`, connector_threshold `23`, detail_mesh `24`, corridor_detail_endcap `20`

Expected PCG native graph handoff counts:

- handoff schema `cubeless_pcg_graph_handoff_v1`
- handoff pass `true`
- point streams by `DungeonMeshKey` `32`
- mesh-only Static Mesh Spawner branches `32`
- material-safe Static Mesh Spawner branches `65`
- material-variant groups requiring `DungeonMaterialName` split or override support `9`
- excluded actor output classes: `TargetPoint`, `TriggerBox`, `PlayerStart`, `NavMeshBoundsVolume`, `PointLight`, `DirectionalLight`, and `SkyLight`

Expected PCG native point-source readiness counts:

- point-source schema `cubeless_pcg_dungeon_native_point_source_v1`
- point-source pass `true`
- point-source status `ready_contract_only`
- output connection allowed `false`
- normalized native points `816`
- native point groups by `DungeonMeshKey` `32`
- source contract point count `816`
- handoff stream point total `816`
- source contract group count `32`
- handoff stream count `32`
- missing required attributes `0`
- invalid transforms `0`
- missing handoff stream keys `0`
- handoff streams without points `0`
- static mesh path mismatches `0`
- material attribute mismatches `0`
- stream count mismatches `0`
- material split count mismatches `0`
- duplicate point indexes `0`
- duplicate source labels `0`

Expected PCG native point-source graph counts:

- graph asset `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativePointSource`
- report schema `cubeless_pcg_dungeon_native_point_source_graph_v1`
- report pass `true`
- material-safe source branches `65`
- native PCG points `816`
- graph nodes `391`
- `PCGCreatePointsSettings` nodes `65`
- `PCGAddAttributeSettings` nodes `325`
- `PCGMergeSettings` nodes `1`
- `PCGStaticMeshSpawnerSettings` nodes `0`
- authored edges `391`
- failed edges `0`
- setup errors `0`
- output connected `true`
- spawns Static Meshes `false`
- branch attributes: `DungeonMeshKey`, `DungeonStaticMeshPath`, `DynamicMeshPath`, `DungeonMaterialMode`, and `DungeonMaterialName`

Expected PCG native skeleton graph counts:

- graph asset `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeSkeleton`
- report schema `cubeless_pcg_dungeon_native_skeleton_graph_report_v1`
- report pass `true`
- graph nodes `140`
- `PCGAttributeFilteringSettings` nodes `74`
- `PCGStaticMeshSpawnerSettings` nodes `65`
- `PCGMergeSettings` nodes `1`
- authored edges `204`
- failed edges `0`
- setup errors `0`
- output connected `false`

Expected PCG native skeleton audit counts:

- audit schema `cubeless_pcg_dungeon_native_skeleton_audit_v1`
- audit pass `true`
- expected mesh filter titles `32`
- expected material filter titles `42`
- expected spawner titles `65`
- actual graph nodes `140`
- actual `PCGAttributeFilteringSettings` nodes `74`
- actual `PCGStaticMeshSpawnerSettings` nodes `65`
- actual `PCGMergeSettings` nodes `1`
- missing filter titles `0`
- missing spawner titles `0`
- unexpected spawner titles `0`
- spawner mesh/material mismatches `0`
- class-count mismatches `0`
- duplicate node titles `0`
- missing node descriptions `0`

Expected PCG native integration graph counts:

- graph asset `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration`
- report schema `cubeless_pcg_dungeon_native_integration_graph_report_v1`
- report pass `true`
- source point graph loaded `true`
- source point graph report pass `true`
- source point graph points `816`
- graph nodes `141`
- `PCGSubgraphSettings` nodes `1`
- `PCGAttributeFilteringSettings` nodes `74`
- `PCGStaticMeshSpawnerSettings` nodes `65`
- `PCGMergeSettings` nodes `1`
- authored edges `205`
- failed edges `0`
- setup errors `0`
- output connected `true`
- spawns Static Meshes `true`
- input node connected `false`

Expected PCG native integration audit counts:

- audit schema `cubeless_pcg_dungeon_native_integration_audit_v1`
- audit pass `true`
- expected mesh filter titles `32`
- expected material filter titles `42`
- expected spawner titles `65`
- actual graph nodes `141`
- actual `PCGSubgraphSettings` nodes `1`
- actual `PCGAttributeFilteringSettings` nodes `74`
- actual `PCGStaticMeshSpawnerSettings` nodes `65`
- actual `PCGMergeSettings` nodes `1`
- missing filter titles `0`
- missing spawner titles `0`
- unexpected spawner titles `0`
- spawner mesh/material mismatches `0`
- class-count mismatches `0`
- duplicate node titles `0`
- missing node descriptions `0`
- subgraph override mismatches `0`

Expected PCG native integration smoke counts:

- test actor `MCP_Cubeless_Dungeon_MVP_NativeIntegrationTest`
- actor report schema `cubeless_pcg_dungeon_native_integration_test_v1`
- actor report pass `true`
- smoke report schema `cubeless_pcg_dungeon_native_integration_smoke_v1`
- smoke report status `passed`
- smoke report pass `true`
- generation request ok `true`
- generation verification pass `true`
- generated `InstancedStaticMeshComponent` count `65`
- generated instance count `816`
- cleanup request ok `true`
- cleanup verification pass `true`
- cleanup residual Static Mesh component count `0`
- cleanup residual Static Mesh instance count `0`

Expected PCG native integration output counts:

- output actor `MCP_Cubeless_Dungeon_MVP_NativeOutput`
- output graph asset `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration`
- output report schema `cubeless_pcg_dungeon_native_integration_output_v1`
- output graph role `production_output_candidate`
- output report status `generated`
- output report pass `true`
- generation request ok `true`
- generation verification pass `true`
- generated `InstancedStaticMeshComponent` count `65`
- generated instance count `816`
- generated bounds min `[-4220.74, -3414.0, -1000.0]`
- generated bounds max `[5020.74, 4283.0, 1000.0]`
- bridge overlap policy `bridge StaticMeshActor output remains the data/contract source`

Expected PCG native output-only review counts:

- review report schema `cubeless_pcg_dungeon_native_output_only_review_v1`
- review report status `native_output_only_enabled`
- review report pass `true`
- hidden bridge StaticMeshActor validation output actors `816`
- visible bridge Static Mesh components after hide `0`
- hidden bridge Static Mesh components after hide `816`
- preview actor found `true`
- visible preview Static Mesh components after hide `0`
- hidden preview Static Mesh components after hide `65`
- native output generation pass `true`
- native output generated `InstancedStaticMeshComponent` count `65`
- native output generated instance count `816`
- active viewport screenshot QA pass `true`
- screenshot path `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeOutputOnly_active_viewport_visual_qa.png`
- current saved review state bridge visible components `0`
- current saved review state preview visible components `0`
- restore roundtrip re-enabled bridge visible components `0`
- restore roundtrip re-enabled preview visible components `0`
- restore roundtrip report pass `true`

Expected PCG native primary refresh counts:

- primary refresh report schema `cubeless_pcg_dungeon_native_primary_refresh_v1`
- primary refresh report status `passed`
- primary refresh report pass `true`
- refreshed validation dungeon actor count `981`
- refreshed validation PCG spawn points `816`
- refreshed native point-source graph pass `true`
- refreshed native skeleton graph pass `true`
- refreshed native skeleton audit pass `true`
- refreshed native integration graph pass `true`
- refreshed native integration audit pass `true`
- refreshed native output generation pass `true`
- refreshed native output generated `InstancedStaticMeshComponent` count `65`
- refreshed native output generated instance count `816`
- refreshed native output-only review pass `true`
- refreshed native output-only camera success `true`
- save dirty packages result `true`
- dirty package count after save `0`
- screenshot QA pass `true`
- screenshot path `Saved/MCP_Dungeon/CubelessDungeonMVP_NativePrimaryRefresh_active_viewport_visual_qa.png`
- primary refresh report embeds screenshot QA evidence `true`
- primary refresh report embeds smoke test evidence `true`
- embedded smoke test pass `true`
- embedded smoke generated `InstancedStaticMeshComponent` count `65`
- embedded smoke generated instance count `816`
- embedded smoke cleanup residual Static Mesh component count `0`
- embedded smoke cleanup residual Static Mesh instance count `0`
- final gate report schema `cubeless_pcg_dungeon_native_primary_refresh_final_gate_v1`
- final gate report status `passed`
- final gate report pass `true`
- final gate failed check count `0`
- final gate gameplay Blueprint state event validation pass `true`
- final gate gameplay Blueprint state event failure count `0`
- final gate gameplay Blueprint state event visual failure count `0`
- final gate gameplay Blueprint state event reset failure count `0`
- final gate gameplay Blueprint state event visual reset failure count `0`
- final gate gameplay placeholder visual validation pass `true`
- final gate gameplay placeholder visual component failure count `0`
- final gate gameplay placeholder visual mesh failure count `0`
- final gate gameplay content outcome contract pass `true`
- final gate gameplay content outcome count `22`
- final gate gameplay content outcome state-event coverage count `22`
- final gate gameplay content outcome failure count `0`
- final gate live native output generated `InstancedStaticMeshComponent` count `65`
- final gate live native output generated instance count `816`
- final gate live dirty package count `0`

Expected PCG native integration preview counts:

- preview actor `MCP_Cubeless_Dungeon_MVP_NativeIntegrationPreview`
- preview point-source graph asset `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativePointSource_PreviewOffset`
- preview integration graph asset `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration_PreviewOffset`
- preview point-source graph report schema `cubeless_pcg_dungeon_native_point_source_preview_graph_v1`
- preview integration graph report schema `cubeless_pcg_dungeon_native_integration_preview_graph_report_v1`
- preview graph location offset `[14000.0, 0.0, 0.0]`
- preview point-source graph pass `true`
- preview integration graph pass `true`
- preview report schema `cubeless_pcg_dungeon_native_integration_preview_v1`
- preview report status `generated`
- preview report pass `true`
- generation request ok `true`
- generation verification pass `true`
- generated `InstancedStaticMeshComponent` count `65`
- generated instance count `816`
- active viewport screenshot QA pass `true`
- side-by-side screenshot QA pass `true`
- review screenshot QA pass `true`
- side-by-side visual result `bridge left, native preview right`
- native-only window capture exists `true`
- bridge StaticMeshActors hidden for native-only capture `816`
- bridge StaticMeshActors left hidden after restore `0`

Expected room shape counts:

- room shape records `11`
- balanced `2`
- large_square `3`
- tall `1`
- vault `3`
- compact `1`
- arena `1`
- room-aware route/detail/theme actors missing shape tags `0`

Expected room theme light counts:

- room theme light actors `11`
- room theme light JSON records `11`
- legacy review light actors `8`
- combat_low_amber `4`
- entry_soft_green `1`
- finale_violet_focus `1`
- key_cyan_focus `1`
- reward_gold_pool `3`
- shop_teal_warm `1`

Expected TargetPoint room detail anchor counts:

- detail anchor actors `24`
- detail JSON records `24`
- start_chamber detail anchors `2`
- main_combat_room detail anchors `6`
- side_combat_room detail anchors `2`
- key_room detail anchors `2`
- shop_room detail anchors `3`
- treasure_vault detail anchors `6`
- boss_exit_chamber detail anchors `3`

Expected StaticMeshActor room detail mesh counts:

- detail mesh actors `24`
- detail mesh JSON records `24`
- detail_pedestal `5`
- detail_cover `7`
- detail_wall_trim `5`
- detail_counter `1`
- detail_brazier `2`
- detail_sign `1`
- detail_arch `2`
- detail_boss_focus `1`

Expected playtest actor counts:

- PlayerStart `1`
- NavMeshBoundsVolume `1`
- playtest JSON records `2`

Expected navigation waypoint counts:

- nav waypoint actors `206`
- nav waypoint JSON records `206`
- isolated waypoint actors `0`
- missing waypoint neighbor refs `0`
- start-to-exit waypoint path length `27` cells for the default seed

Expected lock-key link counts:

- lock-key links `1`
- missing key links `0`
- key-tagged actors `3` (`marker`, `spawn_anchor`, `reward_anchor`)
- lock-tagged actors `5` (`door`, `door_anchor`, `locked_door_seal`, `door volume`, `gate volume`)

Expected gameplay placeholder counts:

- gameplay placeholder Blueprints `8`
- gameplay placeholder actors `22`
- player_start placeholders `1`
- exit placeholders `1`
- shop placeholders `1`
- key placeholders `1`
- reward placeholders `4`
- locked_gate placeholders `1`
- enemy placeholders `12`
- boss placeholders `1`
- placeholder count mismatches `0`
- placeholder missing required tags `0`
- placeholder lock-key linkage pass `true`

Expected gameplay interaction contract counts:

- interaction contract entries `22`
- player_spawn contracts `1`
- exit_activation contracts `1`
- shop_open contracts `1`
- key_pickup contracts `1`
- reward_chest contracts `3`
- exit_unlock_reward contracts `1`
- locked_gate contracts `1`
- enemy_spawn contracts `12`
- boss_spawn contracts `1`
- interaction entry failures `0`
- actor update errors `0`
- instance variable update errors `0`
- key-gate linkage pass `true`
- placeholder coverage pass `true`

Expected gameplay flow simulation:

- flow simulation schema `cubeless_pcg_dungeon_gameplay_flow_simulation_v1`
- flow simulation status `passed`
- flow simulation pass `true`
- player spawn ready pass `true`
- key pickup pass `true`
- locked gate unlock pass `true`
- reward contracts available pass `true`
- encounter spawns available pass `true`
- exit activation ready pass `true`
- opened locks `1`
- inventory keys `1`
- `BP_DungeonGameplay_KeyPickupPlaceholder` custom event `DungeonInteract` exists and compiles
- `BP_DungeonGameplay_KeyPickupPlaceholder` `ActorBeginOverlap` routes to consumed-state setters and then calls `LinkedGateActor.DungeonUnlockGate`
- `BP_DungeonGameplay_KeyPickupPlaceholder` direct `DungeonInteract` also calls `LinkedGateActor.DungeonUnlockGate`
- `BP_DungeonGameplay_LockedGatePlaceholder` custom event `DungeonUnlockGate` exists and compiles
- Reward, shop, exit, enemy spawn, and boss spawn placeholder custom events exist and compile
- key/gate runtime graph compile errors `0`
- key/gate runtime graph compile warnings `0`
- gameplay Blueprint state event validation pass `true`
- gameplay Blueprint state event validation failure count `0`
- gameplay Blueprint state event visual failure count `0`
- gameplay Blueprint state event reset failure count `0`
- gameplay Blueprint state event visual reset failure count `0`

Expected role counts:

- start `1`
- exit `1`
- boss `1`
- key `1`
- shop `1`
- treasure `3`
- combat `4`
- locked_after `1`

Expected encounter kind counts:

- safe `1`
- combat `4`
- utility `2`
- reward `3`
- boss `1`

Expected encounter tier counts for the default seed:

- start `1`
- light `1`
- standard `2`
- elite `1`
- key_reward `1`
- shop `1`
- treasure `3`
- final `1`

The seed suite report is written to `Saved/MCP_Dungeon/CubelessDungeonMVP_SeedSuite_Report.json` and currently checks seeds `142857..142861`.
Each seed summary now reports `door_anchor_count`, `connector_detail_count`, `corridor_detail_count`, `room_shape_count`, `room_shape_counts`, `room_variant_detail_count`, `encounter_spawn_slot_count`, `reward_anchor_count`, `room_archetype_count`, `room_archetype_counts`, `room_theme_count`, `room_theme_counts`, `theme_light_count`, `detail_anchor_count`, `detail_mesh_count`, `navigation_waypoint_count`, `lock_key_link_count`, and `lock_key_missing_key_count`; it verifies `encounter_profile_count=11`, `route_anchor_count=11`, `reward_anchor_count=6`, `room_archetype_count=11`, `room_theme_count=11`, `room_shape_count=room_count`, `room_variant_detail_count=room_shape_count`, `theme_light_count=room_theme_count`, `connector_detail_count=door_anchor_count`, `corridor_detail_count=corridor_cell_count`, `detail_anchor_count>=room_count`, `detail_mesh_count=detail_anchor_count`, `navigation_waypoint_count=cell_count`, and complete lock-key linkage. The current suite passes `5/5`; room variant detail counts for seeds `142857..142861` are all `11`, connector detail counts are `23`, `24`, `27`, `30`, and `31`, and corridor detail counts are `34`, `26`, `31`, `43`, and `30`.

The current progression screenshot is written to `Saved/MCP_Dungeon/CubelessDungeonMVP_ProgressionMarkers.png`.

## Known Limits

- The active gate for the current scope is PCG dungeon generation only. Gameplay placeholder contracts remain documented metadata, but they are not part of the current success criteria.
- The native integration graph can now spawn the Static Mesh layout through native PCG, but the random dungeon structure and point-source graph are still generated from the Python/export contract. It is not yet a fully native procedural layout graph.
- Geometry Script creates the module meshes, then the bridge spawns Static Mesh actors for validation and exports the point data used by the native point-source graph.
- The latest PCG generation gate uses `816` native instances with full default ceiling coverage. Current primary-refresh, smoke, preview, preset archive, and screenshot evidence has been refreshed to the same closed-ceiling contract; only historical work-log entries or archived evidence may still show earlier `630` or `642` instance passes.
- `DungeonGridCellSize` and `DungeonCorridorWidth` are functional transform-scale controls. The source Static Mesh assets are still Geometry Script-authored at the base `TILE=400` dimensions, so non-default sizes are applied through actor/PCG point transforms rather than by regenerating separate mesh assets.
- `DungeonPreviewMode` is still a metadata/review-policy tag.
- The current spawner contract groups future PCG point data by `DungeonMeshKey`, and the graph handoff export records the native point-stream/filter/spawner split. The validation level still uses direct StaticMeshActor spawning until the native PCG graph is promoted.
- `PCG_Cubeless_Dungeon_MVP_NativeSkeleton` is a promotion skeleton only. It already has material-safe native filter/spawner branches, but its merge output is disconnected until native point-source generation replaces the Python bridge data source.
- Groups with multiple material variants need a material-attribute or material-variant strategy before they can become a single production Static Mesh Spawner without losing theme colors.
- Side-by-side bridge/native comparison now uses preview-only offset graphs. Do not use those preview graphs as production sources; production smoke/audit should continue to use the unoffset `NativePointSource` and `NativeIntegration` graphs.
- Lighting/exposure is prototype-grade and exists mostly for layout review.
- Role markers are validation markers. The gameplay placeholder Blueprints now expose runtime handoff variables, key has minimal overlap-driven consumed-state routing, key dispatches to the linked gate actor, and reward/shop/exit/enemy/boss placeholders have callable state events. The interaction contract, content outcome contract, and flow simulation prove placement/linkage/order/content handoff without PIE. Real reward content, shop UI, enemy AI, boss combat, and level-transition/clear behavior still need implementation.
- The module StaticMesh BodySetups are forced to `CTF_USE_COMPLEX_AS_SIMPLE`, and `RecastNavMesh-Default` is set to dynamic generation with `force_rebuild_on_load=true` during validation setup.
- A generated `NavMeshBoundsVolume` and `RecastNavMesh-Default` are present, and `RebuildNavigation`/`BUILDPATHS` can be called from the editor. The current validation still gets `None` from `project_point_to_navigation` at the generated PlayerStart, so Unreal navmesh query readiness is not proven yet.
- Generated `nav_waypoint` TargetPoints are the current no-C++ fallback for movement, AI director, minimap, and route validation logic that cannot depend on Recast data yet.
- Next production step should promote stable layout and assembly rules into native PCG graph nodes or a focused UnrealMCP helper only after the rules stop changing.
