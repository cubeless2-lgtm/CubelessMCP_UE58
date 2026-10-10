# Electric Dreams PCG Cubeless SideMask Profile Batch Validation

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

The SideMask profiles were adjusted so left/right profile selection preserves the center anchor chain:

- `left_only_after_copy`
  - operator: `LESSER_OR_EQUAL`
  - threshold: `0.0`
  - allowed sides: `left`, `center`
- `right_only_after_copy`
  - operator: `GREATER_OR_EQUAL`
  - threshold: `0.0`
  - allowed sides: `center`, `right`

The builder now supports `SIDE_MASK_FILTER_PROFILE_OVERRIDE` so Codex/MCP can validate a profile without changing the default source constant.

The verifier now supports `BUILDER_CONFIG_OVERRIDE`, allowing it to validate the same in-memory builder profile used to generate the graph.

`check_side_mask_profile_matrix.py` was added as a no-asset preflight matrix. It checks the profile rules, expected point counts, side mask counts, density counts, parent gaps, and ambiguous density pruning before Unreal graph execution.

## Note On Batch Execution

One-call build-and-verify batch execution was not kept. Unreal PCG generated output was not consistently materialized inside the same Python execution call immediately after graph regeneration.

Actual Unreal validation was therefore run as separate MCP steps per profile:

1. build/regenerate the graph with one profile override
2. notify/compile the PCG graph
3. save the PCG graph
4. run the verifier with the same profile override

This matches the reliable timing pattern used by the previous PCG passes.

## Preflight Matrix

`check_side_mask_profile_matrix.py` result:

```text
profile=all_after_copy
  operator=GREATER_OR_EQUAL threshold=-1.0 allowed_sides=['left', 'center', 'right']
  source_survivors=11 side_pruned_sources=0 density_pruned_sources=1 expected_points=66
  side_parent_gaps=0 survivor_parent_gaps=0 ambiguous_density_sources=0 preflight_pass=True

profile=left_only_after_copy
  operator=LESSER_OR_EQUAL threshold=0.0 allowed_sides=['left', 'center']
  source_survivors=7 side_pruned_sources=4 density_pruned_sources=1 expected_points=42
  side_parent_gaps=0 survivor_parent_gaps=0 ambiguous_density_sources=0 preflight_pass=True

profile=center_only_after_copy
  operator=EQUAL threshold=0.0 allowed_sides=['center']
  source_survivors=3 side_pruned_sources=8 density_pruned_sources=1 expected_points=18
  side_parent_gaps=0 survivor_parent_gaps=0 ambiguous_density_sources=0 preflight_pass=True

profile=right_only_after_copy
  operator=GREATER_OR_EQUAL threshold=0.0 allowed_sides=['center', 'right']
  source_survivors=7 side_pruned_sources=4 density_pruned_sources=1 expected_points=42
  side_parent_gaps=0 survivor_parent_gaps=0 ambiguous_density_sources=0 preflight_pass=True

profile=center_right_after_copy
  operator=GREATER_OR_EQUAL threshold=0.0 allowed_sides=['center', 'right']
  source_survivors=7 side_pruned_sources=4 density_pruned_sources=1 expected_points=42
  side_parent_gaps=0 survivor_parent_gaps=0 ambiguous_density_sources=0 preflight_pass=True

MCP_PCG_SIDE_MASK_PROFILE_MATRIX_FAILED=[]
```

## Unreal Validation

Each profile was regenerated, compiled/notified, saved, and verified through UnrealMCP.

| Profile | Final Points | Side-Pruned Points | Density-Pruned Points | Parent Gaps | ISM Total | Result |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `all_after_copy` | 66 | 0 | 6 | 0 | 66 | pass |
| `left_only_after_copy` | 42 | 24 | 6 | 0 | 42 | pass |
| `center_only_after_copy` | 18 | 48 | 6 | 0 | 18 | pass |
| `right_only_after_copy` | 42 | 24 | 6 | 0 | 42 | pass |
| `center_right_after_copy` | 42 | 24 | 6 | 0 | 42 | pass |

The final graph state was restored to the default active profile:

```text
side_mask_filter_profile=center_right_after_copy
side_mask_filter_operator=GREATER_OR_EQUAL
side_mask_filter_threshold=0.0
side_mask_filter_allowed_sides=['center', 'right']
expected_point_count=42
point_count=42
missing_parent_count=0
parent_depth_mismatch_count=0
side_profile_failure_count=0
branch_density_filter_pass_count=42
ism_total=42
log_error_count=0
spline_assembly_validation_pass=True
```

External latest-log scan after the profile matrix marker:

```text
ERROR_MATCH_COUNT=0
```

## Result

All SideMask profiles now have deterministic expected outputs and have been validated through UnrealMCP. The profile definitions are hierarchy-safe because side-only profiles preserve the center anchor chain.

## Next Work

- Add a safe whole-subtree density pruning profile.
- Compare these simplified profile controls against Electric Dreams Ditch/Ground graph filters.
- Decide which profile settings should become exposed Cubeless PCG parameters.

## Follow-Up

Implemented in `electric_dreams_pcg_cubeless_density_subtree_pruning_pass.md`.

- Added `BRANCH_DENSITY_PRUNING_PROFILES`.
- Added `BRANCH_DENSITY_PRUNING_PROFILE_OVERRIDE`.
- Kept default `leaf_mud_only` behavior.
- Added `right_upper_subtree`, which prunes `RightUpperEdge` and `RightRockCap` together.
- Verified default profile still produces `42` points.
- Verified `right_upper_subtree` produces `36` points with no parent gaps.
