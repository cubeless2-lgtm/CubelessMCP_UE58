# Cubeless Implementation Roadmap

This is the recommended path for turning the UDS analysis into a Cubeless-owned
sky system without recursively depending on `/Game/UltraDynamicSky`.

## Phase 1 - Lock The Reference

Goal: keep the current UDS look reproducible as a reference.

Tasks:

1. Keep `/Game/UltraDynamicSky/Maps/DemoMap` as the live visual reference.
2. Keep the repaired screenshot:
   `Saved/CodexScreenshots/UDS_Analysis/uds_volumetric_cloud_restored_20260617.png`.
3. Record the current runtime state:
   - UDS actor values
   - UDW actor values
   - `UDS_VolumetricClouds_MPC` runtime values
   - sky MID values
4. Do not save vendor UDS assets unless the user explicitly asks.

Exit check:

- A new Cubeless implementer can reproduce why the cloud disappeared and why
  the runtime density repair made it visible again.

## Phase 2 - Define Cubeless Sky State

Goal: create a small semantic state schema before making materials.

Suggested fields:

- `CloudCoverage`
- `CloudCoverageNormalized`
- `CloudDensity`
- `Layer2Density`
- `CloudBottomAltitude`
- `CloudTopAltitude`
- `CloudScale`
- `MacroScale`
- `MacroVariation`
- `HighFrequencyNoise`
- `Erosion`
- `Extinction`
- `WindIntensity`
- `CloudSpeed`
- `CloudDirection`
- `SunVector`
- `MoonVector`
- `TopLightColor`
- `BottomLightColor`
- `WispOpacity`
- `WispColor`

Use UDS values as range references only. Do not copy runtime MPC values blindly.

Exit check:

- Cubeless has an explicit state/MPC naming plan that contains no UDS asset
  path dependencies.

## Phase 3 - Build The Cubeless Volumetric Cloud Core

Goal: produce a Cubeless-owned volumetric cloud component/material path.

Tasks:

1. Create Cubeless-local material/MPC assets outside `/Game/UltraDynamicSky`.
2. Implement only the minimum volume material controls first:
   - coverage/density
   - bottom/top altitude
   - scale
   - macro variation
   - erosion/extinction
   - top/bottom emissive or lighting tint
3. Add a controller that derives density from coverage, borrowing the UDS
   concept but not the asset graph.
4. Add a debug command/snapshot that reports component, material, and MPC
   values together.

Exit check:

- Cubeless volumetric clouds remain visible after editor restart without
  runtime-only UDS MPC repair.

## Phase 4 - Add Sky-Dome Wisps And 2D Layers

Goal: add the visual detail that volume clouds alone do not carry cheaply.

Tasks:

1. Build a Cubeless wisp overlay using a Cubeless texture parameter.
2. Use a simple direction/gradient response based on sun vector first.
3. Add a 2D cloud layer only after the wisp overlay is stable.
4. Keep texture mapping, distribution, and filtering separate:
   - UV generation
   - texture sampling
   - threshold/soften
   - lighting/distribution
5. For packed static clouds, keep the existing Cubeless RGBA channel convention:
   - `R`: upper-right key light response
   - `G`: upper-left key light response
   - `B`: overhead/front fill response
   - `A`: opacity/density

Exit check:

- Turning off the volumetric component still leaves understandable sky-dome
  cloud/wisp behavior for cheap or fallback modes.

## Phase 5 - Add Weather Presets

Goal: make Cubeless weather drive sky values without importing full UDW.

Start with a small preset table:

| Preset | Cloud | Fog | Rain | Snow | Dust | Wind |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Clear | `0.0` | `0.0` | `0.0` | `0.0` | `0.0` | `2.0` |
| PartlyCloudy | `3.8` | `1.0` | `0.0` | `0.0` | `0.0` | `2.0` |
| Cloudy | `5.0` | `1.0` | `0.0` | `0.0` | `0.0` | `2.5` |
| Overcast | `7.5` | `1.5` | `0.0` | `0.0` | `0.0` | `3.0` |
| Storm | `8.0` | `6.5` | `10.0` | `0.0` | `0.0` | `10.0` |
| Blizzard | `10.0` | `10.0` | `0.0` | `10.0` | `0.0` | `10.0` |

Only cloud, fog, and wind need to affect the first Cubeless sky pass. Rain,
snow, dust, particles, puddles, and sounds can follow later.

Exit check:

- Changing a Cubeless weather preset visibly changes sky coverage, density,
  wisp intensity, and cloud movement without touching UDS.

## Phase 6 - Dependency Audit

Goal: prove the implementation is Cubeless-owned.

Checks:

1. Recursively inspect Cubeless sky assets for `/Game/UltraDynamicSky`
   references.
2. Confirm no Cubeless material functions call UDS material functions.
3. Confirm Cubeless MPCs do not mirror UDS MPC GUIDs unless explicitly doing a
   test asset.
4. Confirm temporary MCP outputs stay under `/Game/_MCP_Temp`.
5. Keep generated validation artifacts out of Git unless explicitly requested.

Exit check:

- The Cubeless sky can be migrated without the UDS content folder.

## Phase 7 - Validation Matrix

Minimum tests:

- Editor restart smoke test.
- Volumetric cloud visibility check.
- Sky MID parameter snapshot.
- Cubeless MPC runtime snapshot.
- Low/high cloud coverage preset switch.
- Wind speed/direction movement check.
- Static/wisp overlay on/off check.
- Dependency audit.
- Screenshot comparison against the UDS reference.

For the specific bug fixed in this pass, add a regression check:

- If sky mode is volumetric and derived density is greater than zero, the active
  runtime cloud density value must not remain zero after the sky update step.
