# Cubeless PCG Production Candidate Decision Sheet

Date: 2026-06-09

## Decision Needed

Approve or reject creating the first isolated production candidate package:

`/Game/Cubeless/PCG/ProductionCandidates/`

Decision status: approved and implemented on 2026-06-09.

This approval would allow creating new Unreal assets in that package only. It
would not approve touching `RuntimeGrass`, `NewPCGGraph`, original Electric
Dreams assets, existing placed production actors, or non-exception C++.

## Recommended Decision

Approve creating the isolated production candidate package.

Reason:

- The learning selector, true material routes, material preview toggle, and
  ecosystem validation already pass.
- The next useful evidence requires a real placeable candidate actor.
- Keeping it under `ProductionCandidates` avoids destructive promotion and
  keeps rollback simple.

## What Approval Enables

Tivret may create:

- `/Game/Cubeless/PCG/ProductionCandidates/Blueprints/BP_Cubeless_PCG_EcosystemCandidate`
- Disposable validation actors in:
  `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_ProductionCandidate_MCP`
- New MCP docs/scripts:
  - `prepare_cubeless_pcg_production_candidate_validation.py`
  - `verify_cubeless_pcg_production_candidate_blueprint.py`

Tivret may update:

- `Plugins/CustomTools/Content/Python/ArtScripts/CubelessEDPCG.py`
  only as needed to add an apply route for the new candidate actor.
- `run_pcg_study_regression.py`
  only after the candidate verifier exists and passes.

## What Approval Does Not Enable

Do not do these unless separately approved:

- Modify `/Game/PCG/RuntimeGrass`.
- Modify `/Game/PCG/NewPCGGraph`.
- Modify original Electric Dreams PCG assets.
- Replace current level production actors.
- Delete or consolidate learning assets.
- Add non-exception C++.
- Commit/push changes unless the user explicitly asks for Git work.

## Default Candidate Settings

Recommended defaults:

- `PresetType = 1 MixedMeadowDefault`
- `DensityOverride = 0 UsePreset`
- `TreeOverride = 0 UsePreset`
- `MaterialMood = 0 UsePreset`
- `DebugMaterialPreview = False`

These defaults are intentionally conservative. The actor should spawn an
inspectable ecosystem result without adding the separate material preview ISM
noise.

## Risk Summary

| Risk | Severity | Mitigation |
| --- | --- | --- |
| Production candidate feels too simplified vs learning actor | Medium | Keep learning axes internal and expose overrides only when needed |
| True material route and material preview overlap in ISM counts | Low | Keep `DebugMaterialPreview=False` by default and verify preview separately |
| PCG generation output is delayed in editor validation | Medium | Use prepare/verify split, same as ecosystem selector validation |
| Candidate accidentally mutates production graph assets | High | Write only under `ProductionCandidates`; do not touch runtime graph paths |
| User wants different art direction presets | Medium | Preset mapping is a first pass and can be changed without altering learning assets |
| Candidate needs validation in the final production level before runtime promotion | Medium | StaticMesh surface and disposable Landscape validation now pass; run a final level-specific check before touching runtime graph assets |

## Approval Phrase

The exact approval that would unblock implementation is:

`ProductionCandidates 패키지 생성 승인. RuntimeGrass/NewPCGGraph/원본 ED/C++는 건드리지 말고 후보 actor와 검증만 진행해.`

## If Not Approved

Continue documentation-only work:

- refine preset names and mappings
- prepare alternate preset tables
- review current learning graph inventory
- wait for a concrete target level or art-direction request
