# Cubeless PCG Production Candidate Verification Plan

Date: 2026-06-09

## Purpose

Prepare the verification and regression design for the production candidate
before creating any Unreal assets.

No production package, Blueprint, PCG graph, runtime graph, C++ source, or
existing placed actor is modified by this plan.

## Reference Inventory

These are the learning-layer references that the production candidate should
reuse or map to after approval.

| Area | Reference | Source of Truth |
| --- | --- | --- |
| Menu/apply logic | `Plugins/CustomTools/Content/Python/ArtScripts/CubelessEDPCG.py` | Project repo |
| Regression runner | `Docs/Analysis/ElectricDreams/run_pcg_study_regression.py` | MCP docs repo |
| Ecosystem selector builder | `build_cubeless_ed_ecosystem_selector_blueprint.py` | MCP docs repo |
| Ecosystem selector prepare | `prepare_cubeless_ed_ecosystem_selector_validation.py` | MCP docs repo |
| Ecosystem selector verifier | `verify_cubeless_ed_ecosystem_selector_blueprint.py` | MCP docs repo |
| True material builder | `build_cubeless_ed_true_material_applied_presets.py` | MCP docs repo |
| True material verifier | `verify_cubeless_ed_true_material_applied_presets.py` | MCP docs repo |

## Graph Folder Inventory

The current menu code resolves graph paths from these package roots:

| Graph Family | Package Root |
| --- | --- |
| Matrix combos | `/Game/Cubeless/PCG/ElectricDreamsLearning/DesignerMatrixCombos` |
| Profile matrix combos | `/Game/Cubeless/PCG/ElectricDreamsLearning/DesignerProfileMatrixCombos` |
| Style profile matrix combos | `/Game/Cubeless/PCG/ElectricDreamsLearning/DesignerStyleProfileMatrixCombos` |
| Tree profile presets | `/Game/Cubeless/PCG/ElectricDreamsLearning/TreeProfilePresets` |
| Material override presets | `/Game/Cubeless/PCG/ElectricDreamsLearning/MaterialOverridePresets` |
| Dynamic material preview | `/Game/Cubeless/PCG/ElectricDreamsLearning/DynamicMaterialPrototype/PCG_Cubeless_ED_DynamicMaterialAxis_ActorPropertySelector_Compat` |
| True material style matrix | `/Game/Cubeless/PCG/ElectricDreamsLearning/TrueMaterialApplied/DesignerStyleProfileMatrixCombos` |
| True material tree presets | `/Game/Cubeless/PCG/ElectricDreamsLearning/TrueMaterialApplied/TreeProfilePresets` |

## Axis Inventory

Current learning axes:

| Axis | Values |
| --- | --- |
| `EcosystemMode` | `1 StyleOnly`, `2 TreeOnly`, `3 Combined` |
| `VisualStyleType` | `1 ClassicGrass`, `2 TallGrass`, `3 MixedGrass`, `4 GroundFoliage`, `5 SmallRocks` |
| `ProfileMode` | `1 GroundOnly`, `2 DitchOnly`, `3 Both` |
| `GroundAmountType` | `1 Sparse`, `2 Normal`, `3 Dense` |
| `DitchAmountType` | `1 Sparse`, `2 Normal`, `3 Dense` |
| `TreeStyleType` | `1 CompactConifer`, `2 ColumnConifer`, `3 MixedConifer` |
| `TreeAmountType` | `1 Solo`, `2 Sparse`, `3 LightGrove` |
| `MaterialDomainType` | `1 GroundFoliage`, `2 SmallRocks`, `3 CompactConifer/Pine family` |
| `MaterialVariantType` | `1 Default`, `2 Cool/Dark`, `3 Warm/Soft` |
| `GenerateMaterialPreview` | `True`, `False` |

Recommended production compression:

- `PresetType`
- `DensityOverride`
- `TreeOverride`
- `MaterialMood`
- `DebugMaterialPreview`

## Verification Script Shape

After approval and asset creation, add:

- `prepare_cubeless_pcg_production_candidate_validation.py`
- `verify_cubeless_pcg_production_candidate_blueprint.py`

The prepare script should:

1. Load or create the disposable validation level:
   `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_ProductionCandidate_MCP`
2. Delete existing validation actors with prefix
   `MCP_Cubeless_PCG_ProductionCandidate_`.
3. Spawn one `BP_Cubeless_PCG_EcosystemCandidate` per validation spec.
4. Configure a deterministic spline with the same point spacing as the learning
   ecosystem verifier unless a production-specific spacing is approved.
5. Set production-facing variables only:
   - `PresetType`
   - `DensityOverride`
   - `TreeOverride`
   - `MaterialMood`
   - `DebugMaterialPreview`
6. Run the project menu/apply function or the candidate-specific apply function.
7. Save no `_MCP_Temp` outputs to Git.

The verify script should:

