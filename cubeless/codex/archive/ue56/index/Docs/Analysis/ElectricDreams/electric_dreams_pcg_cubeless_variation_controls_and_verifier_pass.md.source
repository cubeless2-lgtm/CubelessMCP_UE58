# Electric Dreams PCG Cubeless Variation Controls and Verifier Pass

Date: 2026-06-09

## Scope

- Main Cubeless temp graph:
  - `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`
- Build script:
  - `build_spline_assembly_with_post_copy_offset.py`
- New verification script:
  - `verify_spline_assembly_output.py`

Only `_MCP_Temp` Unreal assets and sibling analysis docs were touched. No C++ was created or modified.

## Builder Changes

The Ditch/RiverBank study builder now has script-level variation controls:

```python
SIDE_MODE = "both"
BRANCH_JITTER = 18.0
WIDTH_VARIANT = 1.08
HEIGHT_VARIANT = 1.15
```

The riverbank shape constants remain:

```python
RIVERBANK_HALF_WIDTH = 260.0
RIVERBANK_FORWARD_STEP = 120.0
RIVERBANK_HEIGHT_STEP = 35.0
RIVERBANK_ROCK_OFFSET = 70.0
```

Branch density values are now defined by role:

```python
DENSITY_BY_ROLE = {
    "root": 1.0,
    "bank": 0.92,
    "silt": 0.78,
    "lower_edge": 0.86,
    "upper_edge": 0.74,
    "mud": 0.68,
    "grass": 0.96,
    "rock": 0.82,
}
```

Each source point now carries:

- point density
- `BranchDensity` metadata
- side metadata in script data
- role metadata in script data

`BranchDensity` is authored as a PCG double attribute before the source branches merge, so it survives `CopyPoints`, the imported Electric Dreams offset Blueprint, and `ApplyHierarchy`.

## Verifier Changes

`verify_spline_assembly_output.py` now provides reusable validation for the current builder config.

It reads `build_spline_assembly_with_post_copy_offset.py` as a config module with `__name__ = "_pcg_builder_config"` so it can reuse:

- `SOURCE_ASSEMBLY_PRESET`
- `SIDE_MODE`
- `TARGET_SAMPLE_COUNT`
- `get_source_assembly()`
- expected depth counts
- expected density counts

It checks:

- output point count
- root and non-root counts
- `ActorIndex` uniqueness
- missing parent count
- parent-depth mismatch count
- root group sizes
- `HierarchyDepth` distribution
- `BranchDensity` distribution
- point density distribution
- generated ISM instance count
- latest project log `Error:` count after the build marker

The verifier avoids reloading the level when the target actor already exists. This matters because reloading the level clears runtime generated PCG output before inspection.

## Verification

Build output:

```text
source_assembly_preset=ditch_riverbank
source_point_count=12
target_sample_count=6
target_point_spacing=320.0
side_mode=both
branch_jitter=18.0
width_variant=1.08
height_variant=1.15
```

PCG checks:

- `compile_or_notify_pcg_graph`: success
- `save_pcg_graph`: success

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
ism_total=72
log_error_count=0
spline_assembly_validation_pass=True
```

External log scan after `MCP_PCG_SPLINE_ASSEMBLY_VERIFY_BEGIN`:

```text
ERROR_MATCH_COUNT=0
```

Side-mode config verification:

```text
mode=both  source_count=12 parent_ok=True depth_counts={0: 1, 1: 3, 2: 6, 3: 2}
mode=left  source_count=8  parent_ok=True depth_counts={0: 1, 1: 2, 2: 4, 3: 1}
mode=right source_count=8  parent_ok=True depth_counts={0: 1, 1: 2, 2: 4, 3: 1}
```

## Result

The Ditch/RiverBank study graph now has reusable variation controls and branch-level density data. The new verifier catches both hierarchy-key failures and density propagation failures, so future preset edits can be validated without repeating long inline Unreal Python snippets.

## Next Work

- Add actual PCG filter/noise branches that consume `BranchDensity`.
- Add `SideMask` or similar metadata if PCG graph-side left/right filtering becomes necessary.
- Add a Cubeless-owned replacement for the imported Electric Dreams `PostCopyPoints-OffsetIndices` Blueprint when the PCG behavior is stable enough.

## Follow-Up

Implemented in `electric_dreams_pcg_cubeless_branch_density_filter_noise_pass.md`.

- Added `PCGAttributeNoiseSettings` to consume `BranchDensity` and produce `BranchDensityNoised`.
- Added `PCGAttributeFilteringSettings` to pass only points where `BranchDensityNoised >= 0.5`.
- Added `BranchDensityFilterPass = true` after the filter path.
- Updated `verify_spline_assembly_output.py` to validate:
  - `BranchDensityNoised` exists and remains within the configured noise ratio range
  - all noised values pass the configured threshold
  - all points went through the filter pass
  - hierarchy and ISM counts remain valid
