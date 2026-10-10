# Electric Dreams PCG Cubeless Density Subtree Pruning Pass

Date: 2026-06-09

## Scope

- Main Cubeless temp graph:
  - `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`
- Build script:
  - `build_spline_assembly_with_post_copy_offset.py`
- Verification script:
  - `verify_spline_assembly_output.py`
- Profile preflight script:
  - `check_side_mask_profile_matrix.py`

Only `_MCP_Temp` Unreal assets and sibling analysis docs were touched. No C++ was created or modified.

## Change

BranchDensity pruning was converted from a fixed constant set into profile specs.

Default active profile remains:

```python
BRANCH_DENSITY_PRUNING_PROFILE = "leaf_mud_only"
```

New override support:

```python
BRANCH_DENSITY_PRUNING_PROFILE_OVERRIDE = None
```

Available density profiles:

```python
BRANCH_DENSITY_PRUNING_PROFILES = {
    "leaf_mud_only": {
        "noise_min": 0.98,
        "noise_max": 1.02,
        "threshold": 0.70,
        "density_overrides": {},
    },
    "right_upper_subtree": {
        "noise_min": 0.98,
        "noise_max": 1.02,
        "threshold": 0.70,
        "density_overrides": {
            "CenterMudPatch": 0.78,
            "RightUpperEdge": 0.68,
            "RightRockCap": 0.68,
        },
    },
}
```

The `right_upper_subtree` profile keeps `CenterMudPatch` alive and lowers both `RightUpperEdge` and its child `RightRockCap` below the filter threshold. This tests a safe parent+child subtree removal rather than an isolated leaf removal.

The builder now uses `branch_density_value(spec)` for:

- created point density
- `BranchDensity` metadata
- verifier expected density counts

## Verifier Update

The verifier now reads active density profile functions from the builder config:

- `active_branch_density_pruning_profile()`
- `branch_density_noise_min()`
- `branch_density_noise_max()`
- `branch_density_filter_threshold()`
- `branch_density_value(spec)`

`classify_pruning()` now accepts a density-value function, so expected survivors and pruned points match the active override profile.

## Verification

### Default Regression

Default active state:

```text
side_mask_filter_profile=center_right_after_copy
branch_density_pruning_profile=leaf_mud_only
expected_point_count=42
point_count=42
density_pruned_source_names=['CenterMudPatch']
missing_parent_count=0
parent_depth_mismatch_count=0
side_profile_failure_count=0
branch_density_filter_pass_count=42
ism_total=42
log_error_count=0
spline_assembly_validation_pass=True
```

### Subtree Override

Override tested through UnrealMCP:

```text
branch_density_pruning_profile=right_upper_subtree
branch_density_overrides={'CenterMudPatch': 0.78, 'RightUpperEdge': 0.68, 'RightRockCap': 0.68}
expected_density_pruned_source_count=2
expected_density_pruned_point_count=12
expected_survivor_source_count=6
expected_point_count=36
point_count=36
depth_counts={0: 6, 1: 12, 2: 18}
missing_parent_count=0
parent_depth_mismatch_count=0
group_sizes=[6, 6, 6, 6, 6, 6]
branch_density_counts={0.78: 12, 0.86: 6, 0.92: 6, 0.96: 6, 1.0: 6}
side_mask_counts={0.0: 24, 1.0: 12}
survivor_source_names=['RiverBankRoot', 'CenterSilt', 'RightBank', 'CenterMudPatch', 'CenterGrassPatch', 'RightLowerEdge']
density_pruned_source_names=['RightUpperEdge', 'RightRockCap']
survivor_parent_gap_count=0
branch_density_filter_pass_count=36
noised_threshold_failure_count=0
noised_ratio_failure_count=0
ism_total=36
log_error_count=0
spline_assembly_validation_pass=True
```

Final graph state was restored to the default `leaf_mud_only` profile and verified again.

External latest-log scan after the final verifier marker:

```text
ERROR_MATCH_COUNT=0
```

## Result

The graph now supports two verified BranchDensity pruning modes:

- `leaf_mud_only`: removes one leaf source point, producing `42` points under the active SideMask profile.
- `right_upper_subtree`: removes a parent+child subtree, producing `36` points under the active SideMask profile.

Both modes preserve hierarchy and produce no parent gaps.

## Next Work

- Compare the simplified SideMask and BranchDensity profiles against Electric Dreams Ditch/Ground graph filters.
- Identify which controls should remain internal learning toggles and which should later become user-facing Cubeless PCG parameters.
- Moving these controls into real project-facing PCG assets outside `_MCP_Temp` should be treated as an approval boundary.
