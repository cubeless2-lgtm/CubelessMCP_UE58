# Cubeless ED Authoring Selector Result

Date: 2026-06-09

## Purpose

Add one authoring-facing selector actor for the already validated Cubeless
Electric Dreams combo presets. This lets a designer place one actor and choose
the generated density intent with `DesignerComboType` instead of manually
choosing separate Sparse, Normal, or Dense Blueprint classes.

## Production asset

- Blueprint:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGAuthoringSelector`
- Class:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGAuthoringSelector.BP_Cubeless_ED_PCGAuthoringSelector_C`

Components:

- `Spline`: authoring spline input.
- `PCG_Sparse`: uses `PCG_Cubeless_ED_DesignerCombo_Sparse`.
- `PCG_Normal`: uses `PCG_Cubeless_ED_DesignerCombo_Normal`.
- `PCG_Dense`: uses `PCG_Cubeless_ED_DesignerCombo_Dense`.

Exposed variable:

- `DesignerComboType`
  - `1`: Sparse
  - `2`: Normal
  - `3`: Dense
  - Default: `2`
  - Instance editable and expose-on-spawn.

Each PCG component uses `Generate On Demand`. The Blueprint intentionally does
not generate from Construction Script because `SetActive`/`Generate` there
compiled with unsafe-call warnings and did not reliably expose generated PCG
output during editor validation.

## Authoring workflow

Primary editor workflow:

- Use `Level Editor > Cubeless > Cubeless ED : Apply PCG Selector`.
- If selector actors are selected, only selected actors are applied.
- If no selector actors are selected, all selector actors in the open level are
  applied.
- The menu command is registered by
  `Plugins/CustomTools/Content/Python/ArtScripts/RegisterMenu.py`.
- The project-local apply implementation is
  `Plugins/CustomTools/Content/Python/ArtScripts/CubelessEDPCG.py`.

Sibling analysis workflow:

Use `apply_cubeless_ed_authoring_selector.py` after placing or editing selector
actors:

- If selector actors are selected, only selected actors are applied.
- If no selector actors are selected, all selector actors in the open level are
  applied.
- The script cleans up all three PCG components, deactivates unselected
  components, activates the selected component, and forces generation.

The workflow is therefore explicit editor scripting, not fully automatic
Blueprint-only authoring. This avoids non-exception C++ and avoids unsafe
Construction Script calls while still giving designers a menu action inside the
editor.

## Validation

Scripts:

- `build_cubeless_ed_authoring_selector_blueprint.py`
- `apply_cubeless_ed_authoring_selector.py`
- `prepare_cubeless_ed_authoring_selector_validation.py`
- `verify_cubeless_ed_authoring_selector_blueprint.py`

Project menu files:

- `Plugins/CustomTools/Content/Python/ArtScripts/CubelessEDPCG.py`
- `Plugins/CustomTools/Content/Python/ArtScripts/RegisterMenu.py`

Validation level:
`/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

Verification is intentionally two-step:

1. `prepare_cubeless_ed_authoring_selector_validation.py` creates fresh
   validation actors and applies the selector.
2. `verify_cubeless_ed_authoring_selector_blueprint.py` reads the existing
   validation actors after the Unreal editor tick has allowed PCG generation to
   finish.

Pass summary:

- Sparse selector:
  - `DesignerComboType=1`
  - selected component `PCG_Sparse`
  - component point counts `{PCG_Sparse: 21, PCG_Normal: 0, PCG_Dense: 0}`
  - profile counts `{10: 3, 20: 18}`
  - amount counts `{201: 3, 301: 18}`
  - combo marker `401/1`
  - total ISM instances `21`
- Normal selector:
  - `DesignerComboType=2`
  - selected component `PCG_Normal`
  - component point counts `{PCG_Sparse: 0, PCG_Normal: 50, PCG_Dense: 0}`
  - profile counts `{10: 8, 20: 42}`
  - amount counts `{202: 8, 302: 42}`
  - combo marker `402/2`
  - total ISM instances `50`
- Dense selector:
  - `DesignerComboType=3`
  - selected component `PCG_Dense`
  - component point counts `{PCG_Sparse: 0, PCG_Normal: 0, PCG_Dense: 100}`
  - profile counts `{10: 16, 20: 84}`
  - amount counts `{203: 16, 303: 84}`
  - combo marker `403/3`
  - total ISM instances `100`

Latest verification reported:

- `validation_pass=True` for Sparse, Normal, and Dense.
- `authoring_selector_validation_pass=True`
- `log_error_count=0`
- Project menu import and no-dialog apply smoke passed through UnrealMCP.

## Boundary

- No C++ changes were made.
- The original Electric Dreams PCG graphs were not modified.
- `RuntimeGrass` and `NewPCGGraph` were not overwritten.
- `_MCP_Temp` validation actors are disposable and should not be committed.

## Next work

The next useful step is to expand beyond the three locked combos into independent
authoring axes, for example separate ground amount, ditch amount, and style
selectors that can be combined without creating one Blueprint class per preset.
