# Current Volumetric Cloud Repair

## Symptom

In `/Game/UltraDynamicSky/Maps/DemoMap`, the UDS actor was configured for
volumetric clouds, but no volumetric clouds were visible.

The component-level state looked healthy:

- `Sky Mode`: `VOLUMETRIC_CLOUDS`
- `Feature Level`: `DESKTOP_CONSOLE`
- `Project Mode`: `GAME_REAL_TIME`
- `VolumetricCloud` component: visible
- Volumetric material: runtime MID based on `Volumetric_Clouds_default`
- `Cloud Coverage`: `3.8`
- `Cloud Coverage 0-3`: `1.14`
- bottom/top cloud MPC altitude: about `59940` / `129940`

The actual blocking value was in the volumetric cloud MPC:

- `/Game/UltraDynamicSky/Materials/Material_Functions/UDS_VolumetricClouds_MPC`
- `Cloud Density`: `0`
- `Layer 2 Density`: `0`

With density at zero, the component renders but samples no visible cloud body.

## Root Cause

UDS' own Blueprint function `Current Volumetric Clouds Density` returned
`1.310999983549118` for the current actor state. So the density formula was not
the problem.

The problem was the update path. `Update Cloud Coverage Material Parameters`
contains the node that writes `Cloud Density` into
`UDS_VolumetricClouds_MPC`, but that branch is gated by
`Composite Weather Change Speed > 0`. In the inspected editor state,
`Composite Weather Change Speed` was zero, so the MPC stayed at its asset default
of zero even though the derived density function returned a usable value.

The graph also writes `Sky Sphere MID.Cloud Density` through the same gate, using
`Cloud Coverage 0-3 * 1.4`.

An attempted Blueprint-path repair by temporarily setting
`Composite Weather Change Speed` was blocked because the property cannot be
edited on instances.

## Repair Applied

The repair used UDS' own derived density value and wrote it to the current editor
runtime MPC instance only:

- `Cloud Density`: `1.311`
- `Layer 2 Density`: `0`
- `set_asset_defaults`: `false`
- `set_runtime`: `true`
- `save`: `false`

The sky sphere MID was also given the matching sky-material density used by the
same UDS graph path:

- `Sky Sphere MID.Cloud Density`: `Cloud Coverage 0-3 * 1.4`
- Result: `1.596`

Verification after repair:

- Runtime `UDS_VolumetricClouds_MPC.Cloud Density`: `1.3109999895095825`
- Runtime `Cloud Coverage Target Opacity`: `1`
- Runtime `Bottom Altitude`: `59940`
- Runtime `Top Altitude`: `129940`
- `VolumetricCloud` component remained visible.
- Screenshot: `Saved/CodexScreenshots/UDS_Analysis/uds_volumetric_cloud_restored_20260617.png`

## Persistence

The MPC value repair was an editor-session runtime repair, not an MPC asset
default change.

That is intentional for now. Saving vendor UDS assets or UDS demo maps should be
explicit. The loaded DemoMap package currently appears modified in Git after
repair/verification, so treat that binary map diff separately from the MPC asset
default state.

If this state needs to survive editor restart, the cleaner fix is to add a small
Cubeless/UnrealMCP repair command or a non-vendor editor utility that recomputes
and writes the runtime MPC values after UDS startup.

## Safe Reapply Procedure

If UDS volumetric clouds disappear again in the current session:

1. Confirm the UDS actor is actually in `VOLUMETRIC_CLOUDS` sky mode.
2. Confirm `r.VolumetricCloud`, `ShowFlag.VolumetricClouds`, and
   `r.VolumetricRenderTarget` have not been disabled in the editor viewport.
3. Call `Current Volumetric Clouds Density` on the UDS actor.
4. Write that value to runtime MPC scalar `Cloud Density`.
5. If `Two Layers` is false, keep `Layer 2 Density` at `0`.
6. Update `Sky Sphere MID.Cloud Density` to `Cloud Coverage 0-3 * 1.4`.
7. Do not save `/Game/UltraDynamicSky` packages unless the user explicitly asks.
