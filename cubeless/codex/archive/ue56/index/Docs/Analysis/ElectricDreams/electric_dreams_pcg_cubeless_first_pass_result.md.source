# Electric Dreams PCG Cubeless First Pass Result

Date: 2026-06-09

## Scope

Implemented the first disposable Cubeless reproduction target from the Electric
Dreams PCG learning plan.

All Unreal assets were created under `/Game/_MCP_Temp/PCG/` and are disposable.
They are ignored by Git and should not be committed unless explicitly requested.

## Created Assets

- `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`
- `/Game/_MCP_Temp/PCG/BP_ElectricDreamsSplineAssemblyTest`
- `/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`

## Graph Shape

Execution path:

`Get Owning Actor Spline -> Spline Sampler -> Copy Points -> Density Filter -> Transform/Merge -> Static Mesh Spawner`

The source assembly branch uses `Create Points Grid`. The spawner uses
`/Engine/BasicShapes/Cube` as a placeholder mesh.

`PCGApplyHierarchySettings` is present as an unconnected reference node named
`07 Apply Hierarchy Reference - disabled until hierarchy attrs exist`.

## Validation

- Cubeless Editor bridge: connected on `127.0.0.1:55557`.
- PCG graph compile/notify: success.
- Test BP compile/save: success.
- Temp level saved: success.
- Test actor generated `ISM_Cube_0` with `20` instances.
- Final log marker check after `MCP_PCG_FINAL_REGEN_NO_APPLY_HIERARCHY_EXEC_BEGIN`: no severity `LogPCG: Error`, `LogPython: Error`, `LogBlueprint: Error`, or `LogScript: Error`.

## Fixes During Execution

- The Electric Dreams graph input had an `Actor` pin, while the new Cubeless
  graph input only had `In`. The Cubeless version now uses `PCGGetSplineSettings`
  to get the owning actor spline before `Spline Sampler`.
- `PCGAttributeFilteringSettings` initially targeted an empty selector. It was
  corrected to the point property `Density`.
- `PCGStaticMeshSpawnerSettings` needed a `PCGMeshSelectorWeightedEntry`; it now
  uses a `PCGSoftISMComponentDescriptor` pointing at `/Engine/BasicShapes/Cube`.
- `PCGApplyHierarchySettings` produced `ActorIndex` accessor errors because the
  placeholder source points do not yet author hierarchy key attributes. The node
  was removed from the execution path and re-added as an unconnected reference
  node. The next pass should add explicit hierarchy attributes before reconnecting
  it.

## Next Pass

Build a real minimal hierarchy-copy subgraph equivalent to
`SG_CopyPointsWithHierarchy`:

- Add point key and parent key attributes.
- Add hierarchy depth and relative transform attributes.
- Reconnect `Apply Hierarchy`.
- Confirm generated instances still appear with zero PCG errors.
