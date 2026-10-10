# StackOBot AnimBP Inventory

This is a compact read-only inventory of the original StackOBot Animation Blueprints.

Source artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimBP_ReadApiProbe.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Bot_AnimGraphInspect_Refresh.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/ABP_Baddy_AnimGraphInspect.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateGraph_NodeAssetProbe.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimBP_StateMachineInventory.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimBP_StateMachineInventory.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionInventory.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionInventory.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimationAsset_Inventory.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimationAsset_Inventory.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SequenceMotion_Profile.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SequenceMotion_ProfileMetrics.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SequenceMotion_Profile.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlueprintCallTopology_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlueprintCallTopology_raw.json`

## ABP_Bot

- Asset: `/Game/StackOBot/Characters/Bot/ABP_Bot.ABP_Bot`
- Target skeleton: `/Game/StackOBot/Characters/Bot/Mesh/SK_Bot.SK_Bot`
- Animation graph count: `25`
- AnimGraph node count: `30`

Main pose chain:

```text
GroundLocomotion -> Save GroundLocoPose
GroundLocoPose -> AirLocomotion -> Save LocomotionPose
LocomotionPose -> UpperBody Slot -> Save CashedPose_UpperBody
LocomotionPose + CashedPose_UpperBody -> LayeredBoneBlend -> Save FullBodyPose
IsInactive ? A_Bot_Idle_Inactive : FullBodyPose -> ControlRig -> Root
```

Important gates:

| Gate | Role |
| --- | --- |
| `IsInactive` | Drives the final `BlendListByBool`; true selects `A_Bot_Idle_Inactive`, false selects `FullBodyPose`. |
| `InteractWorldLocation` | Feeds Control Rig input `InteractionWorldLocation`. |
| `IsInAir?` | Goes through `NOT Boolean`; the result feeds Control Rig input `ShouldDoIKTrace`, so foot tracing is intended while not in air. |

State playback assets:

| State graph | Node | Asset | Play rate | Loop | Sync group |
| --- | --- | --- | ---: | --- | --- |
| `Idle` | `SequencePlayer` | `/Game/StackOBot/Characters/Bot/Animations/A_Bot_Idle.A_Bot_Idle` | `1.0` | `true` | `None` |
| `Walk/Run` | `BlendSpacePlayer` | `/Game/StackOBot/Characters/Bot/Animations/BS_Bot_WalkRunLean.BS_Bot_WalkRunLean` | `1.2` | n/a | `Run` |
| `Fall` | `SequencePlayer` | `/Game/StackOBot/Characters/Bot/Animations/A_Bot_Fall.A_Bot_Fall` | `1.0` | `true` | `None` |
| `LandIdle` | `SequencePlayer` | `/Game/StackOBot/Characters/Bot/Animations/A_Bot_LandIdle.A_Bot_LandIdle` | `1.0` | `true` | `None` |
| `LandRun` | `SequencePlayer` | `/Game/StackOBot/Characters/Bot/Animations/A_Bot_LandRun.A_Bot_LandRun` | `1.0` | `true` | `Run` |
| `StartJetpack` | `SequencePlayer` | `/Game/StackOBot/Characters/Bot/Animations/A_Bot_Hover_Start.A_Bot_Hover_Start` | `1.0` | `false` | `None` |
| `JetpackHovering` | `SequencePlayer` | `/Game/StackOBot/Characters/Bot/Animations/A_Bot_Hover_Loop.A_Bot_Hover_Loop` | `1.0` | `true` | `None` |
| `EndJetpack` | `SequencePlayer` | `/Game/StackOBot/Characters/Bot/Animations/A_Bot_Hover_End.A_Bot_Hover_End` | `1.0` | `true` | `None` |
| `Jump` | `BlendSpacePlayer` | `/Game/StackOBot/Characters/Bot/Animations/BS_Bot_RunIdleJump.BS_Bot_RunIdleJump` | `1.0` | n/a | `None` |

BlendSpace details:

| BlendSpace | Axes | Samples |
| --- | --- | --- |
| `BS_Bot_WalkRunLean` | `Lean -1..1`, `Speed 0..500` | `A_Bot_Walk` at `(0, 96.978)`, `A_Bot_Run` at `(0, 500)`, `A_Bot_Run_LeanLeft` at `(1, 258.546)`, `A_Bot_Run_LeanRight` at `(-1, 259.420)` |
| `BS_Bot_RunIdleJump` | `Speed 0..500` | `A_Bot_IdleJump` at `54.485`, `A_Bot_RunJump` at `54.552` |

Sequence timing summary:

- Bot sequence assets in this pass: `14`, total length about `18.04s`.
- Bot locomotion clips are sampled at about `25 fps`.
- Root motion is disabled on all inspected Bot sequences.
- BlendSpace notify trigger mode is `Highest Weighted Animation`.

AnimGraph comment takeaways:

- Ground locomotion is isolated and cached as `GroundLocoPose`.
- Air locomotion starts from `GroundLocoPose` and handles jump, fall, land, and jetpack states.
- The upper-body slot is layered above pelvis for button/interact montage behavior.
- The full body pose switches to inactive idle through `IsInactive`, then passes into Control Rig.
- Control Rig blends feet near surfaces and interaction IK from `InteractWorldLocation`.

Upper-body slot detail:

| Field | Value |
| --- | --- |
| Slot node | `AnimGraphNode_Slot_1` |
| Slot name | `UpperBody` |
| Source pose | `LocomotionPose` |
| Saved output | `CashedPose_UpperBody` |
| Layered blend node | `AnimGraphNode_LayeredBoneBlend_149` |
| Blend weight | `1.0` |
| Branch filters | `pelvis BlendDepth=4`, `thigh_r BlendDepth=-1`, `thigh_l BlendDepth=-1` |
| Artifact | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SlotLayeredBlend_Inventory.md` |
| Reference artifact | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_UpperBody_InteractionReferenceProbe.md` |

Slot interpretation:

- The overlay path is `LocomotionPose -> UpperBody Slot -> CashedPose_UpperBody -> LayeredBoneBlend`.
- The layered blend keeps `LocomotionPose` as the base pose and applies the upper-body cache as the overlay pose.
- The thigh exclusions protect leg branches from the button/interact overlay.
- Filename and AssetRegistry class scans found no Bot montage-like asset candidate; the only loaded `AnimMontage` found was `/Game/StackOBot/Characters/Blobling/Anim/AM_Baddy_Death.AM_Baddy_Death`.
- `IA_Interact` is referenced by `/Game/StackOBot/Input/IMC_ThirdPersonControls`, but `BP_Bot` dependencies did not include `IA_Interact` or a Bot montage asset.
- The graph comment is no longer the only evidence for the interact path. `inspect_blueprint_graph_call_topology` now shows `BP_Bot` routes the touch-interface `Interact` event through `Potential Interact` and grab component logic, with no direct `Montage` reference found in the smoked event/function topology.

## ABP_Baddy

- Asset: `/Game/StackOBot/Characters/Blobling/Anim/ABP_Baddy.ABP_Baddy`
- Target skeleton: `/Game/StackOBot/Characters/Blobling/SK_Baddy.SK_Baddy`
- Animation graph count: `5`
- AnimGraph node count: `6`

Main pose chain:

```text
New State Machine -> LocalToComponentSpace -> RigidBody -> ComponentToLocalSpace -> DefaultSlot -> Root
```

State playback assets:

| State graph | Node | Asset | Play rate | Loop |
| --- | --- | --- | ---: | --- |
| `A_Baddy_Idle` | `SequencePlayer` | `/Game/StackOBot/Characters/Blobling/Anim/A_Baddy_Idle.A_Baddy_Idle` | `1.0` | `true` |
| `A_Baddy_Walk` | `SequencePlayer` | `/Game/StackOBot/Characters/Blobling/Anim/A_Baddy_Walk.A_Baddy_Walk` | `1.0` | `true` |

Baddy sequence timing summary:

- Baddy sequence assets in this pass: `2`, total length about `1.33s`.
- `A_Baddy_Idle` and `A_Baddy_Walk` are both `0.667s`, `21` sampled keys, about `30 fps`.
- Root motion is disabled on both inspected Baddy sequences.

RigidBody takeaways:

- RigidBody runs in component space by default with `Alpha=1`.
- The RigidBody node sits between `LocalToComponentSpace` and `ComponentToLocalSpace`.
- World geometry collision is disabled in the original node.
- The output then passes through `DefaultSlot` before the root result.

## Transition Graph Inventory

Transition graph objects were first discovered through their owning graph paths. Exact source/target states and rule summaries are now captured through the read-only MCP C++ command `inspect_anim_state_machine_transitions`.

| Asset | State machine | Transition graph count | Transition nodes |
| --- | --- | ---: | --- |
| `ABP_Bot` | `GroundLocomotion` | 2 | `AnimStateTransitionNode_0`, `AnimStateTransitionNode_3` |
| `ABP_Bot` | `AirLocomotion` | 12 | `AnimStateTransitionNode_9`, `AnimStateTransitionNode_11`, `AnimStateTransitionNode_1`, `AnimStateTransitionNode_2`, `AnimStateTransitionNode_24`, `AnimStateTransitionNode_27`, `AnimStateTransitionNode_0`, `AnimStateTransitionNode_3`, `AnimStateTransitionNode_8`, `AnimStateTransitionNode_4`, `AnimStateTransitionNode_5`, `AnimStateTransitionNode_6` |
| `ABP_Baddy` | `New State Machine` | 2 | `AnimStateTransitionNode_0`, `AnimStateTransitionNode_1` |

Read result:

- Python-only inspection can read `AnimationTransitionGraph` paths and each graph's `AnimGraphNode_TransitionResult`, but cannot safely read full protected graph nodes/pins.
- The MCP C++ command reads exact transition source/target states from editor transition nodes rather than graph-path inference.
- `ABP_Bot` MCP inspect result: 2 state machines and 14 transitions.
- `ABP_Baddy` MCP inspect result: 1 state machine and 2 transitions.
- `ABP_Bot` ground locomotion is `Idle -> Walk/Run` on `GroundSpeed >`, and `Walk/Run -> Idle` on `GroundSpeed <=`.
- `ABP_Bot` air locomotion includes `IsHovering`, `IsInAir?`, and `MovementInput?` K2 gates plus automatic sequence-player completion transitions.
- `ABP_Baddy` transitions are `A_Baddy_Idle -> A_Baddy_Walk` on `Is Moving`, and `A_Baddy_Walk -> A_Baddy_Idle` on `NOT Is Moving`.
- MCP inspect artifact: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionMCPInspect.md`.
- Deep probe artifact retained as the Python protected-topology gap record: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TransitionTopology_DeepProbe.md`.

## Runtime State Probe

Runtime feasibility artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstance_StateApiProbe.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_UnrealPython_StateLibraryProbe.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstance_StateProbe_Setup.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstance_SIEDelayedMutabilityProbe.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstance_RuntimeStateProbeSummary.md`

