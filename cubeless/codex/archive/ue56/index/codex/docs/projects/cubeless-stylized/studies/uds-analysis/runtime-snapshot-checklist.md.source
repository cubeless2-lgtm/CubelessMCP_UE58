# Runtime Snapshot Checklist

Use this checklist when UDS visuals do not match expected asset defaults or
when Cubeless needs to capture a reference state.

The current snapshot was taken from:

- Map: `/Game/UltraDynamicSky/Maps/DemoMap`
- UDS actor: `Ultra_Dynamic_Sky`
- UDW actor: `Ultra_Dynamic_Weather`
- Dirty package at snapshot time:
  `/Game/UltraDynamicSky/Maps/DemoMap`

## Snapshot Order

Capture state in this order:

1. Current map and dirty packages.
2. UDS actor mode and cloud scalar properties.
3. UDW actor weather state properties.
4. UDS Blueprint derived function results.
5. UDS volumetric cloud MPC asset defaults and runtime values.
6. UDW weather MPC asset defaults and runtime values.
7. Sky sphere runtime MID values.
8. VolumetricCloud component material and visibility.
9. Screenshot.

The order matters because UDS asset defaults, actor properties, Blueprint
derived values, runtime MPC values, and runtime MID values can disagree.

## Snapshot Helper

Use the closeout helper when you want the whole current UDS/Cubeless sky state
checked in one pass:

```powershell
& 'C:\Users\Lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  Tools\Unreal\run_uds_analysis_closeout.py --timestamped-output --capture-screenshot
```

The closeout helper is read-only. It compiles the local helper scripts, captures
the current UDS/UDW runtime state, captures a screenshot when requested, runs
Cubeless sky promotion preflight, scans the latest Unreal editor log, records
Git status for both managed workspaces, and writes a single report under
`Saved/UDS_Analysis`.

The command prints a compact summary by default. The written report file is
always full JSON; use `--full-json` only if stdout needs the full payload.

Use the read-only helper when the UnrealMCP bridge is live:

```powershell
& 'C:\Users\Lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  Tools\Unreal\capture_uds_sky_snapshot.py --timestamped-output --capture-screenshot
```

Default output:

- `Saved/UDS_Analysis/uds_sky_snapshot.json`
- timestamped JSON when `--timestamped-output` is supplied
- timestamped PNG when `--capture-screenshot` is supplied

The helper captures UDS/UDW actor values, selected Blueprint function results,
runtime MPC values, native MCP MPC default/runtime reads, sky sphere MID values,
VolumetricCloud component state, dirty packages, and a density regression check.
It does not save packages or edit assets.

## Cubeless Dependency Audit Helper

Use the read-only dependency audit when checking whether Cubeless sky assets are
still tied to UDS:

```powershell
& 'C:\Users\Lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  Tools\Unreal\audit_cubeless_sky_dependencies.py --timestamped-output
```

Default target:

- package root: `/Game/Cubeless/Sky`
- forbidden root: `/Game/UltraDynamicSky`
- report root: `Saved/UDS_Analysis`

The audit is expected to fail while known legacy Cubeless sky assets still
depend on UDS. A failure is useful evidence: it names the offending asset,
dependency class, and recursive dependency path.

## Latest Closeout

Generated report:

`Saved/UDS_Analysis/uds_analysis_closeout_20260617_023149.json`

Generated snapshot and screenshot:

- `Saved/UDS_Analysis/uds_sky_snapshot_20260617_023149.json`
- `Saved/UDS_Analysis/uds_sky_snapshot_20260617_023149.png`

Generated promotion preflight:

`Saved/UDS_Analysis/cubeless_sky_promotion_preflight_20260617_023150.json`

Generated editor log scan:

`Saved/UDS_Analysis/unreal_editor_log_scan_20260617_023151.json`

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

Warnings were informational only:

- UDS DemoMap remains a dirty map package from the runtime cloud repair.
- Unrelated untracked `Content/ANGRY_MESH/` is present and was not touched.
- Latest Unreal log contains no fatal or ensure entries, but does contain
  `33` ordinary `Error:` lines and `204` `Warning:` lines from earlier
  exploratory MCP/Python attempts.

## Current UDS Actor Snapshot

| Field | Value |
| --- | ---: |
| `Sky Mode` | `VOLUMETRIC_CLOUDS` |
| `Feature Level` | `DESKTOP_CONSOLE` |
| `Project Mode` | `GAME_REAL_TIME` |
| `Cloud Coverage` | `3.799999952316284` |
| `Cloud Coverage 0-3` | `1.1399999856948853` |
| `Cloud Speed` | `0.35` |
| `Cloud Direction` | `180.0` |
| `Cloud Phase` | `0.0` |
| `Composite Weather Change Speed` | `0.0` |
| `Using Volumetric Clouds` | `true` |
| `Two Layers` | `false` |
| `Time of Day` | `1088.000178` |

Current UDS function results:

| Function | Value |
| --- | ---: |
| `Current Volumetric Clouds Density` | `1.310999983549118` |
| `Get Current Volumetric Cloud Extinction Scale` | `10.0` |
| `Current Volumetric Cloud Macro Variation` | `0.16` |
| `Current Base Clouds Scale` | `1200000.0` |
| `Cloud Shadows Cloud Density` | `1.1399999856948853` |

## Current UDW Actor Snapshot

