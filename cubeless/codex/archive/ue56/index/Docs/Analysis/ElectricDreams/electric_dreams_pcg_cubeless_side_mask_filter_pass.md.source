# Electric Dreams PCG Cubeless SideMask Filter Pass

Date: 2026-06-09

## Scope

- Main Cubeless temp graph:
  - `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`
- Build script:
  - `build_spline_assembly_with_post_copy_offset.py`
- Verification script:
  - `verify_spline_assembly_output.py`

Only `_MCP_Temp` Unreal assets and sibling analysis docs were touched. No C++ was created or modified.

## Change

This pass adds graph-side side filtering after source assembly copy.

Source-side `SIDE_MODE` remains `both`, so the full Ditch/RiverBank source assembly is still created before `CopyPoints`.

New side mask metadata:

```python
SIDE_MASK_BY_SIDE = {
    "left": -1.0,
    "center": 0.0,
    "right": 1.0,
}
```

New graph-side filter controls:

```python
SIDE_MASK_FILTER_PROFILE = "center_right_after_copy"
SIDE_MASK_FILTER_THRESHOLD = 0.0
```

New graph segment:

```text
11 Original PostCopyPoints Offset Blueprint
-> 12 Filter SideMask >= Threshold
-> 13 SideMaskFilterPass True
-> 14 Consume BranchDensity With Noise
-> 15 Filter BranchDensityNoised >= Threshold
-> 16 BranchDensityFilterPass True
-> 17 IgnoreParentRotation False
-> 18 IgnoreParentScale False
-> 19 Apply Offset Hierarchy
-> 20 Spawn Cubeless Grass Mesh
```

The side filter keeps center/right points and prunes left points after copy:

- kept: `SideMask >= 0`
- pruned: `SideMask < 0`

The previous `leaf_mud_only` BranchDensity pruning still runs afterward, so `CenterMudPatch` is also removed.

## Verifier Update

The verifier now derives the expected final set in two stages:

1. side survivors vs side-pruned source points
2. density survivors vs density-pruned source points

It validates:

- final point count
- side mask counts
- BranchDensity counts
- point density counts
- side filter pass count
- BranchDensity filter pass count
- side threshold failures
- density threshold failures
- noise ratio failures
- parent gaps after side filtering and final pruning
- hierarchy depth counts
- root group sizes
- ISM total
- latest Unreal log `Error:` count

## Verification

- Build marker: `MCP_PCG_MAIN_LEARNING_SOURCE_BUILD_BEGIN`
- PCG graph compile/notify: success
- PCG graph save: success
- Verifier marker: `MCP_PCG_SPLINE_ASSEMBLY_VERIFY_BEGIN`
- External latest-log scan after verifier marker:
  - `ERROR_MATCH_COUNT=0`

Reusable verifier result:

```text
source_assembly_preset=ditch_riverbank
side_mode=both
source_point_count=12
side_mask_filter_profile=center_right_after_copy
side_mask_filter_threshold=0.0
branch_density_pruning_profile=leaf_mud_only
target_sample_count=6
expected_side_survivor_source_count=8
expected_side_pruned_source_count=4
expected_side_pruned_point_count=24
expected_density_pruned_source_count=1
expected_density_pruned_point_count=6
expected_survivor_source_count=7
expected_pruned_source_count=5
expected_pruned_point_count=30
expected_point_count=42
point_count=42
root_count=6
non_root_count=36
depth_counts={0: 6, 1: 12, 2: 18, 3: 6}
expected_depth_counts={0: 6, 1: 12, 2: 18, 3: 6}
unique_actor_index=True
missing_parent_count=0
parent_depth_mismatch_count=0
group_sizes=[7, 7, 7, 7, 7, 7]
null_group_count=0
branch_density_counts={0.74: 6, 0.78: 6, 0.82: 6, 0.86: 6, 0.92: 6, 0.96: 6, 1.0: 6}
side_mask_counts={0.0: 18, 1.0: 24}
expected_side_mask_counts={0.0: 18, 1.0: 24}
point_density_counts={0.74: 6, 0.78: 6, 0.82: 6, 0.86: 6, 0.92: 6, 0.96: 6, 1.0: 6}
expected_density_counts={0.74: 6, 0.78: 6, 0.82: 6, 0.86: 6, 0.92: 6, 0.96: 6, 1.0: 6}
survivor_source_names=['RiverBankRoot', 'CenterSilt', 'RightBank', 'CenterGrassPatch', 'RightLowerEdge', 'RightUpperEdge', 'RightRockCap']
side_pruned_source_names=['LeftBank', 'LeftLowerEdge', 'LeftUpperEdge', 'LeftRockCap']
density_pruned_source_names=['CenterMudPatch']
side_parent_gap_count=0
ambiguous_pruning_source_names=[]
survivor_parent_gap_count=0
side_mask_filter_pass_count=42
side_threshold_failure_count=0
branch_density_filter_pass_count=42
noised_threshold_failure_count=0
noised_ratio_failure_count=0
ism_total=42
log_error_count=0
spline_assembly_validation_pass=True
```

## Result

The graph now performs two real post-copy pruning stages while preserving hierarchy:

1. `SideMask >= 0` keeps center/right and removes the left subtree.
2. `BranchDensityNoised >= 0.70` removes the `CenterMudPatch` leaf.

The Ditch/RiverBank output shrinks from the original `72` copied points to `42` final generated points, with no missing parents, no depth mismatch, and no Unreal log errors.

## Next Work

- Add a selectable left/right/center side filter profile instead of the current fixed center/right profile.
- Add a safe whole-subtree pruning profile for non-side density experiments.
- Compare this simplified side filtering against Electric Dreams Ditch/Ground graph filters and decide which controls should become user-facing Cubeless PCG parameters.

## Follow-Up

Implemented in `electric_dreams_pcg_cubeless_side_mask_profiles_pass.md`.

- Replaced the fixed `SideMask >= 0` setup with `SIDE_MASK_FILTER_PROFILES`.
- Added selectable profiles:
  - `all_after_copy`
  - `left_only_after_copy`
  - `center_only_after_copy`
  - `right_only_after_copy`
  - `center_right_after_copy`
- Kept the active profile as `center_right_after_copy`, preserving the validated `42` point output.
- Updated the verifier to read the same profile spec as the graph builder and report `side_profile_failure_count`.
