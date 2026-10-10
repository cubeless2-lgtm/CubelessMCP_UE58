# StackOBot Animation Study Notes

Authoring-pattern guide for future build requests:
`docs/stackobot-animbp-authoring-patterns.md`.

Request execution playbook:
`docs/stackobot-animation-request-playbook.md`.

이 문서는 `<workspace-parent>/SampleProject/StackOBot`을 애니메이션 학습용으로 볼 때의 기준 노트다. 원본 StackOBot 애셋은 읽기 전용으로 두고, 실험은 `/Game/_MCP_Sample/AnimStudy/` 아래 disposable 샘플에서 진행한다.

## 학습 순서

1. `ABP_Bot` 전체 구조를 먼저 본다.
   - 목적: locomotion state machine, cached pose, slot, layered blend, Control Rig까지 큰 흐름을 잡는다.
   - 핵심 질문: EventGraph 변수가 어떤 graph branch를 바꾸는가.

2. Post Process AnimBP는 샘플로 분리해서 본다.
   - 원본 `SKM_Bot`에는 Post Process AnimBP가 없다.
   - 학습용 복제 메쉬 `SKM_Bot_PostProcess_Study`만 `ABP_Bot_PostProcess_Study`를 참조한다.
   - 현재 샘플 체인: `LinkedInputPose -> LocalToComponentSpace -> Transform (Modify) Bone(head) -> ComponentToLocalSpace -> Root`.

3. 물리 애니메이션은 `ABP_Baddy`를 기준으로 본다.
   - `ABP_Baddy`는 실제 active path에 `RigidBody`가 들어간 가장 작은 예제다.
   - `ABP_Bot`의 `Trail controller`는 설정은 있지만 현재 `AnimGraph` 연결이 없다. 학습 노트에는 "설정이 남은 미연결 예제"로 분류한다.

4. Bot의 후반 보정은 Control Rig 쪽으로 본다.
   - `ABP_Bot` 최종 출력 직전 활성 노드는 `ControlRig`.
   - Control Rig asset은 `/Game/StackOBot/Characters/Bot/Rig/CR_Bot_Correction.CR_Bot_Correction_C`.
   - 입력은 `InteractionWorldLocation`과 `ShouldDoIKTrace`가 핵심이다.

## 주요 애셋

| 목적 | 애셋 |
| --- | --- |
| Bot 메인 AnimBP | `/Game/StackOBot/Characters/Bot/ABP_Bot` |
| Bot skeleton | `/Game/StackOBot/Characters/Bot/Mesh/SK_Bot` |
| Bot original skeletal mesh | `/Game/StackOBot/Characters/Bot/Mesh/SKM_Bot` |
| Bot study skeletal mesh | `/Game/_MCP_Sample/AnimStudy/SKM_Bot_PostProcess_Study` |
| Bot study Post Process AnimBP | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study` |
| Bot study actor | `/Game/_MCP_Sample/AnimStudy/BP_Bot_PostProcess_StudyActor` |
| Bot Control Rig | `/Game/StackOBot/Characters/Bot/Rig/CR_Bot_Correction` |
| Baddy AnimBP | `/Game/StackOBot/Characters/Blobling/Anim/ABP_Baddy` |
| Baddy physics asset | `/Game/StackOBot/Characters/Blobling/PA_Baddy` |

## ABP_Bot 핵심 흐름

`ABP_Bot`은 큰 예제다. 처음 볼 때는 노드를 하나씩 따라가기보다 pose cache 단위로 끊어야 한다.

Main AnimGraph 흐름:

```text
GroundLocomotion -> SaveCachedPose GroundLocoPose
AirLocomotion -> SaveCachedPose LocomotionPose
LocomotionPose -> UpperBody slot -> SaveCachedPose CashedPose_UpperBody
LocomotionPose + CashedPose_UpperBody -> LayeredBoneBlend -> SaveCachedPose FullBodyPose
IsInactive ? A_Bot_Idle_Inactive : FullBodyPose -> ControlRig -> Root
```

EventGraph에서 확인한 주요 변수:

- `IsInAir?`: movement component의 `Is Falling`.
- `GroundSpeed`: horizontal velocity length.
- `LeanAmount`: `CalcLean`에서 actor yaw delta를 `FInterp To`로 완화한 값.
- `MovementInput?`: last movement input vector length.
- `JetpackActive`, `IsInactive`, `InteractionWorldLocation`: `BP_Bot`에서 끌어온 값.

Control Rig 활성 경로:

- `ControlRigClass`: `/Game/StackOBot/Characters/Bot/Rig/CR_Bot_Correction.CR_Bot_Correction_C`
- `Source`: inactive/full-body pose blend 결과.
- `Pose`: AnimGraph root로 연결.
- `InteractionWorldLocation`: AnimBP 변수 `InteractWorldLocation`에서 입력.
- `ShouldDoIKTrace`: `NOT IsInAir?`.
- `Alpha`: `1.0`, `bExecute=true`, input/output pose transfer enabled.

Control Rig audit artifacts:

| Purpose | Path |
| --- | --- |
| ControlRig AnimGraph node inspect JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_ControlRigNodeInspect.json` |
| ControlRig asset inventory JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/CR_Bot_Correction_Inventory.json` |
| ControlRig compact summary JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_ControlRigAuditSummary.json` |
| ControlRig summary Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_ControlRigAuditSummary.md` |

Latest Control Rig conclusion:

- The `AnimGraphNode_ControlRig` is active in the root-connected chain and feeds the AnimGraph root.
- `ControlRigClass` is `/Game/StackOBot/Characters/Bot/Rig/CR_Bot_Correction.CR_Bot_Correction_C`.
- `Source` is linked from `AnimGraphNode_BlendListByBool_0.Pose`.
- `Pose` is linked to `AnimGraphNode_Root_5.Result`.
- `InteractionWorldLocation` is linked from AnimBP variable `InteractWorldLocation`.
- `ShouldDoIKTrace` is linked from a function return value.
- `Alpha=1`, `bExecute=true`, `bTransferInputPose=true`, and `bTransferInputCurves=true`.
- The Control Rig hierarchy inventory is readable and reports `64` keys: `54` bones, `4` controls, and `6` curves.

## Post Process AnimBP 샘플

StackOBot 원본에는 Post Process AnimBP가 할당되어 있지 않다. 학습용 샘플은 원본을 건드리지 않기 위해 `_MCP_Sample` 아래에 만들었다.

현재 샘플:

- `ABP_Bot_PostProcess_Study`: `SK_Bot` skeleton을 사용.
- `SKM_Bot_PostProcess_Study`: 원본 `SKM_Bot` 복제본.
- `BP_Bot_PostProcess_StudyActor`: 복제 메쉬와 원본 `ABP_Bot_C`를 붙인 테스트 actor.

Modify Bone 데모:

```text
LinkedInputPose
  -> LocalToComponentSpace
  -> Transform (Modify) Bone
      Bone: head
      Rotation Mode: Additive
      Rotation Space: Bone Space
      Rotation: Pitch 0, Yaw 0, Roll 4
  -> ComponentToLocalSpace
  -> Root
```

Post Process verification artifacts:

| Purpose | Path |
| --- | --- |
| Inventory JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_PostProcess_StudyInventory.json` |
| AnimGraph inspect JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_PostProcess_StudyAnimGraphInspect.json` |
| Summary Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_PostProcess_StudySummary.md` |
| Runtime head SIE JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_PostProcess_HeadSIESamples.json` |
| Runtime head SIE CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_PostProcess_HeadSIESummary.csv` |
| Runtime head SIE chart | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_PostProcess_HeadRotationDelta.png` |

Latest verification:

- Original `SKM_Bot` still has no Post Process AnimBP assignment.
- Study mesh `SKM_Bot_PostProcess_Study` points to `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study.ABP_Bot_PostProcess_Study_C`.
- `ABP_Bot_PostProcess_Study` compile validation passed with `compile_error_count=0`, `compile_warning_count=0`, `validation_pass=true`, and `dirty_after_compile=false`.
- `BP_Bot_PostProcess_StudyActor` compile validation passed with `compile_error_count=0`, `compile_warning_count=0`, `validation_pass=true`, and `dirty_after_compile=false`.
- Direct AnimGraph inspect reports `5` nodes: `LinkedInputPose`, `LocalToComponentSpace`, `Transform (Modify) Bone`, `ComponentToLocalSpace`, and `Root`.
- The Modify Bone node targets `head`, uses additive bone-space rotation, and currently applies `Roll=4`.

Runtime Post Process test:

- Map: `/Game/_MCP_Temp/AnimStudy/M_Bot_PostProcess_Compare_MCP`
- Baseline actor: original `SKM_Bot` with original `ABP_Bot_C`
- Study actor: `SKM_Bot_PostProcess_Study` with original `ABP_Bot_C` and study Post Process AnimBP on the duplicated mesh
- PIE/SIE GameMode: `/Script/Engine.GameModeBase`
- Result: `pelvis` and `neck_01` rotation delta stayed `0.0`, while `head`, `antenna_04_l`, and `antenna_04_r` all showed exactly `4.0` degrees of runtime rotation delta versus baseline.
- Interpretation: the Post Process AnimBP runs after the main AnimBP pose, applies the `head` Modify Bone, and propagates that transform to child antenna bones without changing parent/control bones.

중요한 점:

- `Transform (Modify) Bone`은 실제 skeleton bone이 필요하다.
- `ABP_Bot`의 Trail 노드가 보여주는 `VB VBHead`는 Modify Bone 검증에서는 유효한 skeleton bone이 아니었다.
- Bot skeleton의 실제 본 중 머리는 `head`다.

## Post Process Variant Samples

Additional compiled sample variants were created under `/Game/_MCP_Sample/AnimStudy` only. Original StackOBot assets were not edited.

| Variant | AnimBP | Skeletal Mesh | Actor | Bone | Additive rotation |
| --- | --- | --- | --- | --- | --- |
| `HeadPitch` | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadPitch` | `/Game/_MCP_Sample/AnimStudy/SKM_Bot_PostProcess_Study_HeadPitch` | `/Game/_MCP_Sample/AnimStudy/BP_Bot_PostProcess_StudyActor_HeadPitch` | `head` | `Pitch=6, Yaw=0, Roll=0` |
| `AntennaRoll` | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_AntennaRoll` | `/Game/_MCP_Sample/AnimStudy/SKM_Bot_PostProcess_Study_AntennaRoll` | `/Game/_MCP_Sample/AnimStudy/BP_Bot_PostProcess_StudyActor_AntennaRoll` | `antenna_04_l` | `Pitch=0, Yaw=0, Roll=12` |

Variant artifacts:

| Purpose | Path |
| --- | --- |
| Variant setup JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantSetup.json` |
| Mesh link JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantMeshLink.json` |
| Component template probe JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_ComponentTemplateProbe.json` |
| Variant summary JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantSummary.json` |
| Variant summary Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantSummary.md` |

Variant verification:

- Both variant AnimBPs compile and save with `compile_error_count=0` and `compile_warning_count=0`.
- Both variant actor Blueprints compile and save with `compile_error_count=0` and `compile_warning_count=0`.
- Both duplicated skeletal meshes point at their matching variant Post Process AnimBP generated class.
- Both variant actors use their matching duplicated skeletal mesh and the original main `/Game/StackOBot/Characters/Bot/ABP_Bot.ABP_Bot_C`.
- Original `/Game/StackOBot/Characters/Bot/Mesh/SKM_Bot` still has no Post Process AnimBP assignment.
- Dirty content package count after setup was `0`.
- Runtime SIE same-instance PoseWatch sampling is now complete for both variants through `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process)`.

Additional authoring-pattern smoke:

- Created sample-only `HeadYawAuthoringPattern` through `ensure_postprocess_anim_demo_variant`.
- Target AnimBP: `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadYawAuthoringPattern`.
- Target SkeletalMesh: `/Game/_MCP_Sample/AnimStudy/SKM_Bot_PostProcess_Study_HeadYawAuthoringPattern`.
- Modify Bone settings: `Bone=head`, additive bone-space rotation `Pitch=0`, `Yaw=8`, `Roll=0`.
- The command created both target assets, compiled the AnimBP successfully, saved the AnimBP and SkeletalMesh, assigned the SkeletalMesh Post Process AnimBlueprint to `ABP_Bot_PostProcess_Study_HeadYawAuthoringPattern_C`, and reported `original_assets_modified=false`.
- A follow-up editor restart loaded both assets and confirmed `dirty_package_count=0`.
- Runtime PoseWatch is now complete through a safer no-SIE editor-world transient actor route. The first generic `execute_python` SIE setup attempt crashed the hidden editor with `EXCEPTION_INT_DIVIDE_BY_ZERO`, so that route remains unsafe, but it was not needed for this Post Process proof.
- `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process, prefer_pie_world=false)` resolved the ModifyBone output link `3` and input `ComponentPose` link `2` in the same `ABP_Bot_PostProcess_Study_HeadYawAuthoringPattern_C` Post Process instance.
- Result: `runtime_graph_prepost=true`, `same_instance_prepost=true`, `same_anim_instance_node_mapping=true`, `runtime_node_instance_mapped=true`, `find_debug_anim_node_mapped=true`, `transient_pose_watches=true`, `debug_object_restored=true`, and `original_assets_modified=false`.
- Deltas: `head` rotates about `8.0 deg`; descendant `antenna_04_l` moves about `11.737 cm` and rotates about `8.0 deg`; `antenna_04_r` moves about `11.746 cm` and rotates about `8.0 deg`; `pelvis` and `neck_01` stay at floating-point noise.
- Artifacts:
  - `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_HeadYawAuthoringPatternPoseWatchPrePost_raw.json`
  - `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_HeadYawAuthoringPatternPoseWatchPrePost_Summary.json`

Variant impact map:

| Purpose | Path |
| --- | --- |
| Impact map JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantImpactMap.json` |
| Impact map CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantImpactMap.csv` |
| Impact map Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantImpactMap.md` |
| Impact map SVG | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantImpactMap.svg` |

Impact interpretation:

- `HeadPitch` affects `head` directly by about `6.0 deg` and moves descendant antenna leaf bones about `8.6 cm` in live same-instance sampling.
- `AntennaRoll` targets `antenna_04_l`; live same-instance sampling reports about `12.0 deg` on that leaf while `head`, `neck_01`, `antenna_03_l`, and `antenna_04_r` remain within floating-point noise.

## ABP_Baddy RigidBody

`ABP_Baddy`는 물리 애니메이션을 공부하기 좋은 작은 예제다.

AnimGraph 활성 체인:

```text
New State Machine
  -> LocalToComponentSpace
  -> RigidBody
  -> ComponentToLocalSpace
  -> DefaultSlot
  -> Root
```

State machine:

- Entry -> `A_Baddy_Idle`
- `A_Baddy_Idle` -> `A_Baddy_Walk`: `Is Moving`
- `A_Baddy_Walk` -> `A_Baddy_Idle`: `NOT Is Moving`
- `A_Baddy_Idle`: `A_Baddy_Idle` sequence player.
- `A_Baddy_Walk`: `A_Baddy_Walk` sequence player -> `DefaultSlot`.

EventGraph:

```text
Blueprint Initialize Animation
  -> Try Get Pawn Owner
  -> Set NewVar

Blueprint Update Animation
  -> IsValid(Try Get Pawn Owner)
  -> Get Movement Component
  -> Get Velocity
  -> Vector Length
  -> float > 5.0
  -> Set Is Moving
```

RigidBody settings:

- `OverridePhysicsAsset`: null
- `bDefaultToSkeletalMeshPhysicsAsset`: true
- `SimulationSpace`: `ComponentSpace`
- `Alpha`: `1.0`

Trail inactive proof artifacts:

| Purpose | Path |
| --- | --- |
| Full ABP_Bot AnimGraph inspect JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_AnimGraphInspect.json` |
| Trail Controller inspect JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_TrailControllerInspect.json` |
| Trail inactive summary Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_TrailControllerInactiveSummary.md` |

Latest Trail conclusion:

- `ABP_Bot` AnimGraph inspect reports `30` nodes.
- The active root-connected chain is `Root -> ControlRig -> BlendListByBool`; the Trail Controller is not in that chain.
- The Trail Controller node exists as `AnimGraphNode_Trail` with title `Trail controller / Bone: VB VBHead`.
- Trail settings are retained: `TrailBone=VB VBHead`, `BaseJoint=head`, `ChainLength=2`, `ChainBoneAxis=X`, `Alpha=1`.
- `ComponentPose` input links are empty and `Pose` output links are empty.
- Treat this node as retained settings/reference data, not an active final-pose physics animation path.
- A safe active Trail experiment should be a separate `_MCP_Sample` AnimBP with a narrow MCP command for a validated Trail chain; do not wire this original node directly.
- `ExternalForce`: `(0, 0, 0)`
- `ComponentLinearAccScale`: `(0, 0, 0)`
- `ComponentLinearVelScale`: `(0, 0, 0)`
- `ComponentAppliedLinearAccClamp`: `(10000, 10000, 10000)`
- `bEnableWorldGeometry`: false
- `bOverrideWorldGravity`: false
- `EvaluationResetTime`: `0.01`
- `WorldSpaceMinimumScale`: `0.01`
- `LODThreshold`: `-1`

Baddy PhysicsAsset:

- `PA_Baddy` solver type: `WORLD`
- solver settings: `position_iterations=6`, `velocity_iterations=1`, `projection_iterations=1`
- `cull_distance=3.0`
- `use_linear_joint_solver=true`
- `use_manifolds=true`
- exposed constraint count: `8`

## Baddy RigidBody Study Samples

The active Baddy RigidBody setup has been copied into `_MCP_Sample` so experiments can be compiled and compared without touching original StackOBot assets.

Baseline sample assets:

| Purpose | Asset |
| --- | --- |
| Study AnimBP baseline | `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study` |
| Study skeletal mesh | `/Game/_MCP_Sample/AnimStudy/SKM_Baddy_RigidBody_Study` |
| Study physics asset | `/Game/_MCP_Sample/AnimStudy/PA_Baddy_RigidBody_Study` |
| Study actor baseline | `/Game/_MCP_Sample/AnimStudy/BP_Baddy_RigidBody_StudyActor` |

The study mesh uses the study physics asset, while the original `SKM_Baddy` still uses the original `PA_Baddy`.

Comparison AnimBP variants:

| Variant | AnimBP | RigidBody change |
| --- | --- | --- |
| Baseline | `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study` | `Alpha=1.0`, `ExternalForce=(0,0,0)`, `SimulationSpace=ComponentSpace` |
| AlphaHalf | `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study_AlphaHalf` | `Alpha=0.5` |
| ForceZ | `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study_ForceZ` | `ExternalForce=(0,0,350)` |
| WorldSpace | `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study_WorldSpace` | `SimulationSpace=WorldSpace` |

Comparison actor variants:

| Variant | Actor | AnimClass |
| --- | --- | --- |
| Baseline | `/Game/_MCP_Sample/AnimStudy/BP_Baddy_RigidBody_StudyActor` | `ABP_Baddy_RigidBody_Study_C` |
| AlphaHalf | `/Game/_MCP_Sample/AnimStudy/BP_Baddy_RigidBody_StudyActor_AlphaHalf` | `ABP_Baddy_RigidBody_Study_AlphaHalf_C` |
| ForceZ | `/Game/_MCP_Sample/AnimStudy/BP_Baddy_RigidBody_StudyActor_ForceZ` | `ABP_Baddy_RigidBody_Study_ForceZ_C` |
| WorldSpace | `/Game/_MCP_Sample/AnimStudy/BP_Baddy_RigidBody_StudyActor_WorldSpace` | `ABP_Baddy_RigidBody_Study_WorldSpace_C` |

Temporary comparison map:

| Purpose | Path |
| --- | --- |
| RigidBody comparison preview map | `/Game/_MCP_Temp/AnimStudy/M_Baddy_RigidBody_Compare_MCP` |
| Blueprint spawn smoke-test map | `/Game/_MCP_Temp/AnimStudy/M_BPSpawn_PathSmoke_Fixed_MCP` |
| Review screenshot | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Baddy_RigidBody_Compare_Annotated.png` |

The preview map was created with the native UnrealMCP `safe_new_preview_map` command, not with generic Python map creation APIs. It contains four `SkeletalMeshActor` preview instances using the duplicated study skeletal mesh and the four study AnimBP classes. The actors are named:

- `MCP_RigidBodyCompare_Baseline`
- `MCP_RigidBodyCompare_AlphaHalf`
- `MCP_RigidBodyCompare_ForceZ`
- `MCP_RigidBodyCompare_WorldSpace`

The original Blueprint actor variants were also repaired as reusable asset templates after the first direct-spawn attempt exposed an old `spawn_blueprint_actor` path limitation and null SkeletalMeshComponent defaults. UnrealMCP now accepts full Blueprint package/object/class paths for `spawn_blueprint_actor`, and the study actor templates were updated through the narrow `set_skeletal_mesh_component_anim_defaults` command rather than generic component property writes.

Blueprint actor smoke verification:

- Map: `/Game/_MCP_Temp/AnimStudy/M_BPSpawn_PathSmoke_Fixed_MCP`
- Spawn path: full Blueprint object paths under `/Game/_MCP_Sample/AnimStudy/`
- Actors: `MCP_BPSpawn_Baseline`, `MCP_BPSpawn_AlphaHalf`, `MCP_BPSpawn_ForceZ`, `MCP_BPSpawn_WorldSpace`
- Result: all four actors had nonzero render bounds, the duplicated study skeletal mesh, `ANIMATION_BLUEPRINT` mode, valid animation instances, and no dirty packages after save.

Runtime sampling artifacts:

| Purpose | Path |
| --- | --- |
| Editor-tick sample JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Baddy_RigidBody_EditorTickSamples.json` |
| SIE runtime sample JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Baddy_RigidBody_SIESamples.json` |
| Clean GameMode SIE sample JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Baddy_RigidBody_SIECleanGameModeSamples.json` |
| Clean GameMode metrics CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Baddy_RigidBody_SIECleanGameModeMetrics.csv` |
| Clean GameMode summary Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Baddy_RigidBody_SIECleanGameModeSummary.md` |
| Clean GameMode stalk delta chart | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Baddy_RigidBody_SIECleanGameMode_StalkMaxDelta.png` |

Runtime sampling notes:

- Editor-tick sampling used `SkeletalMeshComponent.set_update_animation_in_editor(True)`, sinusoidal actor movement, and actor-relative socket sampling. It produced `32` samples, but all sampled relative bone ranges were `0.0`; for this AnimBP/RigidBody setup, editor tick alone is not enough to observe physics animation.
- SIE sampling produced `40` samples over `5` seconds using the same four Blueprint actors in `/Game/_MCP_Temp/AnimStudy/M_BPSpawn_PathSmoke_Fixed_MCP`. Socket positions were sampled for `Head_02`, `R_Stalk_04`, `L_Stalk_04`, `TailEnd`, `L_Foot`, and `R_Foot`.
- `Is Moving` exists on the AnimBP instance but cannot be edited directly on instances through Python. The useful runtime driver for this pass was actor movement in SIE, not direct variable editing.
- Head, tail, and feet stayed effectively the same across variants in this short movement sample. The useful differences were on the flexible stalk bones.
- The first SIE pass logged `GM_InGame` spawn-pad errors because the minimal `_MCP_Temp` map did not contain the normal gameplay `ActiveSpawnPad` setup. The map now overrides `default_game_mode` to `/Script/Engine.GameModeBase`, and the clean SIE pass recorded `40` samples with `ErrorCountAfterMarker=0`.
- The clean SIE JSON is the source of truth. The CSV, summary Markdown, and chart are generated review artifacts for fast comparison.

