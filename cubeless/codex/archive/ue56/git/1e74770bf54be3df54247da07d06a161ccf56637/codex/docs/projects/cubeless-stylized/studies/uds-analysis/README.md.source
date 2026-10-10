# Ultra Dynamic Sky Analysis

This folder records the working analysis of `/Game/UltraDynamicSky` for Cubeless.
The goal is not to clone UDS blindly. The goal is to understand which runtime
ideas are worth reusing, which assets are reference-only, and where Cubeless
should own its own sky implementation.

## Reading Order

1. `current-volumetric-cloud-repair.md`
   - Why the current UDS volumetric cloud disappeared, what was restored, and
     what remains a runtime-only repair.
2. `blueprint-flow.md`
   - UDS/UDW Blueprint graph structure, startup/update/tick dispatch model, and
     the exact cloud-density apply path that failed.
3. `volumetric-cloud-stack.md`
   - Volumetric cloud component, Blueprint density formula, MPC runtime
     snapshot, material graph, and Cubeless porting lesson.
4. `asset-map.md`
   - High-level inventory of UDS folders, major Blueprints, materials, MPCs,
     textures, and weather assets.
5. `runtime-flow.md`
   - How `Ultra_Dynamic_Sky` and `Ultra_Dynamic_Weather` update time, weather,
     cloud movement, occlusion, Niagara, sound, and material effects.
6. `material-mpc-flow.md`
   - The material and Material Parameter Collection flow that actually drives
     static clouds, sky dome clouds, and volumetric clouds.
7. `static-and-2d-clouds.md`
   - Sky-dome cloud layers, static packed clouds, wisp overlays, texture
     sampling, and current DemoMap MID values.
8. `weather-preset-flow.md`
   - UDW preset assets, current weather state, preset value table, and the
     UDW-to-UDS connection points that matter for clouds.
9. `cubeless-implementation-roadmap.md`
   - A phased Cubeless-owned implementation plan from reference lock to
     dependency audit.
10. `blueprint-index.md`
   - Navigation index for UDS/UDW event graphs, delegates, function families,
     and the graph families most useful for Cubeless.
11. `material-dependency-index.md`
   - Direct Asset Registry dependencies for core sky, cloud, fog, shadow, and
     cloud-function materials.
12. `runtime-snapshot-checklist.md`
   - Reproducible state capture order, current runtime values, MPC default vs
     runtime split, and cloud disappearance triage.
13. `state-signal-matrix.md`
   - One-page signal map for actor values, Blueprint-derived values, runtime
     MPC/MID values, material meaning, traps, and Cubeless equivalents.
14. `learning-curriculum.md`
   - A module-by-module study path with exercises and pass criteria for
     learning UDS deeply enough to build a Cubeless-owned sky system.
15. `cubeless-sky-backlog.md`
   - Implementation backlog with priorities, acceptance criteria, dependency
     rules, and regression checks derived from the UDS analysis.
16. `cubeless-dependency-audit.md`
   - Repeatable Cubeless sky dependency audit and promotion preflight commands,
     latest UDS dependency findings, and the cleanup list for remaining vendor
     references.
17. `handoff.md`
   - Human handoff for the current branch: what was fixed, what passed, what to
     stage, what to review manually, and what to exclude.
18. `delivery-manifest.md`
   - Read-only Git status classifier for separating stage candidates, manual
     decisions, and unrelated/excluded paths before commit work.
19. `cubeless-transfer-notes.md`
   - What Cubeless should copy as concepts, what should stay reference-only, and
     what should not be copied directly.
20. `mcp-followups.md`
   - MCP/API improvements discovered during analysis. Per the current rule,
     these stay as follow-up items unless analysis is blocked.

## Current Status

- Branch: `codex/uds-analysis`
- Active inspected map: `/Game/UltraDynamicSky/Maps/DemoMap`
- Current UDS actor: `Ultra_Dynamic_Sky_0`
- Current UDW actor: `Ultra_Dynamic_Weather_C_1`
- Volumetric cloud visibility was restored in the editor session by setting the
  runtime value of `UDS_VolumetricClouds_MPC.Cloud Density` to the value returned
  by UDS' own `Current Volumetric Clouds Density` function.
- The exact failed Blueprint gate was `Composite Weather Change Speed > 0` in
  `Update Cloud Coverage Material Parameters`.
- The active DemoMap UDW state matches the `Partly_Cloudy` weather preset
  numerically: `Cloud Coverage=3.8`, `Fog=1.0`, `Wind Intensity=2.0`, and no
  rain/snow/dust.
