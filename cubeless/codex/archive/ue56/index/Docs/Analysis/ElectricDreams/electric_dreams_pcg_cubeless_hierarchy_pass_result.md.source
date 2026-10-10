# Electric Dreams PCG Cubeless Hierarchy Pass Result

Date: 2026-06-09

## Scope

Test asset path:

- Graph: `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`
- Blueprint: `/Game/_MCP_Temp/PCG/BP_ElectricDreamsSplineAssemblyTest`
- Level: `/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

This pass continued the first Cubeless PCG reconstruction pass and reconnected `ApplyHierarchy` into the executable path.

## Graph Shape

Final executable path:

`Get Owning Actor Spline -> Spline Sampler -> Copy Points -> Filter/Transform/Merge -> Add ActorIndex -> Add ParentIndex -> Add HierarchyDepth -> Add RelativeTransform -> Apply Hierarchy Runtime -> Static Mesh Spawner -> Output`

Added hierarchy compatibility attributes:

- `ActorIndex`: `Integer64`, constant `0`
- `ParentIndex`: `Integer64`, constant `-1`
- `HierarchyDepth`: `Integer32`, constant `0`
- `RelativeTransform`: `Transform`, identity transform

`ApplyHierarchy` settings:

- `ApplyHierarchy`: `Always`
- `PointKeyAttributes`: `ActorIndex`
- `ParentKeyAttributes`: `ParentIndex`
- `HierarchyDepthAttribute`: `HierarchyDepth`
- `RelativeTransformAttribute`: `RelativeTransform`
- Parent rotation and parent scale application are set to `Never`
- Invalid parent warnings are disabled for this root-only smoke test

## Verification

Compile/save:

- `compile_or_notify_pcg_graph`: success
- `save_pcg_graph`: success

Runtime smoke marker:

- Final marker: `MCP_PCG_HIERARCHY_PASS_FINAL_REGEN2_BEGIN`
- Latest log scan after that marker found `0` matches for:
  - `LogPCG: Error:`
  - `LogPython: Error:`
  - `LogBlueprint: Error:`
  - `LogScript: Error:`

Generated actor result:

- Actor: `MCP_ElectricDreams_SplineAssembly_Test`
- Generated component: `ISM_Cube_3`
- Instance count: `20`

## Notes

The current hierarchy data is intentionally minimal. It proves that the Cubeless temp graph can execute through `ApplyHierarchy` without PCG log errors, but it does not yet reproduce Electric Dreams' full per-actor parent/child structure.

Next pass should replace the constant root-only hierarchy attributes with real copied source point keys and target parent keys, then swap placeholder cubes for Cubeless/Electric Dreams candidate meshes.
