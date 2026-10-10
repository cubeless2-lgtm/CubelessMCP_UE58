# Cubeless PCG Production Promotion Plan

Date: 2026-06-10

## Purpose

Define the next gate after the isolated production candidate passed baseline,
surface, and direct Landscape validation.

This document started as the no-asset-change promotion plan. After approval, a
Cubeless-owned runtime Blueprint package was created under
`/Game/Cubeless/PCG/Runtime/`. `RuntimeGrass`, `NewPCGGraph`, original Electric
Dreams assets, production levels, and C++ remain untouched.

## Current Evidence

Validated candidate:

`/Game/Cubeless/PCG/ProductionCandidates/Blueprints/BP_Cubeless_PCG_EcosystemCandidate`

Passing validation:

- 12-case production candidate route validation:
  `production_candidate_validation_pass=True`
- StaticMesh plane surface smoke:
  `production_candidate_surface_validation_pass=True`
- Direct Landscape map validation:
  `production_candidate_landscape_validation_pass=True`
- Unsaved real-level staging in `/Game/Cubeless/Map/Scene01`:
  `scene01_staging_validation_pass=True`
- Runtime Blueprint promotion:
  `runtime_candidate_compile_saved=True`
- Runtime Blueprint 12-case validation:
  `production_candidate_validation_pass=True`
- Runtime Blueprint direct Landscape validation:
  `production_candidate_landscape_validation_pass=True`
- Latest direct Landscape marker:
  `log_error_count=0`

Direct Landscape fixture:

`/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`

## Read-Only Target Audit

Audit script:

`audit_cubeless_pcg_production_promotion_targets.py`

Current target state:

| Root | Exists | Current Meaning |
| --- | --- | --- |
| `/Game/PCG` | No | No current production PCG package root was found in this project |
| `/Game/PCG/RuntimeGrass` | No | The previously protected runtime target is absent here |
| `/Game/PCG/NewPCGGraph` | No | The previously protected graph target is absent here |
| `/Game/Cubeless/PCG/ProductionCandidates` | Yes | Contains the isolated candidate Blueprint |
| `/Game/Cubeless/PCG/ElectricDreamsLearning` | Yes | Contains the verified learning graph library |

Latest audit result:

- `candidate_exists=True`
- `runtime_roots_exist=False`
- `learning_root_ready=True`
- `promotion_ready_for_approval=True`
- `approval_required_before_asset_changes=True`
- `/Game/Cubeless/PCG/ProductionCandidates`: `1` Blueprint asset
- `/Game/Cubeless/PCG/ElectricDreamsLearning`: `303` assets:
  `272` PCGGraph, `21` Blueprint, `10` MaterialInstanceConstant

Interpretation:

- There is no existing `/Game/PCG/RuntimeGrass` or `/Game/PCG/NewPCGGraph` asset
  to patch in this current project state.
- The next production step should not assume those paths exist.
- Promotion now needs a target decision: either place the candidate in a real
  level, create a new Cubeless runtime package, or wait for the intended
  production PCG package/level to be provided.

## Scene01 Staging Result

The first approved `Option A` probe was run against:

`/Game/Cubeless/Map/Scene01`

Important context:

- `Scene01` currently has no Landscape actors, so this is a real-level route
  and output-count staging check, not a Landscape contact check.
- The candidate was placed as an unsaved staging actor at `(0, 0, 4)` using the
  conservative `MixedMeadowDefault` preset.
- No original Electric Dreams assets, learning graphs, `RuntimeGrass`,
  `NewPCGGraph`, or C++ were modified.
- The level was not saved after the staging pass.

Validation result:

- Actor label:
  `MCP_Cubeless_PCG_Scene01Candidate_Scene01_MixedMeadowDefault_Staging_Validation`
- `scene01_route_validation_pass=True`
- `scene01_style_points=26`
- `scene01_tree_points=1`
- `scene01_material_points=0`
- `scene01_total_instances=27`
- Instance counts:
  `ISM_SM_Conifer_05_0=1`,
  `ISM_SM_Grass_Medium01_0=3`,
  `ISM_SM_Grass_Medium03_0=5`,
  `ISM_SM_Grass_Medium01_1=13`,
  `ISM_SM_Grass_Medium03_1=5`
- Latest marker `log_error_count=0`
- Dirty map package after the staging pass:
  `/Game/Cubeless/Map/Scene01`
