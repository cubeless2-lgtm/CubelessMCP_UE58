# StylizedSky Plugin

`StylizedSky` is the Cubeless static-sky replacement path. It is designed to keep Ultra Dynamic Sky assets untouched while reproducing the useful part of UDS Static Sky: one controller drives sky-dome color and packed static-cloud lighting from the same sun direction.

## Runtime Actor

Class:

- `/Script/StylizedSky.StylizedSkyActor`
- C++ type: `AStylizedSkyActor`

Main behavior:

- Owns a sky-dome static mesh component.
- Owns 8 cloud-plane static mesh components.
- Reads a linked `SunActor` or falls back to `ManualSunDirection`.
- Computes sun elevation and UDS-like packed cloud channel weights.
- Applies the same look state to sky-dome and cloud materials.

The actor looks for plugin content first:

- `/StylizedSky/Materials/MI_StylizedSky_Dome_Default`
- `/StylizedSky/Materials/MI_StylizedSky_Cloud_Default`
- `/StylizedSky/Materials/MI_StylizedSky_Cloud_Tile_00` through `_07`

If plugin content is not mounted yet, it falls back to:

- `/Game/Cubeless/Env/Sky/Materials/MI_StylizedSky_Dome_Default`
- `/Game/Cubeless/Env/Sky/Materials/MI_StylizedSky_Cloud_Default`
- `/Game/Cubeless/Env/Sky/Materials/MI_StylizedSky_Cloud_Tile_00` through `_07`

## Custom-Node Materials

Generator:

- `SourceArt/Sky/create_stylized_sky_materials.py`

Generated materials:

- `M_StylizedSky_Dome_Custom`
- `MI_StylizedSky_Dome_Default`
- `M_StylizedSky_Cloud_LightPacked_Custom`
- `MI_StylizedSky_Cloud_Default`
- `MI_StylizedSky_Cloud_Tile_00` through `_07`

The cloud material uses custom HLSL nodes for:

- Atlas UV rectangle sampling.
- UDS-like packed cloud channel lighting.
- Alpha extraction from the packed cloud texture.

Packed texture contract:

- `R`: upper-right key light response
- `G`: upper-left key light response
- `B`: overhead/front fill response
- `A`: opacity/density

## Editor Placement Rules

`AStylizedSkyActor` exposes `Apply Cloud Plane Settings On Construction`.

- Default: `false`
- When `false`, construction refreshes mesh/material setup but does not overwrite cloud-plane relative location, rotation, or scale.
- Use `ApplyCloudPlaneSettingsToComponents()` only when the saved `CloudPlaneSettings` array should be pushed back onto the visible components.

This keeps hand-authored cloud art direction stable after moving, scaling, or rotating the planes in the editor.

## Cloud Dome Placement

Cloud planes should be placed as distant sky layers, not near-scene cards. The sky dome uses the engine sphere mesh scaled to `500`, so its approximate radius is `25000uu`.

`AStylizedSkyActor` now generates default cloud transforms from dome-space values:

- radius: `17800uu` to `20800uu`
- elevation: `26` to `55` degrees
- width: `4300uu` to `7600uu`
- height: `6200uu` to `10800uu`

Each cloud plane is rotated tangent to the sky hemisphere, with local Z facing the dome center. This keeps the translucent planes visually aligned to the sky dome instead of reading as vertical rectangular boards in the level.

The tall plane height is intentional. The packed cloud atlas tiles are vertically framed, and thin planes visibly squash the painted clouds.

## Current Level Replacement

`/Game/ThirdPerson/Lvl_ThirdPerson` uses `StylizedSky_Main` as the active stylized sky.

- `SM_SkySphere` is kept in the level but its mesh component is hidden.
- `VolumetricCloud` is kept in the level but its render components are hidden.
- `SkyAtmosphere`, `SkyLight`, and `DirectionalLight` are left enabled.
- `StylizedSky_Main` links to the level `DirectionalLight`.

Night look is intentionally dark:

- cloud light tint: low blue-gray
- cloud intensity: `0.16`
- cloud opacity: `0.32`
- stars intensity: `0.40`

## Look Tuning

The user-facing edit point is the placed `StylizedSky_Main` actor in `/Game/ThirdPerson/Lvl_ThirdPerson`.

Primary cloud color controls:

- `DayLook.CloudLightTint`: daytime lit cloud color
- `DayLook.CloudShadowTint`: daytime cloud shadow color
- `DayLook.TimeOfDayTint`: final daytime cloud multiplier
- `SunsetLook.CloudLightTint`: sunset lit cloud color
- `SunsetLook.CloudShadowTint`: sunset cloud shadow color
- `SunsetLook.TimeOfDayTint`: final sunset cloud multiplier
- `NightLook.CloudLightTint`: moonlit cloud color
- `NightLook.CloudShadowTint`: night cloud shadow color
- `NightLook.TimeOfDayTint`: final night cloud multiplier
- `CloudOpacity` and `CloudIntensity`: per-look opacity and brightness

Default C++ values live in:

- `Plugins/StylizedSky/Source/StylizedSky/Private/StylizedSkyActor.cpp`
- `FStylizedSkyLookSettings::FStylizedSkyLookSettings()` for base defaults
- `AStylizedSkyActor::AStylizedSkyActor()` for Day/Sunset/Night presets and blend thresholds

Current blend thresholds:

- `DayStartElevation`: `4`
- `DayFullElevation`: `14`
- `SunsetPeakElevation`: `0`
- `SunsetWidthDegrees`: `10`
- `NightFullElevation`: `-12`
- `NightEndElevation`: `-2`

This makes sun elevation around `8` degrees read mostly as day instead of staying heavily tinted by sunset.

Reference-inspired palette:

- Day sky: cobalt zenith `(0.015, 0.16, 0.82)`, cyan horizon `(0.22, 0.66, 1.0)`.
- Day cloud: cream light `(1.0, 0.90, 0.72)`, saturated blue shadow `(0.18, 0.34, 0.82)`, intensity `1.22`.
- Sunset sky: ultramarine zenith `(0.015, 0.10, 0.58)`, peach horizon `(1.0, 0.46, 0.26)`.
- Sunset cloud: peach light `(1.0, 0.68, 0.46)`, blue-violet shadow `(0.20, 0.22, 0.68)`, intensity `1.32`.
- Night sky: deep ultramarine zenith `(0.001, 0.006, 0.035)`.
- Night cloud: moonlit blue `(0.08, 0.12, 0.22)`, near-black blue shadow `(0.004, 0.006, 0.018)`, intensity `0.16`.

The provided reference reads closest when the sun elevation is around `0` to `8` degrees, where `SunsetLook` blends into `DayLook`.
