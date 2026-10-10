# Electric Dreams PCG Post-Copy Offset Blueprint Validation

Date: 2026-06-09

## Scope

- Source Blueprint copied from:
  - `D:\Git\SampleProject\ElectricDreamsEnv\Content\PCG\Assets\PCGCustomNodes\PostCopyPoints-OffsetIndices.uasset`
- Cubeless temp Blueprint path:
  - `/Game/_MCP_Temp/PCGCustomNodes/PostCopyPoints-OffsetIndices`
- Validation graph:
  - `/Game/_MCP_Temp/PCG/ElectricDreams_HierarchyOffsetBlueprintValidation_MCP`
- Validation actor:
  - `MCP_HierarchyOffsetBlueprintValidation`
- Work stayed under `_MCP_Temp`; no project C++ was created or modified.

## What Was Tested

The imported Electric Dreams `PostCopyPoints-OffsetIndices` Blueprint was used through a `PCGBlueprintSettings` node with the original custom pins:

- `CopyPointsOut`
- `CopyPointsTarget`
- `Out`
- override pins visible on the node: `Seed`, `AddRandomOffset`

The validation graph creates:

- 3 source local hierarchy points:
  - root: `ActorIndex=0`, `ParentIndex=-1`, `HierarchyDepth=0`
  - child 1: `ActorIndex=1`, `ParentIndex=0`, `HierarchyDepth=1`
  - child 2: `ActorIndex=2`, `ParentIndex=0`, `HierarchyDepth=1`
- 2 target copy points.
- `CopyPoints` with Electric Dreams settings:
  - relative rotation/scale/color/seed inheritance
  - `attribute_inheritance=SOURCE_ONLY`
  - `tag_inheritance=BOTH`
  - `copy_each_source_on_every_target=true`
- The copied original Blueprint as the post-copy offset step.
- `IgnoreParentRotation=false` and `IgnoreParentScale=false` after the Blueprint, matching the source subgraph.

## Verification Result

Blueprint compile/validate:

- `/Game/_MCP_Temp/PCGCustomNodes/PostCopyPoints-OffsetIndices`: compiled
- validation pass: true
- compile errors: 0
- compile warnings: 0

PCG validation graph:

- `compile_or_notify_pcg_graph`: success
- `save_pcg_graph`: success
- output point count: 6

Key validation output:

```text
rows=[
  (0, 20000000, -1, 0),
  (1, 20000006, -1, 0),
  (2, 20000001, 20000000, 1),
  (3, 20000007, 20000006, 1),
  (4, 20000002, 20000000, 1),
  (5, 20000008, 20000006, 1)
]
root_actor_indices=[20000000, 20000006]
children=[
  (20000001, 20000000),
  (20000007, 20000006),
  (20000002, 20000000),
  (20000008, 20000006)
]
unique_actor_indices=True
parents_point_to_roots=True
key_validation_pass=True
hierarchy_depth_values=[0, 0, 1, 1, 1, 1]
```

Latest `Saved/Logs/StylizedCubeless.log` scan after marker `MCP_PCG_OFFSET_BP_KEY_VALIDATION_BEGIN`:

- `ERROR_MATCH_COUNT=0`

## Findings

- The original Electric Dreams Blueprint works in Cubeless when copied into `_MCP_Temp` and assigned to `PCGBlueprintSettings`.
- It offsets `ActorIndex` and `ParentIndex` by target copy range, preserving per-copy parent relationships.
- It also preserved `HierarchyDepth` when the source hierarchy attributes were authored as explicit constants.
- The copied Blueprint appears to add a large random/key offset, e.g. `20000000`, then separates the second target copy by the total copied point count.

## Failed Pure-PCG Attempt

A pure metadata-math validation graph was also attempted at:

- `/Game/_MCP_Temp/PCG/ElectricDreams_HierarchyOffsetValidation_MCP`

It confirmed useful limits:

- `PCGMetadataMathsSettings` supports `DIVIDE`, `FLOOR`, `MODULO`, `MULTIPLY`, and `ADD`.
- `CopyPoints` output order in the 3-source x 2-target probe was source-major.
- Direct Python access to generated points exposes unique `metadata_entry` values.
- However, PCG selectors such as `$Index` and `$MetadataEntry` evaluated to `0` through `PCGAddAttributeSettings` in this path.
- Therefore, this offset behavior should use the original Blueprint point-loop step or an equivalent point-processing element, not pure PCG math nodes, unless a reliable point-index attribute source is introduced.

## Next Step

Integrate the imported `PostCopyPoints-OffsetIndices` Blueprint into the Cubeless spline assembly graph only after the source hierarchy attributes are moved upstream of `CopyPoints`. Applying it to the current smoke graph without upstream local `ActorIndex` and `ParentIndex` would produce offset keys, but not a faithful Electric Dreams hierarchy.