- `scene01_staging_validation_pass=True`

## Runtime Promotion Result

Approved `Option B` was executed as a Cubeless-owned runtime package, leaving
`/Game/PCG`, `/Game/PCG/RuntimeGrass`, and `/Game/PCG/NewPCGGraph` untouched.

Created runtime asset:

`/Game/Cubeless/PCG/Runtime/Blueprints/BP_Cubeless_PCG_EcosystemRuntime`

Source asset:

`/Game/Cubeless/PCG/ProductionCandidates/Blueprints/BP_Cubeless_PCG_EcosystemCandidate`

Promotion and validation scripts:

- `promote_cubeless_pcg_runtime_candidate_blueprint.py`
- `prepare_cubeless_pcg_runtime_candidate_validation.py`
- `verify_cubeless_pcg_runtime_candidate_blueprint.py`

Result:

- `runtime_candidate_created=True`
- `runtime_candidate_compile_saved=True`
- Runtime validation level:
  `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_RuntimeCandidate_MCP`
- Runtime validation actors: `12`
- Latest marker `log_error_count=0`
- Runtime 12-case route/output validation:
  `production_candidate_validation_pass=True`
- Runtime direct Landscape validation:
  `production_candidate_landscape_validation_pass=True`
- Runtime Landscape actors: `4`
- Runtime Landscape marker `log_error_count=0`

Regression runner update:

- `runtime_candidate_promote`
- `runtime_candidate_prepare`
- `runtime_candidate_verify`

The runtime Blueprint still depends on the verified learning graph library under
`/Game/Cubeless/PCG/ElectricDreamsLearning`; it is a stable entry Blueprint, not
a copied graph-library fork.

Runtime Landscape validation used the existing disposable Landscape fixture:

`/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`

Validated runtime Landscape cases:

| Case | Total Instances | Max Height Delta | Max Slope |
| --- | ---: | ---: | ---: |
| `FlatCenter_MixedMeadowDefault` | 27 | `0.0` cm | `0.0` deg |
| `SlopeWest_MixedMeadowDefault` | 27 | `0.0` cm | `21.7547` deg |
| `HighSlope_RockySparse` | 3 | `0.0` cm | `17.5694` deg |
| `TreeOff_DenseGroundFoliage` | 58 | `100.0` cm | `0.0` deg |

## TestMap Staging And Field Level

Target recommendation audit:

- `/Game/Cubeless/TestMap`: Cubeless-owned and contains `1` Landscape actor.
- `/Game/Cubeless/Map/Scene01`: Cubeless-owned but has no Landscape.
- `/Game/Cubeless/Generated/RainyConvenienceStreet/LVL_RainyConvenienceStreet_GS`:
  Cubeless-owned/generated but has no Landscape.
- `/Game/DreamscapeSeries/DreamscapeMountains/Maps/ExampleMap`: has a
  Landscape, but is third-party/demo content rather than a Cubeless production
  save target.

The approved path was:

1. Stage the runtime Blueprint in `/Game/Cubeless/TestMap` without saving.
2. If that passed, duplicate saved `TestMap` into a dedicated Cubeless field
   level.
3. Save only the new field level after route, Landscape contact, log, and dirty
   package checks passed.

TestMap staging result:

- Staging actor:
  `MCP_Cubeless_PCG_TestMapRuntime_TestMap_MixedMeadowDefault_Staging_Validation`
- Placement: `(12000, 12000, -9.369850)`
- Placement trace slope: `0.4378` deg
- `testmap_route_validation_pass=True`
- `testmap_landscape_total_instances=27`
- `testmap_landscape_trace_miss_count=0`
- `testmap_landscape_height_fail_count=0`
- `testmap_landscape_xy_fail_count=0`
- `testmap_landscape_max_abs_height_delta=0.0`
- `testmap_landscape_max_slope_degrees=0.9972`
- Latest marker `log_error_count=0`
- `testmap_runtime_staging_validation_pass=True`
- `TestMap` was not saved.

Created production field level:

`/Game/Cubeless/Map/LVL_Cubeless_PCG_Ecosystem_Field`

Field setup:

- Duplicated from saved `/Game/Cubeless/TestMap`.
- Removed the inherited `PCG_ModularBuilding_Assembler_V2` PCGVolume from the
  new field level.
- Placed runtime actor:
  `Cubeless_PCG_EcosystemRuntime_MixedMeadowDefault_Field_Validation`
