# Material And MPC Flow

## Sky Dome Material

`/Game/UltraDynamicSky/Materials/Ultra_Dynamic_Sky_Mat` is a surface, opaque,
unlit material. It is not a single monolithic Custom node. It is a function
composition graph.

Important function calls include:

- `Base_Sky_Color`
- `Composite_Cloud_Layers`
- `Composite_Static_Clouds`
- `Cloud_Wisps`
- `Sun_Disk`
- `Sun_Centered_Gradient`
- `Moon_Centered_Gradient`
- `Stars`
- `Aurora`
- `Tiling_Stars_UVs`
- `Contrast_Control`
- `Scale_Intensity_Around_Sun`

Static switches determine which major visual branches are active:

- `Static Clouds`
- `Use_Dynamic_Clouds`
- `One Cloud Layer`
- `Aurora`
- `Real Stars`
- `Space`
- `Use Dbuffer`

## Static Clouds

UDS static clouds are a cheap sky mode that mimics volumetric cloud lighting
with a packed texture. The readme states that different lighting angles are
packed into the texture so clouds can approximate lighting as the sun/moon
orientation changes.

Cubeless static-cloud channel convention:

- `R`: upper-right key light response
- `G`: upper-left key light response
- `B`: overhead/front fill response
- `A`: opacity/density

Current project reference:

- `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02`

Current inspected DemoMap sky MID used:

- Parent: `/Game/UltraDynamicSky/Materials/Material_Instances/UDS_K`
- Runtime wisp texture override:
  `/Game/UltraDynamicSky/Textures/Sky/Cloud_Wisps`
- Static cloud texture seen in previous project work:
  `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/UDS_StaticCloud_KeilanRing_RadialPacked_RGBA_2048_20260607`

See `static-and-2d-clouds.md` for the full sky-dome static/wisp analysis.

## Sky Dome 2D Cloud Functions

UDS' sky-dome cloud path is layered:

- `Composite_Cloud_Layers`
  - Calls `Cloud_Layer` twice and gates one/two layer behavior with
    `One Cloud Layer`.
- `Cloud_Layer`
  - Calls `Map_Cloud_Textures`, `Cloud_Distribution`, and `Filter_Clouds`.
  - Does not sample textures directly.
- `Map_Cloud_Textures`
  - Samples `/Game/UltraDynamicSky/Textures/Weather/ParticleClouds` four times
    with wrap sampling.
  - Outputs `Main Clouds`, `Shading Clouds`, `Overcast Swirl`, and
    `First Channel Only`.
- `Cloud_Distribution`
  - Uses sun-centered and moon-centered gradients.
- `Cloud_Wisps`
  - Samples one wisp texture and applies vector/dot/power/saturate shaping.
  - In the current DemoMap MID, `Cloud_Wisps_Texture` is overridden to
    `/Game/UltraDynamicSky/Textures/Sky/Cloud_Wisps`.

These functions use native material expression nodes, not Custom HLSL.

## Volumetric Cloud Material

`/Game/UltraDynamicSky/Materials/Volumetric_Clouds` is a volume material.

Observed graph characteristics:

- Material domain: `MD_Volume`
- Blend mode: additive
- No Custom HLSL nodes in the inspected graph.
- Graph node count: `114`
- Static switches: `2`, both for `TwoLayers`
- Collection parameter nodes: `21`
- Key function calls:
  - `Volumetric_Clouds_Conservative_Density`
  - `Volumetric_Clouds_Extinction`
- Key texture:
  - `/Game/UltraDynamicSky/Textures/Weather/ParticleClouds`

The material reads `UDS_VolumetricClouds_MPC` for almost every meaningful
runtime value.

The two main function roles are:

- `Volumetric_Clouds_Conservative_Density`
  - Reads coverage, density, layer scale, cloud position, cloud scale, macro
    variation, drawn target mapping, and floor variation.
- `Volumetric_Clouds_Extinction`
  - Reads altitude, high/low 3D noise scales, high-frequency detail, erosion,
    extinction, and layer-2 extinction.

## UDS Volumetric Clouds MPC

