# Cubeless Sky Dependency Audit

This document records the repeatable UDS dependency audit for Cubeless sky
assets. The audit checks whether assets under `/Game/Cubeless/Sky` still depend
on `/Game/UltraDynamicSky`.

## Command

Run:

```powershell
& 'C:\Users\Lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  Tools\Unreal\audit_cubeless_sky_dependencies.py --timestamped-output
```

Default behavior:

- package root: `/Game/Cubeless/Sky`
- forbidden root: `/Game/UltraDynamicSky`
- report root: `Saved/UDS_Analysis`
- Unreal asset edits: none
- package saves: none

## Promotion Preflight Command

Run this after dependency cleanup or before promotion:

```powershell
& 'C:\Users\Lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  Tools\Unreal\check_cubeless_sky_promotion_preflight.py --timestamped-output
```

This command is also read-only. It includes the recursive dependency audit, then
checks the current expected replacement state:

- `BP_SkySystem.SkyDomeMesh` uses
  `/Game/Cubeless/Sky/Meshes/SM_Cubeless_SkySphere`
- `DA_Weather_Clear`, `DA_Weather_Cloudy`, and `DA_Weather_Overcast` use
  `/Game/Cubeless/Env/Sky/Textures/T_CloudPlaneAtlas_LightPacked_UDSLike`
- `M_Sky_Dome` has no direct UDS dependency and directly uses the Cubeless
  cloud texture
- dirty Cubeless content packages are clean

## Closeout Command

Run this when you want the current UDS reference snapshot and Cubeless promotion
gate checked together:

```powershell
& 'C:\Users\Lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  Tools\Unreal\run_uds_analysis_closeout.py --timestamped-output --capture-screenshot
```

The closeout command compiles the helper scripts, captures the UDS runtime
snapshot, runs promotion preflight, scans the latest Unreal editor log, records
Git status for `CubelessStylized` and `../unreal-mcp-cubeless`, and writes a
single generated report.

## Latest Audit Before Cleanup

Generated report:

`Saved/UDS_Analysis/cubeless_sky_dependency_audit_20260617_014717.json`

Run result:

| Field | Value |
| --- | ---: |
| AssetRegistry audit success | `true` |
| Package root asset count | `34` |
| Direct offender count | `5` |
| Recursive offender count | `5` |
| Promotion pass | `false` |

The tool succeeded. The promotion gate failed because UDS references were still
present in Cubeless sky assets.

## Cleanup Applied

Tivret removed the current UDS references from `/Game/Cubeless/Sky`:

