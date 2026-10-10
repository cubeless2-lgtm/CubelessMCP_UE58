# Cubeless ED Designer Combo Presets Result

Date: 2026-06-09

## Purpose

Connect the separately validated profile and amount axes into placeable designer
combo presets. This creates the first end-user selection layer for choosing a
complete Cubeless PCG output without changing the original Electric Dreams
graphs.

## Production graph assets

Folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/DesignerCombos`

- `PCG_Cubeless_ED_DesignerCombo_Sparse`
  - Uses `PCG_Cubeless_ED_GroundAmount_Sparse`
  - Uses `PCG_Cubeless_ED_DitchAmount_Sparse`
  - Expected output: 21 points / 21 ISM instances
  - Profile counts: GroundControls 3, DitchHierarchy 18
  - Amount counts: Ground 201 = 3, Ditch 301 = 18
  - Combo marker: `DesignerComboId=401`, `DesignerComboType=1`
- `PCG_Cubeless_ED_DesignerCombo_Normal`
  - Uses `PCG_Cubeless_ED_GroundAmount_Normal`
  - Uses `PCG_Cubeless_ED_DitchAmount_Normal`
  - Expected output: 50 points / 50 ISM instances
  - Profile counts: GroundControls 8, DitchHierarchy 42
  - Amount counts: Ground 202 = 8, Ditch 302 = 42
  - Combo marker: `DesignerComboId=402`, `DesignerComboType=2`
- `PCG_Cubeless_ED_DesignerCombo_Dense`
  - Uses `PCG_Cubeless_ED_GroundAmount_Dense`
  - Uses `PCG_Cubeless_ED_DitchAmount_Dense`
  - Expected output: 100 points / 100 ISM instances
  - Profile counts: GroundControls 16, DitchHierarchy 84
  - Amount counts: Ground 203 = 16, Ditch 303 = 84
  - Combo marker: `DesignerComboId=403`, `DesignerComboType=3`

Each combo graph preserves the upstream `DesignerProfileId`,
`DesignerProfileType`, `DesignerAmountId`, `DesignerAmountType`, and
`DesignerAmountPass` metadata, then adds `DesignerComboId`,
`DesignerComboType`, and `DesignerComboPass=True`.

## Placeable Blueprint assets

Folder:
`/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/DesignerCombos`

- `BP_Cubeless_ED_DesignerCombo_Sparse`
- `BP_Cubeless_ED_DesignerCombo_Normal`
- `BP_Cubeless_ED_DesignerCombo_Dense`

Each Blueprint is duplicated from `BP_Cubeless_ED_PCGDesignerControlActor` and
only changes the PCG component template graph to its matching combo graph.

## Validation

Scripts:

- `build_cubeless_ed_designer_combo_presets.py`
- `verify_cubeless_ed_designer_combo_presets.py`
- `build_cubeless_ed_designer_combo_blueprints.py`
- `verify_cubeless_ed_designer_combo_blueprints.py`

Graph validation:

- Sparse: 21 points, 21 ISM instances, profile counts `{10: 3, 20: 18}`,
  amount counts `{201: 3, 301: 18}`, combo marker `401/1`, pass.
- Normal: 50 points, 50 ISM instances, profile counts `{10: 8, 20: 42}`,
  amount counts `{202: 8, 302: 42}`, combo marker `402/2`, pass.
- Dense: 100 points, 100 ISM instances, profile counts `{10: 16, 20: 84}`,
  amount counts `{203: 16, 303: 84}`, combo marker `403/3`, pass.

Blueprint validation:

- Sparse Blueprint: template graph points to `PCG_Cubeless_ED_DesignerCombo_Sparse`;
  21 points and 21 ISM instances; pass.
- Normal Blueprint: template graph points to `PCG_Cubeless_ED_DesignerCombo_Normal`;
  50 points and 50 ISM instances; pass.
- Dense Blueprint: template graph points to `PCG_Cubeless_ED_DesignerCombo_Dense`;
  100 points and 100 ISM instances; pass.

Latest build-window log scan reported `log_error_count=0` for both combo graph
and combo Blueprint validation.

## Boundary

- No C++ changes were made.
- The original Electric Dreams PCG graphs were not modified.
- `RuntimeGrass` and `NewPCGGraph` were not overwritten.
- `_MCP_Temp` validation actors are disposable and should not be committed.

## Next work

The next useful step is to add an authoring-facing selector layer so designers
can select profile and amount intent from one actor without choosing separate
Blueprint classes manually.
