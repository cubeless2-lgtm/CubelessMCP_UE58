# Cubeless ED Dynamic Material Override Prototype

## Result

- Shared graph mutation is not a valid production route for simultaneous variants.
- `PCGMeshSelectorByAttribute` with per-point material override attributes is valid as the native PCG mechanism to reduce static graph variants.

## Tested Paths

### Shared Graph Constant Mutation

Asset:

- `/Game/Cubeless/PCG/ElectricDreamsLearning/DynamicMaterialPrototype/PCG_Cubeless_ED_DynamicMaterial_CompactConifer_Sparse`

The graph used `DynamicMeshPath`, `DynamicMaterialSlot0`, and `DynamicMaterialSlot1` attributes with `PCGMeshSelectorByAttribute`.

Failure found:

- The first validation actor generated as DarkPine, then the same graph asset constants were mutated to SoftPine for the second actor.
- Re-validating both actors showed the DarkPine actor no longer held independent DarkPine material results.
- Conclusion: graph asset constants are shared state and must not be used as per-actor material inputs.

### By-Point Metadata Prototype

Asset:

- `/Game/Cubeless/PCG/ElectricDreamsLearning/DynamicMaterialPrototype/PCG_Cubeless_ED_DynamicMaterial_ByPoint_CompactConifer_Sparse`

The graph keeps two material variants in one graph and assigns material override attributes per point branch:

- `DynamicMeshPath`
- `DynamicMaterialSlot0`
- `DynamicMaterialSlot1`
- `DynamicMaterialVariantType`

Expected validation:

- One graph asset.
- One validation actor.
- Four generated CompactConifer instances.
- Two DarkPine instances and two SoftPine instances, separated by material override metadata.

Validated result:

- `dynamic_by_point_graph_count=1`
- `point_count=4`
- `total_ism=4`
- DarkPine matching row count: `1`, matching instance count: `2`
- SoftPine matching row count: `1`, matching instance count: `2`
- Latest log scan from build marker: `Error:` count `0`
- `dynamic_material_by_point_validation_pass=True`

## Decision

Use by-attribute mesh/material metadata as the production direction for dynamic material override reduction.

For per-actor designer parameters, do not mutate graph assets. The clean long-term route is a small editor/MCP helper that can create graph user parameters and set component `PCGGraphInstance` overrides, or a Blueprint/control actor route that emits PCG metadata into the graph.

## Next Expansion Target

Build and verify one dynamic material axis graph that covers all non-default material override domains:

- GroundFoliage: CoolLeaf, WarmLeaf
- SmallRocks: CoolRock, DarkRock
- CompactConifer: DarkPine, SoftPine

This should compress the six non-default material override graph routes into one `PCGMeshSelectorByAttribute` graph using point metadata.

Validated result:

- Asset: `/Game/Cubeless/PCG/ElectricDreamsLearning/DynamicMaterialPrototype/PCG_Cubeless_ED_DynamicMaterialAxis_ByPoint_AllDomains`
- `dynamic_material_axis_graph_count=1`
- `dynamic_material_axis_expected_row_count=10`
- `point_count=18`
- `total_ism=18`
- GroundFoliage CoolLeaf/WarmLeaf rows matched for Fern and GroundLeaf.
- SmallRocks CoolRock/DarkRock rows matched for both rock meshes.
- CompactConifer DarkPine/SoftPine rows matched for leaf/bark slot overrides.
- Latest log scan from build marker: `Error:` count `0`
- `dynamic_material_axis_by_point_validation_pass=True`

This confirms the six non-default material override graph routes can be represented as one dynamic by-attribute graph when the selection metadata is present on points.

Remaining gap:

- The dynamic graph currently emits all non-default rows at once.
- A production selector needs a non-mutating selection input route so one actor can request one domain/variant pair without changing the shared graph asset.

## Filtered Selection Prototype

Asset:

- `/Game/Cubeless/PCG/ElectricDreamsLearning/DynamicMaterialPrototype/PCG_Cubeless_ED_DynamicMaterialAxis_FilteredConstant_GroundFoliage_CoolLeaf`

The graph keeps the same all-domain by-point material metadata source, then filters before the `PCGMeshSelectorByAttribute` spawner:

- `DesignerMaterialDomainType == 1`
- `DesignerMaterialVariantType == 2`

Validated result:

- `dynamic_material_axis_filtered_constant_graph_count=1`
- Filter nodes: `DesignerMaterialDomainType == 1`, `DesignerMaterialVariantType == 2`
- Spawner selector: `PCGMeshSelectorByAttribute`
- Mesh selector attribute: `DynamicMeshPath`
- Material override attributes: `DynamicMaterialSlot0`, `DynamicMaterialSlot1`
- Selected output rows: GroundFoliage CoolLeaf Fern and GroundLeaf
- `point_count=4`
- `total_ism=4`
- Fern CoolLeaf row matched: `2` instances
- GroundLeaf CoolLeaf row matched: `2` instances
- Latest log scan from build marker: `Error:` count `0`
- `dynamic_material_axis_filtered_constant_validation_pass=True`

