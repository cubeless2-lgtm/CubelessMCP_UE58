# Electric Dreams PCG Cubeless SideMask Profiles Pass

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

The previous SideMask pass had one fixed graph-side filter: `SideMask >= 0`.

This pass keeps the active behavior the same, but replaces the hard-coded filter with selectable profile specs. The active profile remains:

```python
SIDE_MASK_FILTER_PROFILE = "center_right_after_copy"
```

Available profiles:

```python
SIDE_MASK_FILTER_PROFILES = {
    "all_after_copy": {
        "operator": "GREATER_OR_EQUAL",
        "threshold": -1.0,
        "allowed_sides": ("left", "center", "right"),
    },
    "left_only_after_copy": {
        "operator": "EQUAL",
        "threshold": -1.0,
        "allowed_sides": ("left",),
    },
    "center_only_after_copy": {
        "operator": "EQUAL",
        "threshold": 0.0,
        "allowed_sides": ("center",),
    },
    "right_only_after_copy": {
        "operator": "EQUAL",
        "threshold": 1.0,
        "allowed_sides": ("right",),
    },
    "center_right_after_copy": {
        "operator": "GREATER_OR_EQUAL",
        "threshold": 0.0,
        "allowed_sides": ("center", "right"),
    },
}
```

The builder now configures `PCGAttributeFilteringSettings` from the selected profile:

- `GREATER_OR_EQUAL` profiles use threshold ranges.
- `EQUAL` profiles select a single side mask.
- The active graph node title includes the selected profile name.

## Verifier Update

The verifier now reads the same `side_filter_profile_spec()` from the builder config and uses it for:

- expected side survivors
- expected side-pruned source points
- SideMask output validation
- side profile failure count
- printed operator, threshold, and allowed sides

This avoids duplicating side filter rules between the graph builder and verifier.

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
side_mask_filter_operator=GREATER_OR_EQUAL
side_mask_filter_threshold=0.0
side_mask_filter_allowed_sides=['center', 'right']
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
depth_counts={0: 6, 1: 12, 2: 18, 3: 6}
expected_depth_counts={0: 6, 1: 12, 2: 18, 3: 6}
missing_parent_count=0
parent_depth_mismatch_count=0
group_sizes=[7, 7, 7, 7, 7, 7]
side_mask_counts={0.0: 18, 1.0: 24}
expected_side_mask_counts={0.0: 18, 1.0: 24}
side_pruned_source_names=['LeftBank', 'LeftLowerEdge', 'LeftUpperEdge', 'LeftRockCap']
density_pruned_source_names=['CenterMudPatch']
side_parent_gap_count=0
survivor_parent_gap_count=0
side_mask_filter_pass_count=42
side_profile_failure_count=0
branch_density_filter_pass_count=42
noised_threshold_failure_count=0
noised_ratio_failure_count=0
ism_total=42
log_error_count=0
spline_assembly_validation_pass=True
```

## Result

Side filtering is now reusable rather than hard-coded. The active `center_right_after_copy` profile still produces the previously validated `42` final points, while the builder can now switch to `all`, `left only`, `center only`, or `right only` by changing one profile constant.

## Next Work

- Add profile batch validation that regenerates and verifies each SideMask profile in sequence.
- Add a safe whole-subtree density pruning profile.
- Decide which profile controls should eventually become user-facing Cubeless PCG parameters.

## Follow-Up

Implemented in `electric_dreams_pcg_cubeless_side_mask_profile_batch_validation.md`.

- Added `SIDE_MASK_FILTER_PROFILE_OVERRIDE` for MCP-driven profile validation.
- Added `BUILDER_CONFIG_OVERRIDE` to the verifier so it validates the same in-memory profile used by the builder.
- Added `check_side_mask_profile_matrix.py` for no-asset profile preflight.
- Validated all SideMask profiles through separate UnrealMCP build/compile/save/verify calls:
  - `all_after_copy`: `66` points
  - `left_only_after_copy`: `42` points
  - `center_only_after_copy`: `18` points
  - `right_only_after_copy`: `42` points
  - `center_right_after_copy`: `42` points
- Restored the final graph state to the default `center_right_after_copy` profile.