- The active sky MID wisp texture override is
  `/Game/UltraDynamicSky/Textures/Sky/Cloud_Wisps`; some inspected functions
  use `/Game/UltraDynamicSky/Textures/Weather/ParticleClouds` as their default
  texture, so runtime MID values and function defaults must be documented
  separately.
- Current runtime snapshot confirms `UDS_VolumetricClouds_MPC.Cloud Density`
  has asset default `0` but editor runtime value about `1.311`.
- The current analysis now includes a signal matrix, learning curriculum, and
  Cubeless implementation backlog so the UDS findings can be taught and turned
  into owned work without copying vendor assets blindly.
- A read-only snapshot helper exists at
  `Tools/Unreal/capture_uds_sky_snapshot.py` for repeatable UDS/UDW state,
  runtime MPC, MID, dirty-package, and density-regression capture.
- A read-only dependency audit helper exists at
  `Tools/Unreal/audit_cubeless_sky_dependencies.py` for checking Cubeless sky
  assets against forbidden `/Game/UltraDynamicSky` references.
- A read-only promotion preflight helper exists at
  `Tools/Unreal/check_cubeless_sky_promotion_preflight.py`. It combines the
  dependency audit with targeted checks for `BP_SkySystem.SkyDomeMesh`,
  weather data textures, `M_Sky_Dome` dependencies, and dirty content packages.
- A read-only closeout helper exists at
  `Tools/Unreal/run_uds_analysis_closeout.py`. It compiles the helper scripts,
  captures UDS runtime state, runs Cubeless promotion preflight, records Git
  status for both managed workspaces, scans the latest Unreal editor log, and
  writes one generated closeout report.
- A read-only editor log scanner exists at
  `Tools/Unreal/scan_unreal_editor_log.py`. It reports latest Unreal log
  `Error:` and `Warning:` lines, but only fatal/crash-like entries fail the
  scan.
- A read-only delivery manifest helper exists at
  `Tools/Unreal/build_uds_analysis_delivery_manifest.py`. It classifies current
  Git status into stage candidates, manual decisions, exclusions, and unknown
  review paths.
- A read-only staging scope helper exists at
  `Tools/Unreal/check_uds_analysis_staging_scope.py`. It checks staged Git
  paths before commit work and fails if manual-decision, excluded, or unknown
  paths were staged accidentally.
- A human handoff exists at `docs/uds-analysis/handoff.md`. Use it before
  staging or commit review so the Cubeless sky assets, helper scripts, docs,
  UDS DemoMap manual decision, and unrelated `Content/ANGRY_MESH/` exclusion
  stay separated.
- Current dependency audit cleanup removed the known UDS references under
  `/Game/Cubeless/Sky`; the latest audit reports direct and recursive offender
  counts of `0`. See `cubeless-dependency-audit.md`.
- The latest promotion preflight also passes. It reports the only dirty package
  as the already documented UDS DemoMap map package, not Cubeless content.
- The latest closeout run also passes:
  `Saved/UDS_Analysis/uds_analysis_closeout_20260617_023149.json`.
- The latest editor log scan also passes:
  `Saved/UDS_Analysis/unreal_editor_log_scan_20260617_023151.json`.
  It found `fatal=0`, `ensure=0`, `error=33`, and `warning=204`; the ordinary
  error/warning lines are retained as review warnings because they include
  earlier exploratory MCP/Python attempts.
- The latest delivery manifest also passes:
  `Saved/UDS_Analysis/uds_analysis_delivery_manifest_20260617_023159.json`.
  It classifies `Content/UltraDynamicSky/Maps/DemoMap.umap` as a manual
  decision and `483` file-level `Content/ANGRY_MESH/` entries as excluded
  unrelated content.
- The latest staging scope check also passes before staging:
  `Saved/UDS_Analysis/uds_analysis_staging_scope_20260617_023149.json`.
  It found `0` staged paths and reports `35` current stage candidates not
  staged, which is expected until a commit/stage operation is requested.
- The MPC asset default was not saved. However, the loaded
  `/Game/UltraDynamicSky/Maps/DemoMap` package currently appears modified in
  Git after repair/verification, so review that binary map diff separately
  before staging or committing.

## Working Rule

Treat `/Game/UltraDynamicSky` as reference/vendor content unless the user
explicitly asks to edit it. Cubeless-owned experiments and reusable sky assets
should live outside the UDS content root.