SIE result summary:

| Variant | Main observed difference |
| --- | --- |
| Baseline | Clean SIE `R_Stalk_04` max actor-relative delta about `2.71`; `L_Stalk_04` about `1.26`. |
| AlphaHalf | Blend change altered the stalk response distribution: `R_Stalk_04` max delta about `2.05`, `L_Stalk_04` about `1.82`. |
| ForceZ | Upward force made vertical stalk response stronger: `R_Stalk_04 RangeZ` about `2.08` vs baseline `1.71`, `L_Stalk_04 RangeZ` about `1.35` vs baseline `1.05`. |
| WorldSpace | Simulation-space change had the clearest effect: `R_Stalk_04` max delta about `19.84`, `L_Stalk_04` about `23.05`, much larger than baseline. |

Review capture note:

- Hidden/inactive editor viewport captures rendered only editor primitives for this project session.
- The final review image was therefore produced through `SceneCapture2D -> RTF_RGBA8 RenderTarget -> PNG`, then annotated with 2D labels.
- The screenshot is a shape/order review artifact, not a runtime physics simulation proof. Use the SIE sample JSON above when comparing actual RigidBody motion over time.

Suggested comparison:

1. Place the four actor variants in a temporary preview map or manually in the editor.
2. Use the same movement setup for each actor.
3. Compare how much the simulated physics pass contributes:
   - `AlphaHalf`: weaker blend from the RigidBody result.
   - `ForceZ`: constant upward external force in component space.
   - `WorldSpace`: simulation interpreted in world space rather than component space.
4. Keep changes in `_MCP_Sample`; do not edit the original Baddy assets while comparing.

## ABP_Bot Trail Controller

`ABP_Bot`에는 `Trail controller / Bone: VB VBHead` 노드가 있다. 하지만 live graph 연결 요약 기준으로 이 노드는 현재 `ComponentPose` 입력과 `Pose` 출력이 모두 연결되지 않았다.

분류:

- 설정 참고용 노드: 맞음.
- 현재 최종 pose에 영향을 주는 active path: 아님.

Trail settings:

- `TrailBone`: `VB VBHead`
- `BaseJoint`: `head`
- `ChainLength`: `2`
- `ChainBoneAxis`: `X`
- `bReorientParentToChild`: true
- `bLimitStretch`: false
- `bLimitRotation`: false
- `bUsePlanarLimit`: false
- `TrailRelaxationSpeed`: curve key `(0, 10)` -> `(1, 5)`
- `Alpha`: `1.0`

학습 판단:

- Trail은 spring/trailing 계열 노드의 설정 예시로만 본다.
- 실제 동작 분석이나 재현 샘플은 `ABP_Baddy`의 RigidBody를 우선한다.
- Trail을 active sample로 공부하고 싶으면 `_MCP_Sample/AnimStudy`에 별도 샘플 AnimBP를 만들어 연결한 뒤 컴파일 경고와 bone validity를 확인해야 한다.

## UnrealMCP 학습 도구

이번 학습을 위해 추가된 읽기/작성 보조 명령:

- `inspect_anim_graph_node_settings`: AnimGraph 노드의 내부 `FAnimNode_*` 설정을 read-only JSON으로 덤프.
- `set_anim_graph_rigidbody_settings`: `_MCP_Sample` AnimBP의 `RigidBody` node settings를 좁게 수정해서 comparison variants를 만든다.
- `ensure_anim_graph_input_pose_passthrough`: Post Process AnimBP 입력 포즈를 root로 pass-through 연결.
- `ensure_anim_graph_modify_bone_demo`: Post Process AnimBP에 `Transform (Modify) Bone` 데모 체인 생성.

추천 사용 순서:

```text
list_blueprint_graphs
list_blueprint_nodes
inspect_anim_graph_node_settings
compile_and_validate_blueprint(save=false)
```

쓰기 명령은 `_MCP_Sample` 하위 샘플 애셋에만 사용한다.

## 다음 실험 후보

1. `ABP_Baddy` RigidBody 복제 샘플 만들기
   - `_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study`처럼 별도 AnimBP를 만든다.
   - RigidBody alpha, simulation space, external force를 하나씩 바꿔 compile 결과를 비교한다.

2. Bot Trail active sample 만들기
   - `VB VBHead`가 실제 skeleton bone이 아니라는 점을 전제로, 원본 Trail과 동일한 구조가 어떤 virtual bone/preview 상태에서 유효한지 먼저 확인한다.
   - 바로 원본에 연결하지 않는다.

3. Control Rig 학습
   - `CR_Bot_Correction`에서 foot IK와 interaction world location 처리를 분해한다.
   - AnimBP 변수와 ControlRig input pin 사이의 데이터 흐름을 기준으로 본다.

## Current Next Candidate

- Control Rig direct-gate, forced curve setup, ControlRig input-default forcing, forced-driver AnimBP assembly, and direct transient pre/post solve probing are complete. `controlrig_direct_gate_probe`, `ensure_anim_graph_modify_curve_demo`, `set_anim_graph_controlrig_input_defaults`, `ensure_controlrig_forced_driver_animbp`, and `sample_controlrig_pre_post_runtime_pose` are implemented, build-verified, and StackOBot live-smoked on bridge port `55558`; exact compiled AnimGraph-internal ControlRig source-vs-post attribution remains deferred to future instrumentation.
- Trail active sampling is complete for the safe antenna-chain study sample. Use `ABP_Bot_Trail_Study` with explicit component-level Post Process override for broad runtime comparisons; use `sample_anim_node_pre_post_runtime_pose(mode=isolated_temp_components)` for isolated source-bypass vs post-node Trail deltas.
- BlendSpace source maps and SIE pose grids are complete. Use the SIE grid as the runtime-style result and keep the non-SIE single-node probe as an API-gap record.
- Slot/LayeredBoneBlend inventory, AssetRegistry-level interaction reference probing, and read-only Blueprint graph call-topology probing are complete. `inspect_blueprint_graph_call_topology` is implemented, build-verified, synced into StackOBot, and live-smoked against `BP_Bot` plus `BPC_InteractionHandler`.
- State-machine transition topology is complete for source/target states and rule-graph topology. The read-only `inspect_anim_state_machine_transitions` UnrealMCP API is implemented, build-verified, and live-smoked against StackOBot on alternate bridge port `55558`; `inspect_anim_instance_runtime_state`, `set_anim_instance_runtime_property_for_probe`, and `sample_anim_state_machine_runtime_response` now cover live PIE/SIE current state reading, state weights, transition progress, relevant anim timing, and runtime property case resampling. Meaningful `ABP_Bot` transition-driving data is captured for `GroundSpeed`, `IsInAir?`, `MovementInput?`, and `IsHovering`.

## ABP_Bot Runtime Driver Matrix

