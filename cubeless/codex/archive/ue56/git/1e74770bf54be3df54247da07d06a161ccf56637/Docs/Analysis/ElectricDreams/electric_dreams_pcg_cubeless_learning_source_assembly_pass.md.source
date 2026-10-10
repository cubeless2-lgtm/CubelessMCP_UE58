# Electric Dreams PCG Learning Source Assembly Pass

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

The previous main graph used a fixed three-point source hierarchy:

- root
- child 1
- child 2

This pass expands that source-local hierarchy into a seven-point learning assembly with depth 2:

| Local key | Name | Parent | Depth | Relative location |
| --- | --- | --- | --- | --- |
| 0 | Root | -1 | 0 | `(0, 0, 0)` |
| 1 | LeftChild | 0 | 1 | `(-120, 0, 0)` |
| 2 | CenterChild | 0 | 1 | `(0, 90, 0)` |
| 3 | RightChild | 0 | 1 | `(120, 0, 0)` |
| 4 | LeftGrandchild | 1 | 2 | `(-190, 70, 0)` |
| 5 | CenterGrandchild | 2 | 2 | `(0, 180, 0)` |
| 6 | RightGrandchild | 3 | 2 | `(190, 70, 0)` |

The build script now stores this structure in `SOURCE_ASSEMBLY` and creates the source branches from that list. This keeps the study graph easy to modify without rewriting the PCG graph creation logic.

## Graph Shape

The graph still follows the Electric Dreams hierarchy-copy pattern:

1. `00 Get Owning Actor Spline`
2. `01 Spline Target Sampler`
3. Seven source-local hierarchy branches from `SOURCE_ASSEMBLY`
4. `07 Merge Source Local Hierarchy`
5. `08 Copy Source Onto Spline Targets`
6. `09 Original PostCopyPoints Offset Blueprint`
7. `10 IgnoreParentRotation False`
8. `11 IgnoreParentScale False`
9. `12 Apply Offset Hierarchy`
10. `13 Spawn Cubeless Grass Mesh`

## Verification

- `compile_or_notify_pcg_graph`: success
- `save_pcg_graph`: success
- review follow-up:
  - build marker renamed to `MCP_PCG_MAIN_LEARNING_SOURCE_BUILD_BEGIN/END`
  - actor lookup now prefers `EditorActorSubsystem.get_all_level_actors()`, with `EditorLevelLibrary.get_all_level_actors()` kept only as a compatibility fallback
  - review verification marker: `MCP_PCG_MAIN_LEARNING_SOURCE_REVIEW_VERIFY_BEGIN`
  - review verification result: `main_learning_source_review_validation_pass=True`
- regeneration/inspection marker: `MCP_PCG_MAIN_LEARNING_SOURCE_VERIFY_BEGIN`
- expected point count: `7 source points * 5 target points = 35`
- output point data:
  - point count: `35`
  - root count: `5`
  - non-root count: `30`
  - depth counts: `{0: 5, 1: 15, 2: 15}`
  - unique `ActorIndex`: true
  - missing parent count: `0`
  - parent depth mismatch count: `0`
  - root-group sizes: `[7, 7, 7, 7, 7]`
  - null/root-resolution failure count: `0`
  - `main_learning_source_validation_pass=True`
- generated ISM:
  - component: `ISM_SM_Grass_Medium01_0`
  - mesh: `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`
  - instances: `35`
- latest log scan after marker:
  - `ERROR_MATCH_COUNT=0`

Representative key output:

```text
ActorIndex=41000000 ParentIndex=-1 HierarchyDepth=0
ActorIndex=41000001 ParentIndex=41000000 HierarchyDepth=1
ActorIndex=41000002 ParentIndex=41000000 HierarchyDepth=1
ActorIndex=41000003 ParentIndex=41000000 HierarchyDepth=1
ActorIndex=41000004 ParentIndex=41000001 HierarchyDepth=2
ActorIndex=41000005 ParentIndex=41000002 HierarchyDepth=2
ActorIndex=41000006 ParentIndex=41000003 HierarchyDepth=2
ActorIndex=41000035 ParentIndex=-1 HierarchyDepth=0
ActorIndex=41000036 ParentIndex=41000035 HierarchyDepth=1
ActorIndex=41000037 ParentIndex=41000035 HierarchyDepth=1
ActorIndex=41000038 ParentIndex=41000035 HierarchyDepth=1
ActorIndex=41000039 ParentIndex=41000036 HierarchyDepth=2
ActorIndex=41000040 ParentIndex=41000037 HierarchyDepth=2
ActorIndex=41000041 ParentIndex=41000038 HierarchyDepth=2
```

## Result

The Cubeless temporary study graph now demonstrates a reusable Electric Dreams-style copied assembly, not just a minimal root/child smoke test. The imported `PostCopyPoints-OffsetIndices` Blueprint correctly offsets both direct children and grandchildren so every spline target receives an independent seven-point hierarchy.

## Remaining Work

- Replace the temporary copied Electric Dreams Blueprint with a Cubeless-owned implementation before production use.
- Add user-facing graph controls for assembly preset selection, target density, and optional branch transforms.
- Study a real Ditch or Ground use case next, because those graphs call `SG_CopyPointsWithHierarchy` repeatedly and will show how Electric Dreams chooses source assemblies per terrain feature.
