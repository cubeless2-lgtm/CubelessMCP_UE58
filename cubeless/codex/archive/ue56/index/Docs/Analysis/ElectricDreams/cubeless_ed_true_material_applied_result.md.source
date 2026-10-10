# Cubeless ED True Material Applied Presets

Date: 2026-06-09

## Scope

This pass converts the material override axis from preview-only output into real spawned-material variants for a learning-safe subset of the Cubeless Electric Dreams PCG study.

Created true material-applied graph families under:

- `/Game/Cubeless/PCG/ElectricDreamsLearning/TrueMaterialApplied/StyleAmountPresets`
- `/Game/Cubeless/PCG/ElectricDreamsLearning/TrueMaterialApplied/DesignerStyleProfileMatrixCombos`
- `/Game/Cubeless/PCG/ElectricDreamsLearning/TrueMaterialApplied/TreeProfilePresets`

The original Electric Dreams graphs, source meshes, and source materials were not modified.

## Implemented Material Routes

True material-applied routes now exist for:

- `GroundFoliage`
  - `CoolLeaf`
  - `WarmLeaf`
- `SmallRocks`
  - `CoolRock`
  - `DarkRock`
- `CompactConifer`
  - `DarkPine`
  - `SoftPine`
- `ColumnConifer`
  - `DarkPine`
  - `SoftPine`
- `MixedConifer`
  - `DarkPine`
  - `SoftPine`

Tree slot policy:

- `SM_Conifer_05` overrides slot 0 with pine leaves and slot 1 with pine bark. Billboard slot 2 stays on the source material.
- `SM_Conifer_08` and `SM_Conifer_09` override slot 0 with pine bark only. Billboard slot 1 stays on the source material.

## Generated Assets

The build script generated:

- 24 true material style amount graphs
- 60 true material style profile matrix graphs
- 18 true material tree profile graphs
- 8 direct true-material validation actors

The style profile matrix graph count is `2 styles x 15 profile/amount combinations x 2 variants = 60`.
The true material tree profile graph count is `3 tree styles x 3 amount presets x 2 variants = 18`.

## Runtime Routing

`CubelessEDPCG.apply_ecosystem_selector` now chooses true material-applied graphs when the material domain matches the active style/tree domain and the material variant is a non-default variant.

Routing rules:

- `VisualStyleType=4 GroundFoliage` + `MaterialDomainType=1` + variant `2/3` routes the style component to the true material style matrix graph.
- `VisualStyleType=5 SmallRocks` + `MaterialDomainType=2` + variant `2/3` routes the style component to the true material style matrix graph.
- `TreeStyleType=1/2/3 CompactConifer/ColumnConifer/MixedConifer` + `MaterialDomainType=3` + variant `2/3` routes the tree component to the true material tree graph.
- Non-matching domains and default variants stay on the existing non-true-material style/tree graphs.
- The material preview component remains active so designers can still see the selected material domain sample.

## Verification

Local Python syntax checks passed for:

- `CubelessEDPCG.py`
- `build_cubeless_ed_true_material_applied_presets.py`
- `verify_cubeless_ed_true_material_applied_presets.py`
- `verify_cubeless_ed_ecosystem_selector_blueprint.py`
- `prepare_cubeless_ed_ecosystem_selector_validation.py`

Unreal MCP build verification:

- `true_material_style_amount_graph_count=24`
- `true_material_style_matrix_graph_count=60`
- `true_material_tree_graph_count=18`
- `style_amount_failure_count=0`
- `style_matrix_failure_count=0`
- `tree_failure_count=0`
- `true_material_applied_validation_pass=True`
- Build marker log error count: `0`

Direct validation actors confirmed:

- Expected point counts match generated point counts.
- Expected ISM counts match generated ISM counts.
- Generated ISM material slots match the expected `descriptor.override_materials`.

Ecosystem selector verification:

- 9 ecosystem selector validation actors prepared and verified.
- Existing non-matching-domain routes stayed on default style/tree graphs.
- 2 true style material routes selected `/TrueMaterialApplied/DesignerStyleProfileMatrixCombos/`.
- 3 true tree material routes selected `/TrueMaterialApplied/TreeProfilePresets/`.
- `ecosystem_selector_validation_pass=True`
- Verification marker log error count: `0`

Menu smoke:

- `ecosystem_actor_count=9`
- `apply_result_count=9`
- `true_style_route_count=2`
- `true_tree_route_count=3`
- `cubeless_ed_pcg_menu_tree_material_expansion_smoke_pass=True`

Final clean log scan:

- Marker: `MCP_CUBELESS_ED_TREE_MATERIAL_EXPANSION_FINAL_CLEAN_MARKER`
- Latest log: `D:\Git\CubelessStylized\Saved\Logs\StylizedCubeless.log`
- `log_error_count=0`

## Notes

The ecosystem verifier skips actor-wide ISM count equality for overlapping mesh domains when both the true material component and the material preview component spawn the same mesh family. In those cases graph path, generated point count, and the direct true-material slot verifier are the authoritative checks.

## Next Approval Point

Further work needs a scope decision before continuing:

- Promote any learning-only graph into a production graph path.
- Remove or change the always-on material preview component behavior.
- Replace the generated graph-matrix approach with a more compact dynamic parameter/material-override architecture.