1. Load learning reference config by importing the existing builder/verifier
   scripts rather than duplicating expected counts.
2. Resolve each production preset into the underlying learning axes.
3. Reuse the same expected graph path helpers already used by
   `CubelessEDPCG.py`.
4. Compare component graph paths, generated point counts, and disjoint ISM rows.
5. Verify inactive component outputs are zero.
6. Verify material preview behavior:
   - preview disabled: point/ISM output `0`
   - preview enabled: dynamic material preview graph path selected
7. Scan the latest editor log after a unique marker and require `Error:` count
   `0`.

## Validation Spec Draft

| Case | Purpose | Expected Behavior |
| --- | --- | --- |
| `Preset_MixedMeadowDefault` | default combined baseline | style + solo tree, material preview off |
| `Preset_DenseGroundFoliage` | dense foliage with true leaf material route | style + sparse column tree, material preview off |
| `Preset_RockySparse` | style-only rock preset | style active, tree inactive, material preview off |
| `Preset_LightConiferEdge` | combined tree edge preset | style + light grove tree, true pine material route |
| `Preset_ClassicGrassFill` | safe fill fallback | style only, default material |
| `Override_DensitySparse` | override density compression | selected preset maps to sparse amount |
| `Override_DensityDense` | override density compression | selected preset maps to dense amount |
| `Override_TreeOff` | designer tree disable | tree component output `0` |
| `Override_TreeLightGrove` | tree amount override | tree component uses LightGrove route |
| `Override_MaterialCoolDark` | material mood override | variant `2` route selected |
| `Override_MaterialWarmSoft` | material mood override | variant `3` route selected |
| `Debug_MaterialPreviewOn` | debug preview component | material preview graph generated |

## Regression Runner Patch Plan

Add these entries to `REGRESSION_STEPS` after
`ecosystem_selector_verify`:

```python
(
    "build",
    "production_candidate_prepare",
    "prepare_cubeless_pcg_production_candidate_validation.py",
),
(
    "verify",
    "production_candidate_verify",
    "verify_cubeless_pcg_production_candidate_blueprint.py",
),
```

The production candidate regression should not run before learning ecosystem
verification. If the learning selector fails, the production candidate verifier
should be treated as downstream evidence, not the first debugging target.

## Failure Interpretation

Use this order when debugging failures:

1. Blueprint compile errors or warnings.
2. Candidate preset-to-axis mapping error.
3. Learning graph path resolution mismatch.
4. Delayed PCG generation output not ready.
5. Dynamic material preview metadata relaxation case.
6. ISM count ambiguity caused by overlapping mesh families.
7. Actual editor log errors after the marker.

## Surface-Fitting Follow-Up

The first candidate validation does not require a Landscape. It verifies the
candidate actor surface, graph routing, generated output counts, material
preview gating, and editor log cleanliness.

Before promoting the candidate into a real production level or runtime graph,
add a separate surface-fitting validation using either a target production
surface or a disposable `_MCP_Temp` test surface. That follow-up should verify:

- projection or sampling onto the target surface
- height placement and ground contact
- slope behavior
- collision assumptions
- whether the candidate still behaves correctly when placed in a real level
  context

StaticMesh plane smoke status:

- Added on 2026-06-09.
- `production_candidate_surface_validation_pass=True`
- `surface_out_of_bounds_count=0`
- `surface_z_out_of_range_count=0`
- Total generated instances checked: `148`

Landscape direct validation status:

- Added on 2026-06-09 after the user provided a disposable Landscape map:
  `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`.
- `prepare_cubeless_pcg_production_candidate_landscape_validation.py`
- `verify_cubeless_pcg_production_candidate_landscape_validation.py`
- Landscape actors found: `65`
- Candidate cases checked: `4`
- `production_candidate_landscape_validation_pass=True`
- Latest marker `log_error_count=0`

Landscape direct checks cover:

- trace hit against Landscape or LandscapeStreamingProxy
- route validation for each candidate preset/override case
- generated instance contact against the Landscape surface
- XY containment around the actor placement
- height tolerance against traced Landscape Z
- slope sampling on flat and sloped terrain

The project menu apply route now includes a production-candidate Landscape
conform pass for generated ISM output. It runs immediately and schedules a
short retry window to handle delayed PCG output. Repeated conform must use a
cached original vertical offset per generated instance; otherwise a second
conform pass can apply an already-adjusted Z delta again on sloped terrain.

MCP map-load safety:

- Before switching validation maps inside Unreal Python, clear Python exception
  state, run Python GC, and request Unreal GC.
- This avoids retaining dirty `_MCP_Temp` map packages through Python traceback
  or UObject references while `LevelEditorSubsystem.load_level` is trying to
  clean up the previous world.

## Approval Blocker

This plan is ready for implementation, but actual implementation requires
approval to create the production candidate package root:

`/Game/Cubeless/PCG/ProductionCandidates/`
