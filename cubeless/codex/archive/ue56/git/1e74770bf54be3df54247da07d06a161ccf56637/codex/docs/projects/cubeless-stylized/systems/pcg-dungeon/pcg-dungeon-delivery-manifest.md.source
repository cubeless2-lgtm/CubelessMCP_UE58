# PCG Dungeon Delivery Manifest

This manifest records the current `/Game/Cubeless/PCG/Dungeon` delivery state for the PCG-dungeon-generation scope. It intentionally does not claim gameplay implementation readiness.

## Current State

- Branch: `main` (integrated from `codex/pcg-dungeon-geometry-script-mvp`)
- Live preset: `default`
- Open level: `/Game/Cubeless/PCG/Dungeon/Maps/LVL_Cubeless_PCG_Dungeon_MVP`
- Source/control actor: `MCP_Cubeless_Dungeon_MVP_PCGBridge`
- Review output actor: `MCP_Cubeless_Dungeon_MVP_NativeOutput`
- Production graph candidate: `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration`
- Current output: `65` native components / `816` instances
- Final gate: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGeneration_FinalGate.json`, `pass=true`
- Visual runner summary: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGeneration_VisualGateQA_Report.json`, `success=true`, `exposure_review_pass=true`
- Native evidence freshness: primary refresh, smoke test, and native preview all match `65` native components / `816` instances
- Native evidence summary: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeEvidenceRefresh_Report.json`, `success=true`, `mode=summarize_existing_reports`
- V1 handoff readiness: `Saved/MCP_Dungeon/CubelessDungeonMVP_HandoffReadiness.json`, `pass=true`
- Delivery closeout: `Saved/MCP_Dungeon/CubelessDungeonMVP_DeliveryCloseout.json`, `pass=true`
- Delivery preflight: `Saved/MCP_Dungeon/CubelessDungeonMVP_DeliveryPreflight.json`, `pass=true`
- Live dirty state: `Saved/MCP_Dungeon/CubelessDungeonMVP_LiveDirtyState.json`, `pass=true`
- Git binary asset attributes: `71/71` Unreal binary assets use `filter=lfs`, `merge=lfs`, `diff=lfs`, `text=unset`
- Latest editor log health: no blocking `Error/Fatal/Assertion/Ensure` matches after the latest delivery evidence checkpoint
- Dirty package count at latest gate: `0`
- Current screenshot exposure: top near-white `0.0%`, oblique near-white `0.0%`, manual review exposure volume enabled

## Versioned Work Scope

Include these paths when reviewing or committing this PCG dungeon work:

- `Content/Cubeless/PCG/Dungeon/`
- `Plugins/CustomTools/Content/Python/ArtScripts/CubelessDungeonPCG.py`
- `Plugins/CustomTools/Content/Python/ArtScripts/CubelessDungeonPCGEntrypoint.py`
- `Tools/Unreal/audit_pcg_dungeon_asset_manifest.py`
- `Tools/Unreal/check_pcg_dungeon_delivery_preflight.py`
- `Tools/Unreal/check_pcg_dungeon_handoff_readiness.py`
- `Tools/Unreal/check_pcg_dungeon_live_dirty_state.py`
- `Tools/Unreal/run_pcg_dungeon_delivery_closeout.py`
- `Tools/Unreal/run_pcg_dungeon_authoring_preset_matrix.py`
- `Tools/Unreal/run_pcg_dungeon_generation_visual_gate_qa.py`
- `Tools/Unreal/run_pcg_dungeon_native_evidence_refresh.py`
- `docs/pcg-dungeon-mvp.md`
- `docs/pcg-dungeon-delivery-manifest.md`
- `docs/pcg-dungeon-operator-guide.md`
- `docs/pcg-dungeon-v2-roadmap.md`
- `docs/pcg-dungeon-review-checklist.md`
- `docs/work-log.md`

Generated evidence under `Saved/MCP_Dungeon/` is useful for local review but should not be staged by default unless a specific generated report or screenshot is intentionally requested.

Use `docs/pcg-dungeon-operator-guide.md` as the short day-to-day guide for opening the level, reviewing NativeOutput, applying presets, restoring default, and running closeout.

Use `docs/pcg-dungeon-v2-roadmap.md` only as future planning context. It does not change the current V1 delivery gate.

Use `docs/pcg-dungeon-review-checklist.md` before staging or reviewing this work. It separates versioned source scope from generated evidence that should not be staged by default.

## Unreal Asset Manifest

### Maps

- `Content/Cubeless/PCG/Dungeon/Maps/LVL_Cubeless_PCG_Dungeon_MVP.umap`

### PCG Graphs

- `Content/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_Bridge.uasset`
- `Content/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration.uasset`
- `Content/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration_PreviewOffset.uasset`
- `Content/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativePointSource.uasset`
- `Content/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativePointSource_PreviewOffset.uasset`
- `Content/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeSkeleton.uasset`

### Geometry Script Static Mesh Modules

- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_CeilingPanel.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Ceiling_Corner.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Ceiling_Corridor.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Ceiling_Room.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Column.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Connector_LockedThreshold.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Connector_Threshold.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_CornerSegment.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_CorridorDetail_Corner.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_CorridorDetail_Endcap.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_CorridorDetail_Junction.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_CorridorDetail_Straight.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_CorridorSegment.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Detail_Arch.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Detail_BossFocus.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Detail_Brazier.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Detail_Counter.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Detail_Cover.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Detail_Pedestal.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Detail_Sign.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Detail_WallTrim.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_DoorFrame.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_FloorTile.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_LockedDoorSeal.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_RoomVariant_AmbientRubble.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_RoomVariant_CombatPartition.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_RoomVariant_EntryInlay.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_RoomVariant_FinaleRing.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_RoomVariant_ProgressionRune.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_RoomVariant_RewardBorder.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_RoomVariant_UtilityMarket.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_SpawnMarker.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_Stair.uasset`
- `Content/Cubeless/PCG/Dungeon/Meshes/SM_GS_Dungeon_WallPanel.uasset`

