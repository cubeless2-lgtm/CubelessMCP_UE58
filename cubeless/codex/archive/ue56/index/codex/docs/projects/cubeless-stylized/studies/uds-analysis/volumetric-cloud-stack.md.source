# UDS Volumetric Cloud Stack

## Current DemoMap State

Active actors:

- `Ultra_Dynamic_Sky`
- `Ultra_Dynamic_Weather`

Current inspected UDS values:

| Value | Current |
| --- | --- |
| `Sky Mode` | `VOLUMETRIC_CLOUDS` |
| `Feature Level` | `DESKTOP_CONSOLE` |
| `Project Mode` | `GAME_REAL_TIME` |
| `Using Volumetric Clouds` | `true` |
| `Two Layers` | `false` |
| `Cloud Coverage` | `3.8` |
| `Cloud Coverage 0-3` | `1.14` |
| `Composite Weather Change Speed` | `0.0` |
| `Cloud Speed` | `0.35` |
| `Cloud Movement Update Period` | `0.55` |
| `Active Update Speed` | `4` |

Current inspected UDW values:

| Value | Current |
| --- | --- |
| `Weather Speed` | `1.0` |
| `Cloud Coverage` | `3.8` |
| `Wind Intensity` | `2.0` |
| `Fog` | `1.0` |
| `Rain` / `Snow` / `Dust` | `0.0` |
| `Sky Cloud Speed()` | `0.0735427785` |

## Component Layer

The active `VolumetricCloud` component is visible and uses a runtime MID based
on UDS volumetric cloud material instances.

Important component values:

| Property | Current |
| --- | --- |
| `hidden_in_game` | `false` |
| `visible` | `true` |
| `layer_bottom_altitude` | `0.6000000238` |
| `layer_height` | `0.6999999881` |
| `tracing_start_max_distance` | `100.0` |
| `tracing_max_distance` | `20.0` |
| `planet_radius` | `6360.0` |

UDS also writes altitude values into `UDS_VolumetricClouds_MPC`, where the
current repaired runtime values are:

- `Bottom Altitude`: `59940`
- `Top Altitude`: `129940`
- `Cloud Layer Height`: `100000`
- `Shadows Altitude`: `77440`

## Blueprint Density Formula

`Current Volumetric Clouds Density` has separate branches for layer 1 and layer
2, but the current single-layer state returned the same value for both calls:

- layer 1: `1.310999983549118`
- layer 2: `1.310999983549118`

For layer 1, the graph uses this shape:

1. Read `Cloud Coverage 0-3`.
2. Map low coverage from input range `0.0..0.2` to output range `0.2..0.0`.
3. Subtract that low-coverage adjustment from `Cloud Coverage 0-3`.
4. Multiply by `1.15`.
5. Clamp the result to `-0.2..3.0`.

With `Cloud Coverage 0-3 = 1.14`, the value becomes about `1.311`.

## Apply Path That Failed

`Update Cloud Coverage Material Parameters` is the graph that writes the
derived density into render state.

The first branch checks:

```text
Composite Weather Change Speed > 0
```

When true, it writes:

- `Sky Sphere MID.Cloud Density = Cloud Coverage 0-3 * 1.4`
- if `Using Volumetric Clouds`:
  - `UDS_VolumetricClouds_MPC.Cloud Density = Current Volumetric Clouds Density(layer1=true)`
  - if `Two Layers`:
    - `UDS_VolumetricClouds_MPC.Layer 2 Density = Current Volumetric Clouds Density(layer1=false)`
- else:
  - `UDS_VolumetricClouds_MPC.Cloud Density = Cloud Coverage 0-3`
- if `Use Cloud Shadows`:
  - `Cloud Shadows MID.Cloud Density = Cloud Shadows Cloud Density`

The observed bug state had `Composite Weather Change Speed = 0.0`, so this
entire write path did not run. The component and material were valid, but the
MPC density remained at the asset default `0`.

## Applied Runtime Repair

The repair wrote only the current editor-world runtime MPC instance:

| Parameter | Value |
| --- | ---: |
| `Cloud Density` | `1.311` |
| `Layer 2 Density` | `0.0` |

The sky sphere MID was also repaired with:

```text
Cloud Coverage 0-3 * 1.4 = 1.596
```

This did not save the MPC asset default.

## Material Graph

`/Game/UltraDynamicSky/Materials/Volumetric_Clouds`:

| Metric | Value |
| --- | ---: |
| Graph nodes | 114 |
| Texture samples | 1 |
| Material function calls | 2 |
| Static switches | 2 |
| Custom HLSL nodes | 0 |
| MPC collection parameter nodes | 21 |

