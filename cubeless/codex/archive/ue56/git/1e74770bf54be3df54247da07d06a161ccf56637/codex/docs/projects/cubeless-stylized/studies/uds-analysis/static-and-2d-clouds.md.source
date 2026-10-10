# Static And 2D Cloud Stack

This note covers the sky-dome cloud path in UDS. It is separate from the
`VolumetricCloud` component and its volume material.

## Current DemoMap Sky MID

The active inspected UDS actor is `Ultra_Dynamic_Sky`.

The sky dome mesh component is `Sky_Sphere` and uses a runtime MID:

- MID:
  `/Game/UltraDynamicSky/Maps/DemoMap.DemoMap:PersistentLevel.Ultra_Dynamic_Sky_0.Sky_Sphere.MID_UDS_K_1`
- Parent:
  `/Game/UltraDynamicSky/Materials/Material_Instances/UDS_K`
- Parent chain:
  `UDS_K -> UDS_Default -> Ultra_Dynamic_Sky_Mat`

Important runtime MID values:

| Parameter | Value |
| --- | ---: |
| `Cloud Density` | `1.5959999561309814` |
| `Wispy Cloud Alpha` | `0.5` |
| `Cloud Wisps Gradient` | `(0.4786598, -0.2890313, -0.8290631, 5.0)` |
| `Cloud Wisps Color` | `(0.303107, 0.357943, 0.413193, 1.0)` |
| `Cloud_Wisps_Texture` | `/Game/UltraDynamicSky/Textures/Sky/Cloud_Wisps` |

The current UDS actor is still in `VOLUMETRIC_CLOUDS` mode. Static-cloud
texture overrides are not present on this active runtime MID. The sky dome still
uses wisp and sky color functions around the volumetric result.

## Sky Material Composition

`/Game/UltraDynamicSky/Materials/Ultra_Dynamic_Sky_Mat` is an unlit opaque
surface material built from material functions. The cloud-related functions
include:

- `Composite_Cloud_Layers`
- `Cloud_Layer`
- `Map_Cloud_Textures`
- `Cloud_UVs`
- `Cloud_Distribution`
- `Composite_Static_Clouds`
- `Cloud_Wisps`
- `Filter_Clouds`

There were no Custom HLSL nodes in the inspected cloud functions. UDS mostly
uses native Material Expression nodes and Material Functions.

## Layered 2D Clouds

`Composite_Cloud_Layers` is the high-level sky-dome layer combiner.

Observed graph traits:

- Node count: `23`
- Texture samples: `0`
- Function calls: `2`
- Static switches: `1`
- Main switch: `One Cloud Layer`, default `true`
- Called function: `Cloud_Layer` twice
- Named reroutes include `Layer 1 Mask Alpha` and `Layer 2 Mask Alpha`

`Cloud_Layer` builds one cloud layer from other functions.

Observed graph traits:

- Node count: `114`
- Texture samples: `0`
- Function calls: `14`
- Static switches: `1`
- Custom HLSL: `0`
- Important calls:
  - `Map_Cloud_Textures`
  - `Cloud_Distribution`
  - `Filter_Clouds`
- Important named values:
  - `Cloud Density Middle Threshold`
  - `Edge Gradient`
  - `Edge Shine Masked with Density`
  - `Is Cloud Layer 1`

The key point: `Cloud_Layer` does not sample texture data directly. It calls
`Map_Cloud_Textures` repeatedly, then filters and shades the resulting cloud
masks.

## Texture Mapping

`Map_Cloud_Textures` is the texture sampling hub for the sky-dome cloud layers.

Observed graph traits:

- Node count: `55`
- Texture samples: `4`
- Unique texture: `1`
- Shared sampler count: `4`
- Static switches: `3`
- Custom HLSL: `0`
- Texture:
  `/Game/UltraDynamicSky/Textures/Weather/ParticleClouds`
- Sampler source:
  `SSM_Wrap_WorldGroupSettings`
