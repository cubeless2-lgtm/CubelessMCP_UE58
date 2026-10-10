# Cubeless ED Designer Presets and Blueprints Result

Date: 2026-06-09

## Purpose

Add a first practical control surface for the Electric Dreams PCG learning
assembly without C++ changes. Designers can now choose a production preset graph
or place a preset-specific Blueprint actor.

## Production preset graphs

Folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/Presets`

- `PCG_Cubeless_ED_Preset_GroundOnly`
  - Uses `PCG_Cubeless_GroundControlsPrototype`
  - Expected points: 8
  - `DesignerPresetId=101`
  - `DesignerPresetType=1`
- `PCG_Cubeless_ED_Preset_DitchOnly`
  - Uses `PCG_Cubeless_DitchHierarchyPrototype`
  - Expected points: 42
  - `DesignerPresetId=102`
  - `DesignerPresetType=2`
- `PCG_Cubeless_ED_Preset_Balanced`
  - Wraps `PCG_Cubeless_ED_DesignerControlLayer`
  - Expected points: 50
  - `DesignerPresetId=103`
  - `DesignerPresetType=3`

All preset graphs add:

- `DesignerPresetId`
- `DesignerPresetType`
- `DesignerPresetPass=True`

They preserve the existing profile markers:

- `DesignerProfileId`
- `DesignerProfileType`
- `DesignerControlLayerPass=True`

## Production Blueprint actors

Folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints`

- `BP_Cubeless_ED_PCGDesignerControlActor`
  - Generic designer actor shell.
  - Default PCG graph updated to `PCG_Cubeless_ED_Preset_Balanced`.
- `BP_Cubeless_ED_PCGDesigner_GroundOnly`
  - Default PCG graph: `PCG_Cubeless_ED_Preset_GroundOnly`.
- `BP_Cubeless_ED_PCGDesigner_DitchOnly`
  - Default PCG graph: `PCG_Cubeless_ED_Preset_DitchOnly`.
- `BP_Cubeless_ED_PCGDesigner_Balanced`
  - Default PCG graph: `PCG_Cubeless_ED_Preset_Balanced`.

All Blueprint actors keep the validated Spline + PCG component shell and are
placeable.

## Validation summary

Preset graph validation:

- GroundOnly: 8 points, `GroundControls=8`, ISM 8, preset marker pass 8.
- DitchOnly: 42 points, `DitchHierarchy=42`, ISM 42, preset marker pass 42.
- Balanced: 50 points, `GroundControls=8`, `DitchHierarchy=42`, ISM 50,
  preset marker pass 50.
- For all three: profile type mismatch 0, preset id/type mismatch 0,
  latest marker log Error 0.

Preset Blueprint validation:

- `BP_Cubeless_ED_PCGDesigner_GroundOnly`: Spline 1, PCG 1, point 8,
  template graph matched GroundOnly, log Error 0.
- `BP_Cubeless_ED_PCGDesigner_DitchOnly`: Spline 1, PCG 1, point 42,
  template graph matched DitchOnly, log Error 0.
- `BP_Cubeless_ED_PCGDesigner_Balanced`: Spline 1, PCG 1, point 50,
  template graph matched Balanced, log Error 0.

## Dynamic selector note

`PCGGetActorPropertySettings` is available in Python, but the exposed
`PCGSwitchSettings` surface only showed static `integer_selection` during this
pass. A true per-instance Actor property selector is therefore not promoted yet.
The current practical control path is preset graph selection or preset-specific
Blueprint placement.

## Boundary

- No C++ was added or modified.
- Existing RuntimeGrass `NewPCGGraph` was not modified.
- `_MCP_Temp` validation actors are disposable and should not be committed.
