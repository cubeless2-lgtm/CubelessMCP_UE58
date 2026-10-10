# Cubeless ED Tree Profile Selector Result

Date: 2026-06-09

## Purpose

Add a tree-specific sparse profile axis to the Cubeless Electric Dreams learning
authoring surface without injecting large tree meshes into the existing dense
ground/ditch style matrix.

## Design

New graph folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/TreeProfilePresets`

Tree style axis:

- `TreeStyleType=1 CompactConifer`
  - Mesh: `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Trees/SM_Conifer_05.SM_Conifer_05`
  - Scanned bounds sphere radius: about `699.33`
  - Style id: `501`
- `TreeStyleType=2 ColumnConifer`
  - Mesh: `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Trees/SM_Conifer_08.SM_Conifer_08`
  - Scanned bounds sphere radius: about `962.93`
  - Style id: `502`
- `TreeStyleType=3 MixedConifer`
  - Weighted meshes:
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Trees/SM_Conifer_05.SM_Conifer_05`
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Trees/SM_Conifer_08.SM_Conifer_08`
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Trees/SM_Conifer_09.SM_Conifer_09`
  - Max sampled bounds sphere radius: about `1009.29`
  - Style id: `503`

Tree amount axis:

- `TreeAmountType=1 Solo`: 1 point
- `TreeAmountType=2 Sparse`: 2 points, minimum spacing `1800`
- `TreeAmountType=3 LightGrove`: 3 points, minimum spacing about `2385.37`

This pass intentionally does not add a dense tree amount. The sampled conifer
assets are far larger than grass, ground foliage, and small rocks, so tree
placement stays in a separate sparse profile axis.

## Graph Assets

Created 9 tree profile graphs:

- 3 tree styles x 3 sparse amount profiles
- Graph naming:
  `PCG_Cubeless_ED_TreeProfile_<TreeStyle>_<TreeAmount>`

Each graph creates its own sparse point set, adds metadata, and then spawns the
tree mesh through weighted PCG StaticMeshSpawner entries.

Markers:

- `DesignerProfileId=30`
- `DesignerProfileType=3`
- `DesignerAmountId=601/602/603`
- `DesignerAmountType=1/2/3`
- `DesignerAmountPass=True`
- `DesignerTreeStyleId=501/502/503`
- `DesignerTreeStyleType=1/2/3`
- `DesignerTreeStylePass=True`
- `DesignerTreeProfileId`
- `DesignerTreeProfileType`
- `DesignerTreeProfilePass=True`
- `DesignerTreeMinSpacing`

## Selector Blueprint

- Blueprint:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGTreeProfileSelector`
- Class:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGTreeProfileSelector.BP_Cubeless_ED_PCGTreeProfileSelector_C`
- Components:
  - `Spline`
  - `PCG_TreeProfile`
- Exposed variables:
  - `TreeStyleType`: `1=CompactConifer`, `2=ColumnConifer`, `3=MixedConifer`
  - `TreeAmountType`: `1=Solo`, `2=Sparse`, `3=LightGrove`

## Editor Menu Integration

Updated project-local Python file:

- `Plugins/CustomTools/Content/Python/ArtScripts/CubelessEDPCG.py`

Existing editor path:
`Level Editor > Cubeless > Cubeless ED : Apply PCG Selector`

The menu command now supports:

- `BP_Cubeless_ED_PCGAuthoringSelector`
- `BP_Cubeless_ED_PCGMatrixSelector`
- `BP_Cubeless_ED_PCGProfileMatrixSelector`
- `BP_Cubeless_ED_PCGStyleProfileMatrixSelector`
- `BP_Cubeless_ED_PCGTreeProfileSelector`

## Validation

Scripts:

- `build_cubeless_ed_tree_profile_presets.py`
- `verify_cubeless_ed_tree_profile_presets.py`
- `build_cubeless_ed_tree_profile_selector_blueprint.py`
- `apply_cubeless_ed_tree_profile_selector.py`
- `prepare_cubeless_ed_tree_profile_selector_validation.py`
- `verify_cubeless_ed_tree_profile_selector_blueprint.py`

Validation level:
`/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

Graph validation:

- 9 tree profile graphs were created.
- All graph spawners passed expected mesh-path validation.
- All 9 graph validation actors passed point count, ISM count, profile marker,
  amount marker, tree style marker, tree profile marker, and minimum spacing
  checks.
- MixedConifer validation accepts generated ISM mesh paths as a subset of the
  weighted mesh entries, because sparse random sampling can legitimately emit
  only part of the style's mesh list in a small output set.
- Final graph validation reported
  `tree_profile_presets_validation_pass=True` and `log_error_count=0`.

Selector validation:

- `BP_Cubeless_ED_PCGTreeProfileSelector` compiled with
  `compile_warning_count=0` and `compile_error_count=0`.
- 9 selector validation actors passed graph mapping, point count, ISM count,
  marker, mesh path, and minimum spacing checks.
- Final selector validation reported
  `tree_profile_selector_validation_pass=True` and `log_error_count=0`.

Menu smoke:

- Reloaded `CubelessEDPCG.py` and `RegisterMenu.py` in Unreal Python.
- Selected the 9 tree profile selector validation actors and ran
  `apply_authoring_selectors_from_menu(show_dialog=False)`.
- The command applied all 9 tree selector actors and reported expected point
  counts `1, 2, 3` for each tree style.
- The smoke script reported
  `cubeless_ed_pcg_menu_tree_profile_smoke_pass=True`.

## Boundary

- No C++ changes were made.
- The original Electric Dreams PCG graphs were not modified.
- `RuntimeGrass` and `NewPCGGraph` were not overwritten.
- Existing amount/profile/style selector assets remain available.
- `_MCP_Temp` validation actors are disposable and should not be committed.

## Next Work

The learning authoring surface now has a separate sparse tree profile selector.
Next useful work is either:

- add material override semantics for existing style/tree profiles, or
- design a production-facing control actor that can choose between ground/ditch
  style matrix output and tree profile output without touching `RuntimeGrass`
  until the final integration policy is settled.