Current runtime result:

- `ABP_Bot` instance variables are readable: `GroundSpeed`, `LeanAmount`, `MovementInput?`, `IsInAir?`, `IsInactive`, and `InteractWorldLocation`.
- `ABP_Baddy` instance variables are readable: `Is Moving` and `NewVar`.
- The Python `AnimInstance` wrapper does not expose current-state APIs such as `get_current_state_name`, `get_state_machine_instance_desc`, or `get_state_weight`.
- Editor-world and SIE runtime instances reject `set_editor_property` for these variables with `cannot be edited on instances`; direct `setattr` also fails.
- Delayed SIE produced a valid PIE world and runtime socket samples, so pose reading works. Controlled transition forcing does not work yet through plain Python.

## Post Process AnimBP Variant Samples

Compiled study variants under `/Game/_MCP_Sample/AnimStudy`:

| Variant | Bone | Additive rotation | Linked mesh | Linked actor |
| --- | --- | --- | --- | --- |
| `ABP_Bot_PostProcess_Study_HeadPitch` | `head` | `Pitch=6` | `SKM_Bot_PostProcess_Study_HeadPitch` | `BP_Bot_PostProcess_StudyActor_HeadPitch` |
| `ABP_Bot_PostProcess_Study_AntennaRoll` | `antenna_04_l` | `Roll=12` | `SKM_Bot_PostProcess_Study_AntennaRoll` | `BP_Bot_PostProcess_StudyActor_AntennaRoll` |

