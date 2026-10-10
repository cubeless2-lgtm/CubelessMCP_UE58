# Electric Dreams PCG Cubeless Leaf Pruning Pass

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

The previous BranchDensity filter pass was non-destructive. This pass turns it into a controlled destructive pruning test.

New pruning profile:

```python
BRANCH_DENSITY_PRUNING_PROFILE = "leaf_mud_only"
BRANCH_DENSITY_NOISE_MIN = 0.98
BRANCH_DENSITY_NOISE_MAX = 1.02
BRANCH_DENSITY_FILTER_THRESHOLD = 0.70
```

The threshold is intentionally chosen so the result is not random:

- `CenterMudPatch`
  - density: `0.68`
  - max noised density: `0.68 * 1.02 = 0.6936`
  - result: always pruned
- `LeftUpperEdge` and `RightUpperEdge`
  - density: `0.74`
  - min noised density: `0.74 * 0.98 = 0.7252`
  - result: always kept

This removes only the mud leaf branch while preserving parent branches needed by the rock-cap children.

## Verifier Update

The verifier now derives expected results from the builder config instead of assuming the full source assembly always survives.

It classifies source points into:

- survivors
- pruned points
- ambiguous density-band points
- survivor parent gaps

The validation fails if any survivor's parent was pruned or if a source point's noised density range straddles the threshold.

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
branch_density_pruning_profile=leaf_mud_only
target_sample_count=6
expected_survivor_source_count=11
expected_pruned_source_count=1
expected_pruned_point_count=6
expected_point_count=66
point_count=66
root_count=6
non_root_count=60
depth_counts={0: 6, 1: 18, 2: 30, 3: 12}
expected_depth_counts={0: 6, 1: 18, 2: 30, 3: 12}
unique_actor_index=True
missing_parent_count=0
parent_depth_mismatch_count=0
group_sizes=[11, 11, 11, 11, 11, 11]
null_group_count=0
branch_density_counts={0.74: 12, 0.78: 6, 0.82: 12, 0.86: 12, 0.92: 12, 0.96: 6, 1.0: 6}
point_density_counts={0.74: 12, 0.78: 6, 0.82: 12, 0.86: 12, 0.92: 12, 0.96: 6, 1.0: 6}
expected_density_counts={0.74: 12, 0.78: 6, 0.82: 12, 0.86: 12, 0.92: 12, 0.96: 6, 1.0: 6}
branch_density_noise_min=0.98
branch_density_noise_max=1.02
branch_density_filter_threshold=0.7
survivor_source_names=['RiverBankRoot', 'LeftBank', 'CenterSilt', 'RightBank', 'LeftLowerEdge', 'LeftUpperEdge', 'CenterGrassPatch', 'RightLowerEdge', 'RightUpperEdge', 'LeftRockCap', 'RightRockCap']
pruned_source_names=['CenterMudPatch']
ambiguous_pruning_source_names=[]
survivor_parent_gap_count=0
branch_density_filter_pass_count=66
noised_threshold_failure_count=0
noised_ratio_failure_count=0
ism_total=66
log_error_count=0
spline_assembly_validation_pass=True
```

## Result

`BranchDensity` now performs a real destructive prune in the PCG graph. The Ditch/RiverBank source assembly shrinks from `72` generated points to `66`, and the hierarchy remains valid because only the leaf `CenterMudPatch` branch is removed.

## Next Work

- Add graph-side `SideMask` metadata so left/right filtering can be tested after copy, not only during source assembly construction.
- Add a second pruning profile that removes a whole parent subtree safely instead of only a leaf.
- Compare the current branch-density pruning against Electric Dreams' real Ditch/Ground density filter settings.

## Follow-Up

Implemented in `electric_dreams_pcg_cubeless_side_mask_filter_pass.md`.

- Added `SideMask` metadata to every source point:
  - left: `-1.0`
  - center: `0.0`
  - right: `1.0`
- Added graph-side `SideMask >= 0` filtering after the imported PostCopy offset Blueprint.
- Kept the existing `leaf_mud_only` BranchDensity pruning afterward.
- Final generated point count changed from `66` to `42`.
- Verifier now validates side-pruned source points, side mask counts, side filter pass count, and parent gaps across both pruning stages.