- Outputs:
  - `Main Clouds`
  - `Shading Clouds`
  - `Overcast Swirl`
  - `First Channel Only`
- Named reroutes:
  - `Clouds A UV`
  - `Clouds B UV`
  - `Layer 1 Bool`

This means the 2D cloud layer is not a single beauty texture. UDS samples one
cloud/noise texture multiple times through different UVs and channel meanings,
then treats the results as separate masks.

`Cloud_UVs` is a pure coordinate function:

- Node count: `30`
- Texture samples: `0`
- Function calls: `0`
- Custom HLSL: `0`

For Cubeless, this is the reusable idea: generate movement-friendly UVs first,
then sample a compact cloud mask texture through multiple interpretations.

## Distribution And Filtering

`Cloud_Distribution` shapes where layer clouds appear relative to the sun and
moon.

Observed graph traits:

- Node count: `39`
- Texture samples: `0`
- Function calls: `2`
- Static switches: `1`
- Calls:
  - `Sun_Centered_Gradient`
  - `Moon_Centered_Gradient`
- Static switch:
  `One Cloud Layer`, default `true`

`Filter_Clouds` is a compact threshold/soften helper.

- Node count: `9`
- Texture samples: `0`
- Custom HLSL: `0`

The useful Cubeless lesson is to keep cloud distribution, filtering, and texture
sampling separate. It makes the sky easier to tune than a one-piece cloud
material.

## Wisps

`Cloud_Wisps` is a lightweight sky-dome wisp overlay.

Observed graph traits:

- Node count: `21`
- Texture samples: `1`
- Unique texture in the function default:
  `/Game/UltraDynamicSky/Textures/Weather/ParticleClouds`
- Runtime DemoMap MID override:
  `/Game/UltraDynamicSky/Textures/Sky/Cloud_Wisps`
- Sampler source:
  `SSM_Wrap_WorldGroupSettings`
- Custom HLSL: `0`

The function uses a texture sample, vector parameters, dot/power/saturate math,
and quality/shading-path switches. The default/high path computes shaped wisp
intensity, while mobile/low quality can fall back to a simpler route.

For Cubeless, wisps are a good way to add long fine streaks on top of the
volumetric cloud body without forcing the volumetric material to do all visual
work.

## Static Clouds

`Composite_Static_Clouds` is the packed static-cloud path.

Observed graph traits:

- Node count: `63`
- Texture samples: `1`
- Unique texture in the function default:
  `/Game/UltraDynamicSky/Textures/Weather/ParticleClouds`
- Function calls: `4`
- Static switches: `0`
- Custom HLSL: `0`
- Calls:
  - `Base_Sky_Color`
  - `Light_and_Dark_Cloud_Colors`
  - `Sun_Centered_Gradient`
  - `Moon_Centered_Gradient`
- Named reroutes:
  - `Static Clouds Alpha`
  - `Light Color`
  - `Shadow Color`
  - `Shadow Shine Gradient`
  - `Light Shine Gradient`

Cubeless already treats static cloud source art as packed radial/polar cloud
data:

- `R`: upper-right key light response
- `G`: upper-left key light response
- `B`: overhead/front fill response
- `A`: opacity/density

Current project reference texture:

- `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02`

Previous active custom reference seen in the project:

- `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/UDS_StaticCloud_KeilanRing_RadialPacked_RGBA_2048_20260607`

## Cubeless Porting Notes

Do not copy the UDS sky material wholesale. The useful structure is:

1. A Cubeless-owned sky MID or MPC with explicit semantic parameters.
2. One or two sky-dome cloud layer functions.
3. A compact texture mapping function that can sample one mask texture through
   several UVs.
4. A distribution/filter layer for sun/moon response.
5. A separate wisp overlay for long fine cloud streaks.
6. An optional packed static-cloud mode using the Cubeless RGBA channel
   convention.

Keep texture defaults and runtime overrides separate during analysis. UDS
functions often show `ParticleClouds` as a default, while the live MID can
override a specific parameter such as `Cloud_Wisps_Texture`.
