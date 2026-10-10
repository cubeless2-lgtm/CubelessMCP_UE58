# PCG Dungeon V1 Review Checklist

Use this checklist before staging, committing, or reviewing the current PCG
dungeon V1 work. It is scoped to PCG dungeon generation only.

## Must Pass

Run the one-command closeout from the repository root:

```powershell
python Tools\Unreal\run_pcg_dungeon_delivery_closeout.py
```

Expected:

- `Saved/MCP_Dungeon/CubelessDungeonMVP_DeliveryCloseout.json` has `pass=true`.
- `failed_steps` is empty.
- `Saved/MCP_Dungeon/CubelessDungeonMVP_DeliveryPreflight.json` has `pass=true`.
- `Saved/MCP_Dungeon/CubelessDungeonMVP_HandoffReadiness.json` has `pass=true`.
- `Saved/MCP_Dungeon/CubelessDungeonMVP_LiveDirtyState.json` has `pass=true`.
- `git diff --check` passes.

## Include In Review

Versioned project content:

- `Content/Cubeless/PCG/Dungeon/`
- `Plugins/CustomTools/Content/Python/ArtScripts/CubelessDungeonPCG.py`
- `Plugins/CustomTools/Content/Python/ArtScripts/CubelessDungeonPCGEntrypoint.py`

Local tooling:

- `Tools/Unreal/audit_pcg_dungeon_asset_manifest.py`
- `Tools/Unreal/check_pcg_dungeon_delivery_preflight.py`
- `Tools/Unreal/check_pcg_dungeon_handoff_readiness.py`
- `Tools/Unreal/check_pcg_dungeon_live_dirty_state.py`
- `Tools/Unreal/run_pcg_dungeon_delivery_closeout.py`
- `Tools/Unreal/run_pcg_dungeon_authoring_preset_matrix.py`
- `Tools/Unreal/run_pcg_dungeon_generation_visual_gate_qa.py`
- `Tools/Unreal/run_pcg_dungeon_native_evidence_refresh.py`

Documentation:

- `docs/pcg-dungeon-mvp.md`
- `docs/pcg-dungeon-delivery-manifest.md`
- `docs/pcg-dungeon-operator-guide.md`
- `docs/pcg-dungeon-v2-roadmap.md`
- `docs/pcg-dungeon-review-checklist.md`
- `docs/work-log.md`

## Do Not Stage By Default

Do not stage generated evidence unless explicitly requested:

- `Saved/MCP_Dungeon/`
- screenshots and archived preset QA output
- temporary MCP output under ignored temp folders

These files are useful for local verification but are not required as versioned
source unless a specific report or screenshot is requested.

## Reviewer Focus

Check that:

- V1 opens at `/Game/Cubeless/PCG/Dungeon/Maps/LVL_Cubeless_PCG_Dungeon_MVP`.
- The review actor is `MCP_Cubeless_Dungeon_MVP_NativeOutput`.
- Current default output remains `65` native components / `816` instances.
- Geometry Script module assets are under `/Game/Cubeless/PCG/Dungeon/Meshes`.
- Native PCG spawning uses `PCG_Cubeless_Dungeon_MVP_NativeIntegration`.
- The bridge actor remains the source/control actor for authoring tags.
- `open_cutaway` is still the ceiling-off review preset.
- Authoring preset matrix passes for all documented presets across the current seed window.
- Delivery closeout and handoff readiness are current and passing.

## Non-Goals

Do not review this as gameplay-complete work. The following are explicitly
outside the current gate:

- real reward items
- shop UI
- enemy AI
- boss combat
- exit travel or level transition
- full PIE playthrough

Gameplay placeholder assets and reports are metadata/handoff scaffolding only.
