# Cubeless Ditch Hierarchy Prototype Result

Date: 2026-06-09

## Scope

Created the first Cubeless-owned production Ditch hierarchy prototype from the
Electric Dreams PCG study work.

Production assets:

```text
/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/BP_Cubeless_PostCopyPointsOffsetIndices
/Game/Cubeless/PCG/ElectricDreamsLearning/PCG_Cubeless_DitchHierarchyPrototype
```

Validation actor:

```text
/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP
MCP_Cubeless_DitchHierarchyPrototype_Validation
```

The validation actor remains disposable. No existing Cubeless production PCG
asset was overwritten.

## Graph Shape

The production graph uses the proven Electric Dreams-style hierarchy path:

```text
Get Owning Actor Spline
-> Spline Sampler
-> source hierarchy CreatePoints branches
-> ActorIndex / ParentIndex / HierarchyDepth / RelativeTransform
-> CopyPoints onto spline targets
-> BP_Cubeless_PostCopyPointsOffsetIndices
-> SideMask filter
-> BranchDensity noise/filter
-> Ground density/self-pruning smoke pass
-> IgnoreParentRotation / IgnoreParentScale
-> ApplyHierarchy
-> StaticMeshSpawner using SM_Grass_Medium01
```

Important implementation detail:

```text
PCGBlueprintSettings.blueprint_element_type =
/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/
BP_Cubeless_PostCopyPointsOffsetIndices.BP_Cubeless_PostCopyPointsOffsetIndices_C
```

So the graph no longer depends on the temporary `_MCP_Temp` post-copy helper.

## Validation Result

Scripts:

```text
build_cubeless_ditch_hierarchy_prototype.py
verify_cubeless_ditch_hierarchy_prototype.py
```

Final verifier result:

```text
source_point_count=12
target_sample_count=6
expected_point_count=42
point_count=42
root_count=6
non_root_count=36
depth_counts={0: 6, 1: 12, 2: 18, 3: 6}
unique_actor_index=True
missing_parent_count=0
parent_depth_mismatch_count=0
group_sizes=[7, 7, 7, 7, 7, 7]
null_group_count=0
side_parent_gap_count=0
survivor_parent_gap_count=0
side_mask_filter_pass_count=42
branch_density_filter_pass_count=42
ground_style_smoke_pass_count=42
noised_threshold_failure_count=0
noised_ratio_failure_count=0
ism_total=42
log_error_count=0
spline_assembly_validation_pass=True
```

Profile behavior:

```text
side_mask_filter_profile=center_right_after_copy
side_pruned_source_names=['LeftBank', 'LeftLowerEdge', 'LeftUpperEdge', 'LeftRockCap']
density_pruned_source_names=['CenterMudPatch']
survivor_source_names=[
  'RiverBankRoot',
  'CenterSilt',
  'RightBank',
  'CenterGrassPatch',
  'RightLowerEdge',
  'RightUpperEdge',
  'RightRockCap'
]
```

## Fixes During Execution

- Production helper duplication succeeded, but the compile API is
  `unreal.BlueprintEditorLibrary.compile_blueprint`, not
  `unreal.KismetEditorUtilities`.
- Unreal `exec` does not define `__file__`, so the wrapper scripts now include a
  fixed path fallback for locating sibling scripts.
- The production verifier wrapper now exports `side_mask_value` to the base
  hierarchy verifier config.
- The graph was rebuilt after verifier-wrapper fixes so the final log scan used
  a fresh production build marker and reported `log_error_count=0`.

## Next Step

The next useful step is not more raw learning. It is a Cubeless integration
layer:

```text
designer-facing PCG controls
-> preset/profile selection
-> stable sample/test level or actor
-> optional integration with existing RuntimeGrass graph
```

Do not overwrite `/Game/Cubeless/PCG/RuntimeGrass/NewPCGGraph` until a concrete
merge direction is chosen.
