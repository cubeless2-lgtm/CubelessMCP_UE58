# UDS Learning Curriculum

This curriculum is for teaching UDS deeply enough to build a Cubeless-owned sky
system. It assumes the reader has the current analysis docs and can inspect the
Unreal project, but it does not require modifying UDS vendor assets.

## Goal

Learn UDS as a set of runtime ideas:

- weather state drives sky state
- sky state derives material-facing values
- MPCs and MIDs carry the final visual inputs
- volume, 2D, wisp, fog, and shadow layers are separate render paths
- asset defaults are not reliable proof of the current view

Do not learn UDS as a set of assets to copy wholesale.

## Module 1 - Orientation And Safety

Read:

1. `README.md`
2. `current-volumetric-cloud-repair.md`
3. `asset-map.md`

Exercise:

- Locate the active map, UDS actor, UDW actor, volumetric cloud component, sky
  sphere mesh, UDS volumetric MPC, and weather MPC.
- Write down which items are vendor references and which future items should be
  Cubeless-owned.

Pass criteria:

- The learner can explain why `/Game/UltraDynamicSky` should stay reference
  content.
- The learner can identify why the current cloud repair was runtime-only and
  why the modified DemoMap package must be reviewed separately.

## Module 2 - Runtime State Before Graphs

Read:

1. `runtime-snapshot-checklist.md`
2. `state-signal-matrix.md`

Exercise:

- Capture or inspect the five state layers for the active view:
  - UDS actor values
  - UDW actor values
  - UDS Blueprint function results
  - runtime MPC values
  - runtime MID values
- Compare `Cloud Density` asset default against runtime.

Pass criteria:

- The learner can explain why `Cloud Density=0` on the MPC asset default did
  not remain the correct visual state after repair.
- The learner can explain why `Currently Cloudy=false` does not prove there are
  no clouds in the scene.

## Module 3 - UDS/UDW Blueprint Control Flow

Read:

1. `blueprint-index.md`
2. `blueprint-flow.md`
3. `runtime-flow.md`

Exercise:

- Trace this path:
  - UDW `State Change - Cloud Coverage`
  - UDS `AP - Cloud Coverage`
  - `Update Active Variables`
  - `Update Cloud Coverage Material Parameters`
  - `UDS_VolumetricClouds_MPC.Cloud Density`
- Identify where the current failure happened.

Pass criteria:

- The learner can name `Composite Weather Change Speed > 0` as the gate that
  blocked the density write.
- The learner can distinguish thin apply-property functions from the functions
  that derive final visual values.

## Module 4 - Volumetric Cloud Stack

Read:

1. `volumetric-cloud-stack.md`
2. `material-mpc-flow.md`
3. `material-dependency-index.md`

Exercise:

- List the minimum values needed for visible volume clouds:
  - component visible
  - material assigned
  - positive density
  - valid bottom/top altitude
  - useful scale and erosion
  - sane extinction/scattering
- Compare UDS Blueprint-derived values against runtime MPC values.

Pass criteria:

- The learner can diagnose a missing volume cloud without editing assets first.
- The learner can describe why Cubeless should implement a density guard or
  forced sync when derived density is positive but material-facing density is
  zero.

## Module 5 - Sky Dome, Static Clouds, And Wisps

Read:

1. `static-and-2d-clouds.md`
2. `state-signal-matrix.md`
3. `material-dependency-index.md`

Exercise:

- Trace the sky-dome chain:
  - `Composite_Cloud_Layers`
  - `Cloud_Layer`
  - `Map_Cloud_Textures`
  - `Cloud_Wisps`
  - sky sphere MID texture overrides
- Record which values come from function defaults and which come from runtime
  MID overrides.

Pass criteria:

- The learner can explain the difference between volumetric cloud density and
  sky-dome wisp density.
- The learner can explain the packed static cloud RGBA channel convention.
- The learner can spot that `Map_Cloud_Textures` has a dependency on
  `UDS_VolumetricClouds_MPC`.

## Module 6 - Weather Presets And Events

Read:

1. `weather-preset-flow.md`
2. `runtime-flow.md`
3. `state-signal-matrix.md`

Exercise:

- Compare `Clear_Skies`, `Partly_Cloudy`, `Cloudy`, `Overcast`, `Storm`, and
  `Blizzard` values.
- Decide which fields should drive the first Cubeless sky pass and which fields
  can be postponed.

Pass criteria:

- The learner can identify `Cloud`, `Fog`, and `Wind` as enough for a first
  Cubeless weather-to-sky pass.
- The learner can explain why rain, snow, dust, puddles, particles, and sound
  should not block the first sky implementation.

## Module 7 - Material Dependencies And Vendor Boundaries

Read:

1. `material-dependency-index.md`
2. `cubeless-transfer-notes.md`
3. `cubeless-implementation-roadmap.md`

Exercise:

- Pick one future Cubeless sky asset and define the dependency audit it must
  pass.
- Identify which UDS material functions are useful as concepts but should not
  be referenced by final Cubeless materials.

Pass criteria:

- The learner can run or request a recursive dependency check for
  `/Game/UltraDynamicSky`.
- The learner can explain why direct UDS function references are not acceptable
  in final Cubeless sky assets.

## Module 8 - Cubeless Port Design

Read:

1. `cubeless-sky-backlog.md`
2. `state-signal-matrix.md`
3. `cubeless-implementation-roadmap.md`

Exercise:

- Draft a Cubeless state schema with public weather fields, semantic sky
  fields, and material-facing MPC fields.
- Add one regression check for the fixed UDS cloud-density bug.

Pass criteria:

- The learner can describe the Cubeless update chain from weather preset to
  screenshot.
- The learner can define acceptance criteria for a minimal visible Cubeless
  volumetric cloud.

## Final Practical Test

The learner passes the UDS analysis module when they can do this without
guessing:

1. Explain why the original UDS volumetric cloud disappeared.
2. Restore or validate the current runtime values without saving UDS assets.
3. Trace the relevant Blueprint and material paths.
4. Separate volume clouds from sky-dome wisps/static layers.
5. Convert the learned behavior into a Cubeless-owned backlog item.
6. State which dependencies must be rejected before final Cubeless promotion.

The goal is not memorization. The goal is to recognize which layer owns each
visual responsibility.
