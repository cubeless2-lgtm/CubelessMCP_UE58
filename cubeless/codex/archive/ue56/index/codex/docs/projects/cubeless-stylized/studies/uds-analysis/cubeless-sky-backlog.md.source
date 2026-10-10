# Cubeless Sky Backlog From UDS Analysis

This backlog converts the UDS analysis into implementation tasks for a
Cubeless-owned sky system. It is intentionally stricter than a loose roadmap:
each item has an expected output and acceptance criteria.

## P0 - Lock The UDS Reference State

Purpose:

- Preserve the repaired UDS view as a reference without turning vendor content
  into Cubeless implementation.

Tasks:

1. Keep `codex/uds-analysis` as the analysis branch.
2. Keep the repaired screenshot:
   `Saved/CodexScreenshots/UDS_Analysis/uds_volumetric_cloud_restored_20260617.png`.
3. Keep the current runtime snapshot docs current when the reference changes.
4. Do not save or promote UDS MPC asset defaults as the fix.
5. Review `Content/UltraDynamicSky/Maps/DemoMap.umap` separately before any
   staging decision.

Acceptance criteria:

- A new developer can reproduce the UDS cloud disappearance and runtime repair
  from docs alone.
- Git status makes it clear which changes are docs and which are binary map or
  unrelated user files.

## P0 - Snapshot Helper For Sky State

Purpose:

- Make the state capture repeatable so future UDS or Cubeless sky bugs are not
  diagnosed from asset defaults alone.

Tasks:

1. Use `Tools/Unreal/capture_uds_sky_snapshot.py` or keep it updated so it
   captures:
   - current map
   - dirty packages
   - UDS/UDW actor values
   - selected Blueprint function outputs
   - MPC asset defaults and runtime values
   - sky MID values
   - VolumetricCloud component material/visibility
2. Output JSON under `Saved/CodexScreenshots` or another generated report path.
3. Keep generated outputs out of Git unless explicitly requested.

Acceptance criteria:

- The helper can prove when an MPC asset default differs from the runtime value.
- The helper can detect the specific bad state: derived density greater than
  zero while runtime cloud density is zero.
- The helper remains read-only and writes generated reports under
  `Saved/UDS_Analysis`.

## P1 - Cubeless Sky State Schema

Purpose:

- Define the Cubeless-owned state model before creating or editing materials.

Tasks:

1. Define public weather preset fields:
   - `CloudCoverage`
   - `Fog`
   - `Rain`
   - `Snow`
   - `Dust`
   - `WindIntensity`
2. Define semantic sky fields:
   - `CloudCoverageNormalized`
   - `VolumetricDensity`
   - `Layer2Density`
   - `CloudSpeed`
   - `CloudDirection`
   - `CloudPhase`
   - `TimeOfDay`
3. Define material-facing fields:
   - cloud density
   - bottom/top altitude
   - cloud layer height
   - cloud scale
   - macro scale/variation
   - high frequency noise
   - erosion
   - extinction
   - phase parameters
   - sun/moon vectors
   - wisp color/opacity
4. Choose Cubeless naming that does not mirror UDS asset paths.

Acceptance criteria:

- Cubeless public state, semantic state, and MPC state are documented as
  separate layers.
- The state schema can express the current UDS `Partly_Cloudy` reference state
  without referencing `/Game/UltraDynamicSky`.

## P1 - Minimal Cubeless Volumetric Cloud Prototype

Purpose:

- Build the smallest visible Cubeless-owned volume cloud path.

Tasks:

1. Use Cubeless-local material and MPC assets only.
2. Drive a VolumetricCloud component from Cubeless state.
3. Implement first-pass controls:
   - density
   - bottom/top altitude
   - scale
   - macro variation
   - erosion
   - extinction
   - simple light color or phase controls
4. Add a debug output that reports component, material, and MPC state.

Acceptance criteria:

- Clouds remain visible after editor restart without UDS runtime repairs.
- Recursive dependency audit reports no `/Game/UltraDynamicSky` reference.
- A low/high coverage switch visibly changes volume cloud density.

Do not copy:

- UDS material functions as direct dependencies.
- UDS texture assets as final Cubeless assets.
- UDS MPC GUIDs or vendor collection references.

## P1 - Density Sync Regression Guard

Purpose:

- Prevent the exact fixed bug from reappearing in Cubeless.

Tasks:

1. Add a runtime check:
   - if sky mode uses volumetric clouds
   - and derived density is greater than zero
   - and material-facing density is zero
   - report or force a sync
2. Log the public state and material-facing state when the guard fires.
3. Add this to the snapshot helper or smoke test.

Acceptance criteria:

- The test can fail on the UDS-like bad state.
- The test passes after Cubeless state sync writes the material-facing density.

## P2 - Cubeless Wisp Overlay

Purpose:

- Carry the long layered sky detail that volume clouds alone do not reproduce
  cheaply.

Tasks:

1. Use a Cubeless-owned wisp texture or procedural texture source.
2. Add MID or MPC controls for:
   - texture
   - opacity
   - color
   - directional gradient
   - UV scale
   - motion offset
3. Keep wisp controls separate from volume density controls.

Acceptance criteria:

- Turning off volume clouds still leaves readable wisp behavior.
- Turning off the wisp overlay does not remove the volume cloud layer.
- The wisp material path has no UDS dependencies.

Do not copy:

- `/Game/UltraDynamicSky/Textures/Sky/Cloud_Wisps` as a final asset.
  It may remain a visual reference only.

## P2 - Static And 2D Cloud Layer Prototype

Purpose:

