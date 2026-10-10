# Cubeless ED Profile Matrix Selector Result

Date: 2026-06-09

## Purpose

Add a profile-mode axis on top of the existing independent Ground/Ditch amount
matrix. Designers can now choose whether the generated PCG output is ground
only, ditch/riverbank only, or both, while still choosing Sparse/Normal/Dense
amounts for the active profiles.

## Profile matrix graph assets

Folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/DesignerProfileMatrixCombos`

Profile modes:

- `1 = GroundOnly`
- `2 = DitchOnly`
- `3 = Both`

Every output point receives these markers:

- `DesignerProfileMode`
- `DesignerGroundAmountType`
- `DesignerDitchAmountType`
- `DesignerProfileMatrixId`
- `DesignerProfileMatrixType`
- `DesignerProfileMatrixPass=True`

Identifier formula:

- `DesignerProfileMatrixType = ProfileMode * 100 + GroundAmountType * 10 + DitchAmountType`
- `DesignerProfileMatrixId = 1000 + DesignerProfileMatrixType`
- Ground-only graphs use `DitchAmountType=0`
- Ditch-only graphs use `GroundAmountType=0`

Matrix:

- `GroundOnly_GroundSparse`: 3 points / 3 ISM, profile counts `{10: 3}`, amount counts `{201: 3}`
- `GroundOnly_GroundNormal`: 8 points / 8 ISM, profile counts `{10: 8}`, amount counts `{202: 8}`
- `GroundOnly_GroundDense`: 16 points / 16 ISM, profile counts `{10: 16}`, amount counts `{203: 16}`
- `DitchOnly_DitchSparse`: 18 points / 18 ISM, profile counts `{20: 18}`, amount counts `{301: 18}`
- `DitchOnly_DitchNormal`: 42 points / 42 ISM, profile counts `{20: 42}`, amount counts `{302: 42}`
- `DitchOnly_DitchDense`: 84 points / 84 ISM, profile counts `{20: 84}`, amount counts `{303: 84}`
- `Both_GroundSparse_DitchSparse`: 21 points / 21 ISM, amount counts `{201: 3, 301: 18}`
- `Both_GroundSparse_DitchNormal`: 45 points / 45 ISM, amount counts `{201: 3, 302: 42}`
- `Both_GroundSparse_DitchDense`: 87 points / 87 ISM, amount counts `{201: 3, 303: 84}`
- `Both_GroundNormal_DitchSparse`: 26 points / 26 ISM, amount counts `{202: 8, 301: 18}`
- `Both_GroundNormal_DitchNormal`: 50 points / 50 ISM, amount counts `{202: 8, 302: 42}`
- `Both_GroundNormal_DitchDense`: 92 points / 92 ISM, amount counts `{202: 8, 303: 84}`
- `Both_GroundDense_DitchSparse`: 34 points / 34 ISM, amount counts `{203: 16, 301: 18}`
- `Both_GroundDense_DitchNormal`: 58 points / 58 ISM, amount counts `{203: 16, 302: 42}`
- `Both_GroundDense_DitchDense`: 100 points / 100 ISM, amount counts `{203: 16, 303: 84}`

## Selector Blueprint

- Blueprint:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGProfileMatrixSelector`
- Class:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGProfileMatrixSelector.BP_Cubeless_ED_PCGProfileMatrixSelector_C`
- Components:
  - `Spline`
  - `PCG_ProfileMatrix`
- Exposed variables:
  - `ProfileMode`: `1=GroundOnly`, `2=DitchOnly`, `3=Both`
  - `GroundAmountType`: `1=Sparse`, `2=Normal`, `3=Dense`
  - `DitchAmountType`: `1=Sparse`, `2=Normal`, `3=Dense`

The selector keeps user-facing values in the `1..3` range. The apply path
normalizes inactive axes internally: ground-only uses ditch axis `0`, and
ditch-only uses ground axis `0`.

## Editor menu integration

Updated project-local Python file:

- `Plugins/CustomTools/Content/Python/ArtScripts/CubelessEDPCG.py`

Existing editor path:
`Level Editor > Cubeless > Cubeless ED : Apply PCG Selector`

The menu command now supports:

- `BP_Cubeless_ED_PCGAuthoringSelector`
- `BP_Cubeless_ED_PCGMatrixSelector`
- `BP_Cubeless_ED_PCGProfileMatrixSelector`

If selector actors are selected, only selected actors are applied. If none are
selected, all supported selector actors in the current level are applied.

## Validation

Scripts:

- `build_cubeless_ed_profile_matrix_presets.py`
- `verify_cubeless_ed_profile_matrix_presets.py`
- `build_cubeless_ed_profile_matrix_selector_blueprint.py`
- `apply_cubeless_ed_profile_matrix_selector.py`
- `prepare_cubeless_ed_profile_matrix_selector_validation.py`
- `verify_cubeless_ed_profile_matrix_selector_blueprint.py`

Validation level:
`/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

Graph validation:

- All 15 profile matrix graphs passed point count, ISM count, profile count,
  amount count, profile-mode marker, ground/ditch axis marker, profile matrix
  id/type marker, and pass marker checks.
- Final graph validation reported `profile_matrix_presets_validation_pass=True`
  and `log_error_count=0`.

Selector validation:

- `BP_Cubeless_ED_PCGProfileMatrixSelector` compiled with
  `compile_warning_count=0` and `compile_error_count=0`.
- 15 selector validation actors passed expected graph, point count, ISM count,
  profile/amount marker, profile-mode marker, axis marker, and profile matrix
  marker checks.
- Final selector validation reported
  `profile_matrix_selector_validation_pass=True` and `log_error_count=0`.

Menu smoke:

- Reloaded `CubelessEDPCG.py` and `RegisterMenu.py` in Unreal Python.
- Ran `apply_authoring_selectors_from_menu(show_dialog=False)`.
- The command applied all 15 profile matrix validation actors and reported
  output counts `3, 8, 16, 18, 42, 84, 21, 45, 87, 26, 50, 92, 34, 58, 100`.
- The smoke script reported `cubeless_ed_pcg_menu_profile_matrix_smoke_pass=True`.

## Boundary

- No C++ changes were made.
- The original Electric Dreams PCG graphs were not modified.
- `RuntimeGrass` and `NewPCGGraph` were not overwritten.
- `_MCP_Temp` validation actors are disposable and should not be committed.

## Next work

The amount/profile control layer is now usable as a learning-authoring surface.
The next useful axis is visual style or mesh/material profile selection. That
should stay separate from amount/profile mode so the selector does not turn into
one hardcoded preset list too early.
