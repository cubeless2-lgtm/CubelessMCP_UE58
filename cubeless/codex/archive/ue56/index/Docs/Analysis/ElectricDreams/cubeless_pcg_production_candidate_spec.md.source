# Cubeless PCG Production Candidate Spec

Date: 2026-06-09

## Purpose

Define the first production-candidate shape for the Electric Dreams based
Cubeless PCG work before creating any production assets.

This document is intentionally an approval-gate artifact. It records what can
be promoted, what should stay learning-only, and which Unreal asset operations
need explicit user approval before execution.

## Current Proven Inputs

The learning layer already has these verified pieces:

- Ecosystem selector:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/Blueprints/Authoring/BP_Cubeless_ED_PCGEcosystemSelector`
- Style/profile/amount graph families:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/DesignerStyleProfileMatrixCombos`
- Tree profile graph family:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/TreeProfilePresets`
- True material-applied graph families:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/TrueMaterialApplied/DesignerStyleProfileMatrixCombos`
  and
  `/Game/Cubeless/PCG/ElectricDreamsLearning/TrueMaterialApplied/TreeProfilePresets`
- Dynamic material preview graph:
  `/Game/Cubeless/PCG/ElectricDreamsLearning/DynamicMaterialPrototype/PCG_Cubeless_ED_DynamicMaterialAxis_ActorPropertySelector_Compat`
- Editor apply entry:
  `Level Editor > Cubeless > Cubeless ED : Apply PCG Selector`

Latest verified result:

- Blueprint compile warning/error count: `0`
- Ecosystem selector validation actors: `10`
- `ecosystem_selector_validation_pass=True`
- Latest verification marker `log_error_count=0`
- `GenerateMaterialPreview=False` case passed with material point/ISM output `0`

Production candidate implementation result:

- Candidate asset:
  `/Game/Cubeless/PCG/ProductionCandidates/Blueprints/BP_Cubeless_PCG_EcosystemCandidate`
- Production candidate validation actors: `12`
- `production_candidate_validation_pass=True`
- Latest production candidate verification marker `log_error_count=0`

Landscape note:

- The baseline validation does not require a Landscape because it verifies
  candidate control mapping, graph routing, generated output counts, material
  preview gating, and editor log cleanliness.
- A follow-up direct Landscape validation now passes against:
  `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`
- Direct Landscape validation result:
  `production_candidate_landscape_validation_pass=True`
- The editor apply route includes a production-candidate Landscape conform pass
  for generated ISM output, with a delayed Slate-tick pass for PCG output that
  appears after the initial apply call.
- Promotion into `RuntimeGrass`, `NewPCGGraph`, or a real production level is
  still a separate approval gate because it would touch production placement or
  runtime graph assets.

## Production Candidate Boundary

The first production candidate should be created under a new isolated package
root, not by overwriting existing runtime graphs:

`/Game/Cubeless/PCG/ProductionCandidates/`

Do not modify these without a separate approval:

- Original Electric Dreams graph assets
- `/Game/PCG/RuntimeGrass`
- `/Game/PCG/NewPCGGraph`
- Existing shipped or user-placed production PCG actors
- Non-exception C++ source

The learning assets should remain as the verified reference set.

## Candidate Actor Shape

Proposed Blueprint:

`/Game/Cubeless/PCG/ProductionCandidates/Blueprints/BP_Cubeless_PCG_EcosystemCandidate`

Recommended components:

- `Spline`
- `PCG_Style`
- `PCG_Tree`
- `PCG_MaterialPreview`

Recommended default behavior:

- `PCG_Style` active for style or combined presets.
- `PCG_Tree` active only when the preset enables trees.
- `PCG_MaterialPreview` exists for debug parity, but `DebugMaterialPreview`
  should default to `False` for production candidate actors.
- Actual true-material graph routing should be preferred when the selected
  style/tree domain has a verified true-material route.

## Candidate User-Facing Controls

The learning actor exposes 9 detailed axes. The production candidate should
compress that into a smaller designer-facing surface.

Recommended exposed controls:

- `PresetType`
  - `1 = MixedMeadowDefault`
  - `2 = DenseGroundFoliage`
  - `3 = RockySparse`
  - `4 = LightConiferEdge`
  - `5 = ClassicGrassFill`
- `DensityOverride`
  - `0 = UsePreset`
  - `1 = Sparse`
  - `2 = Normal`
  - `3 = Dense`
- `TreeOverride`
  - `0 = UsePreset`
  - `1 = Off`
  - `2 = Solo`
  - `3 = Sparse`
  - `4 = LightGrove`
- `MaterialMood`
  - `0 = UsePreset`
  - `1 = Default`
  - `2 = CoolOrDark`
  - `3 = WarmOrSoft`
