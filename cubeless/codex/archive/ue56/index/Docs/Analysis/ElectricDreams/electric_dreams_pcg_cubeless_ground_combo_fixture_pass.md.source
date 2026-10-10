# Electric Dreams PCG Cubeless Ground Combo Fixture Pass

Date: 2026-06-09

## Scope

- Fixture graph:
  - `/Game/_MCP_Temp/PCG/ElectricDreams_GroundComboFixture_MCP`
- Fixture actor:
  - `MCP_ElectricDreams_GroundComboFixture`
- Build script:
  - `build_ground_combo_fixture.py`
- Verification script:
  - `verify_ground_combo_fixture.py`

Only `_MCP_Temp` Unreal assets and sibling analysis docs were touched. No C++ was created or modified.

## Goal

This fixture combines the Ground-style controls learned in the previous passes:

- `PCGDensityRemapSettings`
- `PCGBoundsModifierSettings`
- `PCGSelfPruningSettings`

The graph compares two branches built from the same source points:

- `tight` bounds: should preserve all points
- `expanded` bounds: should create overlap and prune points

## Fixture Input

Six source points are reused in both branches:

```text
seed=20 coord=(0, 0, 0)    density=0.20
seed=21 coord=(40, 0, 0)   density=0.32
seed=22 coord=(80, 0, 0)   density=0.44
seed=23 coord=(300, 0, 0)  density=0.56
seed=24 coord=(340, 0, 0)  density=0.68
seed=25 coord=(620, 0, 0)  density=0.80
```

Shared density remap:

```text
range_min=0.0
range_max=1.0
out_range_min=0.1
out_range_max=0.9
exclude_values_outside_input_range=False
```

Shared self-pruning:

```text
pruning_type=LARGE_TO_SMALL
randomized_pruning=False
radius_similarity_factor=1.0
```

Bounds profiles:

```text
tight    id=0 bounds_min=(-8, -8, -8)      bounds_max=(8, 8, 8)
expanded id=1 bounds_min=(-64, -32, -16)   bounds_max=(64, 32, 16)
```

Graph shape:

```text
tight Source -> DensityRemap -> BoundsModifier -> SelfPruning -> BoundsProfileId -> GroundComboFixturePass
expanded Source -> DensityRemap -> BoundsModifier -> SelfPruning -> BoundsProfileId -> GroundComboFixturePass
Merge -> Output
```

## Verification

Expected remapped densities:

```text
seed 20: 0.260
seed 21: 0.356
seed 22: 0.452
seed 23: 0.548
seed 24: 0.644
seed 25: 0.740
```

Verifier result:

```text
source_point_count=6
point_count=9
profile_counts={'tight': 6, 'expanded': 3}
profile_seeds={'tight': [20, 21, 22, 23, 24, 25], 'expanded': [20, 23, 25]}
tight_count=6
expanded_count=3
ground_combo_fixture_pass_count=9
unknown_profile_count=0
unknown_seed_count=0
density_mismatch_count=0
bounds_mismatch_count=0
log_marker_found=True
log_error_count=0
ground_combo_fixture_validation_pass=True
```

The expanded branch kept one point from each overlap group and the far singleton:

```text
Profile=1 Seed=20 Translation=(0, 0, 0)
Profile=1 Seed=23 Translation=(300, 0, 0)
Profile=1 Seed=25 Translation=(620, 0, 0)
```

## Result

The combo fixture proves the intended Ground-style relationship:

- density remap preserves predictable remapped density values
- bounds profile changes are observable in generated point data
- expanded bounds increase overlap enough for self-pruning to reduce points
- tight bounds preserve the full source point count
- profile markers and pass markers survive the chain
- latest log scan remains clean

This is the strongest local `_MCP_Temp` Ground-style regression target so far.

## Approval Boundary

Further `_MCP_Temp` experiments can continue without approval.

Approval is needed before:

- moving any of these controls into production Cubeless PCG assets outside `_MCP_Temp`
- changing Electric Dreams source assets
- exposing designer-facing parameters in real project assets
- writing non-plugin project C++
