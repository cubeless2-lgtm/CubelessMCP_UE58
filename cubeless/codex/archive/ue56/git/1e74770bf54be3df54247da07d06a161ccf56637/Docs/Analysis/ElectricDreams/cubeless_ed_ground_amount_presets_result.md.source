# Cubeless ED Ground Amount Presets Result

Date: 2026-06-09

## Purpose

Add the first practical amount/density control axis for the Electric Dreams PCG
learning assembly without C++ changes.

## Production graph assets

Folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/AmountPresets`

- `PCG_Cubeless_ED_GroundControlsCore`
  - Same GroundControls source/profile logic as `PCG_Cubeless_GroundControlsPrototype`
  - Omits the final Static Mesh Spawner so amount wrappers can filter or
    duplicate before spawning.
- `PCG_Cubeless_ED_GroundAmount_Sparse`
  - Uses `GroundControlsCore`
  - Applies `PCGDensityFilterSettings` with range `0.8..1.0`
  - Expected points/ISM: 3
  - `DesignerAmountId=201`, `DesignerAmountType=1`
- `PCG_Cubeless_ED_GroundAmount_Normal`
  - Uses `GroundControlsCore` without extra amount filtering
  - Expected points/ISM: 8
  - `DesignerAmountId=202`, `DesignerAmountType=2`
- `PCG_Cubeless_ED_GroundAmount_Dense`
  - Uses `GroundControlsCore`
  - Applies `PCGDuplicatePointSettings` with `iterations=1`
  - Duplicate offset: `(42, 42, 0)`
  - Expected points/ISM: 16
  - `DesignerAmountId=203`, `DesignerAmountType=3`

All amount graphs add:

- `DesignerProfileId=10`
- `DesignerProfileType=1`
- `DesignerAmountId`
- `DesignerAmountType`
- `DesignerAmountPass=True`

They preserve:

- `CubelessGroundControlsPass=True`
- `BoundsProfileId`
- point density values

## Production Blueprint assets

Folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Amount`

- `BP_Cubeless_ED_GroundAmount_Sparse`
  - Default graph: `PCG_Cubeless_ED_GroundAmount_Sparse`
- `BP_Cubeless_ED_GroundAmount_Normal`
  - Default graph: `PCG_Cubeless_ED_GroundAmount_Normal`
- `BP_Cubeless_ED_GroundAmount_Dense`
  - Default graph: `PCG_Cubeless_ED_GroundAmount_Dense`

All three Blueprints are placeable Spline + PCG actor shells derived from the
validated `BP_Cubeless_ED_PCGDesignerControlActor`.

## Validation summary

Graph validation:

- Sparse: 3 points, 3 ISM, profile `{10: 3}`, amount `{201: 3}`,
  density range mismatch 0, log Error 0.
- Normal: 8 points, 8 ISM, profile `{10: 8}`, amount `{202: 8}`,
  log Error 0.
- Dense: 16 points, 16 ISM, profile `{10: 16}`, amount `{203: 16}`,
  log Error 0.

Blueprint validation:

- `BP_Cubeless_ED_GroundAmount_Sparse`: Spline 1, PCG 1, template graph match,
  3 points, 3 ISM, log Error 0.
- `BP_Cubeless_ED_GroundAmount_Normal`: Spline 1, PCG 1, template graph match,
  8 points, 8 ISM, log Error 0.
- `BP_Cubeless_ED_GroundAmount_Dense`: Spline 1, PCG 1, template graph match,
  16 points, 16 ISM, log Error 0.

## Boundary

- No C++ was added or modified.
- Existing RuntimeGrass `NewPCGGraph` was not modified.
- Existing production preset graphs and Blueprint variants were not overwritten,
  except that new amount assets were added under separate folders.
- `_MCP_Temp` validation actors are disposable and should not be committed.
