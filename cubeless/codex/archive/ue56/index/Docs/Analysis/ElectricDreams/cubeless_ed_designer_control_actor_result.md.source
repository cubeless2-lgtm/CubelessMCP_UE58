# Cubeless ED Designer Control Actor Result

Date: 2026-06-09

## Production asset

- Blueprint: `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/BP_Cubeless_ED_PCGDesignerControlActor`
- Class: `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/BP_Cubeless_ED_PCGDesignerControlActor.BP_Cubeless_ED_PCGDesignerControlActor_C`
- Default PCG graph:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/PCG_Cubeless_ED_DesignerControlLayer.PCG_Cubeless_ED_DesignerControlLayer`

## Build method

- Duplicated the validated temporary Spline + PCG actor Blueprint from
  `/Game/_MCP_Temp/PCG/BP_ElectricDreamsSplineAssemblyTest`.
- Updated the production Blueprint PCG component template's `PCGGraphInstance.graph`
  to the production DesignerControlLayer graph.
- Compiled and saved the Blueprint with `unreal.BlueprintEditorLibrary.compile_blueprint`.
- No C++ was added or modified.

## Validation

Validation actor:
`MCP_Cubeless_ED_PCGDesignerControlActor_Validation` in
`/Game/_MCP_Temp/PCG/LVL_ElectricDreams_SplineAssembly_MCP`.

Pass summary:

- `spline_component_count=1`
- `pcg_component_count=1`
- `pcg_template_graph_paths=['/Game/Cubeless/PCG/ElectricDreamsLearning/PCG_Cubeless_ED_DesignerControlLayer.PCG_Cubeless_ED_DesignerControlLayer']`
- `point_count=50`
- `profile_counts={'GroundControls': 8, 'DitchHierarchy': 42}`
- `profile_types={'GroundControls': [1], 'DitchHierarchy': [2]}`
- `designer_control_layer_pass_count=50`
- `unknown_profile_count=0`
- `type_mismatch_count=0`
- `total_ism_instances=50`
- `log_error_count=0`
- `designer_control_actor_validation_pass=True`

ISM rows:

- `ISM_SM_Grass_Medium01_0`: 8 instances
- `ISM_SM_Grass_Medium01_1`: 42 instances

## Verification note

PCG output verification must be run in a separate UnrealMCP call after the build
call. Running build and verification in the same Python execution can read the
PCGComponent before generated output is available and produce a false
`No generated point data found` failure.

## Current boundary

This Blueprint is a placeable designer control actor shell for the current
Electric Dreams learning graph assembly. It does not merge into or overwrite the
existing RuntimeGrass `NewPCGGraph`; that integration remains a separate,
riskier step.