Verification artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantSetup.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantMeshLink.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_ComponentTemplateProbe.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantSummary.md`

Result:

- Both variant AnimBPs and actor Blueprints compile with `0` errors and `0` warnings.
- Both duplicated skeletal meshes point at their matching variant Post Process AnimBP.
- Original `SKM_Bot` still has no Post Process AnimBP assignment.
- Runtime sampling remains a future API task; these are compiled asset-level samples.

Impact map artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantImpactMap.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantImpactMap.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantImpactMap.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantImpactMap.svg`

Impact interpretation:

- `HeadPitch` expected affected bones: `head`, `antenna_04_l`, and `antenna_04_r`, based on the prior base `head` roll SIE proof.
- `AntennaRoll` expected affected bone: `antenna_04_l` only; hierarchy probe reports it as a leaf under `antenna_03_l`.
- Exact per-variant runtime deltas remain unclaimed until a safe pre/post Post Process pose sampler exists.

## Animation Asset Read Limits

- Sequence `Notifies` is protected through the current Python property path, so notify names were not expanded in this inventory.
- Sync markers and curve data were not exposed through the current Python property path.
- The asset-level timing and BlendSpace sample data are readable and should be used as the current source of truth for state playback study.
- Direct `AnimSequence.get_anim_pose_at_time` remains safe for source-pose motion profiling, but it does not replace notify, sync-marker, or curve introspection.
- Do not inspect `AnimMontage` internals with broad Unreal Python reflection in this project. A 2026-06-19 readiness retry against the loaded Baddy death montage asserted in `AnimMontage.h:770`; keep Montage evidence to AssetRegistry/name-level until a guarded native MCP command exists.