Material settings:

- domain: `MD_Volume`
- blend mode: `BLEND_Additive`
- main functions:
  - `Volumetric_Clouds_Conservative_Density`
  - `Volumetric_Clouds_Extinction`
- key texture sample:
  - `/Game/UltraDynamicSky/Textures/Weather/ParticleClouds`
- static switch:
  - `TwoLayers`

## Conservative Density Function

`Volumetric_Clouds_Conservative_Density` reads these MPC parameters:

- movement/position:
  - `Clouds Position`
  - `Clouds B Time`
  - `Clouds B Speed`
  - `Z Formation Shift`
- scale:
  - `Clouds Scale`
  - `Layer Scale`
  - `Layer2 X Scale`
  - `Layer2 Y Scale`
  - `Layer 2 MipOffset`
  - `Clouds Mip Level`
- density/coverage:
  - `Cloud Density`
  - `Layer 2 Density`
  - `Cloud Coverage Target Opacity`
  - `Layer 2 Cloud Coverage Target Opacity`
  - `Drawn Target Mapping`
- macro/floor:
  - `Macro Scale`
  - `Macro Variation`
  - `Lerp to Simplified`
  - `Floor Variation Clear`
  - `Floor Variation Cloudy`

This is the shape and occupancy side of the volume.

## Extinction Function

`Volumetric_Clouds_Extinction` reads these MPC parameters:

- `Clouds Position`
- `Bottom Altitude`
- `3D Noise Scale High`
- `3D Noise Scale Low`
- `HF Octave Zero Distance`
- `HF Octaves`
- `HF Distortion`
- `Lerp to Simplified`
- `3D Erosion Power`
- `3D Erosion`
- `Minimum Erosion`
- `High Frequency Noise`
- `Extinction Scale`
- `Layer 2 Extinction`

This is the erosion/detail/extinction side of the volume.

## Runtime MPC Snapshot

Important repaired runtime values:

| Parameter | Runtime value |
| --- | ---: |
| `Cloud Density` | `1.3109999895` |
| `Layer 2 Density` | `0.0` |
| `Cloud Coverage Target Opacity` | `1.0` |
| `Clouds Scale` | `1200000` |
| `Macro Scale` | `1.755` |
| `Macro Variation` | `0.16` |
| `Layer Scale` | `0.5` |
| `High Frequency Noise` | `0.24` |
| `3D Erosion` | `1.2` |
| `3D Erosion Power` | `3.0` |
| `Extinction Scale` | `10.0` |
| `Layer 2 Extinction` | `0.05` |
| `PhaseG` | `0.85` |
| `PhaseG2` | `0.4` |
| `Phase Blend` | `0.65` |
| `MultiScattering Contribution` | `0.85` |
| `MultiScattering Occlusion` | `0.5` |
| `Eccentricity` | `0.4` |

Important vectors:

| Parameter | Runtime value |
| --- | --- |
| `Albedo` | `(0.225, 0.225, 0.225, 1.0)` |
| `Top Emissive Color` | `(0.0465, 0.0737, 0.1277, 1.4525)` |
| `Bottom Emissive Color` | `(0.0263, 0.0418, 0.0724, 0.4525)` |
| `Clouds Position` | `(-150, 250, -90, 1)` |
| `Fog Position` | `(-150, 250, -90, 1)` |
| `3D Noise Scale High` | `(120000, 120000, 69600, 1)` |
| `3D Noise Scale Low` | `(84000, 84000, 48720, 1)` |

## Console Variable Layer

`Apply Volumetric Mode` writes:

- `r.VolumetricRenderTarget.Mode`
- `r.VolumetricRenderTarget`

The graph maps UDS `UDS_VolRT_Mode` enum values to integer console settings,
with a project-mode/runtime guard. This matters because a correct material and
MPC can still render empty if the editor viewport or render-target CVars are
disabled.

## Cubeless Porting Lesson

Minimum viable Cubeless-owned volumetric system:

1. A `VolumetricCloudComponent` with stable altitude/tracing settings.
2. A Cubeless-owned volume material, or a forked UDS material with UDS MPC nodes
   replaced.
3. A Cubeless-owned MPC with explicit parameters for density, coverage,
   movement, scale, erosion, extinction, lighting, and altitude.
4. A small Blueprint or editor utility that computes density from cloud
   coverage and writes MPC runtime values.
5. A validation route that checks render CVars, component visibility, material,
   and runtime MPC values before visual comparison.

Do not treat UDS `Cloud Density` as a hand-tuned constant. It is a derived value
from weather coverage and layer mode, and the failure came from the apply path,
not the density formula.
