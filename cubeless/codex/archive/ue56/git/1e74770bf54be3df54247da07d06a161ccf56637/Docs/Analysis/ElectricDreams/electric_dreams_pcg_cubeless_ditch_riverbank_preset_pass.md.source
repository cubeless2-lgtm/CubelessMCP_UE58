# Electric Dreams PCG Cubeless Ditch Riverbank Preset Pass

Date: 2026-06-09

## Scope

- Main Cubeless temp graph:
  - `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`
- Test level:
  - `/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`
- Test actor:
  - `MCP_ElectricDreams_SplineAssembly_Test`
- Imported source Blueprint:
  - `/Game/_MCP_Temp/PCGCustomNodes/PostCopyPoints-OffsetIndices`
- Build script:
  - `build_spline_assembly_with_post_copy_offset.py`

Only `_MCP_Temp` Unreal assets and sibling analysis docs were touched. No C++ was created or modified.

## Change

The Cubeless study builder now supports source assembly presets and target sampling controls:

```python
SOURCE_ASSEMBLY_PRESET = "ditch_riverbank"
TARGET_SAMPLE_COUNT = 6
TARGET_POINT_SPACING = 320.0

RIVERBANK_HALF_WIDTH = 260.0
RIVERBANK_FORWARD_STEP = 120.0
RIVERBANK_HEIGHT_STEP = 35.0
RIVERBANK_ROCK_OFFSET = 70.0
```

Available source assembly presets:

- `learning_tree`
  - previous 7-point root/child/grandchild learning assembly
- `ditch_riverbank`
  - 12-point Ditch/RiverBank study assembly
  - default preset after this pass

The `PCGSplineSamplerSettings` node is now configured explicitly through `sampler_params`:

- `dimension = ON_SPLINE`
- `mode = NUMBER_OF_SAMPLES`
- `num_samples = TARGET_SAMPLE_COUNT`
- spacing-related fields mirror `TARGET_POINT_SPACING`

The test actor spline length is also derived from `TARGET_SAMPLE_COUNT` and `TARGET_POINT_SPACING`, so the target setup is no longer an implicit default.

## Ditch/RiverBank Source Assembly

| Local key | Name | Parent | Depth |
| --- | --- | ---: | ---: |
| 0 | `RiverBankRoot` | -1 | 0 |
| 1 | `LeftBank` | 0 | 1 |
| 2 | `CenterSilt` | 0 | 1 |
| 3 | `RightBank` | 0 | 1 |
| 4 | `LeftLowerEdge` | 1 | 2 |
| 5 | `LeftUpperEdge` | 1 | 2 |
| 6 | `CenterMudPatch` | 2 | 2 |
| 7 | `CenterGrassPatch` | 2 | 2 |
| 8 | `RightLowerEdge` | 3 | 2 |
| 9 | `RightUpperEdge` | 3 | 2 |
| 10 | `LeftRockCap` | 5 | 3 |
| 11 | `RightRockCap` | 9 | 3 |

This preset is not trying to copy the final Electric Dreams art exactly. It is a controlled study preset that mirrors the Ditch graph's repeated river embankment/edge/rock hierarchy-copy shape.

## Verification

- Build marker: `MCP_PCG_MAIN_LEARNING_SOURCE_BUILD_BEGIN`
- Build output:
  - `source_assembly_preset=ditch_riverbank`
  - `source_point_count=12`
  - `target_sample_count=6`
  - `target_point_spacing=320.0`
- `compile_or_notify_pcg_graph`: success
- `save_pcg_graph`: success
- Verification marker: `MCP_PCG_DITCH_RIVERBANK_VERIFY_BEGIN`
- Expected point count: `12 source points * 6 target points = 72`
- Output point data:
  - point count: `72`
  - root count: `6`
  - non-root count: `66`
  - depth counts: `{0: 6, 1: 18, 2: 36, 3: 12}`
  - unique `ActorIndex`: true
  - missing parent count: `0`
  - parent depth mismatch count: `0`
  - root-group sizes: `[12, 12, 12, 12, 12, 12]`
  - null/root-resolution failure count: `0`
  - `ditch_riverbank_validation_pass=True`
- generated ISM:
  - component: `ISM_SM_Grass_Medium01_0`
  - mesh: `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`
  - instances: `72`
- latest log scan after marker:
  - `ERROR_MATCH_COUNT=0`

Representative key output:

```text
ActorIndex=66000000 ParentIndex=-1 HierarchyDepth=0
ActorIndex=66000001 ParentIndex=66000000 HierarchyDepth=1
ActorIndex=66000002 ParentIndex=66000000 HierarchyDepth=1
ActorIndex=66000003 ParentIndex=66000000 HierarchyDepth=1
ActorIndex=66000004 ParentIndex=66000001 HierarchyDepth=2
ActorIndex=66000005 ParentIndex=66000001 HierarchyDepth=2
ActorIndex=66000006 ParentIndex=66000002 HierarchyDepth=2
ActorIndex=66000007 ParentIndex=66000002 HierarchyDepth=2
ActorIndex=66000008 ParentIndex=66000003 HierarchyDepth=2
ActorIndex=66000009 ParentIndex=66000003 HierarchyDepth=2
ActorIndex=66000010 ParentIndex=66000005 HierarchyDepth=3
ActorIndex=66000011 ParentIndex=66000009 HierarchyDepth=3
```

## Result

The Cubeless temporary PCG study graph now has a Ditch/RiverBank preset with explicit target sampling controls. The imported Electric Dreams post-copy offset Blueprint preserved a depth-3 hierarchy across six target copies, so the next pass can focus on Ditch-style variation controls rather than basic key correctness.

## Next Work

- Add preset variation controls:
  - side selection
  - branch jitter
  - embankment width/height variants
  - per-branch density filters
- Add a compact verification script so future preset changes do not require repeated inline validation snippets.
- Decide when to replace the imported Electric Dreams `PostCopyPoints-OffsetIndices` Blueprint with a Cubeless-owned equivalent.

## Follow-Up

Implemented in `electric_dreams_pcg_cubeless_variation_controls_and_verifier_pass.md`.

- Added `SIDE_MODE`, `BRANCH_JITTER`, `WIDTH_VARIANT`, and `HEIGHT_VARIANT`.
- Added per-role point density and `BranchDensity` metadata.
- Added reusable verifier script `verify_spline_assembly_output.py`.
- Verified the default `ditch_riverbank` / `both` setup:
  - point count: `72`
  - depth counts: `{0: 6, 1: 18, 2: 36, 3: 12}`
  - branch density counts match expected counts
  - point density counts match expected counts
  - ISM instances: `72`
  - latest log `Error:` count: `0`