## Sequence Motion Profile

Direct `AnimSequence.get_anim_pose_at_time` sampling was used to compare authored clip movement before AnimBP graph layers are applied.

Top Bot authored motion:

| Asset | Bone | Main movement |
| --- | --- | --- |
| `A_Bot_Run_LeanLeft` | `foot_l` | Max distance from first pose about `53.10`; strongest inspected Bot clip delta. |
| `A_Bot_Run_LeanRight` | `foot_r` | Max distance about `52.70`. |
| `A_Bot_Run` | `foot_l` / `foot_r` | Max distances about `52.15` and `51.43`. |
| `A_Bot_RunJump` | `foot_r` | Max distance about `33.85`. |
| `A_Bot_LandRun` | `foot_l` | Max distance about `32.09`. |

Top Baddy authored motion:

| Asset | Bone | Main movement |
| --- | --- | --- |
| `A_Baddy_Walk` | `TailEnd` | Max distance from first pose about `36.71`; strongest inspected Baddy clip delta. |
| `A_Baddy_Walk` | `L_Stalk_04` | Max distance about `22.26`. |
| `A_Baddy_Walk` | `R_Stalk_04` | Max distance about `20.85`. |
| `A_Baddy_Idle` | stalk bones | Small idle motion, about `3.88` to `4.24` max distance. |

Motion profile limits:

- This measures authored sequence pose variation only.
- It does not include state transitions, BlendSpace interpolation, Control Rig, RigidBody, slot montages, or Post Process AnimBP.
- Use it to understand which source clips carry motion before runtime graph layers.

## Baddy RigidBody Source vs Runtime Split

Offline artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.svg`

Main result:

- The clean SIE runtime sample behaves like an idle-scale case for `Head_02` and `TailEnd`, not like `A_Baddy_Walk`.
- `Head_02` and `TailEnd` do not materially change across Baseline, AlphaHalf, ForceZ, and WorldSpace variants in the captured sample.
- RigidBody contribution is concentrated on `R_Stalk_04` and `L_Stalk_04`.
- `WorldSpace` makes stalk motion walk-scale: `R_Stalk_04` reached about `95.1%` of walk source max delta and `L_Stalk_04` reached about `103.6%`.
- This comparison is magnitude-level only. Exact per-frame source-vs-post-RigidBody subtraction remains a future runtime probe/API task.

## Bot BlendSpace Source Pose Map

Offline artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.svg`

Main result:

- `BS_Bot_WalkRunLean` maps neutral locomotion from `A_Bot_Walk` at `Lean=0`, `Speed=96.978` to `A_Bot_Run` at `Lean=0`, `Speed=500`.
- Lean samples sit at mid-speed: `A_Bot_Run_LeanLeft` at `Lean=1`, `Speed=258.546`, and `A_Bot_Run_LeanRight` at `Lean=-1`, `Speed=259.420`.
- The strongest authored foot deltas in this BlendSpace stay around run scale: `A_Bot_Run_LeanLeft foot_l` about `53.10`, `A_Bot_Run_LeanRight foot_r` about `52.70`, and `A_Bot_Run` feet about `51` to `52`.
- `BS_Bot_RunIdleJump` samples are almost coincident on the Speed axis: `A_Bot_IdleJump` at `54.485`, `A_Bot_RunJump` at `54.552`.
- This map does not evaluate runtime BlendSpace interpolation; use it as the source sample map next to the SIE pose grid below.

## Bot BlendSpace SIE Pose Grid

Runtime-style artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.svg`

Main result:

- `BS_Bot_WalkRunLean` input changes produced SIE pose changes, with max location delta from first sample `66.061 cm`.
- `BS_Bot_RunIdleJump` input changes produced SIE pose changes, with max location delta from first sample `35.438 cm`.
- A non-SIE full-editor single-node probe also ran and produced `0.0` deltas despite accepted input values, so it is retained as an API-gap artifact: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_LiveTickPoseGrid.md`.
- Use the SIE pose grid, not the non-SIE single-node probe, when discussing actual engine BlendSpace interpolation.

