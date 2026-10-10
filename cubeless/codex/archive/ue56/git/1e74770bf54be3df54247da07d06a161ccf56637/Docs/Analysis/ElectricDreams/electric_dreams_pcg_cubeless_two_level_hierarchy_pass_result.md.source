# Electric Dreams PCG Cubeless Two-Level Hierarchy Pass Result

Date: 2026-06-09

## Scope

Test asset path:

- Graph: `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`
- Blueprint: `/Game/_MCP_Temp/PCG/BP_ElectricDreamsSplineAssemblyTest`
- Level: `/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

This pass replaced the previous root-only hierarchy smoke path with a verifiable two-level parent/child hierarchy path. It still uses disposable `_MCP_Temp` assets only.

## Changes

Downstream hierarchy path now uses:

- `ActorIndex`: copied from PCG extra property `$Index` immediately after `MergePoints`.
- `FilterByIndex`: selects index `0` as the root branch.
- Root branch:
  - `ParentIndex = -1`
  - `HierarchyDepth = 0`
  - `RelativeTransform = $Transform`
- Child branch:
  - `ParentIndex = 0`
  - `HierarchyDepth = 1`
  - `RelativeTransform = $Transform`
- Branches are merged back before `ApplyHierarchy`.
- `ApplyHierarchy` reads `ActorIndex`, `ParentIndex`, `HierarchyDepth`, and `RelativeTransform`.
- Spawner remains the Cubeless project mesh:
  `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`

Final executable shape:

`Get Owning Actor Spline -> Spline Sampler -> Copy Points -> Filter/Transform/Merge -> ActorIndex From Global Index -> Split Root Index 0 -> Root/Child hierarchy attribute branches -> Merge Two-Level Hierarchy Branches -> Apply Two-Level Hierarchy -> Spawn Cubeless Grass Mesh -> Output`

## Verification

Compile/save:

- `compile_or_notify_pcg_graph`: success
- `save_pcg_graph`: success

Runtime smoke marker:

- Final marker: `MCP_PCG_TWO_LEVEL_HIERARCHY_PASS_REGEN_BEGIN`
- Latest log scan after that marker found `0` matches for:
  - `LogPCG: Error:`
  - `LogPython: Error:`
  - `LogBlueprint: Error:`
  - `LogScript: Error:`

Generated actor result:

- Actor: `MCP_ElectricDreams_SplineAssembly_Test`
- Generated component: `ISM_SM_Grass_Medium01_3`
- Static mesh: `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`
- Instance count: `20`

## Notes

This proves that `ApplyHierarchy` can execute in Cubeless with a real parent/child attribute relationship:

- root point key: `ActorIndex = 0`
- child parent key: `ParentIndex = 0`
- child depth: `HierarchyDepth = 1`

The remaining fidelity gap is Electric Dreams' exact `SG_CopyPointsWithHierarchy` Blueprint behavior. The next pass should inspect or reproduce the source graph's `ExecuteBlueprint` key generation so copied assembly points preserve the original source/target hierarchy rather than using the smoke-test root-at-index-0 rule.
