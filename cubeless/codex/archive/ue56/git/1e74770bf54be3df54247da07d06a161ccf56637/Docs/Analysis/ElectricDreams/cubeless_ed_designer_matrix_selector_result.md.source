# Cubeless ED Designer Matrix Selector Result

Date: 2026-06-09

## Purpose

Expand the Cubeless Electric Dreams authoring layer from three locked presets
into independent Ground and Ditch amount axes. Designers can now choose ground
density and ditch/riverbank density separately with one selector actor.

## Matrix graph assets

Folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/DesignerMatrixCombos`

Each graph merges one ground amount subgraph and one ditch amount subgraph, then
adds these markers to every output point:

- `DesignerComboId`
- `DesignerComboType`
- `DesignerGroundAmountType`
- `DesignerDitchAmountType`
- `DesignerComboPass=True`
- `DesignerMatrixPass=True`

Matrix:

- `GroundSparse_DitchSparse`
  - Graph: `PCG_Cubeless_ED_Matrix_GroundSparse_DitchSparse`
  - Markers: `DesignerComboId=511`, `DesignerComboType=11`,
    `DesignerGroundAmountType=1`, `DesignerDitchAmountType=1`
  - Expected output: 21 points / 21 ISM instances
  - Amount counts: `{201: 3, 301: 18}`
- `GroundSparse_DitchNormal`
  - Graph: `PCG_Cubeless_ED_Matrix_GroundSparse_DitchNormal`
  - Markers: `512/12`, axes `1/2`
  - Expected output: 45 points / 45 ISM instances
  - Amount counts: `{201: 3, 302: 42}`
- `GroundSparse_DitchDense`
  - Graph: `PCG_Cubeless_ED_Matrix_GroundSparse_DitchDense`
  - Markers: `513/13`, axes `1/3`
  - Expected output: 87 points / 87 ISM instances
  - Amount counts: `{201: 3, 303: 84}`
- `GroundNormal_DitchSparse`
  - Graph: `PCG_Cubeless_ED_Matrix_GroundNormal_DitchSparse`
  - Markers: `521/21`, axes `2/1`
  - Expected output: 26 points / 26 ISM instances
  - Amount counts: `{202: 8, 301: 18}`
- `GroundNormal_DitchNormal`
  - Graph: `PCG_Cubeless_ED_Matrix_GroundNormal_DitchNormal`
  - Markers: `522/22`, axes `2/2`
  - Expected output: 50 points / 50 ISM instances
  - Amount counts: `{202: 8, 302: 42}`
- `GroundNormal_DitchDense`
  - Graph: `PCG_Cubeless_ED_Matrix_GroundNormal_DitchDense`
  - Markers: `523/23`, axes `2/3`
  - Expected output: 92 points / 92 ISM instances
  - Amount counts: `{202: 8, 303: 84}`
- `GroundDense_DitchSparse`
  - Graph: `PCG_Cubeless_ED_Matrix_GroundDense_DitchSparse`
  - Markers: `531/31`, axes `3/1`
  - Expected output: 34 points / 34 ISM instances
  - Amount counts: `{203: 16, 301: 18}`
- `GroundDense_DitchNormal`
  - Graph: `PCG_Cubeless_ED_Matrix_GroundDense_DitchNormal`
  - Markers: `532/32`, axes `3/2`
  - Expected output: 58 points / 58 ISM instances
  - Amount counts: `{203: 16, 302: 42}`
- `GroundDense_DitchDense`
  - Graph: `PCG_Cubeless_ED_Matrix_GroundDense_DitchDense`
  - Markers: `533/33`, axes `3/3`
  - Expected output: 100 points / 100 ISM instances
  - Amount counts: `{203: 16, 303: 84}`

## Selector Blueprint

- Blueprint:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGMatrixSelector`
- Class:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGMatrixSelector.BP_Cubeless_ED_PCGMatrixSelector_C`
- Components:
  - `Spline`
  - `PCG_Matrix`
- Exposed variables:
  - `GroundAmountType`: `1=Sparse`, `2=Normal`, `3=Dense`
  - `DitchAmountType`: `1=Sparse`, `2=Normal`, `3=Dense`

The selector uses a single PCG component. The apply script resolves the two
integer axes into one matrix graph, sets the PCG component graph, cleans up
previous output, and forces generation.

## Editor menu integration

Updated project-local Python files:

- `Plugins/CustomTools/Content/Python/ArtScripts/CubelessEDPCG.py`
- `Plugins/CustomTools/Content/Python/ArtScripts/RegisterMenu.py`

Editor path:
`Level Editor > Cubeless > Cubeless ED : Apply PCG Selector`

The menu command now supports both:

- `BP_Cubeless_ED_PCGAuthoringSelector`
- `BP_Cubeless_ED_PCGMatrixSelector`

If selector actors are selected, only selected actors are applied. If none are
selected, all supported selector actors in the current level are applied.

## Validation

Scripts:

- `build_cubeless_ed_designer_matrix_presets.py`
- `verify_cubeless_ed_designer_matrix_presets.py`
- `build_cubeless_ed_matrix_selector_blueprint.py`
- `apply_cubeless_ed_matrix_selector.py`
- `prepare_cubeless_ed_matrix_selector_validation.py`
- `verify_cubeless_ed_matrix_selector_blueprint.py`

Validation level:
`/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

Graph validation:

- All 9 matrix graphs passed point count, ISM count, profile count, amount
  count, profile/amount pair count, combo marker, ground axis marker, ditch axis
  marker, and pass marker checks.
- Final graph validation reported `designer_matrix_presets_validation_pass=True`
  and `log_error_count=0`.

Selector validation:

- `BP_Cubeless_ED_PCGMatrixSelector` compiled with
  `compile_warning_count=0` and `compile_error_count=0`.
- 9 selector validation actors passed expected graph, point count, ISM count,
  amount marker, combo marker, and axis marker checks.
- Final selector validation reported `matrix_selector_validation_pass=True` and
  `log_error_count=0`.

Menu smoke:

- Reloaded `CubelessEDPCG.py` in Unreal Python and ran
  `apply_authoring_selectors_from_menu(show_dialog=False)`.
- The command applied all 9 matrix validation actors and reported output counts
  `21, 45, 87, 26, 50, 92, 34, 58, 100`.
- A follow-up selector validation still passed.

## Boundary

- No C++ changes were made.
- The original Electric Dreams PCG graphs were not modified.
- `RuntimeGrass` and `NewPCGGraph` were not overwritten.
- `_MCP_Temp` validation actors are disposable and should not be committed.

## Next work

The next useful axis is style selection. Current amount axes are independent;
style is still inherited from the existing GroundControls and DitchHierarchy
subgraphs. A future selector should expose style/profile intent separately from
amount intent.