| Cubeless asset | Old reference | New reference |
| --- | --- | --- |
| `/Game/Cubeless/Sky/BP_SkySystem` component `SkyDomeMesh` | `/Game/UltraDynamicSky/Meshes/Ultra_Dynamic_Sky_Sphere` | `/Game/Cubeless/Sky/Meshes/SM_Cubeless_SkySphere` |
| `/Game/Cubeless/Sky/Materials/M_Sky_Dome` texture sample 0 | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/FarCloud` | `/Game/Cubeless/Env/Sky/Textures/T_CloudPlaneAtlas_LightPacked_UDSLike` |
| `/Game/Cubeless/Sky/Materials/M_Sky_Dome` texture sample 1 | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02` | `/Game/Cubeless/Env/Sky/Textures/T_CloudPlaneAtlas_LightPacked_UDSLike` |
| `/Game/Cubeless/Sky/Data/DA_Weather_Clear.FarCloudTexture` | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/FarCloud` | `/Game/Cubeless/Env/Sky/Textures/T_CloudPlaneAtlas_LightPacked_UDSLike` |
| `/Game/Cubeless/Sky/Data/DA_Weather_Cloudy.FarCloudTexture` | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02` | `/Game/Cubeless/Env/Sky/Textures/T_CloudPlaneAtlas_LightPacked_UDSLike` |
| `/Game/Cubeless/Sky/Data/DA_Weather_Overcast.FarCloudTexture` | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02` | `/Game/Cubeless/Env/Sky/Textures/T_CloudPlaneAtlas_LightPacked_UDSLike` |

The Cubeless sky sphere mesh was created by duplicating `/Engine/BasicShapes/Sphere`
into:

`/Game/Cubeless/Sky/Meshes/SM_Cubeless_SkySphere`

This keeps the final promoted sky dependency path free of UDS vendor content.

## Latest Audit After Cleanup

Generated report:

`Saved/UDS_Analysis/cubeless_sky_dependency_audit_20260617_015419.json`

Run result:

| Field | Value |
| --- | ---: |
| AssetRegistry audit success | `true` |
| Package root asset count | `35` |
| Direct offender count | `0` |
| Recursive offender count | `0` |
| Promotion pass | `true` |

The current `/Game/Cubeless/Sky` audit gate now passes for the forbidden root
`/Game/UltraDynamicSky`.

## Latest Promotion Preflight

Generated report:

`Saved/UDS_Analysis/cubeless_sky_promotion_preflight_20260617_023150.json`

Associated dependency audit report:

`Saved/UDS_Analysis/cubeless_sky_dependency_audit_for_preflight_20260617_023150.json`

Run result:

| Field | Value |
| --- | ---: |
| Dependency audit pass | `true` |
| Targeted Unreal preflight success | `true` |
| Expected replacement assets exist | `true` |
| `BP_SkySystem.SkyDomeMesh` uses Cubeless mesh | `true` |
| Weather data assets use Cubeless cloud texture | `true` |
| `M_Sky_Dome` has no UDS dependency | `true` |
| `M_Sky_Dome` uses Cubeless cloud texture | `true` |
| Dirty Cubeless content clean | `true` |
| Promotion preflight pass | `true` |

The preflight reported one dirty map package,
`/Game/UltraDynamicSky/Maps/DemoMap`. This is the already documented UDS
reference map touched by the runtime cloud repair, so it is reported as a
warning and does not fail the Cubeless promotion gate.

## Latest Closeout

Generated report:

`Saved/UDS_Analysis/uds_analysis_closeout_20260617_023149.json`

Run result:

| Field | Value |
| --- | ---: |
| Helper scripts compile | `true` |
| UDS snapshot pass | `true` |
| Promotion preflight pass | `true` |
| Editor log scan pass | `true` |
| Project `git diff --check` pass | `true` |
| Sibling MCP status readable | `true` |
| Closeout pass | `true` |

The closeout also generated:

- `Saved/UDS_Analysis/uds_sky_snapshot_20260617_023149.json`
- `Saved/UDS_Analysis/uds_sky_snapshot_20260617_023149.png`
- `Saved/UDS_Analysis/cubeless_sky_promotion_preflight_20260617_023150.json`
- `Saved/UDS_Analysis/unreal_editor_log_scan_20260617_023151.json`

## Offenders Found Before Cleanup

| Cubeless asset | Direct UDS dependency | Classification |
| --- | --- | --- |
| `/Game/Cubeless/Sky/BP_SkySystem` | `/Game/UltraDynamicSky/Meshes/Ultra_Dynamic_Sky_Sphere` | `uds_mesh` |
| `/Game/Cubeless/Sky/Data/DA_Weather_Clear` | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/FarCloud` | `uds_texture` |
| `/Game/Cubeless/Sky/Data/DA_Weather_Cloudy` | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02` | `uds_texture` |
| `/Game/Cubeless/Sky/Data/DA_Weather_Overcast` | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02` | `uds_texture` |
| `/Game/Cubeless/Sky/Materials/M_Sky_Dome` | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/FarCloud` | `uds_texture` |
| `/Game/Cubeless/Sky/Materials/M_Sky_Dome` | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02` | `uds_texture` |

`BP_SkySystem` also has recursive UDS texture hits through:

- `BP_SkySystem -> DA_Weather_Clear -> FarCloud`
- `BP_SkySystem -> M_Sky_Dome -> cloub02`

## Interpretation

Before cleanup, Cubeless sky work was conceptually moving toward UDS-free, but
the legacy `/Game/Cubeless/Sky` asset set was not dependency-clean yet.

This matters because:

- the sky sphere mesh still comes from UDS
- weather data assets still point at UDS static cloud textures
- `M_Sky_Dome` still points at UDS static cloud textures
- packaging or migration without `/Game/UltraDynamicSky` would still carry or
  break those references

## Cleanup Backlog Status

Completed for the current `/Game/Cubeless/Sky` root:

1. Replaced `/Game/UltraDynamicSky/Meshes/Ultra_Dynamic_Sky_Sphere` with
   `/Game/Cubeless/Sky/Meshes/SM_Cubeless_SkySphere`.
2. Replaced `FarCloud` and `cloub02` references in:
   - `DA_Weather_Clear`
   - `DA_Weather_Cloudy`
   - `DA_Weather_Overcast`
   - `M_Sky_Dome`
3. Used Cubeless-owned texture
   `/Game/Cubeless/Env/Sky/Textures/T_CloudPlaneAtlas_LightPacked_UDSLike`
   for the replacement references.
4. Re-ran:

```powershell
& 'C:\Users\Lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  Tools\Unreal\audit_cubeless_sky_dependencies.py --timestamped-output
```

5. Confirmed:
   - `direct_offender_count=0`
   - `recursive_offender_count=0`
   - `pass=true`
6. Ran promotion preflight and confirmed:
   - `dependency_audit_pass=true`
   - `bp_sky_dome_mesh_expected=true`
   - `weather_data_textures_expected=true`
   - `sky_dome_material_uds_free=true`
   - `dirty_content_clean=true`

## Current Decision

The audit, promotion preflight, and closeout commands are now ready to protect
later Cubeless sky changes from reintroducing UDS vendor references or drifting
away from the expected Cubeless replacement assets.