Runtime matrix artifact:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ABP_Bot_RuntimeDriverMatrix.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimStateRuntimeMetrics_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimStateRuntimeMetrics_raw.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlueprintCallTopology_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlueprintCallTopology_raw.json`

Driver variables found from transition topology:

| Variable | Runtime type | State-machine role |
| --- | --- | --- |
| `GroundSpeed` | `DoubleProperty` | `GroundLocomotion`: `Idle <-> Walk/Run` around threshold `0.0`. |
| `IsInAir?` | `BoolProperty` | `AirLocomotion`: `Walk/Run -> Jump`, plus landing conduit exit. |
| `MovementInput?` | `BoolProperty` | Landing conduit split: false to `LandIdle`, true to `LandRun`. |
| `IsHovering` | `BoolProperty` | Jetpack path: `Fall/Jump -> StartJetpack`, hover stop back toward `Fall`. |

Confirmed runtime responses:

| Probe | Result |
| --- | --- |
| `GroundSpeed=0` | `GroundLocomotion=Idle` |
| `GroundSpeed=180` / `420` | `GroundLocomotion=Walk/Run` |
| `IsInAir?=true` | `AirLocomotion=Jump` |
| Landing idle sequence | `Walk/Run -> Jump -> Fall -> LandIdle -> Walk/Run` |
| Landing run sequence | `Walk/Run -> Jump -> Fall -> LandRun -> Walk/Run` |
| Jetpack sequence | `Walk/Run -> Jump -> Fall -> StartJetpack -> JetpackHovering -> Fall` |

Runtime metric smoke:

- The runtime state snapshot now reports `machine_weight`, per-state `state_weight` and `recorded_state_weight`, optional relevant animation time/remaining values, and `transition_progress`.
- `GroundSpeed=420` from idle captured an active `GroundLocomotion` transition `Idle -> Walk/Run`.
- After one `1/60s` tick, transition progress was `elapsed_time=0.0167`, `elapsed_fraction=0.0833`, with `Idle weight=0.9803` and `Walk/Run weight=0.0197`.
- After eight more `1/60s` ticks, transition progress was `elapsed_time=0.1500`, `elapsed_fraction=0.75`, with `Idle weight=0.15625` and `Walk/Run weight=0.84375`.
- The `IsInAir?=true` zero-duration transition guard case completed with no errors; inactive zero-crossfade transitions report `elapsed_fraction=0`.

Blueprint call-topology smoke:

- `inspect_blueprint_graph_call_topology` returned `read_only=true` static topology for `BP_Bot` and `BPC_InteractionHandler`.
- `BP_Bot` `reference_contains=Interact` found four nodes: two `Set Potential Interact` nodes in `Grab_Check`, one `Potential Interact` getter in `EventGraph`, and the `Interact` event from `BPI_TouchInterface`.
- `BP_Bot` `reference_contains=Montage` found zero nodes. A broader event-graph overview found `IA_Grab`, grab init/clear/update calls, sounds/camera shake, and input setup, but no direct montage or dynamic-slot playback call.
- `BPC_InteractionHandler` topology showed `Trigger` / `UnTrigger` events, `Trigger Complete` / `Trigger Reverse` delegate calls, and objective update flow; it does not expose a Bot montage trigger.

Automatic transition rules seen in topology and runtime sequence behavior:

- `Jump -> Fall`
- `LandIdle -> Walk/Run`
- `LandRun -> Walk/Run`
- `StartJetpack -> JetpackHovering`

Remaining state-machine gap:

- Full `EventGraph`/`CalcLean` K2 call topology can now be queried through `inspect_blueprint_graph_call_topology`, but the current StackOBot smoke focused on the interact/button path rather than a complete AnimBP EventGraph audit.
- The remaining exact pre/post attribution tasks are true same-instance compiled graph input/output pose taps for Control Rig and physics. Post Process static pre/post is complete, RigidBody/Trail isolated source-vs-output sampling is covered, and `ABP_Baddy` RigidBody live compiled-node address mapping is now covered.

## Remaining Study Backlog

1. Slot and LayeredBoneBlend pass
   - Completed the `UpperBody` slot, cached `CashedPose_UpperBody`, and pelvis/thigh branch filter inventory.
   - Completed the AssetRegistry-level interaction reference probe.
   - Completed the read-only Blueprint call-topology probe for `BP_Bot` and `BPC_InteractionHandler`.
   - Completed same-instance PoseWatch all-input capture for `ABP_Bot` `AnimGraphNode_LayeredBoneBlend_149`: `BasePose` and `BlendPoses[0]` both captured as pre-input poses against the same runtime post-node output.
   - Bot montage-like filename candidates were not found; the only loaded AnimMontage asset found by class scan is Baddy death.
   - `BP_Bot` has no direct `Montage` graph reference in the smoked event/function topology. The interact path resolves to `BPI_TouchInterface.Interact`, `Potential Interact`, and the grab-init/clear/update component path rather than a dynamic slot or montage playback path.
2. State machine transition pass
   - Completed the deep no-C++ transition topology probe.
   - Keep current transition graph inventory and deep probe as read-only evidence.
   - `inspect_anim_state_machine_transitions` is implemented, build-verified, and StackOBot live-smoked through `UNREAL_MCP_PORT=55558`.
   - Exact source/target state names and rule graph summaries are captured in `StackOBot_StateMachine_TransitionMCPInspect.*`.
   - `inspect_anim_instance_runtime_state` is implemented, build-verified, synced into StackOBot, and live-smoked in PIE/SIE against a transient `SKM_Bot` actor using `ABP_Bot_C`.
   - Current runtime-state artifacts are `StackOBot_AnimInstanceRuntimeState_MCPInspect.*`.
   - `set_anim_instance_runtime_property_for_probe` and `sample_anim_state_machine_runtime_response` are implemented, build-verified, synced into StackOBot, and live-smoked against the same transient `ABP_Bot_C` runtime path.
   - Current runtime property/response artifacts are `StackOBot_AnimRuntimePropertyMCPSet.*` and `StackOBot_AnimStateMachineRuntimeResponseMCPProbe.*`.
   - Runtime state snapshots now include state weights, optional relevant animation timing, and transition progress. Current metrics artifacts are `StackOBot_AnimStateRuntimeMetrics_*`.
   - The metrics smoke captured active `GroundLocomotion Idle -> Walk/Run` progress from `elapsed_fraction=0.0833` to `0.75` under `GroundSpeed=420`.
3. Control Rig pre/post pass
   - Completed the direct-gate MCP command `controlrig_direct_gate_probe`.
   - Current command artifacts are `StackOBot_ControlRig_DirectGateMCPProbe.*`.
   - Completed the sample-only ModifyCurve forcing command `ensure_anim_graph_modify_curve_demo`.
   - Current ModifyCurve sample artifacts are `StackOBot_ModifyCurveMCPEnsure.*`.
   - Completed the sample-only ControlRig input-default command `set_anim_graph_controlrig_input_defaults`.
   - Current ControlRig input-default artifacts are `StackOBot_ControlRigInputDefaultsMCPSet.*`.
   - Completed the combined forced-driver sample command `ensure_controlrig_forced_driver_animbp`.
   - Current forced-driver sample artifacts are `StackOBot_ControlRigForcedDriverMCPEnsure.*`.
   - Completed the direct transient ControlRig pre/post solve probe command `sample_controlrig_pre_post_runtime_pose`.
   - Current ControlRig pre/post artifacts are `StackOBot_ControlRigPrePostMCPProbe.*`.
   - Completed compiled AnimGraph same-instance PoseWatch capture on `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ForcedDriver_Study`.
   - Current ControlRig AnimGraph PoseWatch artifacts are `StackOBot_ControlRigPoseWatchPrePost_*`.
   - Do not hand-edit protected AnimGraph pins through Python.
   - Original inactive `ABP_Bot` still needs its gameplay gates to fire for natural ControlRig motion; the completed exact pre/post evidence uses the safe forced-driver sample.
4. Post Process final runtime pass
   - Static pre/post pose isolation and live same-instance PoseWatch capture are complete for the two variants.
   - A separate `sample_postprocess_pre_post_pose` command is no longer needed for these learning fixtures; use `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process)` for comparable Post Process AnimBP node checks.
   - Reusable sample setup is now covered by `ensure_postprocess_anim_demo_variant`, which creates/reuses `_MCP_Sample` Post Process AnimBP/SkeletalMesh variants, ensures the Modify Bone chain, compiles the AnimBP, assigns the SkeletalMesh Post Process AnimBlueprint, and saves without modifying original StackOBot assets.
5. Physics final runtime pass
   - Baddy RigidBody variants, source-vs-runtime split, Bot Trail runtime comparison, physics evidence synthesis, compiled node mapping preflight, and isolated RigidBody/Trail source-vs-output sampling are enough for the current learning baseline.
   - `sample_anim_node_pre_post_runtime_pose(mode=isolated_temp_components)` now covers RigidBody/Trail-style source-bypass vs post-node comparisons with temp assets.
   - `sample_anim_node_pre_post_runtime_pose(mode=compiled_graph_mapping)` now covers editor-node GUID to live compiled `FAnimNode_*` address mapping and runtime `FPoseLink` / `FComponentSpacePoseLink` link inventory.
   - True same-instance compiled AnimGraph node input/output pose tapping remains future API work, not a manual map-edit task.

## Bot Slot and Layered Blend Inventory

Read-only artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SlotLayeredBlend_Inventory.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SlotLayeredBlend_Inventory.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SlotLayeredBlend_Inventory.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_UpperBody_InteractionReferenceProbe.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_UpperBody_InteractionReferenceProbe.json`

Main result:

- `AnimGraphNode_Slot_1` is `SlotName=UpperBody`, `Group=DefaultGroup`, with `bAlwaysUpdateSourcePose=false`.
- The slot source is `LocomotionPose` through `AnimGraphNode_UseCachedPose_2.Pose`.
- The slot output is saved as `CashedPose_UpperBody`, then used as `AnimGraphNode_LayeredBoneBlend_149.BlendPoses_0`.
- `LayeredBoneBlend` uses `LocomotionPose` as `BasePose` and `CashedPose_UpperBody` as the overlay pose with `BlendWeights_0=1`.
- Branch filters are `pelvis BlendDepth=4`, `thigh_r BlendDepth=-1`, and `thigh_l BlendDepth=-1`.
- Mesh-space rotation blending is enabled and curve blend mode is `Override`.
- Graph comments say the slot is for an interact/press-button montage, but a filesystem scan found no Bot montage-like asset candidate by filename. The only montage-named candidate found in the wider project is `AM_Baddy_Death`.
- The AssetRegistry probe found the same result by class: the only loaded `AnimMontage` asset found was `/Game/StackOBot/Characters/Blobling/Anim/AM_Baddy_Death.AM_Baddy_Death`.
- `BP_Bot` depends on `ABP_Bot`, `BPI_Bot`, input actions such as `IA_Grab`/`IA_Jump`/`IA_Move`, and UI/action assets, but its dependency list did not include `IA_Interact` or a Bot montage asset.
- `IA_Interact` is referenced by `/Game/StackOBot/Input/IMC_ThirdPersonControls`.
- `BPC_InteractionHandler` exists and depends on gameplay/objective/spawn-pad assets, but the current Python Blueprint graph probe did not expose its node topology.

Interpretation:

- The upper-body slot is structurally ready for button/interact overlays while locomotion continues.
- The thigh exclusions protect the leg branches from the overlay, matching the comment that the Bot can press a button while running or flying.
- Current evidence proves slot structure and interaction-related asset references, but not a concrete Bot montage execution path.
- Further proof of the actual interact trigger needs a read-only Blueprint graph topology command, not AnimGraph mutation.

## ABP_Bot Control Rig Runtime Driver Probe

Runtime driver map:

- `/Game/_MCP_Temp/AnimStudy/M_Bot_ControlRig_Driver_MCP`

Placed temporary actors/helpers:

- `MCP_ControlRig_RawBot`: raw `SkeletalMeshActor` using original `SKM_Bot` and original `ABP_Bot_C`.
- `MCP_ControlRig_BotBP`: original `BP_Bot` instance, spawned only in the `_MCP_Temp` map.
- `MCP_ControlRig_Floor`: temporary collision floor.
- `MCP_ControlRig_LeftFootBlock`: temporary raised block near the BP_Bot left-foot region.

Runtime artifacts:

| Purpose | Path |
| --- | --- |
| Raw actor driver samples | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_ControlRig_DriverSamples.json` |
| Raw actor driver summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_ControlRig_DriverSummary.json` |
| Raw-vs-BP comparison samples | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_ControlRig_BPCompareSamples.json` |
| Raw-vs-BP comparison summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_ControlRig_BPCompareSummary.json` |
| Runtime curve probe summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/Bot_ControlRig_CurveProbeSummary.json` |

Observed result:

- `InteractWorldLocation` can be set on the runtime `ABP_Bot_C` AnimInstance and read back through Python.
- Changing `InteractWorldLocation` through neutral, high-Z, low-Z, and side-low phases produced `0.0` distance delta on `pelvis`, `thigh_l`, `calf_l`, `foot_l`, `thigh_r`, `calf_r`, and `foot_r`.
- The same zero-delta result occurred for the raw SkeletalMeshActor and for the spawned `BP_Bot` instance.
- `ShouldDoIKTrace` is wired into the active ControlRig AnimGraph node from `K2Node_CallFunction_0.ReturnValue`, but it is not callable from Python by the tried names (`ShouldDoIKTrace`, `Should Do IKTrace`, `Should Do IK Trace`, `Should_Do_IKTrace`).
- The runtime curve probe reported `IKBlend_l=0.0`, `IK_blend_interact=0.0`, `neck_01=0.0`, `head=0.0`, `upperarm_r=0.0`, and `lowerarm_r=0.0` for both raw and BP actors. `get_all_curve_names()` returned no active curves in this SIE setup.

Current interpretation:

- The ControlRig node is active in the final pose path, but this clean temporary runtime setup does not activate the foot/interaction correction branch.
- `InteractWorldLocation` alone is not enough to move the feet.
- The practical gate appears to be the `ShouldDoIKTrace` function and/or the related Control Rig curves (`IKBlend_l`, `IK_blend_interact`).
- The Control Rig hierarchy contains the relevant targets (`IK_foot_L`, `IK_foot_R`, `IKBlend_l`, `IK_blend_interact`), so the next useful experiment is not another plain actor placement. It should be a controlled `_MCP_Sample` driver that explicitly forces the IK trace/curve inputs or directly exercises the Control Rig controls while keeping original StackOBot assets unchanged.

Suggested next Control Rig experiment:

1. Duplicate the necessary driver layer into `_MCP_Sample/AnimStudy`.
2. Force or expose the `ShouldDoIKTrace`/IK blend gate in the duplicate only.
3. Re-run the same SIE socket sampler against `thigh_l/calf_l/foot_l` and `thigh_r/calf_r/foot_r`.
4. Treat the current zero-delta JSON files as the baseline "gate off" proof.

## CR_Bot_Correction Direct Gate Probe

Direct Control Rig instance artifacts:

| Purpose | Path |
| --- | --- |
| Direct gate probe JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/CR_Bot_Correction_DirectGateProbe.json` |
| Direct gate probe Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/CR_Bot_Correction_DirectGateProbeSummary.md` |
| Direct gate metrics CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/CR_Bot_Correction_DirectGateProbeMetrics.csv` |
| Direct gate distance chart SVG | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/CR_Bot_Correction_DirectGateProbeDistances.svg` |
| MCP direct gate raw JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_DirectGateMCPProbe.json` |
| MCP direct gate normalized JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_DirectGateMCPProbe_Normalized.json` |
| MCP direct gate CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_DirectGateMCPProbe.csv` |
| MCP direct gate Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_DirectGateMCPProbe.md` |

Direct probe method:

- Created runtime ControlRig instances from original `/Game/StackOBot/Characters/Bot/Rig/CR_Bot_Correction.CR_Bot_Correction`.
- Did not save or modify the original Control Rig asset.
- Set instance UPROPERTY inputs directly: `InteractionWorldLocation` and `ShouldDoIKTrace`.
- Set hierarchy curve values directly: `IKBlend_l` and `IK_blend_interact`.
- Executed `Forwards Solve` and sampled global transforms for `foot_l`, `foot_r`, `IK_foot_L`, and `IK_foot_R`.
- The repeatable MCP command now performs the same read-only transient ControlRig instance probe. StackOBot live smoke returned `success=true`, `read_only=true`, `asset_modified=false`, `case_count=6`, `success_cases=6`, and `error_count=0`. `Construction` and `Post Forwards Solve` returned false for this rig and are recorded as warnings; `Forwards Solve` is the useful execution event.

Key result:

| Case | Result |
| --- | --- |
| `ShouldDoIKTrace=false`, curves `0/0` | Baseline; no deltas. |
| `ShouldDoIKTrace=true`, curves `0/0` | No deltas. |
| `ShouldDoIKTrace=true`, `IKBlend_l=1`, `IK_blend_interact=0` | No deltas. |
| `ShouldDoIKTrace=true`, `IKBlend_l=0`, `IK_blend_interact=1`, location `[0,0,0]` | Small movement: `foot_l` distance about `0.1696`, `IK_foot_L` about `0.1252`. |
| `ShouldDoIKTrace=true`, both curves `1`, location `[0,0,120]` | Small movement: `foot_l` about `0.0560`, `IK_foot_L` about `0.0484`. |
| `ShouldDoIKTrace=true`, both curves `1`, location `[80,-40,80]` | Both feet and IK controls moved: `foot_l` about `0.2202`, `foot_r` about `0.2402`, `IK_foot_L` about `0.1594`, `IK_foot_R` about `0.1693`. |

Updated interpretation:

- `CR_Bot_Correction` does respond to the interaction/IK gate when driven directly.
- `IK_blend_interact` is the practical interaction blend gate for the observed foot/control movement.
- `ShouldDoIKTrace` is necessary context but not sufficient on its own.
- The previous AnimBP SIE actor probe stayed at zero because the runtime AnimInstance did not provide active `IK_blend_interact`/related curve values in the temporary setup.
- The MCP version also samples broader leg/root elements. In the side-location case, the largest sampled transform delta is about `20.937` on pelvis/thigh elements, while foot/IK-control movement remains the smaller correction-scale signal.
- The next full runtime proof should duplicate or wrap the AnimBP driver in `_MCP_Sample/AnimStudy` so that `ShouldDoIKTrace`, `InteractionWorldLocation`, and `IK_blend_interact` can be forced or exposed together, then run the same SIE bone sampler.

## ABP_Bot No-C++ Forced Driver Feasibility

Feasibility artifacts:

| Purpose | Path |
| --- | --- |
| Feasibility JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_ControlRig_NoCppForcedDriverFeasibility.json` |
| Feasibility Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_ControlRig_NoCppForcedDriverFeasibility.md` |

Result:

- `UAnimBlueprint.get_animation_graphs()` can find the original `ABP_Bot` AnimGraph.
- `AnimationGraph.get_graph_nodes_of_class(unreal.AnimGraphNode_ControlRig)` can find the active `AnimGraphNode_ControlRig_0`.
- Python cannot read the protected graph `Nodes` array.
- Python cannot read the ControlRig node `pins` property directly.
- `AnimGraphNode_ModifyCurve` exists as a Python class, but the original AnimGraph currently has no such node.
- `K2Node_CallFunction` cannot be queried through `AnimationGraph.get_graph_nodes_of_class`, because that helper only accepts `AnimGraphNode_Base` classes.

Current no-C++ decision:

- A forced AnimBP driver that breaks/relinks the ControlRig input pins is still not safe through plain Python reflection in the current toolset.
- The ModifyCurve half is now covered by `ensure_anim_graph_modify_curve_demo`, which created `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ModifyCurve_Study` and forced `IK_blend_interact=1.0` plus `IKBlend_l=1.0`.
- The ControlRig input-pin half is now covered by `set_anim_graph_controlrig_input_defaults`, which duplicated `/Game/StackOBot/Characters/Bot/ABP_Bot` to `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_InputDefaults_Study`, disconnected linked `ShouldDoIKTrace` and `InteractionWorldLocation` pins in the sample, set defaults to `true` and `[80, -40, 80]`, and compiled/saved with `0` errors and `0` warnings.
- The remaining narrow API candidate should assemble these two forced-driver pieces into one sample AnimBP, compile, then run the existing SIE bone sampler.

Pre/post Control Rig runtime status:

- Existing read-only inspection proves that `ABP_Bot` has an active Control Rig node connected to the final AnimGraph root.
- Direct `CR_Bot_Correction` probing proves the interaction branch can move feet/IK controls when `IK_blend_interact=1`; the strongest tested foot delta was `0.2402`.
- The current no-C++ SIE AnimBP driver cannot force the same Control Rig input pose and output pose in one controlled frame because Python cannot safely rewrite protected AnimGraph pins.
- The curve gate injection and ControlRig input-default parts are now available as sample-only MCP commands. The next useful step is a forced-driver builder that combines them into one validated sample.
- Treat exact source-vs-post-ControlRig subtraction as blocked until a narrow MCP command can sample pre/post transforms from that forced driver without saving original StackOBot assets.

## Deferred UnrealMCP C++ API Candidates

Implemented APIs:

| Command | Purpose | Status |
| --- | --- | --- |
| `controlrig_direct_gate_probe` | Repeatable direct transient ControlRig gate probe. | Implemented in UnrealMCP, build-verified in `MCPGameProjectEditor` and `StackOBotEditor`, and StackOBot live-smoked on bridge port `55558`. Current artifacts are `StackOBot_ControlRig_DirectGateMCPProbe.*`. |
| `ensure_anim_graph_modify_curve_demo` | Create or reuse a sample-only `LinkedInputPose -> ModifyCurve -> Root` chain. | Implemented in UnrealMCP, build-verified in both editor targets, and StackOBot live-smoked against `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ModifyCurve_Study`. The original `ABP_Bot` guard correctly refused non-sample edits. Current artifacts are `StackOBot_ModifyCurveMCPEnsure.*`. |
| `set_anim_graph_controlrig_input_defaults` | Expose/disconnect selected ControlRig input pins and set safe defaults in a duplicate AnimBP. | Implemented in UnrealMCP, build-verified in both editor targets, and StackOBot live-smoked against `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_InputDefaults_Study`. The original `ABP_Bot` guard correctly refused non-sample edits. First call changed defaults and disconnected links; second call was idempotent with `graph_changed=false`. Current artifacts are `StackOBot_ControlRigInputDefaultsMCPSet.*`. |
| `ensure_controlrig_forced_driver_animbp` | Preserve the existing ABP_Bot ControlRig path while inserting a forced `ModifyCurve` driver and forced ControlRig input defaults before the ControlRig node. | Implemented in UnrealMCP, build-verified in both editor targets, and StackOBot live-smoked against `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ForcedDriver_Study`. The original `ABP_Bot` guard correctly refused non-sample edits. First call inserted `ModifyCurve -> ControlRig`, disconnected forced input links, and set defaults; second call was idempotent with `graph_changed=false`. Current artifacts are `StackOBot_ControlRigForcedDriverMCPEnsure.*`. |
| `sample_controlrig_pre_post_runtime_pose` | Sample a transient ControlRig hierarchy before and after execute events, using forced driver properties/curves. | Implemented in UnrealMCP, build-verified in both editor targets, and StackOBot live-smoked against `CR_Bot_Correction`. It is read-only and reports `runtime_source=direct_transient_controlrig`, `runtime_graph_prepost=false`, and `asset_modified=false`. Current artifacts are `StackOBot_ControlRigPrePostMCPProbe.*`. |
| `sample_skeletal_bones_in_sie` | Sample live PIE/SIE SkeletalMeshComponent bone/socket transforms from a matched actor/component. | Implemented in UnrealMCP, build-verified in both editor targets, synced into StackOBot, and StackOBot live-smoked against a transient `SKM_Bot` actor in PIE/SIE. It is read-only, reports `sampled_world_type=PIE`, `is_play_session_active=true`, and sampled `pelvis`, `foot_l`, `foot_r`, `head`, `antenna_04_l`, and `antenna_04_r` with no invalid bones. Current artifacts are `StackOBot_SkeletalBonesInSIE_MCPProbe.*`. |
| `inspect_anim_instance_runtime_state` | Read current state names and elapsed time from a live AnimInstance. | Implemented in UnrealMCP, build-verified in both editor targets, synced into StackOBot, and StackOBot live-smoked in PIE/SIE against a transient `SKM_Bot` actor using `ABP_Bot_C`. The safe MVP reads `FAnimNode_StateMachine` runtime instances and maps them through `StateMachineIndexInClass`; per-state weights/relevant timing are intentionally omitted. Current artifacts are `StackOBot_AnimInstanceRuntimeState_MCPInspect.*`. |
| `set_anim_instance_runtime_property_for_probe` | Set supported reflected properties on a matched live AnimInstance for runtime probing. | Implemented in UnrealMCP, build-verified in both editor targets, synced into StackOBot, and StackOBot live-smoked with `bUseMultiThreadedAnimationUpdate`. It reports `runtime_only=true`, `asset_modified=false`, and property echo before/after assignment. Current artifacts are `StackOBot_AnimRuntimePropertyMCPSet.*`. |
| `sample_anim_state_machine_runtime_response` | Apply runtime property cases, force bounded component animation ticks, sample state-machine snapshots, and restore successful changes per case. | Implemented in UnrealMCP, build-verified in both editor targets, synced into StackOBot, and StackOBot live-smoked with restored runtime property cases plus active transition metric capture. Current artifacts include `StackOBot_AnimStateMachineRuntimeResponseMCPProbe.*` and `StackOBot_AnimStateRuntimeMetrics_*`. |

API follow-up status:

| Command | Status | Notes |
| --- | --- | --- |
| `sample_anim_node_pre_post_runtime_pose` | Implemented, with follow-up expansion | `compiled_graph_mapping`, `active_component_tick_delta`, `isolated_temp_components`, and `pose_watch_capture` are implemented. Same-instance PoseWatch capture is live-smoked for `ABP_Baddy` RigidBody, `ABP_Bot_Trail_Study`, Post Process ModifyBone variants, `ABP_Bot` LayeredBoneBlend all-input capture, and `_MCP_Sample` ControlRig forced-driver capture. Use `anim_instance_source=post_process` when the selected node lives in the component's Post Process AnimBP. Remaining expansion is only for unusual node classes outside the smoked coverage. |
| `ensure_anim_graph_trail_demo` | Implemented | Creates safe `_MCP_Sample` Trail Post Process AnimBP chains and refuses original asset mutation by default. Current Trail learning evidence already uses this path. |
| `inspect_anim_graph_protected_topology` | Implemented | Build-verified in UnrealMCP and StackOBot, synced into StackOBot, and live-smoked against `ABP_Bot` ControlRig topology. It returned one `AnimGraphNode_ControlRig_0`, two pose links, `read_only=true`, and `asset_modified=false`. Current artifacts are `StackOBot_AnimGraphProtectedTopology_ControlRig_*`. |
| `sample_blendspace_runtime_pose_grid` | Implemented | Build-verified in UnrealMCP and StackOBot, synced into StackOBot, and live-smoked against `BS_Bot_WalkRunLean` plus `BS_Bot_RunIdleJump` in PIE/SIE. Current artifacts are `StackOBot_BlendSpaceRuntimePoseGridMCP_*`. |
| `ensure_postprocess_anim_demo_variant` | Implemented | Build-verified in UnrealMCP and StackOBot, synced into StackOBot, and live-smoked against the existing `HeadPitch` Post Process sample. Current artifacts are `StackOBot_PostProcessDemoVariantEnsure_*`. |
| `inspect_anim_state_machine_transitions` | Implemented | Build-verified and StackOBot live-smoked on bridge port `55558`; `StackOBot_StateMachine_TransitionMCPInspect.*` is the current source/target and rule-summary artifact. |
| `inspect_blueprint_graph_call_topology` | Implemented | Build-verified in both editor targets, synced into StackOBot, and StackOBot live-smoked against `BP_Bot` plus `BPC_InteractionHandler`. Current artifacts are `StackOBot_BlueprintCallTopology_*`. |

Priority recommendation:

1. Completed for SIE component bone/socket sampling: `sample_skeletal_bones_in_sie` was run against StackOBot through the primary bridge port and sampled the transient Bot actor from `sampled_world_type=PIE`.
2. Completed ControlRig-in-AnimGraph attribution through `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` on the forced-driver sample; extend the command only when a new unusual node class falls outside the smoked coverage.
3. Completed for direct transient ControlRig pre/post solve: `sample_controlrig_pre_post_runtime_pose` was run against StackOBot through the alternate bridge port.
4. Completed for direct ControlRig gate: `controlrig_direct_gate_probe` was run against StackOBot through the alternate bridge port.
5. Completed for sample curve forcing: `ensure_anim_graph_modify_curve_demo` was run against StackOBot through the alternate bridge port.
6. Completed for sample ControlRig input defaults: `set_anim_graph_controlrig_input_defaults` was run against StackOBot through the alternate bridge port.
7. Completed for combined ControlRig forced driver: `ensure_controlrig_forced_driver_animbp` was run against StackOBot through the alternate bridge port.
8. Completed for source/target and rule topology: `inspect_anim_state_machine_transitions` was run against StackOBot through the alternate bridge port.
9. Completed for live AnimInstance current-state reading: `inspect_anim_instance_runtime_state` was run against StackOBot through the primary bridge port and sampled the transient Bot actor from `sampled_world_type=PIE`.
10. Completed for runtime property set and response scaffolding: `set_anim_instance_runtime_property_for_probe` and `sample_anim_state_machine_runtime_response` were run against StackOBot through the primary bridge port with restored `bUseMultiThreadedAnimationUpdate` cases.
11. Completed for meaningful `ABP_Bot` transition drivers: `GroundSpeed`, `IsInAir?`, `MovementInput?`, and `IsHovering` were set on a transient runtime `ABP_Bot_C` instance and produced the expected ground, jump/fall, landing, and jetpack state sequences.
12. Completed for Blueprint call topology: `inspect_blueprint_graph_call_topology` proved the current `BP_Bot` interact path and found no direct montage/dynamic-slot playback call in the smoked `BP_Bot` event/function topology.
13. Completed for Trail Controller active sample authoring: `ensure_anim_graph_trail_demo` now creates/reuses safe `_MCP_Sample` Trail Post Process AnimBP chains and refuses original asset mutation by default.
14. Completed for protected AnimGraph topology: `inspect_anim_graph_protected_topology` now returns AnimGraph nodes, pins, pose pins, and normalized pose links without saving assets.

## Trail No-C++ Active Sample Feasibility

Feasibility artifacts:

| Purpose | Path |
| --- | --- |
| Feasibility JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_Trail_NoCppActiveSampleFeasibility.json` |
| Feasibility Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_Trail_NoCppActiveSampleFeasibility.md` |

Result:

- Existing MCP inspection confirms the original `ABP_Bot` Trail node is `AnimGraphNode_Trail_1`, titled `Trail controller / Bone: VB VBHead`.
- The node keeps useful settings, including `TrailBone=VB VBHead`, `BaseJoint=head`, `ChainLength=2`, `ChainBoneAxis=X`, `bReorientParentToChild=true`, and `Alpha=1.0`.
- Its `ComponentPose` input and `Pose` output have no links, so it remains inactive in the final pose.
- Python can find the `AnimGraph`, `AnimGraphNode_Trail`, `LocalToComponentSpace`, `ComponentToLocalSpace`, and root node classes.
- Python still cannot safely read protected graph `Nodes` or node `pins`, so it cannot build and connect an active Trail sample graph without a new narrow MCP graph-edit command.

Current no-C++ decision:

- Do not attempt protected AnimGraph mutation through Python.
- `ensure_anim_graph_trail_demo` has since been implemented as the narrow MCP graph-edit command for this case, restricted to `/Game/_MCP_Sample/AnimStudy` by default.

## Learning Map

The compact execution map and sampling checklist live in:

- `<workspace-parent>/CubelessStylized/docs/stackobot-animation-execution-map.md`

## AnimBP State Machine Inventory

Inventory artifacts:

| Purpose | Path |
| --- | --- |
| Read API probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimBP_ReadApiProbe.json` |
| Refreshed ABP_Bot AnimGraph inspect | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_AnimGraphInspect_Refresh.json` |
| ABP_Baddy AnimGraph inspect | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Baddy_AnimGraphInspect.json` |
| State graph node/asset probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateGraph_NodeAssetProbe.json` |
| Compact inventory JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimBP_StateMachineInventory.json` |
| Compact inventory Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimBP_StateMachineInventory.md` |
| Transition inventory JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionInventory.json` |
| Transition inventory Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionInventory.md` |
| Project-facing inventory note | `<workspace-parent>/CubelessStylized/docs/stackobot-animbp-inventory.md` |

Key `ABP_Bot` flow:

```text
GroundLocomotion -> Save GroundLocoPose
GroundLocoPose -> AirLocomotion -> Save LocomotionPose
LocomotionPose -> UpperBody Slot -> Save CashedPose_UpperBody
LocomotionPose + CashedPose_UpperBody -> LayeredBoneBlend -> Save FullBodyPose
IsInactive ? A_Bot_Idle_Inactive : FullBodyPose -> ControlRig -> Root
```

Key `ABP_Baddy` flow:

```text
New State Machine -> LocalToComponentSpace -> RigidBody -> ComponentToLocalSpace -> DefaultSlot -> Root
```

Learning interpretation:

- `ABP_Bot` is the better sample for cached poses, layered upper-body montage slots, final inactive-pose switching, and Control Rig input gates.
- `ABP_Baddy` is the better sample for a clean physics node placement: local pose becomes component-space, passes through `RigidBody`, then returns to local pose before the root path.
- Protected AnimGraph node/pin/link topology is now covered by `inspect_anim_graph_protected_topology`; runtime branch firing and sample pose values still require PIE/SIE probes.

## AnimBP Transition Graph Inventory

Transition inventory artifacts:

| Purpose | Path |
| --- | --- |
| Transition graph GUID/path probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TransitionGraph_GuidProbe.json` |
| Transition API read probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionApiProbe.json` |
| Deep transition topology probe JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TransitionTopology_DeepProbe.json` |
| Deep transition topology probe Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TransitionTopology_DeepProbe.md` |
| Compact transition inventory JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionInventory.json` |
| Compact transition inventory Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionInventory.md` |
| MCP transition inspect JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionMCPInspect.json` |
| MCP transition inspect normalized JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionMCPInspect_Normalized.json` |
| MCP transition inspect CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionMCPInspect.csv` |
| MCP transition inspect Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionMCPInspect.md` |

Read result:

- `ABP_Bot` MCP inspect succeeded with 2 state machines and 14 transitions: 2 under `GroundLocomotion` and 12 under `AirLocomotion`.
- `ABP_Baddy` MCP inspect succeeded with 1 state machine and 2 transitions under `New State Machine`.
- Source and target state names are now read from editor transition nodes through `UAnimStateTransitionNode`, not inferred from graph paths.
- Rule graph summaries are readable through the MCP C++ command, including K2 variable/function nodes where present.
- Several short animation-completion transitions use automatic sequence-player rules rather than explicit K2 boolean nodes.
- `GroundLocomotion` is confirmed as `Idle -> Walk/Run` on `GroundSpeed >` and `Walk/Run -> Idle` on `GroundSpeed <=`.
- Baddy's compact state machine is confirmed as `A_Baddy_Idle -> A_Baddy_Walk` on `Is Moving`, and `A_Baddy_Walk -> A_Baddy_Idle` on `NOT Is Moving`.
- The earlier no-C++ Python probes remain useful fallback evidence: they show transition graph paths and the protected-topology limit that required the MCP C++ command.

Current decision:

- Use `StackOBot_StateMachine_TransitionMCPInspect.*` as the current source of truth for transition source/target states and rule summaries.
- Do not use the older Python-only deep probe to infer transition source/target names; it is now only the record of why the C++ MCP path was needed.
- Runtime active state names, state weights, transition progress, relevant animation timing, and forced-variable response are covered by the runtime AnimInstance API work.
- Use `inspect_anim_graph_protected_topology` when static AnimGraph node/pin/link topology is needed; use runtime AnimInstance or PoseWatch commands when the question is whether a branch actually fired or what pose a node produced.

## AnimInstance Runtime State Probe

Runtime feasibility artifacts:

| Purpose | Path |
| --- | --- |
| AnimInstance state API probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstance_StateApiProbe.json` |
| Unreal Python state library probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_UnrealPython_StateLibraryProbe.json` |
| SkeletalMeshComponent tick method probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SkeletalMeshComponent_TickMethodProbe.json` |
| Temp actor setup probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstance_StateProbe_Setup.json` |
| SIE immediate mutability probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstance_SIEMutabilityProbe.json` |
| SIE delayed mutability probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstance_SIEDelayedMutabilityProbe.json` |
| Runtime state probe summary JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstance_RuntimeStateProbeSummary.json` |
| Runtime state probe summary Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstance_RuntimeStateProbeSummary.md` |

Read result:

- `ABP_Bot` variables are readable on the generated class/default object and runtime instance: `GroundSpeed`, `LeanAmount`, `MovementInput?`, `IsInAir?`, `IsInactive`, and `InteractWorldLocation`.
- `ABP_Baddy` variables are readable: `Is Moving` and `NewVar`.
- The Python `AnimInstance` wrapper exposes transition event helpers, but not current-state helpers such as `get_current_state_name`, `get_state_machine_instance_desc`, or `get_state_weight`.
- Temp map actors in `/Game/_MCP_Temp/AnimStudy/M_AnimInstance_StateProbe_MCP` produced valid runtime AnimInstances for both Bot and Baddy.
- A delayed SIE probe produced one PIE world and valid socket samples for Bot (`pelvis`, `head`, `foot_l`, `foot_r`) and Baddy (`Head_02`, `R_Stalk_04`, `L_Stalk_04`, `TailEnd`).
- Editor-world and SIE runtime instances rejected variable forcing: `set_editor_property` reported `cannot be edited on instances`, and direct `setattr` also failed.

Current decision:

- Runtime pose/socket sampling works.
- Controlled state-machine transition sampling does not work through plain Python because current state names are not exposed and runtime variables cannot be forced.
- Keep the next step as deferred MCP/C++ API work, not another ad hoc Python probe.

## Animation Asset Playback Inventory

Animation asset inventory artifacts:

| Purpose | Path |
| --- | --- |
| Raw read/API probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimationAsset_ReadApiProbe.json` |
| BlendSpace detail probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_DetailProbe.json` |
| Compact inventory JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimationAsset_Inventory.json` |
| Compact inventory Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimationAsset_Inventory.md` |

Read result:

- `16` sequence assets and `2` BlendSpace assets were loaded with `0` load errors.
- Bot sequence set: `14` sequences, total length about `18.04s`, sampled at about `25 fps`, root motion disabled.
- Baddy sequence set: `2` sequences, total length about `1.33s`, sampled at about `30 fps`, root motion disabled.
- `BS_Bot_WalkRunLean` axes: `Lean -1..1`, `Speed 0..500`.
- `BS_Bot_WalkRunLean` samples: `A_Bot_Walk` at `(0, 96.978)`, `A_Bot_Run` at `(0, 500)`, `A_Bot_Run_LeanLeft` at `(1, 258.546)`, and `A_Bot_Run_LeanRight` at `(-1, 259.420)`.
- `BS_Bot_RunIdleJump` axis: `Speed 0..500`.
- `BS_Bot_RunIdleJump` samples: `A_Bot_IdleJump` at `54.485` and `A_Bot_RunJump` at `54.552`.
- Both BlendSpaces use notify trigger mode `HIGHEST_WEIGHTED_ANIMATION` and target weight interpolation speed `0.0`.

Learning interpretation:

- Ground locomotion combines movement speed and turn lean through `BS_Bot_WalkRunLean`.
- Jump start selection is speed-based but the two jump samples are clustered near the same speed value; treat this as a narrow blend rather than a full idle-to-run continuum.
- Baddy remains the clean compact sample for state-machine and RigidBody placement; its playback set is intentionally tiny.
- Sequence `Notifies`, sync markers, and curve data remain protected through the current Python property path, so notify/curve study should either use a future protected read API or manual editor inspection.

## Sequence Motion Profile

Sequence motion profile artifacts:

| Purpose | Path |
| --- | --- |
| Pose sampling API probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SequenceMotion_PoseSamplingApiProbe.json` |
| AnimPose API probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SequenceMotion_AnimPoseApiProbe.json` |
| Full motion profile JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SequenceMotion_Profile.json` |
| Motion profile metrics CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SequenceMotion_ProfileMetrics.csv` |
| Motion profile Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SequenceMotion_Profile.md` |
| Top deltas chart SVG | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SequenceMotion_ProfileTopDeltas.svg` |

Read result:

- Direct `AnimSequence.get_anim_pose_at_time` sampling worked for all `16` inspected sequence assets with `0` errors.
- The sampled pose object exposes `get_bone_pose`, `get_curve_names`, `get_curve_weight`, `get_socket_pose`, and reference-pose comparison helpers.
- Bot sampled bones: `pelvis`, `head`, `foot_l`, and `foot_r`.
- Baddy sampled bones: `Head_02`, `R_Stalk_04`, `L_Stalk_04`, and `TailEnd`.
- Largest Bot authored movement: `A_Bot_Run_LeanLeft foot_l` about `53.10`, `A_Bot_Run_LeanRight foot_r` about `52.70`, and `A_Bot_Run foot_l` about `52.15` max distance from first sampled pose.
- Largest Baddy authored movement: `A_Baddy_Walk TailEnd` about `36.71`, `L_Stalk_04` about `22.26`, and `R_Stalk_04` about `20.85`.

Learning interpretation:

- Source clip motion is now separated from final runtime graph output.
- The Bot run/lean clips carry the biggest authored foot displacement before BlendSpace interpolation and Control Rig correction.
- The Baddy walk clip already moves tail/stalk bones, so RigidBody analysis should compare against this authored baseline rather than treating all stalk movement as physics.
- This profile does not include state transitions, BlendSpace interpolation, slots, Control Rig, RigidBody, or Post Process AnimBP.

Verification note:

- Pose sampling itself completed with `0` errors and did not require saving original animation assets.
- A cleanup attempt to switch from dirty `/Game/StackOBot/Maps/Lvl_Empty` to a `_MCP_Temp` map through native `open_editor_level` hit UE's `World Memory Leaks` assert because `/Game/StackOBot/Maps/Lvl_Empty` was still referenced by `FPyReferenceCollector`.
- Treat this as a cleanup-path limitation: after Python pose/map probes that touch the current level, prefer closing the editor without map switching instead of attempting another level open while Python references may still exist.

## Baddy RigidBody Source vs Runtime Comparison

This pass compared the authored Baddy source clip motion against the previously captured clean SIE RigidBody samples. It was done offline from saved JSON/CSV artifacts; Unreal Editor was not opened and original StackOBot assets were not touched.

Artifacts:

| Purpose | Path |
| --- | --- |
| Source vs runtime JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.json` |
| Source vs runtime CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.csv` |
| Source vs runtime Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.md` |
| Stalk comparison chart | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.svg` |

Key numeric comparison:

| Variant | Bone | Runtime max delta | Idle source max | Walk source max | Runtime % of walk |
| --- | --- | ---: | ---: | ---: | ---: |
| `Baseline` | `Head_02` | `1.411` | `1.448` | `10.007` | `14.1%` |
| `Baseline` | `TailEnd` | `0.189` | `0.191` | `36.706` | `0.5%` |
| `WorldSpace` | `R_Stalk_04` | `19.836` | `3.876` | `20.854` | `95.1%` |
| `WorldSpace` | `L_Stalk_04` | `23.050` | `4.236` | `22.260` | `103.6%` |

Interpretation:

- The clean SIE sample aligns closer to `A_Baddy_Idle` than `A_Baddy_Walk` for `Head_02` and `TailEnd`, so the captured runtime case should be treated as idle-scale animation with physics response.
- `Head_02` and `TailEnd` stayed effectively unchanged across the RigidBody variants.
- The RigidBody node mainly changes `R_Stalk_04` and `L_Stalk_04`.
- `WorldSpace` produces walk-scale stalk displacement while the tail remains idle-scale, which points to simulation-space amplification rather than a simple switch to the walk clip.
- This is a magnitude-level comparison, not a frame-synchronized subtraction. A true per-frame source-vs-post-RigidBody split still needs future runtime sampling support.

## Bot BlendSpace Source Pose Map

This pass mapped Bot BlendSpace sample coordinates to the already sampled source sequence motion. It is an offline sample-coordinate map, not a runtime BlendSpace evaluation.

Artifacts:

| Purpose | Path |
| --- | --- |
| BlendSpace pose-map JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.json` |
| BlendSpace pose-map CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.csv` |
| BlendSpace pose-map Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.md` |
| BlendSpace pose-map SVG | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.svg` |

Key readings:

| BlendSpace | Sample | Coordinates | Source motion reading |
| --- | --- | --- | --- |
| `BS_Bot_WalkRunLean` | `A_Bot_Walk` | `Lean=0`, `Speed=96.978` | `foot_r` is strongest, about `28.47`. |
| `BS_Bot_WalkRunLean` | `A_Bot_Run` | `Lean=0`, `Speed=500.000` | Feet are run-scale, `foot_l` about `52.15`, `foot_r` about `51.43`. |
| `BS_Bot_WalkRunLean` | `A_Bot_Run_LeanLeft` | `Lean=1`, `Speed=258.546` | Left foot strongest, about `53.10`. |
| `BS_Bot_WalkRunLean` | `A_Bot_Run_LeanRight` | `Lean=-1`, `Speed=259.420` | Right foot strongest, about `52.70`. |
| `BS_Bot_RunIdleJump` | `A_Bot_IdleJump` | `Speed=54.485` | Left foot strongest, about `29.24`. |
| `BS_Bot_RunIdleJump` | `A_Bot_RunJump` | `Speed=54.552` | Right foot strongest, about `33.85`. |

Interpretation:

- `BS_Bot_WalkRunLean` is readable as a neutral walk/run vertical line plus two mid-speed lean samples.
- The lean samples sit at nearly the same speed band, so `LeanAmount` should be treated as the side-bias control and `GroundSpeed` as the walk/run scale control.
- `BS_Bot_RunIdleJump` has two Speed-axis samples only `0.067` apart, so this BlendSpace does not by itself create a broad speed ramp between idle jump and run jump.
- Runtime state selection and transition gates are still required to explain when `BS_Bot_RunIdleJump` is entered and why the near-overlapping samples are used.

## Bot BlendSpace SIE Pose Grid

This pass evaluated the Bot BlendSpaces through a transient `SkeletalMeshActor` in SIE game-world tick. It is the preferred runtime-style BlendSpace evidence because the earlier full-editor `AnimationSingleNode` tick path accepted input values but produced `0.0` pose deltas.

Artifacts:

| Purpose | Path |
| --- | --- |
| SIE pose grid JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.json` |
| SIE pose grid CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.csv` |
| SIE pose grid Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.md` |
| SIE pose grid SVG | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.svg` |
| Non-SIE live tick gap probe | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_LiveTickPoseGrid.md` |
| Reusable MCP runtime pose-grid summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpaceRuntimePoseGridMCP_Summary.json` |
| Reusable MCP runtime pose-grid raw JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpaceRuntimePoseGridMCP_raw.json` |