- Actor Blueprint:
  `/Game/Cubeless/PCG/Runtime/Blueprints/BP_Cubeless_PCG_EcosystemRuntime`
- Placement: `(12000, 12000, -9.369850)`

Field validation/save result:

- `ecosystem_field_route_validation_pass=True`
- `ecosystem_field_landscape_total_instances=27`
- `ecosystem_field_landscape_trace_miss_count=0`
- `ecosystem_field_landscape_height_fail_count=0`
- `ecosystem_field_landscape_xy_fail_count=0`
- `ecosystem_field_landscape_max_abs_height_delta=0.0`
- `ecosystem_field_landscape_max_slope_degrees=0.9972`
- Latest marker `log_error_count=0`
- `ecosystem_field_validation_pass=True`
- `ecosystem_field_saved=True`
- `ecosystem_field_dirty_after_save=[]`

Standalone implementation scripts:

- `prepare_cubeless_pcg_testmap_runtime_staging.py`
- `verify_cubeless_pcg_testmap_runtime_staging.py`
- `prepare_cubeless_pcg_ecosystem_field_level.py`
- `verify_save_cubeless_pcg_ecosystem_field_level.py`

## Field Layout Refine

The first saved field actor produced valid output, but data QA showed it was a
thin strip: `27` instances with an effective Y extent of `0.0`. The field was
refined into three runtime actors to form a broader meadow patch:

- `Cubeless_PCG_EcosystemRuntime_MeadowCenter`
  - Preset: `MixedMeadowDefault`
  - Instances: `27`
  - Meshes: conifer, medium grass 01, medium grass 03
- `Cubeless_PCG_EcosystemRuntime_GroundFoliageSouth`
  - Preset: `DenseGroundFoliage`
  - Tree override: `Off`
  - Instances: `58`
  - Meshes: ferns, ground leaves, white/yellow flower groups
- `Cubeless_PCG_EcosystemRuntime_RockyEdgeEast`
  - Preset: `RockySparse`
  - Instances: `3`
  - Meshes: small rocks

Refine result:

- `field_total_instances=88`
- All three actors passed Landscape contact validation.
- `trace_miss_count=0`
- `height_fail_count=0`
- `xy_fail_count=0`
- Latest marker `log_error_count=0`
- `field_layout_refine_validation_pass=True`
- `field_layout_refine_saved=True`
- `dirty_after_save=[]`

Additional scripts:

- `prepare_cubeless_pcg_ecosystem_field_layout_refine.py`
- `verify_save_cubeless_pcg_ecosystem_field_layout_refine.py`
- `verify_cubeless_pcg_ecosystem_field_level.py`

Regression coverage:

- Added `ecosystem_field_level_verify` to `run_pcg_study_regression.py`.
- The regression step is read-only: it verifies the saved field level but does
  not save it.
- Latest targeted run:
  `ecosystem_field_level_verify|PASS|0.194s`,
  `pcg_study_regression_pass=True`.

## Tuning Gallery And Tuned Field Layout

After the checkpoint, a disposable tuning gallery was created under:

`/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_TuningGallery_MCP`

Gallery result:

- `9` candidate cases
- `324` total instances
- `trace_miss_count=0`
- `height_fail_count=0`
- `xy_fail_count=0`
- latest marker `log_error_count=0`

The best balanced field direction was then expanded after visual QA showed the
four-actor version was still too small. The saved field level now uses a
ten-actor broad patch:

| Actor | Role | Instances |
| --- | --- | ---: |
| `Cubeless_PCG_EcosystemRuntime_DenseMeadowWest` | dense meadow west row | 101 |
| `Cubeless_PCG_EcosystemRuntime_DenseMeadowCenter` | dense meadow center | 101 |
| `Cubeless_PCG_EcosystemRuntime_DenseMeadowEast` | dense meadow east row | 101 |
| `Cubeless_PCG_EcosystemRuntime_GroundFoliageSouthWestWarm` | warm low foliage southwest row | 58 |
| `Cubeless_PCG_EcosystemRuntime_GroundFoliageSouthWarm` | warm low foliage and flowers | 58 |
| `Cubeless_PCG_EcosystemRuntime_GroundFoliageSouthEastWarm` | warm low foliage southeast row | 58 |
| `Cubeless_PCG_EcosystemRuntime_RockyCoolEdgeEastNorth` | cool rocky north-east accent | 3 |
| `Cubeless_PCG_EcosystemRuntime_RockyCoolEdgeEastSouth` | cool rocky south-east accent | 3 |
| `Cubeless_PCG_EcosystemRuntime_ConiferGroveNorthWest` | light conifer edge | 29 |
| `Cubeless_PCG_EcosystemRuntime_ConiferGroveNorthEast` | light conifer edge | 29 |