`/Game/UltraDynamicSky/Materials/Material_Functions/UDS_VolumetricClouds_MPC`
is the main cloud bus.

Important groups:

- Density and coverage:
  - `Cloud Density`
  - `Layer 2 Density`
  - `Cloud Coverage Target Opacity`
  - `Layer 2 Cloud Coverage Target Opacity`
- Shape and movement:
  - `Clouds Position`
  - `Fog Position`
  - `Clouds Scale`
  - `Clouds B Time`
  - `Clouds B Speed`
  - `Macro Scale`
  - `Macro Variation`
  - `Layer Scale`
  - `Layer2 X Scale`
  - `Layer2 Y Scale`
- Altitude:
  - `Bottom Altitude`
  - `Top Altitude`
  - `Cloud Layer Height`
  - `Shadows Altitude`
- Lighting/scattering:
  - `Albedo`
  - `Top Emissive Color`
  - `Bottom Emissive Color`
  - `PhaseG`
  - `PhaseG2`
  - `Phase Blend`
  - `MultiScattering Contribution`
  - `MultiScattering Occlusion`
  - `Eccentricity`
- Erosion/extinction/detail:
  - `Extinction Scale`
  - `Layer 2 Extinction`
  - `HF Octaves`
  - `HF Distortion`
  - `High Frequency Noise`
  - `3D Erosion`
  - `3D Erosion Power`
  - `Minimum Erosion`
- Shadow approximation:
  - `Cloud Shadows Light Vector`
  - `Cloud Shadows Cancel`
  - `Cloud Shadow Falloff`

## Current DemoMap Runtime Snapshot

After repair, the inspected DemoMap runtime MPC had:

- `Cloud Density`: `1.3109999895095825`
- `Layer 2 Density`: `0`
- `Cloud Coverage Target Opacity`: `1`
- `Composite Weather Change Speed`: `0` on the UDS actor, which is why the
  normal Blueprint apply branch did not write these values before the repair.
- `Bottom Altitude`: `59940`
- `Top Altitude`: `129940`
- `Cloud Layer Height`: `100000`
- `Clouds Scale`: previously read as `1200000`
- `Macro Scale`: previously read as `1.755`
- `Macro Variation`: previously read as `0.16`
- `Albedo`: `(0.225, 0.225, 0.225, 1)`
- `Top Emissive Color`: about `(0.0465, 0.0737, 0.1277, 1.4525)`
- `Bottom Emissive Color`: about `(0.0263, 0.0418, 0.0724, 0.4525)`

The original failure state had `Cloud Density=0` while the derived UDS Blueprint
function returned `1.311`.

The derived density graph maps low coverage `0..0.2` to `0.2..0`, subtracts
that low-coverage adjustment from `Cloud Coverage 0-3`, multiplies by `1.15`,
and clamps the result to `-0.2..3.0`.

## Weather MPC

`/Game/UltraDynamicSky/Materials/Weather/UltraDynamicWeather_Parameters` is the
shared weather material bus.

Important inspected runtime values:

- `Cloud Coverage`: `3.799999952316284`
- `Time of Day`: `960`
- `Wind Intensity`: `0.2`
- `Wind Angle`: `180`
- `Fog`: `1`
- `Cloud Bottom Altitude`: about `70090`
- `Sun Vector`: about `(0.4787, -0.2890, -0.8291, 1)`
- `Moon Vector`: about `(-0.6246, 0.1415, 0.7680, 1)`
- `Ambient Fog Color`: about `(0.1435, 0.2596, 0.4862, 0.96)`
- `Wind Force`: about `(-178.885, 0, 0, 1)`
- `Lightning Color`: about `(0.3113, 0.4392, 0.7031, 35)`

## Key Lesson

For UDS look-matching, copying a material instance is not enough. The visible
look is the product of:

1. UDS/UDW Blueprint state.
2. Runtime MPC values.
3. Sky sphere MID values.
4. Volumetric cloud component settings.
5. Weather state and local overrides.
6. Cloud movement cache.

For Cubeless, treat the MPC values as semantic signals, not as values to copy
blindly.
