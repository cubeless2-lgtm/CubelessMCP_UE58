# Electric Dreams PCG Cubeless Ground Density Self-Pruning Smoke Pass

Date: 2026-06-09

## Scope

- Main Cubeless temp graph:
  - `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`
- Build script:
  - `build_spline_assembly_with_post_copy_offset.py`
- Verification script:
  - `verify_spline_assembly_output.py`

Only `_MCP_Temp` Unreal assets and sibling analysis docs were touched. No C++ was created or modified.

## Goal

The previous comparison showed that Electric Dreams `PCGDemo_Ground` relies heavily on:

- `PCGDensityFilterSettings`
- `PCGSelfPruningSettings`
- density/noise driven point acceptance

This pass adds a non-destructive Ground-style smoke path to the Cubeless study graph while preserving the existing Ditch-style hierarchy validation.

## Implementation

New builder constants:

```python
GROUND_STYLE_DENSITY_FILTER_ENABLED = True
GROUND_STYLE_DENSITY_FILTER_LOWER = 0.0
GROUND_STYLE_DENSITY_FILTER_UPPER = 1.0
GROUND_STYLE_SELF_PRUNING_ENABLED = True
GROUND_STYLE_SELF_PRUNING_RANDOMIZED = False
GROUND_STYLE_SELF_PRUNING_RADIUS_SIMILARITY = 0.0
```

New main-chain nodes after `BranchDensityFilterPass`:

```text
17 GroundStyle DensityFilter Smoke
18 GroundStyle SelfPruning Smoke
19 GroundStyleSmokePass True
20 IgnoreParentRotation False
21 IgnoreParentScale False
22 Apply Offset Hierarchy
23 Spawn Cubeless Grass Mesh
```

The density filter is intentionally broad:

```text
lower_bound=0.0
upper_bound=1.0
invert_filter=False
keep_zero_density_points=False
```

The self-pruning pass is deterministic and no-op oriented:

```text
randomized_pruning=False
radius_similarity_factor=0.0
pruning_type=LARGE_TO_SMALL
```

The verifier now reads and requires `GroundStyleSmokePass=True` for every surviving generated point.

## Pin Note

`PCGDensityFilterSettings` does not use the same filtered output pin names as `PCGAttributeFilteringSettings`.

- `PCGAttributeFilteringSettings` output used here:
  - `InsideFilter`
- `PCGDensityFilterSettings` output used here:
  - `Out`

An initial verifier run failed with no generated point data because the graph attempted to connect the density filter through `InsideFilter`. The graph was inspected through UnrealMCP, the empty edge on `PCGDensityFilterSettings.Out` was found, and the builder was corrected to connect:

```python
graph.add_edge(ground_density_filter, "Out", ground_self_pruning, "In")
```

## Verification

Build result:

```text
source_assembly_preset=ditch_riverbank
source_point_count=12
target_sample_count=6
side_mask_filter_profile=center_right_after_copy
branch_density_pruning_profile=leaf_mud_only
ground_style_density_filter_enabled=True
ground_style_density_filter_lower=0.0
ground_style_density_filter_upper=1.0
ground_style_self_pruning_enabled=True
ground_style_self_pruning_randomized=False
ground_style_self_pruning_radius_similarity=0.0
```

Verifier result:

```text
expected_point_count=42
point_count=42
root_count=6
non_root_count=36
depth_counts={0: 6, 1: 12, 2: 18, 3: 6}
missing_parent_count=0
parent_depth_mismatch_count=0
group_sizes=[7, 7, 7, 7, 7, 7]
null_group_count=0
side_mask_filter_pass_count=42
side_profile_failure_count=0
branch_density_filter_pass_count=42
ground_style_smoke_pass_count=42
noised_threshold_failure_count=0
noised_ratio_failure_count=0
ism_total=42
log_marker_found=True
log_error_count=0
spline_assembly_validation_pass=True
```

## Result

The Cubeless `_MCP_Temp` study graph now contains a verified Ground-style density filter and self-pruning smoke path.

This is not yet a destructive Ground-style spacing/pruning test. It proves that:

- `PCGDensityFilterSettings` can be inserted into the current graph without breaking hierarchy output.
- `PCGSelfPruningSettings` can run in the chain while preserving all current points under the smoke settings.
- metadata survives the new path through `GroundStyleSmokePass`.
- final hierarchy count, parent links, ISM output, and log health remain clean.

## Next Work

- Add a destructive self-pruning fixture that is separate from the production-like hierarchy chain, so overlap/spacing behavior can be measured without silently creating parent gaps.
- Add `PCGDensityRemapSettings` and `PCGBoundsModifierSettings` smoke passes to cover more of the Ground graph pattern.
- Treat moving any of these controls outside `_MCP_Temp` as an approval boundary.
