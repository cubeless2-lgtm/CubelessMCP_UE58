# Electric Dreams PCG Learning Plan

Generated from a live UnrealMCP read of `ElectricDreamsEnv`.
No Unreal assets were modified during this analysis.

## Working Locations

- Reference project: `D:\Git\SampleProject\ElectricDreamsEnv`
- Real implementation project: `D:\Git\CubelessStylized`
- MCP bridge/tooling repo: `D:\Git\unreal-mcp-cubeless`
- Shared UnrealMCP plugin repo: `D:\Git\UnrealMCPPlugin`

Use Electric Dreams as a read-only reference. Build actual Cubeless PCG graphs in
`CubelessStylized`, starting under `/Game/_MCP_Temp/PCG/` until the pattern is
validated. Modify `unreal-mcp-cubeless` or `UnrealMCPPlugin` only when the bridge
or PCG tooling itself needs a fix.

## Current Reference Index

- Asset index: `electric_dreams_pcg_asset_index.json`
- Graph summaries: `electric_dreams_pcg_graph_summaries.json`

Representative graph sizes from the live read:

- `SplineExampleGraph`: 22 nodes, 6 PCGBlueprintSettings refs, 1 subgraph ref.
- `SG_CopyPointsWithHierarchy`: 6 nodes, 1 PCGBlueprintSettings ref.
- `DistanceToNeighbors`: 5 nodes.
- `DiscardPointsInBumpyAreas`: 52 nodes.
- `PCGDemo_Ditch`: 413 nodes, 32 PCGBlueprintSettings refs, 21 subgraph refs.
- `PCGDemo_Forest`: 412 nodes, 12 PCGBlueprintSettings refs, 8 subgraph refs.
- `PCGDemo_Ground`: 237 nodes, 8 PCGBlueprintSettings refs, 2 subgraph refs.

## Learning Order

1. `SplineExampleGraph`
   Learn the smallest full pipeline: actor spline input, spline sampling, assembly
   point output, hierarchy copy, filtering, transform, merge, apply hierarchy, and
   static mesh spawning.

2. `SG_CopyPointsWithHierarchy`
   Learn the reusable copy/hierarchy subgraph before touching larger graphs.
   Ditch, Ground, and SplineExample all depend on this pattern.

3. `DistanceToNeighbors`
   Learn the tiny spatial-distance helper used by forest filtering.

4. `DiscardPointsInBumpyAreas`
   Learn the bumpy-area rejection logic: bounds, metadata math, vector assembly,
   unions, and point transforms.

5. `PCGDemo_Ditch`
   Learn spline-guided river/ditch assembly placement. This is the first real
   production-scale target after the small sample.

6. `PCGDemo_Forest`
   Learn biome layering: landscape/surface sampling, density filters, projection,
   difference, attribute noise, data/settings import, and mesh spawning.

7. `PCGDemo_Ground`
   Learn the advanced ground/embankment system after Ditch and Forest.

## First Cubeless Target

Create a disposable Cubeless validation graph:

`/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`

Goal: reproduce the structure of Electric Dreams `SplineExampleGraph` with
Cubeless-safe placeholder assets, not Electric Dreams content.

Minimum behavior:

- Input actor spline drives a `Spline Sampler`.
- A source point/assembly branch is copied onto spline target points.
- The copied points preserve enough hierarchy attributes to survive an
  `Apply Hierarchy` style step.
- One branch is filtered, transformed, merged back, then spawned by
  `Static Mesh Spawner`.
- Validation must compile the PCG graph and show visible generated instances in a
  temporary test area.

## Success Criteria

- The temporary Cubeless graph compiles without PCG errors.
- A simple spline actor can generate visible placeholder meshes.
- The graph keeps source, target, filter, transform, merge, hierarchy, and spawn
  stages visually separable.
- The result is disposable under `/Game/_MCP_Temp/` and is not staged in Git unless
  explicitly requested.
- If the UnrealMCP bridge lacks a needed PCG creation/editing operation, fix the
  bridge/tooling first in the shared UnrealMCP plugin repo, then resync both
  projects.

## 티브렛에게 전달할 지시

Cubeless Editor로 전환한 뒤, Electric Dreams Editor는 참조용으로만 사용해.
`/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP`에 임시 PCG 그래프를 만들고
Electric Dreams `SplineExampleGraph`의 최소 구조를 Cubeless placeholder mesh로 재현해.

실행 순서:

1. Cubeless의 UnrealMCP 브릿지 `127.0.0.1:55557` 연결을 확인한다.
2. `/Game/_MCP_Temp/PCG/` 아래에 임시 PCG 그래프와 필요한 임시 테스트 액터/볼륨을 만든다.
3. 그래프는 `Input Actor/Spline -> Spline Sampler -> Target Points`와
   `Source Points -> Copy Points/Hierarchy -> Filter -> Transform -> Merge -> Apply Hierarchy -> Static Mesh Spawner`
   흐름으로 구성한다.
4. Electric Dreams 전용 assembly asset은 복사하지 말고 Cubeless의 기본 StaticMesh 또는 엔진 기본 mesh를 사용한다.
5. 그래프 compile/notify를 실행하고, compile error가 있으면 바로 수정한다.
6. 생성 결과가 보이는지 테스트 레벨 또는 임시 영역에서 확인한다.
7. 생성 산출물은 `/Game/_MCP_Temp/` 밖으로 옮기지 말고, Git에도 올리지 않는다.

Rollback boundary: `/Game/_MCP_Temp/PCG/ElectricDreams_SplineAssembly_MCP` 및 같은
작업에서 만든 임시 `_MCP_Temp` 산출물만 삭제 대상으로 본다.
