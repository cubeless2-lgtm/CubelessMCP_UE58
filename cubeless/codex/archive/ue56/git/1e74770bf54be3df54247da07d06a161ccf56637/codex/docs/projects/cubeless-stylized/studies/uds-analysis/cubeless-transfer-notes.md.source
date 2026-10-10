# Cubeless Transfer Notes

## Keep As Reference-Only

Do not directly save or modify `/Game/UltraDynamicSky` assets unless the user
explicitly asks. UDS is vendor/reference content for this project.

Reference-only UDS assets include:

- UDS Blueprints and demo maps.
- UDS material functions and material instances.
- UDS MPC assets.
- Static cloud reference textures such as `cloub02`.
- UDS weather presets and Niagara systems.

## Concepts Worth Reusing

### Sparse Cloud Movement Cache

UDS separates cloud movement into:

- infrequent cache update: compute new cloud/fog positions
- per-frame interpolation: write simple MPC values

Cubeless should reuse this idea. It is cheaper and easier to tune than
rebuilding all cloud coordinates every frame.

### MPC As Semantic Bus

UDS makes the same cloud/weather state visible to multiple systems:

- visible volumetric cloud material
- cloud shadows/fog approximations
- sky material
- weather effects
- sound/weather systems

Cubeless should keep its own MPC bus, but with smaller, clearer parameter groups.

### Static Cloud Packed Texture

UDS static clouds are valuable because one radial/polar texture can approximate
directional lighting cheaply.

Cubeless should keep using its own packed source art with explicit RGBA channel
roles instead of overwriting UDS static-cloud textures.

### Function-Composed Materials

UDS' sky material is large but modular. Cubeless should keep that principle:
native material nodes and material functions for semantics, with Custom nodes
only for math islands that become unreadable as native nodes.

### Separate Volume, Wisp, And 2D Layer Responsibilities

UDS does not make all cloud detail inside the volume material. The visible cloud
look is shared across:

- the volumetric cloud component/material
- sky-dome wisp overlay
- 2D cloud layer functions
- packed static-cloud fallback/reference textures

Cubeless should keep those responsibilities separate. It will be easier to
match UDS-like silhouettes and long streaks by combining a simple volume core
with a wisp/static overlay than by making the volume material carry every detail.

### Small Weather Preset Schema

UDW weather presets can be reduced to a small first-pass Cubeless schema:

- cloud coverage
- fog
- rain
- snow
- dust
- wind intensity

Only cloud, fog, and wind need to drive the first Cubeless sky pass. Rain/snow,
ground wetness, local weather masks, particles, and sound can remain later
modules.

## Do Not Copy Directly

### Full Runtime MPC Values

Previous Cubeless tests showed that exact UDS MPC copies can produce dark,
empty, smeared, or artifacted cloud looks in the Cubeless sky. UDS values are
contextual: they assume UDS textures, functions, cloud profile, scale, lighting,
and weather state.

Use UDS values as range references, not final values.

### UDS Dependency Paths

Cubeless production sky assets should not depend recursively on
`/Game/UltraDynamicSky`.

Existing project rule:

- editable Cubeless sky assets live outside UDS, usually under Cubeless sky
  folders such as `/Game/Cubeless/Env/Sky` or `/Game/Cubeless/Sky`.
- UDS source assets remain untouched.

### Static Cloud Preview Orientation

Static cloud source art must be authored for polar/radial sampling, not ordinary
flat viewport composition. Review both texture-space and applied sky-space
orientation before approving source art.

## Current Cubeless Direction

For a UDS-like but Cubeless-owned sky:

1. Keep UDS as a live reference map.
2. Capture current UDS runtime values and screenshots.
3. Use Cubeless-local textures/materials/MPCs for the target.
4. Match broad shape with volumetric cloud parameters.
5. Match long fine streaks with sky-dome wisp/static texture work.
6. Avoid direct UDS MPC copies as final values.
7. Verify recursive dependencies stay free of `/Game/UltraDynamicSky`.
8. Add wisp/2D overlay after the volumetric core is stable.
9. Add weather presets as data driving Cubeless state, not as UDW imports.

## Practical Ranges From Current Analysis

Useful reference values:

- `Cloud Coverage`: about `3.8` in current DemoMap, `5.0` in previous richer
  candidate.
- `Cloud Coverage 0-3`: about `1.14` current, `1.5` previous richer candidate.
- derived `Cloud Density`: about `1.311` current, previously `1.725`.
- `Bottom Altitude`: around `60k`.
- `Top Altitude`: around `130k`.
- `Cloud Layer Height`: `100k`.
- `Clouds Scale`: around `1.2M`.
- `Macro Scale`: around `1.755`.
- `Macro Variation`: around `0.16`.
- `PhaseG`: around `0.85` in previous richer candidate.
- `PhaseG2`: around `0.4` in previous richer candidate.
- `Phase Blend`: around `0.65` in previous richer candidate.
- `Extinction Scale`: `10`.

These are starting points only.
