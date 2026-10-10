# Cubeless ED Ditch Amount Presets Result

Date: 2026-06-09

## Purpose

Add the same amount/density control axis to the DitchHierarchy side of the
Electric Dreams PCG learning assembly without C++ changes.

## Production graph assets

Folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/AmountPresets`

- `PCG_Cubeless_ED_DitchHierarchyCore`
  - Reuses the validated DitchHierarchy generation chain.
  - Removes the final Static Mesh Spawner.
  - Outputs hierarchy-applied point data for amount wrappers.
- `PCG_Cubeless_ED_DitchAmount_Sparse`
  - Uses `DitchHierarchyCore`
  - Applies `PCGDensityFilterSettings` with range `0.9..1.0`
  - Expected points/ISM: 18
  - `DesignerAmountId=301`, `DesignerAmountType=1`
- `PCG_Cubeless_ED_DitchAmount_Normal`
  - Uses `DitchHierarchyCore` without extra amount filtering
  - Expected points/ISM: 42
  - `DesignerAmountId=302`, `DesignerAmountType=2`
- `PCG_Cubeless_ED_DitchAmount_Dense`
  - Uses `DitchHierarchyCore`
  - Applies `PCGDuplicatePointSettings` with `iterations=1`
  - Duplicate offset: `(55, 55, 0)`
  - Expected points/ISM: 84
  - `DesignerAmountId=303`, `DesignerAmountType=3`

All amount graphs add:

- `DesignerProfileId=20`
- `DesignerProfileType=2`
- `DesignerAmountId`
- `DesignerAmountType`
- `DesignerAmountPass=True`

They preserve the Ditch pass markers:

- `SideMaskFilterPass=True`
- `BranchDensityFilterPass=True`
- `GroundStyleSmokePass=True`

## Production Blueprint assets

Folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Amount`

- `BP_Cubeless_ED_DitchAmount_Sparse`
  - Default graph: `PCG_Cubeless_ED_DitchAmount_Sparse`
- `BP_Cubeless_ED_DitchAmount_Normal`
  - Default graph: `PCG_Cubeless_ED_DitchAmount_Normal`
- `BP_Cubeless_ED_DitchAmount_Dense`
  - Default graph: `PCG_Cubeless_ED_DitchAmount_Dense`

All three Blueprints are placeable Spline + PCG actor shells derived from the
validated `BP_Cubeless_ED_PCGDesignerControlActor`.

## Validation summary

Graph validation:

- Sparse: 18 points, 18 ISM, profile `{20: 18}`, amount `{301: 18}`,
  density range mismatch 0, log Error 0.
- Normal: 42 points, 42 ISM, profile `{20: 42}`, amount `{302: 42}`,
  log Error 0.
- Dense: 84 points, 84 ISM, profile `{20: 84}`, amount `{303: 84}`,
  log Error 0.

Blueprint validation:

- `BP_Cubeless_ED_DitchAmount_Sparse`: Spline 1, PCG 1, template graph match,
  18 points, 18 ISM, log Error 0.
- `BP_Cubeless_ED_DitchAmount_Normal`: Spline 1, PCG 1, template graph match,
  42 points, 42 ISM, log Error 0.
- `BP_Cubeless_ED_DitchAmount_Dense`: Spline 1, PCG 1, template graph match,
  84 points, 84 ISM, log Error 0.

For all validation runs:

- `DesignerProfileId/Type` mismatch count: 0
- `DesignerAmountId/Type` mismatch count: 0
- `DesignerAmountPass` count matched point count
- `SideMaskFilterPass`, `BranchDensityFilterPass`, and `GroundStyleSmokePass`
  counts matched point count

## Boundary

- No C++ was added or modified.
- Existing RuntimeGrass `NewPCGGraph` was not modified.
- Existing Ground amount assets, preset graphs, and Blueprint variants were not
  overwritten.
- `_MCP_Temp` validation actors are disposable and should not be committed.