| Field | Value |
| --- | ---: |
| `Weather Speed` | `1.0` |
| `Cloud Coverage` | `3.799999952316284` |
| `Fog` | `1.0` |
| `Rain` | `0.0` |
| `Snow` | `0.0` |
| `Dust` | `0.0` |
| `Wind Intensity` | `2.0` |

Current UDW function results:

| Function | Value |
| --- | ---: |
| `Currently Cloudy` | `false` |
| `Currently Raining` | `false` |
| `Currently Snowing` | `false` |
| `Currently Foggy` | `false` |
| `Currently Dusty` | `false` |
| `Sky Cloud Speed` | `0.0735427785233055` |
| `Get Weather Speed` | `1.0` |

Current global/local weather state objects are transient instances inside the
loaded DemoMap:

- Global:
  `/Game/UltraDynamicSky/Maps/DemoMap.DemoMap:PersistentLevel.Ultra_Dynamic_Weather_C_1.UDS_Weather_Settings_C_5`
- Local:
  `/Game/UltraDynamicSky/Maps/DemoMap.DemoMap:PersistentLevel.Ultra_Dynamic_Weather_C_1.UDS_Weather_Settings_C_7`

## UDS Volumetric MPC Snapshot

Collection:

`/Game/UltraDynamicSky/Materials/Material_Functions/UDS_VolumetricClouds_MPC`

| Parameter | Asset Default | Runtime |
| --- | ---: | ---: |
| `Cloud Density` | `0` | `1.3109999895095825` |
| `Layer 2 Density` | `0` | `0` |
| `Cloud Coverage Target Opacity` | `1` | `1` |
| `Layer 2 Cloud Coverage Target Opacity` | `0` | `0` |
| `Bottom Altitude` | `0` | `59940` |
| `Top Altitude` | `0` | `129940` |
| `Cloud Layer Height` | `50000` | `100000` |
| `Clouds Scale` | `1157414.25` | `1200000` |
| `Macro Scale` | `1` | `1.7549999952316284` |
| `Macro Variation` | `0.5` | `0.1599999964237213` |
| `High Frequency Noise` | `0.2` | `0.23999999463558197` |
| `3D Erosion` | `1` | `1.2000000476837158` |
| `Extinction Scale` | `10` | `10` |
| `PhaseG` | `0.75` | `0.8500000238418579` |
| `PhaseG2` | `0.1` | `0.4000000059604645` |
| `Phase Blend` | `0.6000000238418579` | `0.6499999761581421` |
| `Cloud Shadows Cancel` | `1` | `1` |
| `Cloud Shadow Falloff` | `20000` | `15000` |

This is the key proof that the current cloud restoration is runtime-only:
`Cloud Density` is still `0` on the MPC asset default but `1.311` in the editor
runtime instance.

## Weather MPC Snapshot

Collection:

`/Game/UltraDynamicSky/Materials/Weather/UltraDynamicWeather_Parameters`

| Parameter | Asset Default | Runtime |
| --- | ---: | ---: |
| `Cloud Coverage` | `0` | `3.799999952316284` |
| `Fog` | `0` | `1` |
| `Wind Intensity` | `0` | `0.20000000298023224` |
| `Wind Angle` | `0` | `180` |
| `Cloud Bottom Altitude` | `70000` | `59940` |
| `Time of Day` | `960` | `960` |

Runtime vectors:

| Parameter | Runtime |
| --- | --- |
| `Wind Force` | `(-178.885437, 0.000004, 0, 1)` |
| `Sun Vector` | `(0.4786598, -0.2890313, -0.8290631, 1)` |
| `Moon Vector` | `(-0.6245860, 0.1414588, 0.7680376, 1)` |
| `Ambient Fog Color` | `(0.1435080, 0.2595527, 0.4861628, 0.96)` |
| `Lightning Color` | `(0.3112790, 0.4392300, 0.7031250, 35)` |

Note the scale difference: UDW actor `Wind Intensity` is `2.0`, while the
weather MPC runtime `Wind Intensity` is `0.2`. UDS/UDW often remap actor-facing
values before writing MPCs.

## Volumetric Cloud Triage

If volumetric clouds disappear:

1. Confirm sky mode:
   `Sky Mode == VOLUMETRIC_CLOUDS`.
2. Confirm the VolumetricCloud component is visible and has a material.
3. Read UDS `Cloud Coverage` and `Cloud Coverage 0-3`.
4. Call `Current Volumetric Clouds Density`.
5. Read runtime `UDS_VolumetricClouds_MPC.Cloud Density`.
6. If derived density is greater than zero but runtime MPC density is zero,
   inspect `Update Cloud Coverage Material Parameters`.
7. Check `Composite Weather Change Speed`.
8. Apply a runtime-only repair only if the user wants the current editor session
   restored:
   - `set_asset_defaults=false`
   - `set_runtime=true`
   - `save=false`
9. Re-read runtime MPC values and take a screenshot.
10. Report dirty package state separately from asset-default state.

The current confirmed failure was exactly this:

- derived UDS density: about `1.311`
- runtime MPC density before repair: `0`
- blocking gate: `Composite Weather Change Speed > 0`
- current dirty package after repair/verification:
  `/Game/UltraDynamicSky/Maps/DemoMap`

## Cubeless Snapshot Rule

For Cubeless work, never treat UDS asset defaults as the final visual state.
Always capture:

- actor properties
- Blueprint function results
- runtime MPC values
- runtime MID values
- component visibility/material
- screenshot

Only after those agree should values be translated into Cubeless-owned assets.
