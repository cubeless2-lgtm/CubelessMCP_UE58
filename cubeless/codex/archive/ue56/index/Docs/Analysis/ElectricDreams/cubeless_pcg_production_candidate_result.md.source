# Cubeless PCG Production Candidate Result

Date: 2026-06-09

## Result

The first isolated Cubeless PCG production candidate was created and verified.

Created asset:

`/Game/Cubeless/PCG/ProductionCandidates/Blueprints/BP_Cubeless_PCG_EcosystemCandidate`

Validation level:

`/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_ProductionCandidate_MCP`

Verification result:

- Blueprint compile validation: passed
- Blueprint compile warnings: `0`
- Blueprint compile errors: `0`
- Validation actors: `12`
- `production_candidate_validation_pass=True`
- Latest verification marker `log_error_count=0`

## Candidate Shape

The candidate actor has these components:

- `Spline`
- `PCG_Style`
- `PCG_Tree`
- `PCG_MaterialPreview`

The designer-facing controls are:

- `PresetType`
- `DensityOverride`
- `TreeOverride`
- `MaterialMood`
- `DebugMaterialPreview`

`DebugMaterialPreview` defaults to `False`, so normal candidate use avoids
extra preview-only material ISM output.

## Presets Verified

The verifier covered these production-facing cases:

- `Preset_MixedMeadowDefault`
- `Preset_DenseGroundFoliage`
- `Preset_RockySparse`
- `Preset_LightConiferEdge`
- `Preset_ClassicGrassFill`
- `Override_DensitySparse`
- `Override_DensityDense`
- `Override_TreeOff`
- `Override_TreeLightGrove`
- `Override_MaterialCoolDark`
- `Override_MaterialWarmSoft`
- `Debug_MaterialPreviewOn`

For each case, the verifier checked the resolved learning axes, component graph
paths, expected style/tree output, material preview behavior, and editor log
errors after the verification marker.

## Landscape Status

The first candidate validation did not require a Landscape. It proved candidate
control mapping, graph routing, generated output counts, material preview
gating, and editor log cleanliness.

A direct Landscape validation was added after the user provided:

`/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`

That map is now the current disposable Landscape validation fixture for this
candidate.

## Surface Smoke Follow-Up

After the editor-close crash report, the bridge was restarted and a disposable
surface smoke validation was added:

- Prepare script:
  `prepare_cubeless_pcg_production_candidate_surface_validation.py`
- Verify script:
  `verify_cubeless_pcg_production_candidate_surface_validation.py`
- Validation level:
  `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_ProductionCandidate_Surface_MCP`
- Surface mode: `StaticMeshPlane`
- Candidate cases: `4`
- Total generated instances checked: `148`
- `surface_out_of_bounds_count=0`
- `surface_z_out_of_range_count=0`
- Latest marker `log_error_count=0`
- `production_candidate_surface_validation_pass=True`

This confirms the candidate can be placed in a simple surface context without
obvious bounds or vertical placement issues. It is still not a direct Landscape
heightfield or slope validation because the Unreal Python environment did not
expose `LandscapeEditorSubsystem` for stable automated Landscape creation.

## Landscape Direct Follow-Up

After the user-created Landscape validation map was available, a direct
Landscape validation was added:

- Prepare script:
  `prepare_cubeless_pcg_production_candidate_landscape_validation.py`
- Verify script:
  `verify_cubeless_pcg_production_candidate_landscape_validation.py`
- Validation level:
  `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`
- Landscape actors found: `65`
- Candidate cases: `4`
- Latest marker `log_error_count=0`
- `production_candidate_landscape_validation_pass=True`

Validated cases:

| Case | Style ISM | Tree ISM | Total Instances | Max Height Delta | Max Slope |
| --- | ---: | ---: | ---: | ---: | ---: |
| `FlatCenter_MixedMeadowDefault` | 26 | 1 | 27 | `0.0` cm | `0.0` deg |
| `SlopeWest_MixedMeadowDefault` | 26 | 1 | 27 | `0.0` cm | `21.7547` deg |
| `HighSlope_RockySparse` | 3 | 0 | 3 | `0.0` cm | `17.5694` deg |
| `TreeOff_DenseGroundFoliage` | 58 | 0 | 58 | `100.0` cm | `0.0` deg |

The `TreeOff_DenseGroundFoliage` height delta is within the validation
tolerance because that generated output intentionally preserves its vertical
offset above the actor origin.

Implementation change:

- `CubelessEDPCG.py` now performs a Landscape trace for generated ISM output and
  conforms production-candidate instances to the Landscape surface.
- The conform pass is also scheduled after apply because PCG output can appear
  after the initial call. It retries briefly and caches each instance's original
  vertical offset so repeated conform passes are idempotent.
- The same production-candidate validation was rerun after this change and
  still reported `production_candidate_validation_pass=True` with
  `log_error_count=0`, so the Landscape conform path did not break the
  non-Landscape validation route.