## Control Rig Contribution Synthesis

Offline artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_Contribution_Synthesis.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_Contribution_Synthesis.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_Contribution_Synthesis.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_Contribution_Synthesis.svg`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_DirectGateMCPProbe.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_DirectGateMCPProbe.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_DirectGateMCPProbe_Normalized.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ModifyCurveMCPEnsure.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ModifyCurveMCPEnsure_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ModifyCurveMCPEnsure_Normalized.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigInputDefaultsMCPSet.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigInputDefaultsMCPSet_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigInputDefaultsMCPSet_Normalized.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigForcedDriverMCPEnsure.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigForcedDriverMCPEnsure_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigForcedDriverMCPEnsure_Normalized.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPrePostMCPProbe.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPrePostMCPProbe_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPrePostMCPProbe_Normalized.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPoseWatchPrePost_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPoseWatchPrePost_raw.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstanceRuntimeState_MCPInspect.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstanceRuntimeState_MCPInspect_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimInstanceRuntimeState_MCPInspect_raw.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimRuntimePropertyMCPSet.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimRuntimePropertyMCPSet_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimStateMachineRuntimeResponseMCPProbe.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimStateMachineRuntimeResponseMCPProbe_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimStateMachineRuntimeResponseMCPProbe_raw.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimStateRuntimeMetrics_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimStateRuntimeMetrics_raw.json`

Main result:

- `ABP_Bot` has an active Control Rig node connected to the final AnimGraph root.
- The strongest authored BlendSpace source foot delta is about `53.10`.
- The strongest direct Control Rig foot delta in the existing direct-gate probe is `0.2402`, about `0.45%` of that source value.
- The SIE AnimBP probe produced `0.0` foot delta across tested phases because the interaction curve path stayed inactive (`IKBlend_l=0.0`, `IK_blend_interact=0.0`).
- `controlrig_direct_gate_probe` is now a repeatable read-only UnrealMCP command. StackOBot live smoke returned `success=true`, `read_only=true`, `asset_modified=false`, `6/6` successful cases, and `0` errors.
- `ensure_anim_graph_modify_curve_demo` created and saved sample asset `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ModifyCurve_Study`, forcing `IK_blend_interact=1.0` and `IKBlend_l=1.0` through a sample-only `ModifyCurve` node before the final root. The original `ABP_Bot` guard refused mutation, and the sample compile/save returned `0` errors and `0` warnings.
- `set_anim_graph_controlrig_input_defaults` created and saved sample asset `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_InputDefaults_Study`, refused original `ABP_Bot` mutation, disconnected linked `InteractionWorldLocation` and `ShouldDoIKTrace` pins in the sample, set them to `[80, -40, 80]` and `true`, compiled/saved with `0` errors and `0` warnings, and was idempotent on the second call with `graph_changed=false`.
- `ensure_controlrig_forced_driver_animbp` created and saved sample asset `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ForcedDriver_Study`, refused original `ABP_Bot` mutation, preserved the original upstream pose into `ModifyCurve -> ControlRig`, forced `IK_blend_interact=1.0`, `IKBlend_l=1.0`, `ShouldDoIKTrace=true`, and `InteractionWorldLocation=(80,-40,80)`, compiled/saved with `0` errors and `0` warnings, and was idempotent on the second call with `graph_changed=false`.
- `sample_controlrig_pre_post_runtime_pose` sampled a transient `CR_Bot_Correction` instance before and after `Forwards Solve` with forced driver values. StackOBot live smoke returned `read_only=true`, `asset_modified=false`, `runtime_source=direct_transient_controlrig`, `runtime_graph_prepost=false`, `0` errors, max translation delta `pelvis=20.9368`, and max rotation delta `calf_r=40.3937 deg`.
- `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` sampled the forced-driver sample's compiled `AnimGraphNode_ControlRig` input and output in the same runtime AnimInstance. StackOBot live smoke returned `runtime_graph_prepost=true`, `same_instance_prepost=true`, output link `42`, input `Source` link `45`, `debug_object_restored=true`, and `errors=[]` / `warnings=[]`. Strong deltas included `spine_03=35.386 cm`, `head=35.113 cm`, `pelvis=34.920 cm`, `calf_r=21.443 cm / 73.779 deg`, and `calf_l=19.181 cm / 78.123 deg`.
- `sample_skeletal_bones_in_sie` sampled a transient `SKM_Bot` actor from an active PIE/SIE world. StackOBot live smoke returned `read_only=true`, `asset_modified=false`, `sampled_world_type=PIE`, `is_play_session_active=true`, no warnings, and valid transforms for `pelvis`, `foot_l`, `foot_r`, `head`, `antenna_04_l`, and `antenna_04_r`.
- `inspect_anim_instance_runtime_state` sampled the same kind of transient `SKM_Bot` actor with `ABP_Bot_C` from active PIE/SIE. StackOBot live smoke returned `read_only=true`, `asset_modified=false`, `sampled_world_type=PIE`, `is_play_session_active=true`, `AirLocomotion=Walk/Run`, and `GroundLocomotion=Idle`; requested curve names were warning-only missing values on this idle smoke actor.
- `set_anim_instance_runtime_property_for_probe` set `bUseMultiThreadedAnimationUpdate=false` on the transient live `ABP_Bot_C` runtime instance. StackOBot live smoke returned `runtime_only=true`, `asset_modified=false`, `sampled_world_type=PIE`, `is_play_session_active=true`, and property echo `true -> false`.
- `sample_anim_state_machine_runtime_response` ran two restored runtime property cases for `bUseMultiThreadedAnimationUpdate=false/true`, forced bounded component animation ticks, and returned valid snapshots for `AirLocomotion=Walk/Run` and `GroundLocomotion=Idle` in both cases. This proves the response scaffold but not a meaningful locomotion transition.
- Runtime state snapshots now include `machine_weight`, per-state `state_weight` and `recorded_state_weight`, optional relevant animation timing, and transition progress.
- The metrics smoke captured `GroundLocomotion Idle -> Walk/Run` under `GroundSpeed=420`: after one `1/60s` tick, `elapsed_fraction=0.0833`, `Idle weight=0.9803`, `Walk/Run weight=0.0197`; after eight more ticks, `elapsed_fraction=0.75`, `Idle weight=0.15625`, `Walk/Run weight=0.84375`.
- The `IsInAir?=true` zero-duration transition guard case completed with no errors; inactive zero-crossfade transitions report `elapsed_fraction=0`.
- Exact compiled AnimGraph source-vs-post-ControlRig subtraction is now covered for the safe forced-driver sample through PoseWatch capture. For the original gameplay `ABP_Bot`, natural runtime ControlRig motion still depends on the interaction curve and trace gates becoming active during gameplay.

