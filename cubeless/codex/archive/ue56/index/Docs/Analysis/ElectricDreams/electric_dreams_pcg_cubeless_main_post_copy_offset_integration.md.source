# Electric Dreams PCG Main Post-Copy Offset Integration

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

## Integration

The main graph was rebuilt into a single source-to-spawn chain:

1. `00 Get Owning Actor Spline`
2. `01 Spline Target Sampler`
3. Three source-local hierarchy branches:
   - root: `ActorIndex=0`, `ParentIndex=-1`, `HierarchyDepth=0`, `RelativeTransform=$Transform`
   - child 1: `ActorIndex=1`, `ParentIndex=0`, `HierarchyDepth=1`, `RelativeTransform=$Transform`
   - child 2: `ActorIndex=2`, `ParentIndex=0`, `HierarchyDepth=1`, `RelativeTransform=$Transform`
4. `07 Merge Source Local Hierarchy`
5. `08 Copy Source Onto Spline Targets`
   - relative rotation/scale/color/seed inheritance
   - `attribute_inheritance=SOURCE_ONLY`
   - `tag_inheritance=BOTH`
   - `copy_each_source_on_every_target=true`
6. `09 Original PostCopyPoints Offset Blueprint`
   - Blueprint class: `/Game/_MCP_Temp/PCGCustomNodes/PostCopyPoints-OffsetIndices.PostCopyPoints-OffsetIndices_C`
   - `CopyPointsOut` input connected from `CopyPoints`
   - `CopyPointsTarget` input connected from the spline sampler target stream
7. `10 IgnoreParentRotation False`
8. `11 IgnoreParentScale False`
9. `12 Apply Offset Hierarchy`
   - parent rotation/scale are `OPT_OUT_BY_ATTRIBUTE`
   - attributes: `IgnoreParentRotation`, `IgnoreParentScale`
10. `13 Spawn Cubeless Grass Mesh`
    - `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`

## Verification

- `compile_or_notify_pcg_graph`: success
- `save_pcg_graph`: success
- regeneration marker: `MCP_PCG_MAIN_POST_COPY_OFFSET_VERIFY_BEGIN`
- output point data:
  - point count: `15`
  - root count: `5`
  - child count: `10`
  - unique `ActorIndex`: true
  - child `ParentIndex` values point to copy-local roots: true
  - `HierarchyDepth` values: `[0, 1]`
  - `main_key_validation_pass=True`
- generated ISM:
  - component: `ISM_SM_Grass_Medium01_0`
  - mesh: `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`
  - instances: `15`
- latest log scan after marker:
  - `ERROR_MATCH_COUNT=0`

Representative key output:

```text
roots:
  21000000 -> ParentIndex -1
  21000015 -> ParentIndex -1
  21000030 -> ParentIndex -1
  21000045 -> ParentIndex -1
  21000060 -> ParentIndex -1

children:
  21000001 -> 21000000
  21000002 -> 21000000
  21000016 -> 21000015
  21000017 -> 21000015
  21000031 -> 21000030
  21000032 -> 21000030
  21000046 -> 21000045
  21000047 -> 21000045
  21000061 -> 21000060
  21000062 -> 21000060
```

## Result

The main Cubeless temp spline assembly graph now uses the original Electric Dreams post-copy offset Blueprint in the correct position: after `CopyPoints` and after source-local hierarchy attributes exist. This is the first main-graph pass where the hierarchy key generation behavior is structurally aligned with `SG_CopyPointsWithHierarchy`.

## Remaining Work

- Replace the hardcoded three-point source-local hierarchy with a learned/sample-driven local assembly source.
- Expose source assembly count and target sampling controls so the setup can become a reusable PCG study graph.
- Decide whether the copied Electric Dreams Blueprint should remain a temporary study asset or be recreated as a Cubeless-owned Blueprint/PCG element before any durable production use.
