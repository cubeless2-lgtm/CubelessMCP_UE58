# Material Dependency Index

This file records direct Asset Registry dependencies for the UDS material stack.
It is meant for Cubeless dependency audits and port planning.

The dependency reads used `AssetRegistryDependencyOptions` with hard package,
soft package, searchable name, hard management, and soft management references
enabled.

## Core Materials

| Asset | Dependency Count | Main Dependency Types |
| --- | ---: | --- |
| `/Game/UltraDynamicSky/Materials/Ultra_Dynamic_Sky_Mat` | `31` | 26 material functions, 3 textures, weather MPC |
| `/Game/UltraDynamicSky/Materials/Volumetric_Clouds` | `7` | 3 material functions, 4 textures |
| `/Game/UltraDynamicSky/Materials/Cloud_Shadows_and_Caustics` | `17` | 9 material functions, 7 textures, weather MPC |
| `/Game/UltraDynamicSky/Materials/Global_Volumetric_Fog` | `14` | 5 material functions, 8 textures, weather MPC |
| `/Game/UltraDynamicSky/Materials/Cloud_Fog_PostProcess` | `8` | 3 material functions, 3 textures, weather MPC, engine remap function |

## Sky Dome Material Dependencies

`Ultra_Dynamic_Sky_Mat` depends on:

- Engine function:
  `/Engine/Functions/Engine_MaterialFunctions02/Texturing/CustomRotator`
- UDS material functions:
  - `Aurora`
  - `Base_Sky_Color`
  - `Cloud_Distribution`
  - `Cloud_Layer`
  - `Cloud_UVs`
  - `Cloud_Wisps`
  - `Composite_Cloud_Layers`
  - `Composite_Static_Clouds`
  - `Contrast_Control`
  - `Filter_Clouds`
  - `FlatOvercast_Texture`
  - `Light_and_Dark_Cloud_Colors`
  - `Map_Cloud_Textures`
  - `Moon`
  - `Moon_Centered_Gradient`
  - `SC_DirectionalScattering`
  - `Scale_Intensity_Around_Sun`
  - `Scale_Radial_Gradient_Around_White`
  - `Shading_Gradients`
  - `Sky_Material_Ambient_Fog`
  - `Stars`
  - `Sun_Centered_Gradient`
  - `Sun_Disk`
  - `Sun_Shine_Edges`
  - `Tiling_Stars_UVs`
  - `UDS_VolumetricClouds_MPC`
- Weather MPC:
  `/Game/UltraDynamicSky/Materials/Weather/UltraDynamicWeather_Parameters`
- Textures:
  - `/Game/UltraDynamicSky/Textures/3D_Clouds/3D_Cells_32`
  - `/Game/UltraDynamicSky/Textures/Sky/Stars_Noise`
  - `/Game/UltraDynamicSky/Textures/Weather/ParticleClouds`

Cubeless implication: even the sky dome material has both weather and volumetric
MPC dependencies. A clean Cubeless sky material should be rebuilt around a
Cubeless MPC instead of inheriting this graph.

## Volumetric Cloud Material Dependencies

`Volumetric_Clouds` depends on:

- `/Game/UltraDynamicSky/Materials/Material_Functions/UDS_VolumetricClouds_MPC`
- `/Game/UltraDynamicSky/Materials/Material_Functions/Volumetric_Clouds_Conservative_Density`
- `/Game/UltraDynamicSky/Materials/Material_Functions/Volumetric_Clouds_Extinction`
- `/Game/UltraDynamicSky/Textures/3D_Clouds/3D_Cells_32`
- `/Game/UltraDynamicSky/Textures/Sky/CloudPaintTarget`
- `/Game/UltraDynamicSky/Textures/Volumetric_Clouds/Cloud_Profile`
- `/Game/UltraDynamicSky/Textures/Weather/ParticleClouds`

The two volumetric functions then depend on:

- `Volumetric_Clouds_Conservative_Density`
  - `UDS_VolumetricClouds_MPC`
  - `3D_Cells_32`
  - `CloudPaintTarget`
  - `Cloud_Profile`
- `Volumetric_Clouds_Extinction`
  - `UDS_VolumetricClouds_MPC`
  - `3D_Cells_32`
  - `Cloud_Profile`

Cubeless implication: the minimum clean fork needs a Cubeless MPC replacement,
a 3D noise texture, a cloud profile texture, and a decision about whether to
support cloud paint targets.

## Shadow And Fog Dependencies

`Cloud_Shadows_and_Caustics` depends on:

- `2D_Cloud_Shadows_MF`
- `Caustic_Refraction_Light`
- `Cloud_Shadow_Light_Angle_Offset`
- `Filter_Clouds`
- `LightFunctionAtlas_Position`
- `UDS_VolumetricClouds_MPC`
- `Volumetric_Cloud_Shadows_MF`
- `Volumetric_Clouds_Conservative_Density`
- `Water_Level_Local`
- `UltraDynamicWeather_Parameters`
- `3D_Cells_32`
- `CloudsAlpha`
- `Caustic_Pattern`
- `CloudPaintTarget`
- `WaterLevelTarget`
- `Cloud_Profile`
- `ParticleClouds`

`Global_Volumetric_Fog` depends on:

- `Caustic_Refraction_Light`
- `Sample_Weather_Mask_Brushes`
- `UDS_VolumetricClouds_MPC`
- `Volumetric_Clouds_Conservative_Density`
- `Water_Level_Local`
- `UltraDynamicWeather_Parameters`
- `3D_Cells_32`
- `3D_Cells_64`
- `Caustic_Pattern`
- `CloudPaintTarget`
- `WaterLevelTarget`
- `Cloud_Profile`
- `Weather_Mask_Brush_Target`
- `Weather_Mask_Height_Target`

`Cloud_Fog_PostProcess` depends on:

- Engine function:
  `/Engine/Functions/Engine_MaterialFunctions03/Math/RemapValueRange`
- `UDS_VolumetricClouds_MPC`
- `Volumetric_Clouds_Conservative_Density`
- `Volumetric_Clouds_Extinction`
- `UltraDynamicWeather_Parameters`
- `3D_Cells_32`
- `CloudPaintTarget`
- `Cloud_Profile`

Cubeless implication: shadow/fog parity is a second stage. Bringing these over
too early would import weather masks, water-level targets, caustics, and extra
texture targets before the cloud core is stable.

## 2D, Static, And Wisp Function Dependencies

`Composite_Cloud_Layers`:

- `Cloud_Layer`

`Composite_Static_Clouds`:

- `Base_Sky_Color`
- `Light_and_Dark_Cloud_Colors`
- `Moon_Centered_Gradient`
- `Sun_Centered_Gradient`
- `ParticleClouds`

`Cloud_Wisps`:

- `ParticleClouds`

`Map_Cloud_Textures`:

- `UDS_VolumetricClouds_MPC`
- `ParticleClouds`

Cubeless implication: a clean 2D/wisp path can be much smaller than the full
UDS sky material. The one surprising dependency is that `Map_Cloud_Textures`
also sees `UDS_VolumetricClouds_MPC`, so it should be rebuilt rather than copied
directly.

## Dependency Audit Rules For Cubeless

When building Cubeless-owned sky assets:

1. Search all new Cubeless sky assets for `/Game/UltraDynamicSky`.
2. Replace `UDS_VolumetricClouds_MPC` with a Cubeless MPC.
3. Replace `UltraDynamicWeather_Parameters` with a Cubeless weather/sky MPC or
   explicit parameters.
4. Decide texture ownership explicitly:
   - UDS textures are reference-only.
   - Cubeless textures should live outside `/Game/UltraDynamicSky`.
5. Defer cloud shadows, caustics, weather masks, and water-level targets until
   the visible cloud body is stable.