Remaining exact-runtime API:

- Use `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` for ControlRig, RigidBody, Trail, Post Process Modify Bone, and LayeredBoneBlend-style node attribution when the selected node exposes a valid runtime pose link.
- Add deeper instrumentation only if a future custom or unusual AnimGraph node does not expose evaluable runtime pose links through the current PoseWatch route.

## Bot Active Trail Sample

Sample-only active Trail assets:

| Asset role | Path |
| --- | --- |
| Trail Post Process AnimBP | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study` |
| Skeletal mesh with Post Process AnimBP set to `ABP_Bot_Trail_Study_C` | `/Game/_MCP_Sample/AnimStudy/SKM_Bot_Trail_Study` |
| Actor template using original main `ABP_Bot_C` and the Trail study mesh | `/Game/_MCP_Sample/AnimStudy/BP_Bot_Trail_StudyActor` |

Connected AnimGraph path:

```text
LinkedInputPose -> LocalToComponentSpace -> Trail -> ComponentToLocalSpace -> Root
```

Trail node settings:

- `TrailBone=antenna_04_l`
- `BaseJoint=head`
- `ChainLength=4`
- `ChainBoneAxis=X`
- `Alpha=1.0`

Notes:

- The retained original `ABP_Bot` Trail node still uses `VB VBHead` and remains disconnected.
- Connecting `VB VBHead` in the sample produced a compile warning because the skeleton could not find that bone, so the clean active sample uses `antenna_04_l`.
- `ABP_Bot_Trail_Study` and `BP_Bot_Trail_StudyActor` compile with `0` errors and `0` warnings.
- Runtime comparison artifacts were written to `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_Trail_RuntimeComparison.*`.
- Editor tick and SIE using only mesh-level Post Process defaults produced no measurable Trail-vs-raw difference on transient proof actors.
- SIE with explicit component-level `set_override_post_process_anim_bp(ABP_Bot_Trail_Study_C, true)` produced measurable Trail output.
- Strongest measured Trail-vs-raw distance was `antenna_04_l` at about `2.945 cm`; the response grows toward the antenna leaf.
- `sample_anim_node_pre_post_runtime_pose(mode=isolated_temp_components)` now provides isolated source-bypass vs post-node Trail evidence through disposable `_MCP_Temp` assets.
- The static no-FakeVelocity case stayed at noise level, about `0.000005 cm`; the controlled temp duplicate with `FakeVelocity=(0,0,80)` produced a strongest `antenna_04_l` delta of about `21.948 cm` translation and `34.072 deg` rotation.
- Trail isolated sampler artifacts were written to `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimNodePrePostTrailIsolatedTempSmoke_*`.
- Temporary proof actors were removed, but `/Game/StackOBot/Maps/Lvl_Empty` stayed dirty from reversible temp actor spawning. Discard by closing the editor without saving.

## Physics Pre/Post Evidence Synthesis

Offline artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Physics_PrePostEvidenceSynthesis.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Physics_PrePostEvidenceSynthesis.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Physics_PrePostEvidenceSynthesis.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphMapping_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphMapping_raw.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphPoseLinks_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphPoseLinks_raw.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PoseWatchPrePost_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PoseWatchPrePost_raw.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TrailPoseWatchPrePost_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TrailPoseWatchPrePost_raw.json`