- Add a cheap fallback or stylized layer after the wisp overlay is stable.

Tasks:

1. Build small Cubeless material functions for:
   - UV mapping
   - packed texture sampling
   - threshold/filtering
   - simple lighting response
2. Support the existing static cloud packing convention:
   - `R`: upper-right key light response
   - `G`: upper-left key light response
   - `B`: overhead/front fill response
   - `A`: opacity/density
3. Keep polar/radial UV reference rules documented for generated cloud art.

Acceptance criteria:

- A static cloud texture can be swapped without changing the graph.
- Packed channel behavior is verified with a test texture or preview.
- No UDS material function or texture dependency remains.

## P2 - Weather Preset Data

Purpose:

- Replace UDW with a small Cubeless weather table.

Tasks:

1. Start with these presets:
   - `Clear`
   - `PartlyCloudy`
   - `Cloudy`
   - `Overcast`
   - `Storm`
   - `Blizzard`
2. For first implementation, wire only:
   - cloud coverage
   - fog
   - wind
3. Postpone rain, snow, dust, particles, puddles, thunder, and sound.

Acceptance criteria:

- Switching from `Clear` to `PartlyCloudy` increases sky coverage and density.
- Switching to `Storm` increases coverage, fog, and wind.
- The weather table lives outside `/Game/UltraDynamicSky`.

## P2 - Dependency Audit Command

Purpose:

- Make vendor leakage visible before promotion.

Tasks:

1. Use `Tools/Unreal/audit_cubeless_sky_dependencies.py` or keep it updated as
   the repeatable check for recursive dependencies under a Cubeless sky root.
2. Flag any path containing `/Game/UltraDynamicSky`.
3. Flag UDS MPC references and material function references separately.
4. Produce a generated report path under `Saved/UDS_Analysis`.
5. Use `Tools/Unreal/check_cubeless_sky_promotion_preflight.py` as the stricter
   promotion gate after cleanup.

Acceptance criteria:

- The command reports zero UDS dependencies for promoted Cubeless sky assets.
- The report names exact offending assets when a dependency exists.
- The current audit is allowed to fail while known legacy sky assets still
  reference UDS; treat failures as cleanup backlog, not as an audit-tool failure.
- Promotion preflight passes only when the expected Cubeless mesh, weather
  data textures, sky-dome material texture, dependency audit, and dirty content
  checks all pass.

Completed cleanup list:

- `/Game/Cubeless/Sky/BP_SkySystem` references
  `/Game/UltraDynamicSky/Meshes/Ultra_Dynamic_Sky_Sphere`.
- `/Game/Cubeless/Sky/Data/DA_Weather_Clear` references
  `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/FarCloud`.
- `/Game/Cubeless/Sky/Data/DA_Weather_Cloudy` references
  `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02`.
- `/Game/Cubeless/Sky/Data/DA_Weather_Overcast` references
  `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02`.
- `/Game/Cubeless/Sky/Materials/M_Sky_Dome` references both `FarCloud` and
  `cloub02`.

These were removed in the 2026-06-17 cleanup pass. The current replacement
assets are:

- `/Game/Cubeless/Sky/Meshes/SM_Cubeless_SkySphere`
- `/Game/Cubeless/Env/Sky/Textures/T_CloudPlaneAtlas_LightPacked_UDSLike`

Latest audit report:

`Saved/UDS_Analysis/cubeless_sky_dependency_audit_20260617_015419.json`

Latest promotion preflight report:

`Saved/UDS_Analysis/cubeless_sky_promotion_preflight_20260617_023150.json`

Latest closeout report:

`Saved/UDS_Analysis/uds_analysis_closeout_20260617_023149.json`

Latest editor log scan:

`Saved/UDS_Analysis/unreal_editor_log_scan_20260617_023151.json`

Latest delivery manifest:

`Saved/UDS_Analysis/uds_analysis_delivery_manifest_20260617_023159.json`

Latest staging scope preview:

`Saved/UDS_Analysis/uds_analysis_staging_scope_20260617_023149.json`

Human handoff:

`docs/uds-analysis/handoff.md`

## P3 - Shadows, Fog, Caustics, And Weather Effects

Purpose:

- Bring over later UDS concepts only after the core sky is stable.

Tasks:

1. Study `Cloud_Shadows_and_Caustics`.
2. Study `Global_Volumetric_Fog`.
3. Study `Cloud_Fog_PostProcess`.
4. Decide which effects are actually needed for Cubeless art direction.
5. Implement only Cubeless-local versions.

Acceptance criteria:

- Each effect can be toggled independently.
- Each effect has an explicit performance and visual acceptance check.
- No effect blocks the first Cubeless volume/wisp/weather pass.

## Promotion Checklist

Before treating the Cubeless sky as production-ready:

1. Editor restart smoke test passes.
2. Clouds are visible without touching UDS runtime MPCs.
3. Low/high coverage preset switch passes.
4. Wind direction/speed movement test passes.
5. Wisp on/off test passes.
6. Dependency audit, promotion preflight, and closeout pass.
7. Screenshot comparison against the UDS reference is saved.
8. Dirty package report is clean or intentionally documented.
9. Latest editor log scan has no fatal or ensure entries.
10. Delivery manifest has no `unknown_review` paths before staging.
11. Human handoff has been reviewed so stage candidates, manual decisions, and
    exclusions are not mixed.
12. Staging scope check passes after staging, with no accidental manual,
    excluded, or unknown staged paths.

The first production milestone is not visual parity. It is a Cubeless-owned
system whose state is explicit, inspectable, and free of hidden UDS dependency
paths.