Key readings:

| BlendSpace | Runtime-style result |
| --- | --- |
| `BS_Bot_WalkRunLean` | Input changes produced visible pose changes; max location delta from first sample was `66.061 cm`, strongest at `run_authored antenna_04_r`. |
| `BS_Bot_RunIdleJump` | Input changes produced visible pose changes; max location delta from first sample was `35.438 cm`, strongest at `axis_min_speed antenna_04_r`. |

Reusable MCP command smoke:

| BlendSpace | Controlled runtime command result |
| --- | --- |
| `BS_Bot_WalkRunLean` | `sample_blendspace_runtime_pose_grid` sampled `9` poses with `valid_pose_count=9`, `input_changed_pose=true`, and max location delta `30.707 cm`. |
| `BS_Bot_RunIdleJump` | `sample_blendspace_runtime_pose_grid` sampled `5` poses with `valid_pose_count=5`, `input_changed_pose=true`, and max location delta `5.757 cm`. |

Interpretation:

- SIE/game-world component tick is required for this Python route; the non-SIE single-node editor tick path is not enough to refresh a meaningful BlendSpace pose.
- The reusable MCP command is now the repeatable route for future audits. Its fixed sample-time/forced-tick `UAnimSingleNodeInstance` measurements are controlled tooling evidence, so the exact deltas are allowed to differ from the older async SIE script artifacts.
- `BS_Bot_WalkRunLean` runtime-style pose changes are large at the upper body and antenna chain because the sampled locomotion clips change the whole body posture, not just feet.
- `BS_Bot_RunIdleJump` still has near-overlapping authored jump samples, but off-sample axis probes show the BlendSpace can change output when the input moves away from that narrow authored band.
- Original StackOBot assets were not saved or modified.