Main result:

- Baddy RigidBody is proven through SIE runtime variants plus authored source-clip magnitude baselines.
- Bot Trail is proven through SIE raw-vs-trail component comparison when the Trail component explicitly overrides its Post Process AnimBP, isolated source-bypass vs post-node sampling, and same-instance PoseWatch capture on the `ABP_Bot_Trail_Study_C` Post Process AnimInstance.
- Current evidence is enough for the learning baseline. `sample_anim_node_pre_post_runtime_pose(mode=compiled_graph_mapping)` proves the `ABP_Baddy` RigidBody editor node maps to the live compiled `FAnimNode_RigidBody` instance in PIE with `pointer_match=true`, and its runtime `ComponentPose` link resolves to `AnimGraphNode_LocalToComponentSpace` with `LinkID=11`.
- `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` now captures `ABP_Baddy` RigidBody same-instance pre/post poses with `runtime_graph_prepost=true`, `same_instance_prepost=true`, output link `1`, input link `11`, and stalk deltas around `4.904 cm` / `26.743 deg` on `R_Stalk_04` and `5.546 cm` / `12.181 deg` on `L_Stalk_04`.
- `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process)` now captures `ABP_Bot_Trail_Study` same-instance pre/post poses with `runtime_graph_prepost=true`, `same_instance_prepost=true`, output link `4`, input link `1`, and `antenna_04_l` about `0.110 cm` / `28.035 deg`.
- Exact isolated source-vs-output RigidBody/Trail attribution is covered by `sample_anim_node_pre_post_runtime_pose(mode=isolated_temp_components)`; same-instance PoseWatch attribution is confirmed for Baddy RigidBody and Bot Trail sample paths.

## Bot Post Process Static Pose Comparison

Post Process comparison artifacts:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_RuntimeSamples.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.svg`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_PrePostPoseIsolation.md`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_PrePostPoseIsolation.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_PrePostPoseIsolation.csv`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_PrePostPoseIsolation.svg`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcessPoseWatchPrePost_Summary.json`
- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcessPoseWatchPrePost_raw.json`

Main result:

- `HeadPitch` changes `head` by about `5.99 deg` and moves descendant antenna leaf sockets about `8.6 cm`.
- `AntennaRoll` changes only `antenna_04_l` roll by `12.0 deg`; `head`, `neck`, and `antenna_04_r` remain unchanged within floating-point noise.
- Static pre/post isolation is represented as `main-only A_Bot_Idle at time 0.0 -> Post Process variant output`.
- Live same-instance PoseWatch capture also passed for both variants with `anim_instance_source=post_process`, `runtime_graph_prepost=true`, and `same_instance_prepost=true`. Both variants resolved output link `3` and input link `2`.
- The static single-node `A_Bot_Idle` comparison is the clean isolation result. The dynamic SIE run is a smoke test only because separate AnimInstances can drift in phase.
- Proof actors should explicitly set Post Process AnimBP through `set_override_post_process_anim_bp(..., true)`.

## Remaining Study Backlog

