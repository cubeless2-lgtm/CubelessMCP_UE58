# UDS Asset Map

## Top-Level Inventory

`Content/UltraDynamicSky` contains the following top-level content folders.
Folder counts are filesystem counts, so they group Blueprint assets, generated
class-like preset assets, widgets, and utilities by folder rather than Unreal
Asset Registry class:

| Folder | Count | Role |
| --- | ---: | --- |
| `Blueprints` | 246 | Sky/weather actors, tools, widgets, utility actors, presets, managers |
| `Materials` | 214 | Sky dome, volumetric clouds, cloud shadows, fog, weather material effects |
| `Textures` | 167 | Static clouds, 3D cloud noises, sky/moon/star textures, weather masks |
| `Sound` | 96 | MetaSounds, buses, weather ambience, thunder/wind/rain layers |
| `Particles` | 59 | Niagara systems for rain, snow, dust, wind debris, lightning |
| `Meshes` | 24 | Sky sphere and editor/helper meshes |
| `Maps` | 4 | Demo and example maps |

The Unreal Asset Registry view is more precise by asset class. In the inspected
editor state it reported, among others:

| Asset class | Count |
| --- | ---: |
| `Blueprint` | 83 |
| `Material` | 76 |
| `MaterialFunction` | 82 |
| `MaterialInstanceConstant` | 62 |
| `MaterialParameterCollection` | 3 |
| `Texture2D` | 153 |
| `VolumeTexture` | 6 |
| `NiagaraSystem` | 20 |
| `NiagaraScript` | 32 |
| `SoundWave` | 68 |
| `StaticMesh` | 24 |
| `UserDefinedEnum` | 40 |
| `UserDefinedStruct` | 15 |
| `UDS_Weather_Settings_C` | 13 |
| `UDS_Climate_Preset_C` | 21 |

## Main Blueprints

- `/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky`
  - Owns the sky actor, sky sphere, sky atmosphere, height fog, skylights,
    sun/moon lights, volumetric cloud component, player occlusion, cloud paint
    manager, and editor widgets.
- `/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Weather`
  - Owns weather state, weather transitions, Niagara weather components, sound,
    material effects, lightning, wind source, local weather volumes, and WOV
    rendering.
- `/Game/UltraDynamicSky/Blueprints/Occlusion/UDS_PlayerOcclusion`
  - Used by UDS/UDW to estimate player inside/outside or directional occlusion.

## Main Components On UDS

The inspected UDS Blueprint has these important component roles:

- `SkyRoot`: root transform.
- `HeightFog`: `ExponentialHeightFogComponent`.
- `SkyAtmosphere`: `SkyAtmosphereComponent`.
- `Sky_Sphere`: static mesh using a runtime sky MID.
- `VolumetricCloud`: `VolumetricCloudComponent`.
- `Sun` / `Moon`: directional lights.
- `Cubemap Sky Light` and `Captured Scene Sky Light`: two skylight modes.
- `Cloud Paint Actors Manager`: cloud paint actor array manager.
- `UDS_PlayerOcclusion`: player/camera occlusion helper.
- editor-only handles/labels/compass meshes.

## Main Materials

- `/Game/UltraDynamicSky/Materials/Ultra_Dynamic_Sky_Mat`
  - Surface, opaque, unlit sky dome material.
  - Function-composed graph; main output is Emissive.
  - Key static switches include `Static Clouds`, `Use_Dynamic_Clouds`,
    `One Cloud Layer`, `Aurora`, `Real Stars`, `Space`, and `Use Dbuffer`.
- `/Game/UltraDynamicSky/Materials/Volumetric_Clouds`
  - Volume material used by the `VolumetricCloud` component.
  - Calls `Volumetric_Clouds_Conservative_Density` and
    `Volumetric_Clouds_Extinction`.
  - Reads `UDS_VolumetricClouds_MPC` heavily.
- `/Game/UltraDynamicSky/Materials/Cloud_Shadows_and_Caustics`
  - Light-function/shadow approximation path.
- `/Game/UltraDynamicSky/Materials/Global_Volumetric_Fog`
  - UDS fog support.
- `/Game/UltraDynamicSky/Materials/Cloud_Fog_PostProcess`
  - Inside-cloud/cloud-fog post process support.

## Main Material Functions

- `Composite_Static_Clouds`
  - Static cloud compositing for the sky material.
  - Combines lighting/shadow color logic with sun/moon-centered gradients.
- `Composite_Cloud_Layers`
  - 2D dynamic cloud layer composition.
- `Cloud_Layer`, `Cloud_UVs`, `Cloud_Wisps`
  - 2D cloud texture layout, movement, masking, and wisps.
- `Volumetric_Clouds_Conservative_Density`
  - Volumetric density shape, macro variation, coverage, layer scale, and
    cloud-position logic.
- `Volumetric_Clouds_Extinction`
  - Volumetric erosion/extinction/high-frequency detail.
- `UDS_VolumetricClouds_MPC`
  - Material Parameter Collection asset used by the volume and related cloud
    approximations.

## Main MPCs

- `/Game/UltraDynamicSky/Materials/Material_Functions/UDS_VolumetricClouds_MPC`
  - Shared volumetric cloud bus.
  - Carries density, coverage, altitude, scattering, extinction, noise scale,
    cloud movement, shadow vector, and related values.
- `/Game/UltraDynamicSky/Materials/Weather/UltraDynamicWeather_Parameters`
  - Shared weather bus.
  - Carries sun/moon vectors, wind, fog, cloud coverage, material state, wetness,
    snow/dust/rain state, lightning color, and weather masks.

## Important Textures

- Static clouds:
  - `/Game/UltraDynamicSky/Textures/StaticClouds/StaticClouds_A`
  - `/Game/UltraDynamicSky/Textures/StaticClouds/StaticClouds_B`
  - `/Game/UltraDynamicSky/Textures/StaticClouds/StaticClouds_C`
  - `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02`
  - `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/FarCloud`
  - `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/UDS_StaticCloud_KeilanRing_RadialPacked_RGBA_2048_20260607`
- Sky:
  - `Cloud_Wisps`, `CloudPaintTarget`, `Static_Overcast`, `Tiling_Stars`,
    moon textures, atmosphere LUTs.
- Volumetric:
  - `3D_Cells_*`, `3DCells_*`, `FormationVolume`, `Cloud_Profile`.
- Weather:
  - `ParticleClouds`, DLWE masks/noises, WOV targets, weather mask brush
    targets, rain/snow/dust/splash/noise textures.
