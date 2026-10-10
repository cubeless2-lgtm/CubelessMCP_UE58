# StackOBot Sample Asset Manifest

This manifest records the local learning/sample assets under:

`<workspace-parent>/SampleProject/StackOBot/Content/_MCP_Sample/AnimStudy`

The StackOBot sample project is not a git repository. Treat these assets as local
learning artifacts. Do not stage them in `CubelessStylized`, and do not modify
original StackOBot assets unless the user explicitly approves that scope.

## Asset Groups

| Group | Asset count | Purpose |
| --- | ---: | --- |
| Bot Post Process AnimBP | 5 | Late ModifyBone samples for head/antenna offsets |
| Bot Post Process SkeletalMesh | 5 | Duplicated Bot meshes with sample Post Process AnimBP assignment |
| Bot Trail | 3 | Post Process Trail sample AnimBP, mesh, and actor template |
| Bot ControlRig | 3 | Forced-driver and input/default samples for `CR_Bot_Correction` |
| Baddy RigidBody | 10 | RigidBody study AnimBPs, actor templates, duplicated mesh, and PhysicsAsset |
| BlendSpace | 2 | `BS_Bot_WalkRunLean` lean response sample variants |
| Other sample actors | 3 | Supporting sample actor templates |

## Package Manifest

Post Process ModifyBone:

- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study`
- `/Game/_MCP_Sample/AnimStudy/SKM_Bot_PostProcess_Study`
- `/Game/_MCP_Sample/AnimStudy/BP_Bot_PostProcess_StudyActor`
- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadPitch`
- `/Game/_MCP_Sample/AnimStudy/SKM_Bot_PostProcess_Study_HeadPitch`
- `/Game/_MCP_Sample/AnimStudy/BP_Bot_PostProcess_StudyActor_HeadPitch`
- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_AntennaRoll`
- `/Game/_MCP_Sample/AnimStudy/SKM_Bot_PostProcess_Study_AntennaRoll`
- `/Game/_MCP_Sample/AnimStudy/BP_Bot_PostProcess_StudyActor_AntennaRoll`
- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadYawAuthoringPattern`
- `/Game/_MCP_Sample/AnimStudy/SKM_Bot_PostProcess_Study_HeadYawAuthoringPattern`
- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadYawPlus5Study`
- `/Game/_MCP_Sample/AnimStudy/SKM_Bot_PostProcess_Study_HeadYawPlus5Study`

Trail secondary motion:

- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study`
- `/Game/_MCP_Sample/AnimStudy/SKM_Bot_Trail_Study`
- `/Game/_MCP_Sample/AnimStudy/BP_Bot_Trail_StudyActor`

ControlRig:

- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ModifyCurve_Study`
- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_InputDefaults_Study`
- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ForcedDriver_Study`

Baddy RigidBody:

- `/Game/_MCP_Sample/AnimStudy/PA_Baddy_RigidBody_Study`
- `/Game/_MCP_Sample/AnimStudy/SKM_Baddy_RigidBody_Study`
- `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study`
- `/Game/_MCP_Sample/AnimStudy/BP_Baddy_RigidBody_StudyActor`
- `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study_AlphaHalf`
- `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study_ForceZ`
- `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study_WorldSpace`
- `/Game/_MCP_Sample/AnimStudy/BP_Baddy_RigidBody_StudyActor_AlphaHalf`
- `/Game/_MCP_Sample/AnimStudy/BP_Baddy_RigidBody_StudyActor_ForceZ`
- `/Game/_MCP_Sample/AnimStudy/BP_Baddy_RigidBody_StudyActor_WorldSpace`

BlendSpace:

- `/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanWideStudy`
- `/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanTemplateRehearsal`

## Regeneration Routes

| Asset family | Preferred regeneration route |
| --- | --- |
| Post Process ModifyBone | `ensure_postprocess_anim_demo_variant` |
| Bot Trail | `ensure_anim_graph_trail_demo` |
| Bot ControlRig forced driver | `ensure_controlrig_forced_driver_animbp` |
| Baddy RigidBody variants | duplicate study assets, then `set_anim_graph_rigidbody_settings` |
| BlendSpace variants | `ensure_blendspace_sample_variant` |

## Route Token Sample Target Map

Use this table when a request route names a sample target. Concrete `_MCP_Sample`
targets must appear in the package manifest above; no-sample routes remain
explicit boundaries until a concrete request justifies new sample authoring.

| Route token | Manifest status | Sample target boundary |
| --- | --- | --- |
| `Post Process ModifyBone` | listed | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadYawPlus5Study` |
| `BlendSpace sample variant` | listed | `/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanWideStudy` |
| `Bot Trail sample` | listed | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study` |
| `UpperBody Slot and LayeredBlend` | no stable sample yet | none for route proof; future sample overlay only if action source is required |
| `protected metadata boundary` | no sample asset | none until a guarded native API is approved/implemented |
| `ControlRig gate probe` | listed | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ForcedDriver_Study` |
| `state-machine runtime-driver proof` | no stable sample yet | none for first pass; future sample graph only if runtime-driver proof is insufficient |
| `Baddy RigidBody` | listed | `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study` |
| `node resolver plus same-instance pre/post proof` | no stable sample by default | none unless a controlled sample actor is needed for runtime proof |

## Evidence Root

Primary evidence and runtime artifacts live under:

`<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy`

Use the newest `*TemplateRehearsal*`, `*PoseWatch*`, `*RigidBody*`,
`*Trail*`, `*BlendSpace*`, and `*ControlRig*` files as proof references.

## Safety Notes

- `_MCP_Sample` assets are local study assets. They are not production StackOBot
  assets.
- `_MCP_Temp` assets are disposable validation artifacts and should not be used
  as stable references.
- If `docs/stackobot-animation-route-matrix.md` or
  `docs/stackobot-animation-request-run-examples.md` names a concrete
  `/Game/_MCP_Sample/AnimStudy/...` target, list that package here as a known
  local sample asset.
- When a new sample asset is created or regenerated for a real request, update
  this manifest and the matching `docs/work-log.md` entry in the same docs
  change.
- If a sample asset is missing, regenerate it with the route above instead of
  editing original StackOBot assets.
- If regeneration requires a command that is not exposed in the current tool
  list, treat it as an UnrealMCP plugin command-surface sync issue before adding
  new C++.
