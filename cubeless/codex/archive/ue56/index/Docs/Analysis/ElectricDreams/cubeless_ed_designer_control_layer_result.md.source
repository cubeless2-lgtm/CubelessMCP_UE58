# Cubeless ED Designer Control Layer Result

Date: 2026-06-09

## Scope

Created a designer-facing wrapper layer for the Electric Dreams PCG learning
assets without modifying the existing RuntimeGrass graph.

Production graph:

```text
/Game/Cubeless/PCG/ElectricDreamsLearning/PCG_Cubeless_ED_DesignerControlLayer
```

Wrapped production graphs:

```text
/Game/Cubeless/PCG/ElectricDreamsLearning/PCG_Cubeless_GroundControlsPrototype
/Game/Cubeless/PCG/ElectricDreamsLearning/PCG_Cubeless_DitchHierarchyPrototype
```

Validation actor:

```text
/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP
MCP_Cubeless_ED_DesignerControlLayer_Validation
```

## Graph Shape

The graph uses `PCGSubgraphSettings` nodes as stable profile entry points:

```text
Input
-> Profile 10 GroundControls Subgraph
-> DesignerProfileId = 10
-> DesignerProfileType = 1
-> DesignerControlLayerPass = true
-> Merge
-> Output
```

```text
Input
-> Profile 20 DitchHierarchy Subgraph
-> DesignerProfileId = 20
-> DesignerProfileType = 2
-> DesignerControlLayerPass = true
-> Merge
-> Output
```

Subgraph references:

```text
Profile 10 GroundControls Subgraph:
/Game/Cubeless/PCG/ElectricDreamsLearning/PCG_Cubeless_GroundControlsPrototype

Profile 20 DitchHierarchy Subgraph:
/Game/Cubeless/PCG/ElectricDreamsLearning/PCG_Cubeless_DitchHierarchyPrototype
```

The profile IDs are intentionally simple:

```text
10 = GroundControls
20 = DitchHierarchy
```

The profile types are also simple:

```text
1 = Ground
2 = Ditch
```

## Validation Result

Scripts:

```text
build_cubeless_ed_designer_control_layer.py
verify_cubeless_ed_designer_control_layer.py
```

Final verifier result:

```text
expected_total_points=50
point_count=50
expected_profile_counts={'GroundControls': 8, 'DitchHierarchy': 42}
profile_counts={'GroundControls': 8, 'DitchHierarchy': 42}
profile_types={'GroundControls': [1], 'DitchHierarchy': [2]}
designer_control_layer_pass_count=50
unknown_profile_count=0
type_mismatch_count=0
count_mismatch_count=0
subgraph_ref_mismatch_count=0
total_ism_instances=50
ISM rows:
  GroundControls = 8 instances of SM_Grass_Medium01
  DitchHierarchy = 42 instances of SM_Grass_Medium01
log_error_count=0
designer_control_layer_validation_pass=True
```

## RuntimeGrass Boundary

`/Game/Cubeless/PCG/RuntimeGrass/NewPCGGraph` was inspected but not modified.
The graph currently contains useful RuntimeGrass nodes, but some branches are
disconnected from the final output. Merging the Electric Dreams layer into it
now would risk changing behavior before the intended RuntimeGrass execution path
is cleaned up.

The safer next step is a dedicated integration pass:

```text
1. decide whether RuntimeGrass remains a landscape/grass graph only
2. decide whether ElectricDreamsLearning remains a separate spline/ditch graph
3. add a shared preset/profile convention if both graphs need to coexist
4. only then merge or reference one from the other
```

## Next Step

The next practical task is a stable designer actor or graph instance:

```text
BP_Cubeless_ED_PCGDesignerControlActor
```

It should expose the graph choice/profile controls in the Details panel and own
the validation spline/PCG component setup. That would turn the current graph
layer from a technical wrapper into something a level designer can place and
adjust directly.
