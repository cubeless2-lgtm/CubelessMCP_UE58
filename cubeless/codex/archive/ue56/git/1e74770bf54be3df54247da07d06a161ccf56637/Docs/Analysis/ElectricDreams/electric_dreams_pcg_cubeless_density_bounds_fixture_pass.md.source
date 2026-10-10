# Electric Dreams PCG Cubeless Density Bounds Fixture Pass

Date: 2026-06-09

## Scope

- Fixture graph:
  - `/Game/_MCP_Temp/PCG/ElectricDreams_DensityBoundsFixture_MCP`
- Fixture actor:
  - `MCP_ElectricDreams_DensityBoundsFixture`
- Build script:
  - `build_ground_density_bounds_fixture.py`
- Verification script:
  - `verify_ground_density_bounds_fixture.py`

Only `_MCP_Temp` Unreal assets and sibling analysis docs were touched. No C++ was created or modified.

## Goal

Electric Dreams `PCGDemo_Ground` uses density remap and bounds modification patterns that were not covered by the earlier Ditch-style hierarchy graph. This fixture verifies:

- `PCGDensityRemapSettings`
- `PCGBoundsModifierSettings`
- reading remapped point density and modified bounds through UnrealMCP

## Reflected Settings

`PCGDensityRemapSettings` useful properties:

```text
range_min
range_max
out_range_min
out_range_max
exclude_values_outside_input_range
seed
```

`PCGBoundsModifierSettings` useful properties:

```text
mode
bounds_min
bounds_max
seed
```

`use_seed` exists on both classes but is deprecated/protected and should not be read by these scripts.

Bounds modifier modes found:

```text
SET
INTERSECT
INCLUDE
TRANSLATE
SCALE
```

## Fixture

Input points:

```text
seed=10 coord=(0, 0, 0)    density=0.2 bounds=(-10..10)
seed=11 coord=(220, 0, 0)  density=0.5 bounds=(-12..12)
seed=12 coord=(440, 0, 0)  density=0.8 bounds=(-14..14)
```

Graph path:

```text
01 Density Source Points x3
02 Ground DensityRemap 0.1-0.9
03 Ground BoundsModifier Set
04 DensityBoundsFixturePass True
Output
```

Density remap settings:

```text
range_min=0.0
range_max=1.0
out_range_min=0.1
out_range_max=0.9
exclude_values_outside_input_range=False
```

Bounds modifier settings:

```text
mode=SET
bounds_min=(-64.0, -32.0, -16.0)
bounds_max=(64.0, 32.0, 16.0)
```

## Verification

Expected density values:

```text
seed 10: 0.26
seed 11: 0.50
seed 12: 0.74
```

Verifier result:

```text
source_point_count=3
point_count=3
expected_density_by_seed={10: 0.26, 11: 0.5, 12: 0.74}
expected_bounds_min=(-64.0, -32.0, -16.0)
expected_bounds_max=(64.0, 32.0, 16.0)
density_bounds_fixture_pass_count=3
unknown_seed_count=0
density_mismatch_count=0
bounds_mismatch_count=0
log_marker_found=True
log_error_count=0
density_bounds_fixture_validation_pass=True
```

Rows:

```text
Seed=10 Density=0.26 Translation=(0.0, 0.0, 0.0) BoundsMin=(-64.0, -32.0, -16.0) BoundsMax=(64.0, 32.0, 16.0)
Seed=11 Density=0.5 Translation=(220.0, 0.0, 0.0) BoundsMin=(-64.0, -32.0, -16.0) BoundsMax=(64.0, 32.0, 16.0)
Seed=12 Density=0.74 Translation=(440.0, 0.0, 0.0) BoundsMin=(-64.0, -32.0, -16.0) BoundsMax=(64.0, 32.0, 16.0)
```

## Result

The fixture proves that `PCGDensityRemapSettings` and `PCGBoundsModifierSettings` can be configured through Unreal Python and verified through generated point data.

This adds a second Ground-style foundation next to the destructive self-pruning fixture:

- density remap can be verified by seed-specific output densities
- bounds modification can be verified by exact point bounds
- marker metadata survives the pass chain
- latest log scan is clean

## Next Work

- Combine density remap, bounds modifier, and self-pruning into one `_MCP_Temp` Ground combo fixture.
- Add a verifier that checks how bounds expansion/shrink changes self-pruning survivor count.
- Moving the combo behavior into production Cubeless PCG assets remains an approval boundary.
