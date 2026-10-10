# Cubeless ED Ecosystem Selector Result

Date: 2026-06-09

## Purpose

Add a production-facing learning control actor that can drive the existing
style profile matrix output and the new sparse tree profile output from one
placeable actor. This keeps the authoring surface closer to the final user
workflow while avoiding changes to `RuntimeGrass` and `NewPCGGraph`.

## Selector Blueprint

- Blueprint:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGEcosystemSelector`
- Class:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGEcosystemSelector.BP_Cubeless_ED_PCGEcosystemSelector_C`
- Components:
  - `Spline`
  - `PCG_StyleProfileMatrix`
  - `PCG_TreeProfile`

The actor does not create new PCG logic. It assigns already-validated graph
assets to the two PCG components and generates them on demand through the
project menu apply path.

## Exposed Variables

- `EcosystemMode`
  - `1 = StyleOnly`
  - `2 = TreeOnly`
  - `3 = Combined`
- `VisualStyleType`
  - `1 = ClassicGrass`
  - `2 = TallGrass`
  - `3 = MixedGrass`
  - `4 = GroundFoliage`
  - `5 = SmallRocks`
- `ProfileMode`
  - `1 = GroundOnly`
  - `2 = DitchOnly`
  - `3 = Both`
- `GroundAmountType`
  - `1 = Sparse`
  - `2 = Normal`
  - `3 = Dense`
- `DitchAmountType`
  - `1 = Sparse`
  - `2 = Normal`
  - `3 = Dense`
- `TreeStyleType`
  - `1 = CompactConifer`
  - `2 = ColumnConifer`
  - `3 = MixedConifer`
- `TreeAmountType`
  - `1 = Solo`
  - `2 = Sparse`
  - `3 = LightGrove`

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
- `BP_Cubeless_ED_PCGEcosystemSelector`

For ecosystem selectors, the menu apply path:

- normalizes style/profile/amount axes using the existing style profile matrix
  graph naming rules,
- normalizes tree style/amount axes using the tree profile graph naming rules,
- sets `PCG_StyleProfileMatrix` to the selected style graph,
- sets `PCG_TreeProfile` to the selected tree graph,
- generates style only, tree only, or both depending on `EcosystemMode`.

## Validation

Scripts:

- `build_cubeless_ed_ecosystem_selector_blueprint.py`
- `prepare_cubeless_ed_ecosystem_selector_validation.py`
- `verify_cubeless_ed_ecosystem_selector_blueprint.py`

Validation level:
`/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

Validation cases:

- `Combined_GroundFoliage_Both_GroundNormal_DitchSparse_ColumnSparse`
  - Style output: 26 points / 26 ISM
  - Tree output: 2 points / 2 ISM
- `Combined_ClassicGrass_GroundOnly_GroundDense_CompactSolo`
  - Style output: 16 points / 16 ISM
  - Tree output: 1 point / 1 ISM
- `StyleOnly_SmallRocks_DitchOnly_DitchNormal`
  - Style output: 42 points / 42 ISM
  - Tree output: 0 points / 0 ISM
- `TreeOnly_MixedConifer_LightGrove`
  - Style output: 0 points / 0 ISM
  - Tree output: 3 points / 3 ISM

Results:

- `BP_Cubeless_ED_PCGEcosystemSelector` compiled with
  `compile_warning_count=0` and `compile_error_count=0`.
- All validation actors passed style graph mapping, tree graph mapping, point
  count, and ISM count checks.
- Final selector validation reported
  `ecosystem_selector_validation_pass=True` and `log_error_count=0`.

Menu smoke:

- Reloaded `CubelessEDPCG.py` and `RegisterMenu.py` in Unreal Python.
- Selected the 4 ecosystem selector validation actors and ran
  `apply_authoring_selectors_from_menu(show_dialog=False)`.
- The command applied all 4 ecosystem selector actors with expected component
  point counts.
- The smoke script reported
  `cubeless_ed_pcg_menu_ecosystem_smoke_pass=True`.

## Boundary

- No C++ changes were made.
- The original Electric Dreams PCG graphs were not modified.
- `RuntimeGrass` and `NewPCGGraph` were not overwritten.
- Existing selector assets remain available.
- `_MCP_Temp` validation actors are disposable and should not be committed.

## Next Work

The learning authoring surface now has a single actor that can drive ground,
ditch, style, and sparse tree outputs. Next useful work is either:

- material override semantics for the existing style/tree outputs, or
- a controlled production integration pass that maps this selector's choices
  into the real Cubeless runtime PCG entry point after the user approves the
  exact production target.
