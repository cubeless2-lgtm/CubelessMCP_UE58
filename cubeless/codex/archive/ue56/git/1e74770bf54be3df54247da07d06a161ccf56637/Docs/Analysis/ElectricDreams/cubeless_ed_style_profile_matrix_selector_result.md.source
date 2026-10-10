# Cubeless ED Style Profile Matrix Selector Result

Date: 2026-06-09

## Purpose

Add a visual style axis to the Cubeless Electric Dreams learning-authoring
surface without changing the existing amount/profile behavior. This pass keeps
placement logic stable and swaps only the mesh/style profile.

## Style axis

Visual style types:

- `1 = ClassicGrass`
  - Mesh:
    `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`
  - Style id: `401`
- `2 = TallGrass`
  - Mesh:
    `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium03.SM_Grass_Medium03`
  - Style id: `402`
- `3 = MixedGrass`
  - Weighted meshes:
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium03.SM_Grass_Medium03`
  - Style id: `403`
- `4 = GroundFoliage`
  - Weighted meshes:
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Plants/SM_Fern_01.SM_Fern_01`
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Plants/SM_GroundLeaf_01.SM_GroundLeaf_01`
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Flowers/SM_FlowerGroup_01_White.SM_FlowerGroup_01_White`
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Flowers/SM_FlowerGroup_01_Yellow.SM_FlowerGroup_01_Yellow`
  - Style id: `404`
- `5 = SmallRocks`
  - Weighted meshes:
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Stones/Rocks/SM_SmallRock_01.SM_SmallRock_01`
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Stones/Rocks/SM_SmallRock_02.SM_SmallRock_02`
  - Style id: `405`

Original Electric Dreams `/Game/PCG/...` candidate paths from the extracted
index did not load in the current Cubeless project, so this pass uses existing
Dreamscape static meshes that are present in the project.

Conifer/tree assets were sampled but deferred: their bounds are much larger
than the dense amount matrix used here, so they need a separate sparse/tree
profile axis instead of being injected into the current ground/ditch density
matrix. Material style families are also deferred.

## Graph assets

Style amount folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/StyleAmountPresets`

Created 30 style amount graphs:

- 5 visual styles x 3 ground amounts
- 5 visual styles x 3 ditch amounts

Each style amount graph preserves existing amount/profile markers and adds:

- `DesignerVisualStyleId`
- `DesignerVisualStyleType`
- `DesignerVisualStylePass=True`

Style profile matrix folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/DesignerStyleProfileMatrixCombos`

Created 75 style profile matrix graphs:

- 5 visual styles x 15 profile/amount combinations
- Profile modes: `GroundOnly`, `DitchOnly`, `Both`
- Amounts: `Sparse`, `Normal`, `Dense`

Each style profile matrix graph also adds:

- `DesignerProfileMode`
- `DesignerGroundAmountType`
- `DesignerDitchAmountType`
- `DesignerProfileMatrixId`
- `DesignerProfileMatrixType`
- `DesignerProfileMatrixPass=True`
- `DesignerStyleProfileMatrixId`
- `DesignerStyleProfileMatrixType`
- `DesignerStyleProfileMatrixPass=True`

## Selector Blueprint

- Blueprint:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGStyleProfileMatrixSelector`
- Class:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGStyleProfileMatrixSelector.BP_Cubeless_ED_PCGStyleProfileMatrixSelector_C`
- Components:
  - `Spline`
  - `PCG_StyleProfileMatrix`
- Exposed variables:
  - `VisualStyleType`: `1=ClassicGrass`, `2=TallGrass`, `3=MixedGrass`,
    `4=GroundFoliage`, `5=SmallRocks`
  - `ProfileMode`: `1=GroundOnly`, `2=DitchOnly`, `3=Both`
  - `GroundAmountType`: `1=Sparse`, `2=Normal`, `3=Dense`
  - `DitchAmountType`: `1=Sparse`, `2=Normal`, `3=Dense`

Inactive axes are normalized internally: ground-only uses ditch axis `0`, and
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
- `BP_Cubeless_ED_PCGStyleProfileMatrixSelector`

## Validation

Scripts:

- `build_cubeless_ed_style_profile_matrix_presets.py`
- `verify_cubeless_ed_style_profile_matrix_presets.py`
- `build_cubeless_ed_style_profile_matrix_selector_blueprint.py`
- `apply_cubeless_ed_style_profile_matrix_selector.py`
- `prepare_cubeless_ed_style_profile_matrix_selector_validation.py`
- `verify_cubeless_ed_style_profile_matrix_selector_blueprint.py`

Validation level:
`/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

Graph validation:

- 30 style amount graphs and 75 style profile matrix graphs were created.
- All 30 style amount graph spawners passed expected mesh-path validation.
- All 75 style profile matrix validation actors passed point count, ISM count,
  profile count, amount count, visual style marker, profile matrix marker,
  style profile matrix marker, and mesh path checks.
- Multi-entry style validation accepts generated ISM mesh paths as a subset of
  the weighted mesh entries, because sparse random sampling can legitimately
  emit only part of a style's mesh list in a small output set.
- Final graph validation reported
  `style_profile_matrix_presets_validation_pass=True` and `log_error_count=0`.

Selector validation:

- `BP_Cubeless_ED_PCGStyleProfileMatrixSelector` compiled with
  `compile_warning_count=0` and `compile_error_count=0`.
- 75 selector validation actors passed graph mapping, point count, ISM count,
  marker, and mesh path checks.
- Final selector validation reported
  `style_profile_matrix_selector_validation_pass=True` and `log_error_count=0`.

Menu smoke:

- Reloaded `CubelessEDPCG.py` and `RegisterMenu.py` in Unreal Python.
- Ran `apply_authoring_selectors_from_menu(show_dialog=False)`.
- The command applied all 75 style profile matrix selector validation actors and
  reported the expected output counts for all five styles:
  `3, 8, 16, 18, 42, 84, 21, 45, 87, 26, 50, 92, 34, 58, 100`.
- The smoke script reported
  `cubeless_ed_pcg_menu_style_family_smoke_pass=True`.

## Boundary

- No C++ changes were made.
- The original Electric Dreams PCG graphs were not modified.
- `RuntimeGrass` and `NewPCGGraph` were not overwritten.
- Existing amount/profile selector assets remain available.
- `_MCP_Temp` validation actors are disposable and should not be committed.

## Next work

The authoring surface now has independent axes for profile mode, ground amount,
ditch amount, and mesh/style family. Next useful work is either:

- add a sparse tree profile axis with bounds/scale validation, or
- add material style profiles after deciding material override semantics, or
- connect this learning selector into a production-facing Cubeless PCG actor
  once the desired user controls are settled.