- `DebugMaterialPreview`
  - default `False`

Keep advanced learning axes hidden or internal unless a later designer workflow
requires them:

- `EcosystemMode`
- `VisualStyleType`
- `ProfileMode`
- `GroundAmountType`
- `DitchAmountType`
- `TreeStyleType`
- `TreeAmountType`
- `MaterialDomainType`
- `MaterialVariantType`

## First Preset Mapping

These mappings are the recommended first pass. They are conservative because
they use already-verified learning graph routes.

| PresetType | Label | EcosystemMode | VisualStyleType | ProfileMode | GroundAmountType | DitchAmountType | TreeStyleType | TreeAmountType | MaterialDomainType | MaterialVariantType | DebugMaterialPreview |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 1 | MixedMeadowDefault | 3 | 3 | 3 | 2 | 1 | 1 | 1 | 1 | 2 | False |
| 2 | DenseGroundFoliage | 3 | 4 | 3 | 3 | 2 | 2 | 2 | 1 | 3 | False |
| 3 | RockySparse | 1 | 5 | 1 | 1 | 2 | 1 | 1 | 2 | 3 | False |
| 4 | LightConiferEdge | 3 | 1 | 3 | 2 | 1 | 3 | 3 | 3 | 3 | False |
| 5 | ClassicGrassFill | 1 | 1 | 1 | 3 | 2 | 1 | 1 | 1 | 1 | False |

Notes:

- `RockySparse` is style-only to avoid making rocks read like a full ecosystem.
- `LightConiferEdge` uses combined grass + mixed conifer for a clear edge
  prototype.
- `ClassicGrassFill` is the safest baseline fill preset and should be useful as
  a fallback when art direction is uncertain.

## Validation Plan

The production candidate needs a separate validator from the learning selector
validator.

Recommended validation level:

`/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_ProductionCandidate_MCP`

Recommended validation actors:

- One actor per `PresetType`.
- One actor with each override mode enabled:
  - density override
  - tree override off
  - tree override light grove
  - material mood cool/dark
  - material mood warm/soft
  - debug material preview enabled

Required checks:

- Blueprint compile warning/error count is `0`.
- Component graph paths match expected preset or override routing.
- Active style/tree point counts match the referenced learning graph outputs.
- Inactive components produce `0` generated points.
- `DebugMaterialPreview=False` produces material preview point/ISM output `0`.
- `DebugMaterialPreview=True` uses the dynamic material preview graph and
  produces expected disjoint material ISM rows when the mesh family is disjoint.
- Latest editor log after the verification marker has `Error:` count `0`.

## Regression Runner Addition

After the production candidate assets exist, add two steps to
`run_pcg_study_regression.py`:

- `production_candidate_prepare`
- `production_candidate_verify`

These steps should run after the learning ecosystem selector verification so
the learning reference remains the first diagnostic layer.

## Approval Gates

Stop and ask for explicit approval before:

- Creating `/Game/Cubeless/PCG/ProductionCandidates/` Unreal assets.
- Copying or promoting learning graphs into a production package.
- Changing `RuntimeGrass`, `NewPCGGraph`, or original Electric Dreams assets.
- Replacing any current level production PCG actor.
- Adding non-exception C++.
- Deleting or consolidating learning assets.
- Committing/pushing the production candidate after asset creation unless the
  user explicitly requests Git work.

## Tivret Instruction Draft

When approved, give Tivret this instruction:

Create a non-C++ production-candidate PCG actor under
`/Game/Cubeless/PCG/ProductionCandidates/Blueprints/` using the verified
learning ecosystem selector as the reference behavior. Do not modify
RuntimeGrass, NewPCGGraph, original Electric Dreams assets, or any existing
placed production PCG actor. Build `BP_Cubeless_PCG_EcosystemCandidate` with
`Spline`, `PCG_Style`, `PCG_Tree`, and `PCG_MaterialPreview` components.
Expose only `PresetType`, `DensityOverride`, `TreeOverride`, `MaterialMood`,
and `DebugMaterialPreview`, with `DebugMaterialPreview=False` by default.
Route the presets according to this spec, prefer true-material graph paths when
the selected domain has a verified true route, compile/save only after
Blueprint validation passes, then create prepare/verify scripts and validate all
five presets plus override cases in `_MCP_Temp`.

## Next No-Approval Work

The remaining work that can still be done before the asset-creation approval is
read-only or documentation-only:

- Build a read-only inventory table of all referenced learning graph paths.
- Draft the production candidate verifier pseudocode.
- Draft a regression runner patch plan.
- Prepare a user-facing decision sheet for the approval gate.

Actual Unreal asset creation is the next approval-gated step.