## Control Rig Contribution Synthesis

This pass synthesized existing source-motion, direct Control Rig, and SIE AnimBP probes. It did not open Unreal Editor or modify any assets.

Artifacts:

| Purpose | Path |
| --- | --- |
| Control Rig synthesis JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_Contribution_Synthesis.json` |
| Control Rig synthesis CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_Contribution_Synthesis.csv` |
| Control Rig synthesis Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_Contribution_Synthesis.md` |
| Control Rig synthesis SVG | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_Contribution_Synthesis.svg` |
| Forced-driver ensure Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigForcedDriverMCPEnsure.md` |
| Forced-driver ensure summary JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigForcedDriverMCPEnsure_Summary.json` |
| Forced-driver ensure normalized JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigForcedDriverMCPEnsure_Normalized.json` |
| Direct pre/post probe Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPrePostMCPProbe.md` |
| Direct pre/post probe summary JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPrePostMCPProbe_Summary.json` |
| Direct pre/post probe normalized JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPrePostMCPProbe_Normalized.json` |

Active graph fact:

- `ABP_Bot` has an active `AnimGraphNode_ControlRig` connected to the final AnimGraph root.
- `ControlRigClass` is `/Game/StackOBot/Characters/Bot/Rig/CR_Bot_Correction.CR_Bot_Correction_C`.
- `Source`, `Pose`, `InteractionWorldLocation`, and `ShouldDoIKTrace` are linked in the active graph path.

Contribution reading:

