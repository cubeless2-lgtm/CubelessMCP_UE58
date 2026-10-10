# Electric Dreams PCG Cubeless Seed-Key Grass Pass Result

Date: 2026-06-09

## Scope

Test asset path:

- Graph: `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`
- Blueprint: `/Game/_MCP_Temp/PCG/BP_ElectricDreamsSplineAssemblyTest`
- Level: `/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

This pass continued the hierarchy smoke graph and moved it closer to Electric Dreams' `SG_CopyPointsWithHierarchy` pattern without adding C++ or touching non-temp assets.

## Changes

Hierarchy attribute path:

- `ActorIndex` now copies from the PCG point property `$Seed` instead of using constant `0`.
- `ParentIndex` remains a root-only constant `-1`.
- `HierarchyDepth` remains a root-only constant `0`.
- `RelativeTransform` now copies from the PCG point property `$Transform` instead of using only an identity constant.

Spawner path:

- Replaced `/Engine/BasicShapes/Cube` placeholder spawning with a real Cubeless project mesh:
  `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`

Final executable path:

`Get Owning Actor Spline -> Spline Sampler -> Copy Points -> Filter/Transform/Merge -> ActorIndex From Point Seed -> ParentIndex Root Constant -> HierarchyDepth Root Constant -> RelativeTransform From Point Transform -> Apply Hierarchy Runtime -> Spawn Cubeless Grass Mesh -> Output`

## Verification

Compile/save:

- `compile_or_notify_pcg_graph`: success
- `save_pcg_graph`: success

Runtime smoke marker:

- Final marker: `MCP_PCG_SEED_KEY_GRASS_PASS_REGEN_BEGIN`
- Latest log scan after that marker found `0` matches for:
  - `LogPCG: Error:`
  - `LogPython: Error:`
  - `LogBlueprint: Error:`
  - `LogScript: Error:`

Generated actor result:

- Actor: `MCP_ElectricDreams_SplineAssembly_Test`
- Generated component: `ISM_SM_Grass_Medium01_1`
- Static mesh: `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01.SM_Grass_Medium01`
- Instance count: `20`

## Notes

This is still not a full Electric Dreams hierarchy clone. It proves the graph can execute through `ApplyHierarchy` using point-derived keys and project-local mesh spawning.

The remaining fidelity gap is parent/child hierarchy reconstruction. The next pass should derive real `ParentIndex` values instead of root-only `-1`, likely by recreating the `ExecuteBlueprint` behavior inside `SG_CopyPointsWithHierarchy` or by replacing it with an MCP-editable PCG node chain if a pure-node equivalent is available.