Tuned field result:

- `field_total_instances=541`
- top-down QA bounds: `44.2m x 23.0m`
- top-down QA category counts:
  `300` meadow grass, `174` warm foliage/flowers, `61` conifer, and `6` rock
- `trace_miss_count=0`
- `height_fail_count=0`
- `xy_fail_count=0`
- latest marker `log_error_count=0`
- `field_layout_refine_saved=True`
- `dirty_after_save=[]`
- read-only regression:
  `ecosystem_field_level_verify|PASS|0.328s`
- top-down QA regression:
  `ecosystem_field_topdown_qa|PASS|0.114s`

Additional scripts:

- `prepare_cubeless_pcg_ecosystem_tuning_gallery.py`
- `verify_cubeless_pcg_ecosystem_tuning_gallery.py`
- `prepare_cubeless_pcg_ecosystem_field_tuned_layout.py`
- `verify_save_cubeless_pcg_ecosystem_field_tuned_layout.py`
- `export_cubeless_pcg_ecosystem_field_topdown_qa.py`
- `run_pcg_study_regression.py` includes `ecosystem_field_topdown_qa`

Result docs:

- `cubeless_pcg_ecosystem_tuning_gallery_result.md`
- `cubeless_pcg_ecosystem_field_tuned_layout_result.md`

QA artifacts are written under:

`D:\Git\CubelessStylized\Saved\MCP_Screenshots`

## Intent Gallery

An intent-based staging layer was added so a user-facing request can map to a
validated PCG recipe before production levels are edited.

Temp gallery:

`/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_IntentGallery_MCP`

First supported intents:

| Intent | Meaning |
| --- | --- |
| `MeadowPatch` | dense mixed meadow |
| `FlowerBand` | warm low foliage and flowers |
| `RockEdge` | cool sparse rocks for borders |
| `ConiferEdge` | light conifer edge |
| `BalancedEcosystem` | meadow + flowers + rocks + conifer edge |

Intent gallery validation:

- `intent_gallery_actor_count=9`
- `field_total_instances=483`
- `trace_miss_count=0`
- `height_fail_count=0`
- `xy_fail_count=0`
- latest marker `log_error_count=0`
- `dirty_after_save=[]`
- regression runner:
  `intent_gallery_verify|PASS|0.328s`

Additional scripts:

- `prepare_cubeless_pcg_intent_gallery.py`
- `verify_cubeless_pcg_intent_gallery.py`

Result doc:

- `cubeless_pcg_intent_gallery_result.md`

## Recommended Next Gate

Recommended path:

1. Visual-review `/Game/Cubeless/Map/LVL_Cubeless_PCG_Ecosystem_Field` in the
   editor.
2. If the look is acceptable, commit and push the tuned field checkpoint.
3. If the look is not acceptable, duplicate the field level or adjust the
   runtime actor in a new staging pass before saving more production changes.

## Approval Required

Stop before any of these:

- creating another runtime package under `/Game/Cubeless/PCG/Runtime/`
- creating `/Game/PCG` or any asset under it
- placing more PCG actors into real non-temp levels
- modifying `RuntimeGrass` or `NewPCGGraph` if those paths later appear
- saving additional production level packages
- adding non-exception C++

## Tivret Instruction For The Next Approved Step

When the user approves a production target, give Tivret this instruction:

Use the already validated runtime Blueprint
`/Game/Cubeless/PCG/Runtime/Blueprints/BP_Cubeless_PCG_EcosystemRuntime` as the
placement source. Do not modify original Electric Dreams assets, learning
graphs, `RuntimeGrass`, `NewPCGGraph`, or non-exception C++. First create or use
a staging placement under `_MCP_Temp` or the explicitly approved target level.
Configure one conservative `MixedMeadowDefault` actor and one target
art-direction preset if specified. Validate PCG route counts, generated ISM
counts, Landscape contact, editor log errors, and dirty packages before any
production package is saved.