1. Control Rig pre/post: direct-gate MCP probing, sample ModifyCurve curve forcing, sample ControlRig input-default forcing, combined forced-driver sample assembly, direct transient ControlRig pre/post solve sampling, and reusable SIE bone/socket component sampling are complete; exact compiled AnimGraph source-vs-post subtraction still needs future instrumentation.
2. Physics pre/post: evidence synthesis is complete for the learning baseline; isolated source-bypass vs post-node RigidBody/Trail subtraction is covered by `sample_anim_node_pre_post_runtime_pose(mode=isolated_temp_components)`, live compiled-node address plus pose-link mapping is covered by `mode=compiled_graph_mapping`, and RigidBody plus sample Post Process Trail same-instance input/output capture is covered by `mode=pose_watch_capture`.
3. BlendSpace runtime pose grid: source pose mapping and SIE game-world pose-grid sampling are complete for `BS_Bot_WalkRunLean` and `BS_Bot_RunIdleJump`; use `StackOBot_BlendSpace_SIEPoseGrid.*` as current interpolation evidence.
4. Post Process pre/post: static single-input-pose isolation and live same-instance PoseWatch capture are complete for the two variants. A separate `sample_postprocess_pre_post_pose` command is no longer needed for these fixtures.
5. State-machine transitions: no-C++ topology probing is complete; `inspect_anim_state_machine_transitions` is implemented, build-verified, and StackOBot live-smoked on bridge port `55558`. Live current-state reading, state weights, transition progress, relevant animation timing, and runtime property case resampling are now covered by MCP APIs. Meaningful `ABP_Bot` driver cases are captured for `GroundSpeed`, `IsInAir?`, `MovementInput?`, and `IsHovering`.
6. Blueprint call topology: AssetRegistry-level interaction reference probing and exact read-only call topology are complete for `BP_Bot` and `BPC_InteractionHandler`. `BP_Bot` exposes `IA_Grab`, `BPI_TouchInterface.Interact`, `Potential Interact`, and grab init/clear/update links; no direct montage/dynamic-slot playback call was found in the smoked topology.

## Read Limitations

- Blueprint variables and function graphs are protected in Python on this UE 5.7 setup.
- Full pin/link-level transition condition internals are still not expanded through the Python path. Use the C++ MCP topology commands for static graph reads instead of protected Python reflection.
- Use `inspect_anim_state_machine_transitions` for exact transition source/target state names and rule graph summaries.
- Use `inspect_anim_instance_runtime_state` for current live PIE/SIE state names, state weights, transition progress, and relevant animation timing; use `set_anim_instance_runtime_property_for_probe` for runtime-only property echo checks; and use `sample_anim_state_machine_runtime_response` for restored property-case snapshots. The real `ABP_Bot` state-change matrix exists at `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ABP_Bot_RuntimeDriverMatrix.md`; the remaining state-machine gap is full K2 call topology.
- Use the SIE BlendSpace pose grid for current engine interpolation evidence; `sample_skeletal_bones_in_sie` is now available for the reusable live component bone/socket read step, while SIE startup/tick orchestration still remains external.
- Use `controlrig_direct_gate_probe` for repeatable direct-rig gate checks, `ensure_anim_graph_modify_curve_demo` for sample-only `IKBlend_l` / `IK_blend_interact` curve forcing, `set_anim_graph_controlrig_input_defaults` for sample-only `ShouldDoIKTrace` / `InteractionWorldLocation` input-default forcing, `ensure_controlrig_forced_driver_animbp` for the combined sample forced-driver AnimBP, `sample_controlrig_pre_post_runtime_pose` for direct transient same-instance ControlRig pre/post solve deltas, `sample_skeletal_bones_in_sie` for live PIE/SIE component bone/socket reads, `inspect_anim_instance_runtime_state` for live AnimInstance state-machine current-state reads, and the runtime property/response commands for case scaffolding. Use future compiled AnimGraph node-stack instrumentation for exact source-vs-output subtraction; the current synthesis only compares existing source, direct-rig, sample curve-forcing, sample input-default forcing, forced-driver graph assembly, direct-transient pre/post solve, SIE probe, runtime-state inspector, and runtime-property response artifacts.
- Use `sample_anim_node_pre_post_runtime_pose(mode=compiled_graph_mapping)` when the next instrumentation step needs to prove that an editor AnimGraph node GUID resolves to a live compiled `FAnimNode_*` instance. Use `mode=pose_watch_capture` for same-instance AnimGraph node input/output pose capture when the selected node has an evaluable runtime pose link path; add `anim_instance_source=post_process` for Post Process AnimBP nodes such as Trail and Transform Modify Bone.
- Use `inspect_anim_graph_protected_topology` for read-only static AnimGraph nodes, pins, pose-pin summaries, and normalized pose links. Current StackOBot artifacts are `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimGraphProtectedTopology_ControlRig_Summary.json` and `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimGraphProtectedTopology_ControlRig_raw.json`.
- Use `inspect_blueprint_graph_call_topology` to prove exact static Blueprint call/reference/link topology. Current StackOBot artifacts are `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlueprintCallTopology_Summary.json` and `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlueprintCallTopology_raw.json`.
- This inventory did not modify or save original StackOBot assets.