Follow-up correction:

- A direct retest showed the editor was left on the non-Landscape validation
  level after the normal 12-case regression. The editor was relaunched directly
  into `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP` and the Landscape
  validation was rerun there.
- During that retest, loading the Landscape map from a dirty temp map through
  MCP Python hit a UE `World Memory Leaks` assert because a previous Python
  error path retained a package reference. The production candidate validators
  now clear Python exception state, run Python GC, and request Unreal GC before
  loading or creating validation maps.
- The first retry exposed a non-idempotent Landscape conform bug on sloped
  terrain. Repeated conform passes could reuse already-adjusted Z as the next
  offset. The editor apply route now shares a vertical offset cache between the
  immediate and scheduled conform passes.
- Final retest on the Landscape map:
  `production_candidate_landscape_validation_pass=True`, `log_error_count=0`.

## Editor Close Crash Note

Observed crash:

- Time: 2026-06-09 after user-initiated editor close.
- Phase: editor shutdown after the `_MCP_Temp` production candidate validation
  map was saved.
- Log signature:
  `PCGControlFlowSettings` was reported as an abstract class being loaded in
  `/Engine/Transient`, followed by a PythonScriptPlugin access violation.

Interpretation:

- The crash happened during editor shutdown/teardown, not during candidate
  graph validation.
- The candidate Blueprint had already been saved and was not listed as dirty.
- The risky action is saving disposable PCG `_MCP_Temp` validation maps before
  closing the editor.

Mitigation added:

- Production candidate validation scripts no longer save newly created
  `_MCP_Temp` levels automatically.
- Treat `_MCP_Temp` validation maps as disposable. If the editor prompts to
  save them on close, choose not to save unless a specific temp fixture is being
  intentionally preserved.

## Implementation Notes

The production candidate route was added to:

`Plugins/CustomTools/Content/Python/ArtScripts/CubelessEDPCG.py`

The Level Editor menu tooltip was widened to include production candidate
actors:

`Plugins/CustomTools/Content/Python/ArtScripts/RegisterMenu.py`

Regression runner additions were added after the learning ecosystem selector
checks:

- `production_candidate_prepare`
- `production_candidate_verify`
- `runtime_candidate_promote`
- `runtime_candidate_prepare`
- `runtime_candidate_verify`

The candidate reuses the verified learning graph assets instead of copying or
overwriting runtime graph assets.

## Material Property Fix

The first verification pass found that the dynamic material preview graph could
not read `MaterialDomainType` and `MaterialVariantType` from the candidate
actor because those properties existed but were not visible to PCG
`GetActorProperty`.

The Blueprint builder now keeps those two internal properties visible in the
`Cubeless PCG Production Internal` category while not treating them as
designer-facing controls. The verifier now reports:

- `internal_material_check=True`
- `log_error_count=0`

## Boundaries Preserved

These were not modified:

- Original Electric Dreams assets
- `/Game/PCG/RuntimeGrass`
- `/Game/PCG/NewPCGGraph`
- Existing placed production PCG actors
- Non-exception C++ source

Generated `_MCP_Temp` validation outputs remain disposable and should not be
staged or committed.

## Scene01 Real-Level Staging

After the direct Landscape validation passed, the first approved real-level
staging probe was run in:

`/Game/Cubeless/Map/Scene01`

This level has no Landscape actors, so the staging result proves real-level
placement, selector routing, generated ISM output, and log cleanliness. It does
not replace the direct Landscape contact validation, which remains covered by:

`/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`

Staging details:

- Actor label:
  `MCP_Cubeless_PCG_Scene01Candidate_Scene01_MixedMeadowDefault_Staging_Validation`
- Location: `(0, 0, 4)`
- Preset: `MixedMeadowDefault`
- `scene01_route_validation_pass=True`
- `scene01_total_instances=27`
- Latest marker `log_error_count=0`
- `scene01_staging_validation_pass=True`

The `Scene01` package was left dirty by the unsaved staging actor and was not
saved during this pass.

## Runtime Blueprint Promotion

After the Landscape retest and Scene01 route probe, the candidate was promoted
into a Cubeless-owned runtime entry Blueprint:

`/Game/Cubeless/PCG/Runtime/Blueprints/BP_Cubeless_PCG_EcosystemRuntime`

Source Blueprint:

`/Game/Cubeless/PCG/ProductionCandidates/Blueprints/BP_Cubeless_PCG_EcosystemCandidate`

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
- Runtime Landscape marker `log_error_count=0`

The runtime Blueprint remains a stable entry point that references the verified
learning graph library. It does not copy or fork the Electric Dreams learning
graphs.

Runtime Landscape cases:

| Case | Total Instances | Max Height Delta | Max Slope |
| --- | ---: | ---: | ---: |
| `FlatCenter_MixedMeadowDefault` | 27 | `0.0` cm | `0.0` deg |
| `SlopeWest_MixedMeadowDefault` | 27 | `0.0` cm | `21.7547` deg |
| `HighSlope_RockySparse` | 3 | `0.0` cm | `17.5694` deg |
| `TreeOff_DenseGroundFoliage` | 58 | `100.0` cm | `0.0` deg |

Boundaries preserved:

- Original Electric Dreams assets were not modified.
- `/Game/PCG`, `/Game/PCG/RuntimeGrass`, and `/Game/PCG/NewPCGGraph` were not
  created or modified.
- No production level package was saved.
- Non-exception C++ was not modified.

## Next Gate

The next high-impact step is integration into a real Landscape production level
or another explicitly approved production target. That requires a separate
target decision because it would touch production placement or level packages
instead of only the isolated candidate/runtime Blueprint packages.

Promotion target audit:

- Added script:
  `audit_cubeless_pcg_production_promotion_targets.py`
- `/Game/PCG`, `/Game/PCG/RuntimeGrass`, and `/Game/PCG/NewPCGGraph` do not
  currently exist in this project.
- `/Game/Cubeless/PCG/ProductionCandidates` exists with the isolated candidate
  Blueprint.
- `/Game/Cubeless/PCG/Runtime/Blueprints` now exists with the runtime entry
  Blueprint.
- `/Game/Cubeless/PCG/ElectricDreamsLearning` exists with the verified learning
  graph library.
- `promotion_ready_for_approval=True`
- `approval_required_before_asset_changes=True`

The next user decision is the actual production placement target: an existing
Landscape level, a new Cubeless-owned Landscape level, or waiting for the
intended production PCG package/map.

## Production Field Level

After comparing available project maps, `/Game/Cubeless/TestMap` was selected
as the source Landscape because it is Cubeless-owned and contains a Landscape.
`Scene01` and `RainyConvenienceStreet` did not contain Landscape actors, and
Dreamscape `ExampleMap` was treated as third-party/demo content.

TestMap staging:

- Runtime Blueprint:
  `/Game/Cubeless/PCG/Runtime/Blueprints/BP_Cubeless_PCG_EcosystemRuntime`
- Staging actor:
  `MCP_Cubeless_PCG_TestMapRuntime_TestMap_MixedMeadowDefault_Staging_Validation`
- Placement: `(12000, 12000, -9.369850)`
- `testmap_runtime_staging_validation_pass=True`
- `testmap_landscape_total_instances=27`
- `testmap_landscape_max_abs_height_delta=0.0`
- Latest marker `log_error_count=0`
- `/Game/Cubeless/TestMap` was not saved.

Created and saved level:

`/Game/Cubeless/Map/LVL_Cubeless_PCG_Ecosystem_Field`

Field level details:

- Duplicated from saved `/Game/Cubeless/TestMap`.
- Removed inherited `PCG_ModularBuilding_Assembler_V2` from the new level.
- Placed runtime actor:
  `Cubeless_PCG_EcosystemRuntime_MixedMeadowDefault_Field_Validation`
- Preset: `MixedMeadowDefault`
- `ecosystem_field_validation_pass=True`
- `ecosystem_field_landscape_total_instances=27`
- `ecosystem_field_landscape_trace_miss_count=0`
- `ecosystem_field_landscape_height_fail_count=0`
- `ecosystem_field_landscape_xy_fail_count=0`
- `ecosystem_field_landscape_max_abs_height_delta=0.0`
- `ecosystem_field_landscape_max_slope_degrees=0.9972`
- Latest marker `log_error_count=0`
- `ecosystem_field_saved=True`
- `ecosystem_field_dirty_after_save=[]`

The editor is currently opened to the saved field level with the runtime actor
selected for visual review.

Field layout refine:

- The first saved actor was valid but too narrow for a field: `27` instances
  with an effective Y extent of `0.0`.
- The level was refined to three saved runtime actors:
  `Cubeless_PCG_EcosystemRuntime_MeadowCenter`,
  `Cubeless_PCG_EcosystemRuntime_GroundFoliageSouth`, and
  `Cubeless_PCG_EcosystemRuntime_RockyEdgeEast`.
- Total instances after refine: `88`.
- Actor composition: mixed meadow center (`27`), dense ground foliage south with
  trees off (`58`), and rocky sparse east edge (`3`).
- All actors passed Landscape contact validation with `trace_miss_count=0`,
  `height_fail_count=0`, and `xy_fail_count=0`.
- Latest marker `log_error_count=0`.
- `field_layout_refine_validation_pass=True`.
- `field_layout_refine_saved=True`.
- `dirty_after_save=[]`.
- A read-only saved-field verifier was added:
  `verify_cubeless_pcg_ecosystem_field_level.py`.
- It was added to `run_pcg_study_regression.py` as
  `ecosystem_field_level_verify` and passed a targeted regression run with
  `pcg_study_regression_pass=True`.
