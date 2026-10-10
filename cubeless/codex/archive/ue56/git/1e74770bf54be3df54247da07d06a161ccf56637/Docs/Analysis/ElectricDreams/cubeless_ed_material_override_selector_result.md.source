# Cubeless ED PCG Material Override Selector Result

Date: 2026-06-09

## Scope

This pass adds a learning-only material override axis for Electric Dreams PCG study. It does not modify the original Electric Dreams graphs, `RuntimeGrass`, or `NewPCGGraph`.

Created assets live under:

- `/Game/Cubeless/PCG/ElectricDreamsLearning/Materials/MaterialOverrides`
- `/Game/Cubeless/PCG/ElectricDreamsLearning/MaterialOverridePresets`
- `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGMaterialOverrideSelector`

Validation actors live in `/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`.

## Material Override Semantics

The implementation uses `PCGStaticMeshSpawnerSettings` with `PCGSoftISMComponentDescriptor.override_materials`. This keeps material overrides on the spawner descriptor instead of changing source mesh assets or source materials.

Three domains were validated:

- `MaterialDomainType=1`: `GroundFoliage`
- `MaterialDomainType=2`: `SmallRocks`
- `MaterialDomainType=3`: `CompactConifer`

Three variants were validated per domain:

- `MaterialVariantType=1`: default source material
- `MaterialVariantType=2`: cool/dark variant
- `MaterialVariantType=3`: warm/soft variant

## Created Material Variants

Created material instances:

- `MI_Cubeless_ED_Fern_Cool`
- `MI_Cubeless_ED_Fern_Warm`
- `MI_Cubeless_ED_GroundLeaves_Cool`
- `MI_Cubeless_ED_GroundLeaves_Warm`
- `MI_Cubeless_ED_Rock_Cool`
- `MI_Cubeless_ED_Rock_Dark`
- `MI_Cubeless_ED_PineLeaves_Dark`
- `MI_Cubeless_ED_PineBark_Dark`
- `MI_Cubeless_ED_PineLeaves_Soft`
- `MI_Cubeless_ED_PineBark_Soft`

All variants keep Dreamscape material assets as parents.

## Created PCG Preset Graphs

Created 9 graphs in `/Game/Cubeless/PCG/ElectricDreamsLearning/MaterialOverridePresets`:

- `PCG_Cubeless_ED_MaterialOverride_GroundFoliage_Default`
- `PCG_Cubeless_ED_MaterialOverride_GroundFoliage_CoolLeaf`
- `PCG_Cubeless_ED_MaterialOverride_GroundFoliage_WarmLeaf`
- `PCG_Cubeless_ED_MaterialOverride_SmallRocks_Default`
- `PCG_Cubeless_ED_MaterialOverride_SmallRocks_CoolRock`
- `PCG_Cubeless_ED_MaterialOverride_SmallRocks_DarkRock`
- `PCG_Cubeless_ED_MaterialOverride_CompactConifer_Default`
- `PCG_Cubeless_ED_MaterialOverride_CompactConifer_DarkPine`
- `PCG_Cubeless_ED_MaterialOverride_CompactConifer_SoftPine`

Each graph writes metadata markers:

- `DesignerMaterialDomainId`
- `DesignerMaterialDomainType`
- `DesignerMaterialVariantId`
- `DesignerMaterialVariantType`
- `DesignerMaterialOverrideId`
- `DesignerMaterialOverrideType`
- `DesignerMaterialOverrideMode`
- `DesignerMaterialOverridePass`

## Selector

Created Blueprint:

- `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGMaterialOverrideSelector`

Exposed variables:

- `MaterialDomainType`: 1-3
- `MaterialVariantType`: 1-3

The existing `Cubeless ED : Apply PCG Selector` menu entry now recognizes this selector and assigns the matching material override graph.

## Verification

Passed checks:

- `material_override_presets_validation_pass=True`
- `material_override_selector_validation_pass=True`
- `cubeless_ed_pcg_menu_material_override_smoke_pass=True`
- Blueprint compile: `compile_error_count=0`, `compile_warning_count=0`
- Final real log scan after the menu smoke: `real_log_error_count=0`

The verifier confirmed:

- 10 material variants exist and keep expected parent material paths.
- 9 preset graphs have the expected `descriptor.override_materials` per mesh.
- Generated ISM components use the expected override material slots for non-default variants.
- Selector actors switch to the expected graph for all 9 domain/variant pairs.

## Notes

The first broad post-menu log scan caught a false positive because the failed scan command itself wrote a code string containing `Error:` into the log. A second scan using a fresh marker and real Unreal log-line filtering found no actual editor error lines.

## Next Step

The material axis is now isolated and validated. The next useful stage is to integrate this material axis into the ecosystem selector so one actor can drive style, amount, tree, and material together.