| Probe layer | Key result |
| --- | --- |
| Source clips | Strongest BlendSpace source foot delta is `A_Bot_Run_LeanLeft foot_l`, about `53.10`. |
| Direct Control Rig | `ShouldDoIKTrace` alone and `IKBlend_l` alone produced `0.0` foot delta. |
| Direct Control Rig | `IK_blend_interact=1` produced measurable deltas; strongest tested foot delta was `0.2402`. |
| Forced-driver sample | `ensure_controlrig_forced_driver_animbp` preserved the original upstream pose into `ModifyCurve -> ControlRig`, forced `IK_blend_interact=1`, `IKBlend_l=1`, `ShouldDoIKTrace=true`, and `InteractionWorldLocation=(80,-40,80)` on a duplicate sample AnimBP. |
| Direct pre/post ControlRig | `sample_controlrig_pre_post_runtime_pose` sampled the same transient rig before and after `Forwards Solve`; max translation delta was `pelvis=20.9368`, and max rotation delta was `calf_r=40.3937 deg`. |
| SIE AnimBP | Raw and `BP_Bot` phase tests produced `0.0` foot deltas, with `IKBlend_l=0.0` and `IK_blend_interact=0.0` in the last curve probe. |

Interpretation:

- The large locomotion foot motion is authored in the source clips before Control Rig.
- Control Rig is still an active late-stage correction layer, but the interaction branch stayed gated off in the temp SIE runtime setup.
- The sample forced-driver assembly is now complete and compile-validated.
- Direct transient ControlRig pre/post solve evidence is now available and read-only, but it is not compiled AnimGraph-internal node instrumentation. A true source-vs-post-ControlRig split still needs a runtime probe that samples both sides of the ControlRig node inside the AnimGraph stack in the same tick.

## ABP_Bot Active Trail Sample

Implemented sample-only Trail authoring support through the UnrealMCP command `ensure_anim_graph_trail_demo`.

Sample assets:

| Purpose | Path |
| --- | --- |
| Trail Post Process AnimBP | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study` |
| Skeletal mesh using that Post Process AnimBP | `/Game/_MCP_Sample/AnimStudy/SKM_Bot_Trail_Study` |
| Actor template using original `ABP_Bot` plus the Trail study mesh | `/Game/_MCP_Sample/AnimStudy/BP_Bot_Trail_StudyActor` |

Final connected chain:

```text
LinkedInputPose -> LocalToComponentSpace -> Trail -> ComponentToLocalSpace -> Root
```

Clean Trail settings:

- `TrailBone`: `antenna_04_l`
- `BaseJoint`: `head`
- `ChainLength`: `4`
- `ChainBoneAxis`: `X`
- `Alpha`: `1.0`
- `FakeVelocity`: `(0, 0, 0)`

Validation:

- `ensure_anim_graph_trail_demo` refused a direct call against original `/Game/StackOBot/Characters/Bot/ABP_Bot` when `allow_non_sample=false`.
- Activating the original retained `VB VBHead` setting in the sample produced a compile warning that `VB VBHead` was not found in the skeleton.
- The final `antenna_04_l` sample compiled and saved with `0` errors and `0` warnings.
- The actor template compiled and saved with `0` errors and `0` warnings.
- Dirty content/map package count after setup was `0`.
- Runtime SIE sampling was intentionally skipped in this pass to avoid dirtying/switching the current map after the earlier world-reference cleanup crash.

## ABP_Bot Trail Runtime Comparison

Runtime comparison artifacts:

| Purpose | Path |
| --- | --- |
| Runtime comparison Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_Trail_RuntimeComparison.md` |
| Runtime comparison JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_Trail_RuntimeComparison.json` |
| Runtime comparison CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_Trail_RuntimeComparison.csv` |
| Runtime comparison chart | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_Trail_RuntimeComparison.svg` |

Comparison result:

| Probe | Result |
| --- | --- |
| Editor tick, mesh Post Process default | No measurable Trail-vs-raw difference. |
| SIE, mesh Post Process default | No measurable Trail-vs-raw difference on transient proof actors. |
| SIE, explicit component Post Process override | Measurable Trail-vs-raw difference. |

Key numbers from `SIE_ExplicitPPOverride`:

| Bone | Max Trail-Raw Distance |
| --- | ---: |
| `head` | `0.846 cm` |
| `antenna_01_l` | `2.206 cm` |
| `antenna_02_l` | `2.425 cm` |
| `antenna_03_l` | `2.672 cm` |
| `antenna_04_l` | `2.945 cm` |

Interpretation:

- The Trail AnimBP graph is valid and produces runtime pose differences when the proof component explicitly overrides its Post Process AnimBP to `ABP_Bot_Trail_Study_C`.
- The response grows toward the antenna leaf, matching the intended `TrailBone=antenna_04_l` chain.
- Future scripted proof actors should call `set_override_post_process_anim_bp(..., true)` instead of relying only on the skeletal mesh asset default.
- Temporary proof actors were removed. The current map package remained dirty from reversible temp actor spawning; do not save `/Game/StackOBot/Maps/Lvl_Empty` for this run.

## Physics Pre/Post Evidence Synthesis

This pass combined the existing Baddy RigidBody source-vs-runtime split with the Bot active Trail runtime comparison. It was done offline from saved artifacts; Unreal Editor was not opened and original StackOBot assets were not touched.

Artifacts:

| Purpose | Path |
| --- | --- |
| Physics synthesis Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Physics_PrePostEvidenceSynthesis.md` |
| Physics synthesis JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Physics_PrePostEvidenceSynthesis.json` |
| Physics synthesis CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Physics_PrePostEvidenceSynthesis.csv` |
| Compiled node mapping summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphMapping_Summary.json` |
| Compiled node mapping raw | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphMapping_raw.json` |
| Compiled pose-link mapping summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphPoseLinks_Summary.json` |
| Compiled pose-link mapping raw | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphPoseLinks_raw.json` |
| PoseWatch same-instance pre/post summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PoseWatchPrePost_Summary.json` |
| PoseWatch same-instance pre/post raw | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PoseWatchPrePost_raw.json` |
| Trail PoseWatch same-instance pre/post summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TrailPoseWatchPrePost_Summary.json` |
| Trail PoseWatch same-instance pre/post raw | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TrailPoseWatchPrePost_raw.json` |

Current evidence:

| System | Current evidence | Strongest observation | Exact pre/post status |
| --- | --- | --- | --- |
| Baddy RigidBody | SIE runtime variant comparison, authored source-clip magnitude baseline, compiled node mapping, runtime pose-link mapping, and PoseWatch same-instance capture. | `pose_watch_capture` samples `ComponentPose` input link `11` vs RigidBody output link `1` in the same `ABP_Baddy_C` instance; `R_Stalk_04` delta is about `4.904 cm` / `26.743 deg`, `L_Stalk_04` about `5.546 cm` / `12.181 deg`. | `same_instance_posewatch_prepost_verified` |
| Bot Trail | SIE raw-vs-trail component comparison, isolated source-bypass vs post-node sampling, and Post Process AnimBP PoseWatch same-instance capture with `anim_instance_source=post_process`. | `pose_watch_capture` samples Trail input link `1` vs output link `4` in the same `ABP_Bot_Trail_Study_C` Post Process instance; `antenna_04_l` delta is about `0.110 cm` / `28.035 deg` in the smoke frame. | `same_instance_posewatch_prepost_verified_for_sample_postprocess_trail` |

Interpretation:

- The physics systems are proven active under the correct runtime setup.
- Baddy RigidBody now has exact same-instance PoseWatch input/output evidence for the selected compiled RigidBody node.
- Bot Trail now has exact same-instance PoseWatch input/output evidence for the sample Post Process Trail node, in addition to the earlier runtime raw-vs-trail comparison and isolated source-bypass sampling.
- `sample_anim_node_pre_post_runtime_pose(mode=compiled_graph_mapping)` now proves the `ABP_Baddy` RigidBody editor node maps to the live compiled `FAnimNode_RigidBody` instance in PIE: `same_anim_instance_node_mapping=true`, `runtime_node_instance_mapped=true`, `find_debug_anim_node_mapped=true`, and `pointer_match=true`.
- The mapping response now includes runtime pose-link inventory. For the RigidBody smoke, `ComponentPose` resolves to `AnimGraphNode_LocalToComponentSpace` with `LinkID=11`, `SourceLinkID=1`, and `linked_pointer_match=true`.
- `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` now uses transient debug-data PoseWatches and confirmed `runtime_graph_prepost=true`, `same_instance_prepost=true`, `transient_pose_watches=true`, `debug_object_restored=true`, and `original_assets_modified=false` on `ABP_Baddy`.
- The same mode now supports `anim_instance_source=post_process`; the Trail smoke resolved `ABP_Bot_Trail_Study_C`, output link `4`, input link `1`, and `same_instance_prepost=true` without modifying original StackOBot assets.

## Post Process Runtime and Static Pose Comparison

This pass measured the two Post Process AnimBP variants against a raw Bot pose:

- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadPitch`
- `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_AntennaRoll`

Artifacts:

| Purpose | Path |
| --- | --- |
| SIE dynamic raw-vs-variant samples | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_RuntimeSamples.json` |
| Static pose comparison Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.md` |
| Static pose comparison JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.json` |
| Static pose comparison CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.csv` |
| Static pose comparison chart | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.svg` |
| Pre/post isolation Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_PrePostPoseIsolation.md` |
| Pre/post isolation JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_PrePostPoseIsolation.json` |
| Pre/post isolation CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_PrePostPoseIsolation.csv` |
| Pre/post isolation chart | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_PrePostPoseIsolation.svg` |
| PoseWatch same-instance pre/post summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcessPoseWatchPrePost_Summary.json` |
| PoseWatch same-instance pre/post raw | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcessPoseWatchPrePost_raw.json` |

Main result:

| Variant | Bone | Key static result |
| --- | --- | --- |
| `HeadPitch` | `head` | About `5.99 deg` pitch-axis rotation change. |
| `HeadPitch` | `antenna_04_l` | About `8.59 cm` location delta from inherited head motion. |
| `HeadPitch` | `antenna_04_r` | About `8.61 cm` location delta from inherited head motion. |
| `AntennaRoll` | `antenna_04_l` | Exactly `12.0 deg` roll-axis change within floating-point tolerance. |
| `AntennaRoll` | `head`, `neck`, `antenna_04_r` | No meaningful change. |
| `HeadPitch` PoseWatch | `head` / antenna leaves | Same-instance Post Process input/output capture passed with output link `3`, input link `2`; `head` rotates about `6.0 deg`, and antenna leaves move about `8.59-8.61 cm`. |
| `AntennaRoll` PoseWatch | `antenna_04_l` | Same-instance Post Process input/output capture passed with output link `3`, input link `2`; only `antenna_04_l` rotates about `12.0 deg`. |
| `HeadYawAuthoringPattern` PoseWatch | `head` / antenna leaves | Same-instance Post Process input/output capture passed in editor-world no-SIE mode with output link `3`, input link `2`; `head` rotates about `8.0 deg`, and antenna leaves move about `11.74 cm`. |

Interpretation:

- The SIE dynamic comparison works as a runtime smoke test, but separate AnimInstances can drift in phase and introduce pelvis/head location noise.
- The static single-node `A_Bot_Idle` pose at time `0.0` is the authoritative isolation pass for these Post Process variants.
- The pre/post isolation table reclassifies that same static sample as `main-only A_Bot_Idle at time 0.0 -> Post Process variant output`.
- `HeadPitch` modifies a parent bone, so descendants move.
- `AntennaRoll` modifies a leaf bone, so only `antenna_04_l` rotates.
- Live PoseWatch captures used `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process)` on transient SIE actors and returned `runtime_graph_prepost=true` / `same_instance_prepost=true` for both variants.
- The `HeadYawAuthoringPattern` capture proves the same Post Process ModifyBone parent-bone behavior without SIE by using an editor-world transient actor, `prefer_pie_world=false`, and the existing C++ PoseWatch tick path. This is the safer route for static Post Process ModifyBone authoring proofs.
- Scripted proof actors should explicitly call `set_override_post_process_anim_bp(..., true)` on the component.
- A separate `sample_postprocess_pre_post_pose` command is no longer needed for these two learning fixtures.