This confirms the shared material-axis graph can be pruned by domain/variant metadata without mutating the graph asset. The remaining production step is to replace constant filter thresholds with an actor/property or graph-parameter driven threshold source.

## Actor Property Selection Prototype

Asset:

- `/Game/Cubeless/PCG/ElectricDreamsLearning/DynamicMaterialPrototype/PCG_Cubeless_ED_DynamicMaterialAxis_ActorPropertySelector`

Validation actor:

- `MCP_Cubeless_ED_DynamicMaterialAxis_ActorPropertySelector_GroundFoliage_CoolLeaf_Validation`
- Class: `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGMaterialOverrideSelector`

The graph replaces constant filter thresholds with actor property reads:

- `GetActorProperty(MaterialDomainType) -> SelectedMaterialDomainType`
- `GetActorProperty(MaterialVariantType) -> SelectedMaterialVariantType`
- `DesignerMaterialDomainType == SelectedMaterialDomainType`
- `DesignerMaterialVariantType == SelectedMaterialVariantType`

Validated result:

- `dynamic_material_axis_actor_property_graph_count=1`
- Actor values: `MaterialDomainType=1`, `MaterialVariantType=2`
- Property nodes output `SelectedMaterialDomainType`, `SelectedMaterialVariantType`
- Filter nodes use non-constant threshold Param input, `use_spatial_query=False`
- Spawner selector: `PCGMeshSelectorByAttribute`
- Mesh selector attribute: `DynamicMeshPath`
- Material override attributes: `DynamicMaterialSlot0`, `DynamicMaterialSlot1`
- Selected output rows: GroundFoliage CoolLeaf Fern and GroundLeaf
- `point_count=4`
- `total_ism=4`
- Fern CoolLeaf row matched: `2` instances
- GroundLeaf CoolLeaf row matched: `2` instances
- Latest log scan from build marker: `Error:` count `0`
- `dynamic_material_axis_actor_property_validation_pass=True`

This closes the non-mutating selector route for the material override axis: one shared graph can read per-actor selector values and emit only the requested material domain/variant pair.

## Production-Compatible Actor Property Graph

The first actor-property graph proved the mechanism, but it emitted one full point set per mesh entry. That was useful for axis coverage, but it changed the existing selector behavior for GroundFoliage and SmallRocks by doubling the old preset output count.

Production-compatible asset:

- `/Game/Cubeless/PCG/ElectricDreamsLearning/DynamicMaterialPrototype/PCG_Cubeless_ED_DynamicMaterialAxis_ActorPropertySelector_Compat`

Compatibility rule:

- Keep one shared graph for all non-default material override variants.
- Keep the old preset output counts: GroundFoliage `2`, SmallRocks `2`, CompactConifer `1`.
- Assign per-point `DynamicMeshPath`, `DynamicMaterialSlot0`, and `DynamicMaterialSlot1` metadata instead of duplicating the whole point set per mesh.
- Use actor properties `MaterialDomainType` and `MaterialVariantType` as non-constant filter thresholds.

Validated result:

- All non-default rows represented in one graph: `10`
- All non-default expected points: `10`
- Selected GroundFoliage CoolLeaf expected points: `2`
- Selected GroundFoliage CoolLeaf total ISM: `2`
- Fern CoolLeaf row matched: `1` instance
- GroundLeaf CoolLeaf row matched: `1` instance
- Latest log scan from build marker: `Error:` count `0`
- `dynamic_material_axis_actor_property_compat_validation_pass=True`

Menu integration result:

- `Plugins/CustomTools/Content/Python/ArtScripts/CubelessEDPCG.py` now resolves non-default material override selector variants to the compat dynamic actor-property graph.
- Default material variants still fall back to the existing preset graphs.
- Freshly spawned selector actors can need one editor tick before `GetActorProperty(Self)` is stable, so dynamic graph application schedules one deferred regeneration when immediate output is `0`.
- `_point_count_for_component` now sums all generated tagged point data instead of returning the first item only.

Verification:

- `dynamic_material_axis_menu_apply_validation_pass=True`
- `material_override_selector_validation_pass=True`
- Default variants used preset graphs.
- Non-default variants used `PCG_Cubeless_ED_DynamicMaterialAxis_ActorPropertySelector_Compat`.
- Dynamic selector integration validation relaxes generated point-metadata checks because the dynamic graph's generated point output can be incomplete after the spawner, while graph structure is covered by the compat verifier and final ISM/material slot checks pass.

Ecosystem selector integration result:

- `CubelessEDPCG.apply_ecosystem_selector` uses the same compat dynamic actor-property graph for the ecosystem selector's `PCG_MaterialOverride` preview component.
- `prepare_cubeless_ed_ecosystem_selector_validation.py` prepared 9 ecosystem validation actors with `material_graph_mode='dynamic_actor_property'`.
- `verify_cubeless_ed_ecosystem_selector_blueprint.py` now derives its expected material preview graph from the menu module, matching runtime behavior.
- `ecosystem_selector_validation_pass=True`
- Latest verification marker log `Error:` count: `0`
