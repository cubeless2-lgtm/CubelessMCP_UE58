# Electric Dreams PCG Cubeless Self-Pruning Fixture Pass

Date: 2026-06-09

## Scope

- Fixture graph:
  - `/Game/_MCP_Temp/PCG/ElectricDreams_SelfPruningFixture_MCP`
- Fixture actor:
  - `MCP_ElectricDreams_SelfPruningFixture`
- Build script:
  - `build_ground_self_pruning_fixture.py`
- Verification script:
  - `verify_ground_self_pruning_fixture.py`

Only `_MCP_Temp` Unreal assets and sibling analysis docs were touched. No C++ was created or modified.

## Goal

The main spline assembly graph now has a non-destructive `PCGSelfPruningSettings` smoke path. This separate fixture tests destructive self-pruning behavior without risking hierarchy parent gaps in the main graph.

## Fixture Input

The fixture creates 8 source points:

```text
ClusterA_Large        seed=100  coord=(0, 0, 0)     bounds=(120, 120, 80)
ClusterA_SmallCenter  seed=101  coord=(0, 0, 0)     bounds=(35, 35, 35)
ClusterA_SmallRight   seed=102  coord=(24, 0, 0)    bounds=(30, 30, 30)
ClusterA_TinyLeft     seed=103  coord=(-18, 8, 0)   bounds=(20, 20, 20)
ClusterB_Large        seed=200  coord=(460, 0, 0)   bounds=(110, 110, 70)
ClusterB_SmallCenter  seed=201  coord=(460, 0, 0)   bounds=(32, 32, 32)
FarSingletonA         seed=300  coord=(900, 0, 0)   bounds=(35, 35, 35)
FarSingletonB         seed=301  coord=(1120, 0, 0)  bounds=(35, 35, 35)
```

The graph path is:

```text
01 Overlap Source Points x8
02 Ground DensityFilter Broad Pass
03 Destructive SelfPruning LargeToSmall
04 SelfPruningFixturePass True
Output
```

Settings:

```text
density_filter_lower=0.0
density_filter_upper=1.0
self_pruning_randomized=False
self_pruning_radius_similarity=1.0
pruning_type=LARGE_TO_SMALL
```

## Verification

The verifier checks:

- output point count is lower than source point count
- at least two points survive
- all survivor seeds are known source seeds
- `SelfPruningFixturePass=True` exists on every output point
- unique survivor positions remain
- latest editor log scan after the fixture build marker has no `Error:` lines

Final result:

```text
source_point_count=8
point_count=4
pruned_count=4
survived_seeds=[100, 200, 300, 301]
self_pruning_fixture_pass_count=4
unique_position_count=4
unique_positions=[(0.0, 0.0, 0.0), (460.0, 0.0, 0.0), (900.0, 0.0, 0.0), (1120.0, 0.0, 0.0)]
log_marker_found=True
log_error_count=0
self_pruning_fixture_validation_pass=True
```

The first build produced a deprecation warning from `EditorLevelLibrary.get_all_level_actors`. The builder was updated to prefer `EditorActorSubsystem.get_all_level_actors()`, then rebuilt and reverified cleanly.

## Result

The fixture proves destructive `PCGSelfPruningSettings` behavior in UE 5.7 PCG under UnrealMCP:

- overlapping small points under large cluster points are removed
- large cluster points survive
- far singleton points survive
- broad density filtering does not remove the fixture points
- post-pruning metadata can still be marked and verified

This is now a stable local reference for adding spacing/overlap controls to future Cubeless PCG generation.

## Next Work

- Add `PCGDensityRemapSettings` fixture/pass to learn Ground-style density curve remapping.
- Add `PCGBoundsModifierSettings` fixture/pass to learn bounds inflation/shrink before self-pruning.
- Later combine density remap, bounds modifier, and self-pruning in `_MCP_Temp`.
- Moving these controls into production Cubeless PCG assets remains an approval boundary.