### Materials

- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Boss_Magenta.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Ceiling_SootStone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Chest_Gold.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Door_WornBronze.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Enemy_Red.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Exit_Blue.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Floor_Stone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Key_Cyan.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_LockedDoor_Violet.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Shop_Teal.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Start_Green.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Theme_AmbientStone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Theme_CombatStone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Theme_ConnectorStone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Theme_CorridorStone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Theme_EntryStone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Theme_FinaleStone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Theme_KeyStone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Theme_RewardStone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Theme_UtilityStone.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Trim_DarkIron.uasset`
- `Content/Cubeless/PCG/Dungeon/Materials/M_Dungeon_Wall_ColdStone.uasset`

### Placeholder Blueprints

- `Content/Cubeless/PCG/Dungeon/Blueprints/BP_DungeonGameplay_BossSpawnPlaceholder.uasset`
- `Content/Cubeless/PCG/Dungeon/Blueprints/BP_DungeonGameplay_EnemySpawnPlaceholder.uasset`
- `Content/Cubeless/PCG/Dungeon/Blueprints/BP_DungeonGameplay_ExitPlaceholder.uasset`
- `Content/Cubeless/PCG/Dungeon/Blueprints/BP_DungeonGameplay_KeyPickupPlaceholder.uasset`
- `Content/Cubeless/PCG/Dungeon/Blueprints/BP_DungeonGameplay_LockedGatePlaceholder.uasset`
- `Content/Cubeless/PCG/Dungeon/Blueprints/BP_DungeonGameplay_PlayerStartPlaceholder.uasset`
- `Content/Cubeless/PCG/Dungeon/Blueprints/BP_DungeonGameplay_RewardPlaceholder.uasset`
- `Content/Cubeless/PCG/Dungeon/Blueprints/BP_DungeonGameplay_ShopPlaceholder.uasset`

## Preset Evidence

The latest archived preset gate summaries all include `archive.pass=true`:

| Archive label | Preset | Native components | Native instances | Failed checks | Dirty packages |
| --- | --- | ---: | ---: | ---: | ---: |
| `wide_looped_postprocess` | `wide_looped` | `64` | `764` | `0` | `0` |
| `compact_branching_postprocess` | `compact_branching` | `64` | `624` | `0` | `0` |
| `open_cutaway_postprocess` | `open_cutaway` | `61` | `573` | `0` | `0` |
| `default_restored_after_postprocess_preset_suite` | `default` | `65` | `816` | `0` | `0` |
| `small_route_v1qa` | `small_route` | `64` | `529` | `0` | `0` |
| `long_route_v1qa` | `long_route` | `65` | `757` | `0` | `0` |
| `loop_dense_v1qa` | `loop_dense` | `64` | `854` | `0` | `0` |
| `boss_focus_v1qa` | `boss_focus` | `65` | `701` | `0` | `0` |
| `default_restored_after_v1qa_preset_expansion` | `default` | `65` | `816` | `0` | `0` |

Archive root:

- `Saved/MCP_Dungeon/PresetQA/`

## Asset Manifest Audit

Latest read-only Unreal AssetRegistry/load audit:

- Report: `Saved/MCP_Dungeon/CubelessDungeonMVP_AssetManifestAudit.json`
- Audit pass: `true`
- Local expected assets: `71`
- Registry assets under `/Game/Cubeless/PCG/Dungeon`: `71`
- Loaded assets: `71`
- Redirectors: `0`
- Missing from registry: `0`
- Load failures: `0`
- Class counts: `Blueprint=8`, `PCGGraph=6`, `World=1`, `Material=22`, `StaticMesh=34`

## Verification Commands

Compile and whitespace checks:

```powershell
python -m py_compile Plugins/CustomTools/Content/Python/ArtScripts/CubelessDungeonPCG.py Plugins/CustomTools/Content/Python/ArtScripts/CubelessDungeonPCGEntrypoint.py Tools/Unreal/audit_pcg_dungeon_asset_manifest.py Tools/Unreal/check_pcg_dungeon_delivery_preflight.py Tools/Unreal/check_pcg_dungeon_handoff_readiness.py Tools/Unreal/check_pcg_dungeon_live_dirty_state.py Tools/Unreal/run_pcg_dungeon_delivery_closeout.py Tools/Unreal/run_pcg_dungeon_generation_visual_gate_qa.py Tools/Unreal/run_pcg_dungeon_native_evidence_refresh.py Tools/Unreal/run_pcg_screenshot_visual_qa.py Tools/Unreal/run_pcg_bookmark_visual_qa.py
git diff --check
```

Run the one-command local closeout:

```powershell
python Tools\Unreal\run_pcg_dungeon_delivery_closeout.py
```

The closeout refreshes live dirty state, the read-only asset manifest audit, native evidence summary, authoring preset matrix, V1 handoff readiness, delivery preflight, and `git diff --check` in sequence.

Run the layout-only preset matrix directly:

```powershell
python Tools\Unreal\run_pcg_dungeon_authoring_preset_matrix.py --seed-count 5
```

Refresh the live editor dirty-state report:

```powershell
python Tools\Unreal\check_pcg_dungeon_live_dirty_state.py
```

Run the local delivery preflight:

```powershell
python Tools\Unreal\check_pcg_dungeon_delivery_preflight.py
```

The preflight checks Python syntax, expected Git scope, sibling workspace cleanliness, Unreal binary asset Git attributes, manifest coverage, latest final gate, asset manifest audit, live dirty-state report, preset archive summaries, combined native evidence summary, native evidence freshness, V1 handoff readiness, and latest editor log health. Log errors older than the latest delivery evidence checkpoint are recorded for context but do not block the current preflight.

Run the read-only V1 handoff readiness check:

```powershell
python Tools\Unreal\check_pcg_dungeon_handoff_readiness.py
```

Run the read-only asset manifest audit:

```powershell
python Tools\Unreal\audit_pcg_dungeon_asset_manifest.py --mcp-response-timeout-seconds 240
```

Refresh the current live visual gate:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --redraw-count 2
```

Refresh native primary/smoke/preview evidence after the point contract changes:

```powershell
python Tools\Unreal\run_pcg_dungeon_native_evidence_refresh.py --redraw-count 2 --mcp-response-timeout-seconds 900 --refresh-timeout-seconds 900
```

Regenerate only the combined native evidence summary from existing reports:

```powershell
python Tools\Unreal\run_pcg_dungeon_native_evidence_refresh.py --summarize-existing
```

Run a preset and then restore default:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --preset open_cutaway --archive-label open_cutaway_postprocess --redraw-count 2
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --preset default --archive-label default_restored_after_postprocess_preset_suite --redraw-count 2
```

## Explicit Non-Goals

- No project C++ was added or modified.
- No `unreal-mcp-cubeless` sibling workspace changes are required for this delivery.
- Gameplay placeholder assets are metadata and future handoff scaffolding only.
- Reward items, shop UI, enemy AI, boss combat, exit travel, and playthrough implementation are outside the current gate.
