# Cubeless ED Ecosystem Material Preview Toggle Result

Date: 2026-06-09

## Summary

`BP_Cubeless_ED_PCGEcosystemSelector` now exposes `GenerateMaterialPreview` as a Boolean designer control.

- Default: `True`
- Category: `Cubeless PCG Ecosystem`
- Purpose: keep the separate `PCG_MaterialOverride` preview component available by default, but allow it to be disabled when the designer wants to inspect only the active style/tree ecosystem output.

## Runtime Behavior

`CubelessEDPCG.apply_ecosystem_selector` now reads `GenerateMaterialPreview`.

- When `True`, behavior is unchanged: the material override preview graph is generated alongside the active ecosystem mode.
- When `False`, `PCG_MaterialOverride` still receives the selected material graph path, but it is left cleaned/deactivated and is not generated.
- Deferred regeneration for the dynamic actor-property material graph only runs when the preview is enabled.

## Updated Validation

`verify_cubeless_ed_ecosystem_selector_blueprint.py` now includes a disabled-preview case:

`NoPreview_Combined_ClassicGrass_GroundOnly_GroundDense_CompactSolo_MaterialLeafWarm`

Observed result:

- `generate_material_preview=False`
- `style_points=16`, `expected_style_points=16`
- `tree_points=1`, `expected_tree_points=1`
- `material_points=0`, `expected_material_points=0`
- `material_ism=0`, `expected_material_ism=0`
- `material_preview_enabled_check=True`
- `validation_pass=True`

## Verification

- Python compile passed:
  - `Plugins/CustomTools/Content/Python/ArtScripts/CubelessEDPCG.py`
  - `build_cubeless_ed_ecosystem_selector_blueprint.py`
  - `verify_cubeless_ed_ecosystem_selector_blueprint.py`
- Unreal Blueprint compile validation passed:
  - `compiled=True`
  - `validation_pass=True`
  - `compile_error_count=0`
  - `compile_warning_count=0`
- Ecosystem selector validation passed:
  - prepared 10 validation actors
  - `ecosystem_selector_validation_pass=True`
  - `log_error_count=0`

## Notes

This change is intentionally non-destructive. The default enabled state preserves previous behavior for existing actors, while the new toggle gives designers a cleaner inspection path when material preview meshes overlap or add visual noise.
