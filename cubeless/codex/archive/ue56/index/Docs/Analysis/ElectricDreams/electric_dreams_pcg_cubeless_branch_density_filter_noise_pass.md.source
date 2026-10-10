# Electric Dreams PCG Cubeless Branch Density Filter/Noise Pass

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

The graph now consumes `BranchDensity` through actual PCG nodes instead of only carrying it as metadata.

New script-level controls:

```python
BRANCH_DENSITY_NOISE_MIN = 0.95
BRANCH_DENSITY_NOISE_MAX = 1.05
BRANCH_DENSITY_FILTER_THRESHOLD = 0.5
```

New graph segment after the imported Electric Dreams post-copy offset Blueprint:

```text
09 Original PostCopyPoints Offset Blueprint
-> 10 Consume BranchDensity With Noise
-> 11 Filter BranchDensityNoised >= Threshold
-> 12 BranchDensityFilterPass True
-> 13 IgnoreParentRotation False
-> 14 IgnoreParentScale False
-> 15 Apply Offset Hierarchy
-> 16 Spawn Cubeless Grass Mesh
```

Node behavior:

- `PCGAttributeNoiseSettings`
  - input: `BranchDensity`
  - output: `BranchDensityNoised`
  - mode: `MULTIPLY`
  - range: `0.95` to `1.05`
- `PCGAttributeFilteringSettings`
  - target: `BranchDensityNoised`
  - operator: `GREATER_OR_EQUAL`
  - constant threshold: `0.5`
  - output path used: `InsideFilter`
- `PCGAddAttributeSettings`
  - adds `BranchDensityFilterPass = true`
  - this makes it easy for the verifier to confirm the filter path actually ran

The current preset intentionally keeps every point above the threshold. This pass validates the PCG consumption path without breaking hierarchy counts. Later passes can raise the threshold or branch the outside-filter output when destructive pruning is desired.

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
target_sample_count=6
expected_point_count=72
point_count=72
root_count=6
non_root_count=66
depth_counts={0: 6, 1: 18, 2: 36, 3: 12}
expected_depth_counts={0: 6, 1: 18, 2: 36, 3: 12}
unique_actor_index=True
missing_parent_count=0
parent_depth_mismatch_count=0
group_sizes=[12, 12, 12, 12, 12, 12]
null_group_count=0
branch_density_counts={0.68: 6, 0.74: 12, 0.78: 6, 0.82: 12, 0.86: 12, 0.92: 12, 0.96: 6, 1.0: 6}
point_density_counts={0.68: 6, 0.74: 12, 0.78: 6, 0.82: 12, 0.86: 12, 0.92: 12, 0.96: 6, 1.0: 6}
expected_density_counts={0.68: 6, 0.74: 12, 0.78: 6, 0.82: 12, 0.86: 12, 0.92: 12, 0.96: 6, 1.0: 6}
branch_density_noise_min=0.95
branch_density_noise_max=1.05
branch_density_filter_threshold=0.5
branch_density_filter_pass_count=72
noised_threshold_failure_count=0
noised_ratio_failure_count=0
ism_total=72
log_error_count=0
spline_assembly_validation_pass=True
```

Representative rows:

```text
ActorIndex=78000000 ParentIndex=-1 HierarchyDepth=0 BranchDensity=1.0 BranchDensityNoised=1.03 BranchDensityFilterPass=True PointDensity=1.0
ActorIndex=78000001 ParentIndex=78000000 HierarchyDepth=1 BranchDensity=0.92 BranchDensityNoised=0.889 BranchDensityFilterPass=True PointDensity=0.92
ActorIndex=78000006 ParentIndex=78000002 HierarchyDepth=2 BranchDensity=0.68 BranchDensityNoised=0.672 BranchDensityFilterPass=True PointDensity=0.68
ActorIndex=78000010 ParentIndex=78000005 HierarchyDepth=3 BranchDensity=0.82 BranchDensityNoised=0.856 BranchDensityFilterPass=True PointDensity=0.82
```

## Result

`BranchDensity` is now consumed by real PCG noise and filter nodes while preserving the Electric Dreams-style hierarchy copy path. The current filter is non-destructive for the Ditch/RiverBank preset, so all 72 points remain and all hierarchy checks still pass.

## Next Work

- Add a destructive pruning preset or threshold test that intentionally sends low-density points to `OutsideFilter`.
- Decide whether to preserve hierarchy by pruning complete root groups rather than individual children.
- Add `SideMask` or side metadata to make left/right filtering possible inside the PCG graph instead of only in the source builder.

## Follow-Up

Implemented in `electric_dreams_pcg_cubeless_leaf_pruning_pass.md`.

- Added `leaf_mud_only` pruning profile.
- Narrowed BranchDensity noise to `0.98-1.02`.
- Raised BranchDensity threshold to `0.70`.
- Pruned only `CenterMudPatch`, reducing generated points from `72` to `66`.
- Updated the verifier to derive expected survivor/pruned counts from the builder config and reject ambiguous threshold bands or survivor parent gaps.
