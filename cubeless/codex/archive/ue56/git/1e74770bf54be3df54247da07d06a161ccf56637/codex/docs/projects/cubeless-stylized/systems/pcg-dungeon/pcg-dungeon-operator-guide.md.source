# PCG Dungeon V1 Operator Guide

This guide is the short operational path for using the current PCG dungeon V1
under `/Game/Cubeless/PCG/Dungeon`. It covers dungeon generation and review
only; gameplay implementation is intentionally out of scope.

## Open And Review

Open this level:

```text
/Game/Cubeless/PCG/Dungeon/Maps/LVL_Cubeless_PCG_Dungeon_MVP
```

Review this actor as the current generated dungeon output:

```text
MCP_Cubeless_Dungeon_MVP_NativeOutput
```

The source/control actor is:

```text
MCP_Cubeless_Dungeon_MVP_PCGBridge
```

The production candidate graph is:

```text
/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration
```

The current default result is `65` native components and `816` instances.
NativeOutput-only review mode hides the bridge validation StaticMeshActors, the
offset preview actor, and generated review lights so the visible result is the
native PCG output.

## Fast Health Check

Run this from the repository root:

```powershell
python Tools\Unreal\run_pcg_dungeon_delivery_closeout.py
```

Expected result:

- `Saved/MCP_Dungeon/CubelessDungeonMVP_DeliveryCloseout.json` has `pass=true`.
- `failed_steps` is empty.
- live dirty state is `0`.
- asset manifest audit passes with `71 / 71 / 71` expected, registry, and loaded assets.
- handoff readiness and delivery preflight both pass.

Use this closeout before handing the dungeon to another user, before a commit,
or after changing authoring tags/presets.

## Visual Refresh

To recapture the current default view and final gate without changing presets:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --redraw-count 2
```

The runner captures top and oblique NativeOutput-only screenshots, checks
exposure, and records `CubelessDungeonMVP_PCGGeneration_FinalGate.json`.

## Preset Workflow

Supported presets:

- `default`: current closed-ceiling delivery preset.
- `compact_branching`: smaller branching layout.
- `wide_looped`: wider layout with more loop edges.
- `open_cutaway`: ceiling disabled for structural review.
- `small_route`: short dungeon for fast layout iteration.
- `long_route`: longer critical-path layout with limited loops.
- `loop_dense`: loop-heavy stress preset for branch/connector checks.
- `boss_focus`: compact combat-heavy layout with a prominent boss route.

To list and layout-check every documented preset across a 5-seed window:

```powershell
python Tools\Unreal\run_pcg_dungeon_authoring_preset_matrix.py --seed-count 5
```

To apply a preset, refresh the dungeon, capture screenshots, and close the gate:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --preset open_cutaway --archive-label open_cutaway_review --redraw-count 2
```

Non-default preset runs intentionally leave the bridge tags on that preset. To
return the live state to default:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --preset default --archive-label default_restored_after_review --redraw-count 2
```

Then run the closeout:

```powershell
python Tools\Unreal\run_pcg_dungeon_delivery_closeout.py
```

## Manual Tag Editing

The bridge actor uses actor tags as the current no-C++ authoring surface. Edit
tags only on `MCP_Cubeless_Dungeon_MVP_PCGBridge`.

Common tags:

- `DungeonSeed=<int>`
- `DungeonRoomCount=<int>`
- `DungeonBranchChancePercent=<int>`
- `DungeonMaxLoopEdges=<int>`
- `DungeonGridCellSize=<int>`
- `DungeonCorridorWidth=<int>`
- `DungeonUseCeiling=<0|1>`
- `DungeonCeilingStride=<int>`
- `DungeonUseThemeMaterials=<0|1>`

After manual tag edits, use the current-bridge refresh path:

```powershell
python Tools\Unreal\run_pcg_dungeon_generation_visual_gate_qa.py --refresh-current --redraw-count 2
python Tools\Unreal\run_pcg_dungeon_delivery_closeout.py
```

## Do Not Treat As Gameplay Complete

The placeholder Blueprints and gameplay reports are metadata for later handoff.
They do not mean reward items, shop UI, enemy AI, boss combat, exit travel, or a
full playthrough are implemented.

## V2 Planning

For a later alternate dungeon implementation, use:

```text
docs/pcg-dungeon-v2-roadmap.md
```

That roadmap explains which V1 assets, reports, and closeout gates should be
reused, and which layout-generation pieces should be redesigned. It does not
change the current V1 delivery gate.

## Useful Reports

- Final gate: `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGGeneration_FinalGate.json`
- Closeout: `Saved/MCP_Dungeon/CubelessDungeonMVP_DeliveryCloseout.json`
- Handoff readiness: `Saved/MCP_Dungeon/CubelessDungeonMVP_HandoffReadiness.json`
- Asset manifest audit: `Saved/MCP_Dungeon/CubelessDungeonMVP_AssetManifestAudit.json`
- Native evidence summary: `Saved/MCP_Dungeon/CubelessDungeonMVP_NativeEvidenceRefresh_Report.json`
- Live dirty state: `Saved/MCP_Dungeon/CubelessDungeonMVP_LiveDirtyState.json`
- Preset archives: `Saved/MCP_Dungeon/PresetQA/`

## If Something Fails

Use this order:

1. Check `failed_steps` in `CubelessDungeonMVP_DeliveryCloseout.json`.
2. If `live_dirty_state` failed, inspect dirty packages before saving or closing the editor.
3. If `asset_manifest_audit` failed, check missing registry assets, load failures, or redirectors.
4. If `handoff_readiness` failed, compare NativeOutput counts, graph paths, and screenshot reports.
5. If `delivery_preflight` failed, inspect the named failed section in `CubelessDungeonMVP_DeliveryPreflight.json`.

Do not fix a failed report by editing gameplay placeholders unless the active
task explicitly changes gameplay. The V1 gate is PCG dungeon generation only.
