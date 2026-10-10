# StackOBot Animation Execution Map

This note summarizes the current learning state for StackOBot animation systems. It is based on the MCP study artifacts in:

- `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy`

Authoring-pattern guide for future build requests:
`docs/stackobot-animbp-authoring-patterns.md`.

Quickstart:
`docs/stackobot-animation-quickstart.md`.

Closeout/readiness gate:
`docs/stackobot-animation-study-closeout.md`.

Request execution playbook:
`docs/stackobot-animation-request-playbook.md`.

Request compiler drills:
`docs/stackobot-request-compiler-drills.md`.

Physics request grammar:
`docs/stackobot-physics-request-grammar.md`.

Sample asset manifest:
`docs/stackobot-sample-asset-manifest.md`.

## Route Token Evidence Map

Use this after a route token is selected and deeper evidence context is needed. Quick execution should still start from `docs/stackobot-animation-quickstart.md`, `docs/stackobot-animation-route-matrix.md`, and `docs/stackobot-animation-mcp-command-syntax.md`.

| Route token | Evidence section | First read or authoring command | Verification command |
| --- | --- | --- | --- |
| `Post Process ModifyBone` | `Post Process Variant Samples`; `Post Process Runtime/Static Comparison` | `ensure_postprocess_anim_demo_variant` | `sample_anim_node_pre_post_runtime_pose` |
| `BlendSpace sample variant` | `Bot BlendSpace Source Pose Map`; `Bot BlendSpace SIE Pose Grid` | `ensure_blendspace_sample_variant` | `sample_blendspace_runtime_pose_grid` |
| `Bot Trail sample` | `Trail Controller Status`; `Physics Pre/Post Evidence Synthesis` | `ensure_anim_graph_trail_demo` | `sample_anim_node_pre_post_runtime_pose` |
| `UpperBody Slot and LayeredBlend` | `Main AnimBP Chains`; `Remaining Study Backlog` | `slot/cached-pose inventory` | `sample_anim_node_pre_post_runtime_pose` |
| `protected metadata boundary` | `Playback Asset Data`; `Deferred API Work` | `safe animation asset inventory` | `none for protected internals` |
| `ControlRig gate probe` | `Control Rig Gate Summary`; `Physics Pre/Post Evidence Synthesis` | `inspect_anim_graph_protected_topology`, then `controlrig_direct_gate_probe` | `sample_anim_node_pre_post_runtime_pose` |
| `state-machine runtime-driver proof` | `Transition Inventory Status`; `Runtime State Probe Status` | `inspect_anim_state_machine_transitions` | `sample_anim_state_machine_runtime_response` |
| `Baddy RigidBody` | `Baddy RigidBody Source vs Runtime Split`; `Physics Pre/Post Evidence Synthesis` | `inspect_anim_graph_node_settings` | `sample_anim_node_pre_post_runtime_pose` |
| `node resolver plus same-instance pre/post proof` | `Physics Pre/Post Evidence Synthesis`; `Sampling Checklist`; `Deferred API Work` | `inspect_anim_graph_protected_topology` plus `compiled mapping` | `sample_anim_node_pre_post_runtime_pose` |

## Runtime Pose Flow

```mermaid
flowchart LR
    Game["Gameplay / BP_Bot state"] --> AnimInstance["ABP_Bot AnimInstance"]
    AnimInstance --> StateMachine["Main AnimGraph state machine / cached poses"]
    StateMachine --> ControlRigNode["AnimGraphNode_ControlRig"]
    ControlRigNode --> FinalPose["AnimGraph Root Result"]
    FinalPose --> PostProcess["Optional SkeletalMesh Post Process AnimBP"]
    PostProcess --> ComponentPose["Final component-space skeletal pose"]
```

Current observations:

- `ABP_Bot` has an active ControlRig node in the root-connected final pose path.
- The original `SKM_Bot` has no Post Process AnimBP assigned.
- The duplicated study mesh `SKM_Bot_PostProcess_Study` proves Post Process AnimBP runs after the main AnimBP.
- The original Trail Controller node in `ABP_Bot` is disconnected and does not affect the final pose.

## System Roles

| System | Where it runs | Current StackOBot proof | Useful sample targets |
| --- | --- | --- | --- |
| Main AnimBP | `USkeletalMeshComponent` AnimInstance | `ABP_Bot` drives the base bot pose. | State machine node outputs, root chain, runtime variables. |
| Post Process AnimBP | SkeletalMesh post-process slot, after main AnimBP | Study mesh applied `head` roll delta of `4.0` degrees at runtime; two compiled variants target `head` pitch and `antenna_04_l` roll. | Parent/child bone rotation deltas: `neck_01`, `head`, antenna bones. |
| RigidBody node | AnimGraph physics pass inside AnimBP | Baddy study variants showed simulation-space and force differences on stalk bones. | Flexible bones: `R_Stalk_04`, `L_Stalk_04`, tail/head/foot controls. |
| Trail Controller | AnimGraph spring/trail node | Original `ABP_Bot` node exists but is disconnected. | `VB VBHead`, `head`, active chain output once a sample node is connected. |
| Control Rig | Active `AnimGraphNode_ControlRig` in `ABP_Bot` | Direct ControlRig probe proves `IK_blend_interact` gates foot/control movement. | `foot_l`, `foot_r`, `IK_foot_L`, `IK_foot_R`, interaction curves. |

## Main AnimBP Chains

`ABP_Bot`:

```text
GroundLocomotion -> Save GroundLocoPose
GroundLocoPose -> AirLocomotion -> Save LocomotionPose
LocomotionPose -> UpperBody Slot -> Save CashedPose_UpperBody
LocomotionPose + CashedPose_UpperBody -> LayeredBoneBlend -> Save FullBodyPose
IsInactive ? A_Bot_Idle_Inactive : FullBodyPose -> ControlRig -> Root
```

Key gates:

- `IsInactive`: selects inactive idle versus full-body pose before Control Rig.
- `InteractWorldLocation`: feeds Control Rig interaction location.
- `IsInAir?`: inverted through `NOT Boolean`, then drives Control Rig `ShouldDoIKTrace`.

Upper-body overlay:

- `AnimGraphNode_Slot_1` is the `UpperBody` slot in `DefaultGroup`.
- `LocomotionPose -> UpperBody Slot -> CashedPose_UpperBody -> LayeredBoneBlend.BlendPoses_0`.
- The `LayeredBoneBlend` base pose is also `LocomotionPose`; overlay weight is `1.0`.
- Branch filters are `pelvis BlendDepth=4`, `thigh_r BlendDepth=-1`, and `thigh_l BlendDepth=-1`.
- This means the interact/button overlay is intended to affect upper body branches while excluding leg branches.
- Artifact: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_SlotLayeredBlend_Inventory.md`.
- Request-template rehearsal artifacts: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_LayeredBlendTemplateRehearsal_Topology_Summary.json` and `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_LayeredBlendTemplateRehearsal_PoseWatch_Summary.json`.
- The 2026-06-19 rehearsal spawned transient `MCP_LayeredBlendTemplate_Rehearsal_Bot` in PIE and captured the target `LayeredBoneBlend` with `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, input_pose_mode=all)`.
- Runtime proof: `runtime_graph_prepost=true`, `same_instance_prepost=true`, output link `45`, `BasePose` link `49` from `LocomotionPose`, and `BlendPoses[0]` link `50` from `CashedPose_UpperBody`.
- No montage/action clip was played during that rehearsal, so near-zero pose deltas are expected; the proof is the existing overlay route and same-instance all-input capture, not a visible authored upper-body action.
- AssetRegistry probe artifact: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_UpperBody_InteractionReferenceProbe.md`.
- The probe found no Bot `AnimMontage` asset by class or filename. The only loaded `AnimMontage` asset found was Baddy death.
- `IA_Interact` is referenced by `IMC_ThirdPersonControls`; exact Blueprint node/call topology for the interact overlay remains unread through the current Python probe.

`ABP_Baddy`:

```text
New State Machine -> LocalToComponentSpace -> RigidBody -> ComponentToLocalSpace -> DefaultSlot -> Root
```

This is the cleanest sample in the project for studying where a physics AnimGraph node sits in the pose chain.

## Playback Asset Data

`ABP_Bot` state machines are driven by a small set of sequence and BlendSpace assets:

| Runtime value | Playback target | Asset data |
| --- | --- | --- |
| `GroundSpeed` + `LeanAmount` | `BS_Bot_WalkRunLean` | `Lean -1..1`, `Speed 0..500`; samples are walk, run, lean-left, and lean-right clips. |
| `GroundSpeed` while jumping | `BS_Bot_RunIdleJump` | `Speed 0..500`; samples are idle-jump and run-jump clips clustered around speed `54.5`. |
| Air/landing/jetpack states | Sequence players | `A_Bot_Fall`, `A_Bot_LandIdle`, `A_Bot_LandRun`, `A_Bot_Hover_Start`, `A_Bot_Hover_Loop`, `A_Bot_Hover_End`. |
| Baddy moving gate | Sequence players | `A_Baddy_Idle` and `A_Baddy_Walk`, both `0.667s` at about `30 fps`. |

Read result:

- The inspected Bot sequence set contains `14` sequences, totaling about `18.04s`.
- The inspected Baddy sequence set contains `2` sequences, totaling about `1.33s`.
- Root motion is disabled on all inspected sequences.
- Sequence notify, sync marker, and curve internals are still protected through the current Python read path.
- Existing safe asset-read artifacts are `StackOBot_AnimationAsset_Inventory.*` and `StackOBot_AnimationAsset_ReadApiProbe.json`.
- The safe Python path can read timing, skeleton, root-motion flags, BlendSpace samples, BlendSpace notify trigger mode, and source poses through `AnimSequence.get_anim_pose_at_time`.
- `AnimSequence.Notifies` is protected, `raw_curve_data` and `authored_sync_markers` are not exposed as Python properties, and `AnimSequence.get_data_model` is not available in this StackOBot UE 5.7 Python environment.
- A 2026-06-19 retry confirmed that broad Python reflection against `AnimMontage` internals is unsafe: probing the only loaded montage, `/Game/StackOBot/Characters/Blobling/Anim/AM_Baddy_Death.AM_Baddy_Death`, hit an `AnimMontage.h:770` assertion through `AnimationBlueprintLibrary` and closed the editor.
- Do not use generic Python reflection to inspect Montage slot tracks, sections, branching points, or notify data in this project. Keep montage internals at AssetRegistry/name-level until a guarded native MCP command exists.

## Source Clip Motion Profile

Direct sequence pose sampling gives a baseline before runtime graph layers:

| Area | Strongest inspected source motion | Interpretation |
| --- | --- | --- |
| Bot run/lean | `A_Bot_Run_LeanLeft foot_l` about `53.10`, `A_Bot_Run_LeanRight foot_r` about `52.70` max distance from first pose. | Ground locomotion foot motion is primarily authored in run/lean source clips before BlendSpace mixing. |
| Bot jump/land | `A_Bot_RunJump foot_r` about `33.85`, `A_Bot_LandRun foot_l` about `32.09`. | Air/landing states carry smaller but clear authored foot displacement. |
| Baddy walk | `A_Baddy_Walk TailEnd` about `36.71`, stalk bones about `20.85` to `22.26`. | Baddy source walk already moves tail/stalk bones before the RigidBody node adds secondary motion. |
| Baddy idle | Stalk bones about `3.88` to `4.24`. | Idle provides subtle authored motion, useful as baseline before physics simulation. |

Sampling interpretation:

- This profile is source-clip movement only.
- Final runtime movement can differ after BlendSpace interpolation, state transitions, Control Rig, RigidBody, slots, and Post Process AnimBP.
- Use source motion profile first, then compare against SIE/PIE final-pose samples when studying graph nodes.

## Post Process Variant Samples

Compiled sample variants under `/Game/_MCP_Sample/AnimStudy`:

| Variant | Target bone | Additive rotation | Verification |
| --- | --- | --- | --- |
| `ABP_Bot_PostProcess_Study_HeadPitch` | `head` | `Pitch=6` | AnimBP compile passed; matching duplicated mesh and actor linked. |
| `ABP_Bot_PostProcess_Study_AntennaRoll` | `antenna_04_l` | `Roll=12` | AnimBP compile passed; matching duplicated mesh and actor linked. |
| `ABP_Bot_PostProcess_Study_HeadYawAuthoringPattern` | `head` | `Yaw=8` | Sample-only authoring smoke passed compile/save/load and no-SIE editor-world same-instance PoseWatch verification. |
| `ABP_Bot_PostProcess_Study_HeadYawPlus5Study` | `head` | `Yaw=5` | Sample-only request-template rehearsal passed dry-run, compile/save, editor-world no-SIE same-instance PoseWatch, and transient actor cleanup. |

Artifact: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantSummary.md`.

Runtime note:

- The existing base Post Process sample has SIE proof for `head` roll propagation.
- The `HeadPitch` and `AntennaRoll` variants now have live same-instance PoseWatch sampling through `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process)`.
- The `HeadYawAuthoringPattern` variant proves the current authoring recipe can create a new sample-only Post Process Modify Bone asset pair without touching originals, then verify it through the existing C++ PoseWatch command without starting SIE.
- The `HeadYawPlus5Study` variant is the first request-template rehearsal using the documented `티브렛에게 전달할 지시` route. It created only `_MCP_Sample` targets and verified the same Post Process ModifyBone node in editor-world without SIE.
- PoseWatch artifacts: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcessPoseWatchPrePost_Summary.json` and `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcessPoseWatchPrePost_raw.json`.
- HeadYaw PoseWatch artifacts: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_HeadYawAuthoringPatternPoseWatchPrePost_Summary.json` and `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_HeadYawAuthoringPatternPoseWatchPrePost_raw.json`.
- HeadYawPlus5 artifacts: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcessHeadYawPlus5Ensure_raw.json`, `StackOBot_PostProcessHeadYawPlus5PoseWatch_raw.json`, `StackOBot_PostProcessHeadYawPlus5PoseWatch_Summary.json`, `StackOBot_PostProcessHeadYawPlus5_Setup.json`, and `StackOBot_PostProcessHeadYawPlus5_Cleanup.json`.

Impact map artifact: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_VariantImpactMap.md`.

Impact summary:

- `HeadPitch` affects `head` directly by about `6.0 deg`, and inherited child motion moves antenna leaf bones about `8.6 cm`.
- `AntennaRoll` affects only `antenna_04_l`, about `12.0 deg`; parent/sibling bones stay within floating-point noise.
- `HeadYawAuthoringPattern` affects `head` directly by about `8.0 deg`, and inherited child motion moves antenna leaf bones about `11.74 cm`; pelvis and neck stay within floating-point noise.
- `HeadYawPlus5Study` affects `head` directly by about `5.0 deg`, moves `antenna_04_l` about `7.339 cm` and `antenna_04_r` about `7.345 cm`, while pelvis and neck remain at floating-point noise.

## Baddy RigidBody Source vs Runtime Split

Offline comparison artifacts:

| Purpose | Path |
| --- | --- |
| Comparison Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.md` |
| Comparison CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.csv` |
| Comparison JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.json` |
| Chart SVG | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Baddy_RigidBody_SourceVsRuntime.svg` |

Result:

- The clean SIE runtime capture is closer to `A_Baddy_Idle` than `A_Baddy_Walk` for `Head_02` and `TailEnd`.
- `Head_02` stayed around `1.411` runtime max delta across all RigidBody variants; `TailEnd` stayed around `0.189`.
- RigidBody variant differences are concentrated on `R_Stalk_04` and `L_Stalk_04`.
- `WorldSpace` pushed `R_Stalk_04` to `19.836` max delta, about `95.1%` of the `A_Baddy_Walk` source value, and `L_Stalk_04` to `23.050`, about `103.6%` of walk source.
- Because the tail remains idle-scale while the stalks become walk-scale, treat this as simulation-space amplification rather than evidence that the walk clip is active.

## Bot BlendSpace Source Pose Map

Offline artifacts:

| Purpose | Path |
| --- | --- |
| Pose-map Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.md` |
| Pose-map CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.csv` |
| Pose-map JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.json` |
| Pose-map SVG | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_BlendSpace_SourcePoseMap.svg` |

Result:

- `BS_Bot_WalkRunLean` has a neutral walk/run line: `A_Bot_Walk` at `Lean=0`, `Speed=96.978`, and `A_Bot_Run` at `Lean=0`, `Speed=500`.
- The lean samples sit at mid-speed: `A_Bot_Run_LeanLeft` at `Lean=1`, `Speed=258.546`; `A_Bot_Run_LeanRight` at `Lean=-1`, `Speed=259.420`.
- Run and lean samples all carry about `51` to `53` max foot delta, so most big foot motion is authored before Control Rig correction.
- `BS_Bot_RunIdleJump` places `A_Bot_IdleJump` and `A_Bot_RunJump` almost on top of each other on the Speed axis (`54.485` vs `54.552`). Treat its authored sample choice as state/transition-driven; SIE axis probes now confirm the BlendSpace output can still change away from that narrow band.

## Bot BlendSpace SIE Pose Grid

Runtime-style artifacts:

| Purpose | Path |
| --- | --- |
| SIE pose grid Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.md` |
| SIE pose grid CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.csv` |
| SIE pose grid JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.json` |
| SIE pose grid SVG | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpace_SIEPoseGrid.svg` |
| Reusable MCP runtime pose-grid summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpaceRuntimePoseGridMCP_Summary.json` |
| Reusable MCP runtime pose-grid raw JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_BlendSpaceRuntimePoseGridMCP_raw.json` |

Result:

- `BS_Bot_WalkRunLean` changed under SIE input sampling; max location delta from the first sample was `66.061 cm`.
- `BS_Bot_RunIdleJump` changed under SIE input sampling; max location delta from the first sample was `35.438 cm`.
- Reusable MCP command `sample_blendspace_runtime_pose_grid` is now implemented and StackOBot live-smoked. Its controlled `UAnimSingleNodeInstance` sampling returned `BS_Bot_WalkRunLean` max delta `30.707 cm` and `BS_Bot_RunIdleJump` max delta `5.757 cm`.
- The non-SIE full-editor `AnimationSingleNode` path wrote `StackOBot_BlendSpace_LiveTickPoseGrid.*` but returned `0.0` pose deltas, so use it as an API-gap record rather than as animation evidence.
- For current study purposes, the execution map should treat BlendSpace interpolation as proven through SIE/game-world component tick. Use the reusable MCP command for repeatable audits; its fixed sample-time/forced-tick deltas are not expected to numerically match the older async SIE script exactly.
- Sample authoring route is now available through `ensure_blendspace_sample_variant`.
  `LeanWideStudy` created `/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanWideStudy`,
  widened left/right lean samples to `1.25/-1.25`, saved the target asset, and
  editor-world pose-grid fallback returned `input_changed_pose=true`.
- The request-template rehearsal for "make run lean wider left/right" was repeated under
  `StackOBot_BlendSpaceTemplateRehearsal_*`. It created
  `/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanTemplateRehearsal`,
  expanded the sample Lean axis to `-1.5..1.5`, moved `A_Bot_Run_LeanLeft`
  to `Lean=1.30`, moved `A_Bot_Run_LeanRight` to `Lean=-1.30`, saved only the
  sample target, and reported `original_assets_modified=false`. Follow-up
  `sample_blendspace_runtime_pose_grid(require_pie_world=false)` returned
  `success=true`, `runtime_only=true`, `asset_modified=false`, `valid_pose_count=3`,
  `input_changed_pose=true`, and max delta `5.046 cm` from the
  `run_lean_left_wide` input.

## Transition Inventory Status

Current read-only transition inventory:

| Asset | State machine | Transition graph count | Current read depth |
| --- | --- | ---: | --- |
| `ABP_Bot` | `GroundLocomotion` | 2 | Exact source/target states and K2 rule summaries through `inspect_anim_state_machine_transitions`. |
| `ABP_Bot` | `AirLocomotion` | 12 | Exact source/target states, automatic sequence-player rules, and K2 rule summaries through `inspect_anim_state_machine_transitions`. |
| `ABP_Baddy` | `New State Machine` | 2 | Exact source/target states and K2 rule summaries through `inspect_anim_state_machine_transitions`. |

Learning interpretation:

- The state-machine layout is now clear enough for source/target pose-flow study.
- Source and target state names are read from editor transition nodes by the MCP C++ command, not inferred from graph paths.
- Rule summaries distinguish K2 variable/function checks from automatic sequence-player rules.
- `ABP_Bot` ground locomotion is `Idle <-> Walk/Run` gated by `GroundSpeed`.
- `ABP_Bot` air locomotion includes explicit `IsHovering`, `IsInAir?`, and `MovementInput?` gates plus automatic completion transitions for landing/jump/jetpack sequences.
- `ABP_Baddy` is the compact two-state sample: `A_Baddy_Idle <-> A_Baddy_Walk` gated by `Is Moving`.
- Artifact: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachine_TransitionMCPInspect.md`.
- Earlier Python-only artifact retained as protected-topology gap evidence: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TransitionTopology_DeepProbe.md`.

## Runtime State Probe Status

Current non-C++ runtime-state feasibility:

| Probe | Result |
| --- | --- |
| Read AnimBP variables | Works for key variables such as `GroundSpeed`, `MovementInput?`, `IsInAir?`, `IsInactive`, `InteractWorldLocation`, and `Is Moving`. |
| Read current state name | Works through `inspect_anim_instance_runtime_state` / `sample_anim_state_machine_runtime_response`; blocked only through plain Python wrapper calls. |
| Force variables on AnimInstance | Works through `sample_anim_state_machine_runtime_response`; blocked only through plain Python wrapper calls. |
| Read runtime pose/socket data | Works in delayed SIE; Bot and Baddy runtime socket transforms were sampled. |

Interpretation:

- Runtime pose sampling is viable.
- Controlled transition sampling is viable through the reusable MCP command because it combines runtime state-name reading, safe variable forcing, ticking, and restore in one command.
- Plain Python remains insufficient for this route because it cannot reliably force instance variables and read current state names on this UE wrapper.
- Request-template rehearsal artifact: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_StateMachineRuntimeRehearsal_GroundSpeedResponse_Summary.json`.
- In that rehearsal, editor-world transient `SKM_Bot + ABP_Bot_C` stayed runtime-only: `GroundSpeed=0` sampled `GroundLocomotion=Idle`, `GroundSpeed=500` sampled `GroundLocomotion=Walk/Run`, and returning to `GroundSpeed=0` sampled `Idle`; `asset_modified=false`, `saves_assets=false`, `errors=[]`, and `warnings=[]`.

## Control Rig Gate Summary

```mermaid
flowchart TD
    ABPVar["ABP_Bot InteractWorldLocation"] --> CRInput["ControlRig input: InteractionWorldLocation"]
    TraceFn["ABP_Bot ShouldDoIKTrace function"] --> TraceInput["ControlRig input: ShouldDoIKTrace"]
    CurveGate["Curve: IK_blend_interact"] --> CRLogic["CR_Bot_Correction Forwards Solve"]
    IKBlend["Curve: IKBlend_l"] --> CRLogic
    CRInput --> CRLogic
    TraceInput --> CRLogic
    CRLogic --> Controls["IK_foot_L / IK_foot_R controls"]
    Controls --> Bones["foot_l / foot_r bones"]
```

Direct probe result:

- `ShouldDoIKTrace=true` alone did not move the feet.
- `IKBlend_l=1` alone did not move the feet.
- `IK_blend_interact=1` produced measurable foot/control deltas.
- The AnimBP SIE actor probe stayed at zero because the temporary runtime setup did not produce active `IK_blend_interact` curve values.
- The read-only UnrealMCP command `controlrig_direct_gate_probe` now repeats this direct transient ControlRig probe. StackOBot live smoke on bridge port `55558` returned `success=true`, `read_only=true`, `asset_modified=false`, `6/6` successful cases, and `0` errors. Artifacts: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_DirectGateMCPProbe.*`.

Contribution synthesis:

- Offline artifact: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRig_Contribution_Synthesis.md`.
- Strongest source BlendSpace foot delta: `A_Bot_Run_LeanLeft foot_l`, about `53.10`.
- Strongest direct Control Rig foot delta in the probe: `0.2402`, about `0.45%` of that source clip foot delta.
- SIE AnimBP phase deltas remained `0.0` for both raw skeletal actor and spawned `BP_Bot`, and the curve probe reported `IKBlend_l=0.0`, `IK_blend_interact=0.0`.
- `ensure_controlrig_forced_driver_animbp` now creates a sample duplicate that preserves the original `ABP_Bot` upstream pose, inserts `ModifyCurve -> ControlRig`, forces `IK_blend_interact=1.0`, `IKBlend_l=1.0`, `ShouldDoIKTrace=true`, and `InteractionWorldLocation=(80,-40,80)`, and compiles with `0` errors and `0` warnings. Artifacts: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigForcedDriverMCPEnsure.*`.
- `sample_controlrig_pre_post_runtime_pose` now provides read-only direct transient ControlRig pre/post solve evidence. StackOBot live smoke returned `runtime_source=direct_transient_controlrig`, `runtime_graph_prepost=false`, `asset_modified=false`, `0` errors, max translation delta `pelvis=20.9368`, and max rotation delta `calf_r=40.3937 deg`. Artifacts: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPrePostMCPProbe.*`.
- The request-template rehearsal for the forced-driver route was repeated under `StackOBot_ControlRigTemplateRehearsal_*`: direct gate preflight stayed read-only, the `interact_side` case moved `foot_l=0.09345 cm` and `foot_r=0.09338 cm` from baseline, the sample AnimBP stayed under `_MCP_Sample`, and delayed SIE PoseWatch returned `success=true`, `sampled_world_type=PIE`, `runtime_graph_prepost=true`, `same_instance_prepost=true`, `post_node_link_id=42`, `pre_input_link_id=45`, `errors=[]`, and `warnings=[]`. The strongest same-instance deltas were `spine_03=34.918 cm`, `head=34.639 cm`, `pelvis=34.443 cm`, `calf_r=21.330 cm / 74.215 deg`, and `calf_l=19.035 cm / 78.463 deg`.
- ControlRig PoseWatch setup caveat: an immediate SIE sample can match the PIE duplicate before its AnimInstance exists. Start SIE, then poll `inspect_anim_instance_runtime_state(require_pie_world=true)` until the PIE AnimInstance is present before calling `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)`.
- Treat Control Rig here as an active late-stage correction layer whose interaction branch requires a live curve/input gate, not as the source of the main locomotion foot arcs.

## Trail Controller Status

```mermaid
flowchart LR
    InputPose["ComponentPose input"] -. empty link .-> Trail["Trail Controller: VB VBHead"]
    Trail -. empty link .-> OutputPose["Pose output"]
```

Original Trail settings retained in `ABP_Bot`:

- `TrailBone`: `VB VBHead`
- `BaseJoint`: `head`
- `ChainLength`: `2`
- `ChainBoneAxis`: `X`
- `bReorientParentToChild`: true
- `Alpha`: `1.0`

Current interpretation:

- Treat the original Trail node as retained reference settings, not active runtime behavior.
- The active Trail learning sample now exists in `_MCP_Sample/AnimStudy`.
- Final active sample chain:
  `LinkedInputPose -> LocalToComponentSpace -> Trail -> ComponentToLocalSpace -> Root`.
- The first activation attempt with original `TrailBone=VB VBHead` compiled with a warning that the skeleton could not find `VB VBHead`. The clean sample therefore uses a real antenna chain instead.
- Clean sample settings: `TrailBone=antenna_04_l`, `BaseJoint=head`, `ChainLength=4`, `ChainBoneAxis=X`, `Alpha=1.0`.

Trail sample assets:

| Purpose | Path |
| --- | --- |
| Active Trail Post Process AnimBP | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study` |
| Skeletal mesh with Trail Post Process AnimBP | `/Game/_MCP_Sample/AnimStudy/SKM_Bot_Trail_Study` |
| Study actor template | `/Game/_MCP_Sample/AnimStudy/BP_Bot_Trail_StudyActor` |

Implementation note:

- UnrealMCP now has `ensure_anim_graph_trail_demo`, which creates or reuses the safe Trail demo chain and refuses non-`/Game/_MCP_Sample/` AnimBP edits unless explicitly allowed.
- `ABP_Bot_Trail_Study` compiled and saved with `0` errors and `0` warnings after switching to `antenna_04_l`.
- Runtime comparison artifacts:

| Purpose | Path |
| --- | --- |
| Runtime comparison Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_Trail_RuntimeComparison.md` |
| Runtime comparison JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_Trail_RuntimeComparison.json` |
| Runtime comparison CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_Trail_RuntimeComparison.csv` |
| Runtime comparison chart | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Bot_Trail_RuntimeComparison.svg` |

Runtime interpretation:

- Editor tick alone produced no measurable Trail-vs-raw difference.
- SIE with only the skeletal mesh asset's Post Process AnimBP assignment also produced no measurable Trail-vs-raw difference on transient proof actors.
- SIE with explicit runtime component override, `set_override_post_process_anim_bp(ABP_Bot_Trail_Study_C, true)`, produced a clear Trail-vs-raw difference.
- Strongest measured Trail-vs-raw distance was `antenna_04_l`, about `2.945 cm`.
- The response increases toward the antenna leaf, which matches the expected Trail Controller direction.
- Temporary SIE/editor proof actors were removed. The current map package stayed dirty from reversible temp actor spawning; close the editor without saving to discard it rather than saving over `/Game/StackOBot/Maps/Lvl_Empty`.
- The request-template rehearsal for "make the antenna lag/trail behind movement" was repeated under `StackOBot_TrailTemplateRehearsal_*`. `ensure_anim_graph_trail_demo` reused the existing connected sample chain without touching original assets, then SIE PoseWatch with explicit component-level Post Process override returned `success=true`, `sampled_world_type=PIE`, `anim_instance_source=post_process`, `runtime_graph_prepost=true`, `same_instance_prepost=true`, output link `4`, input `ComponentPose` link `1`, `errors=[]`, and `warnings=[]`. The strongest same-instance response was `antenna_04_l=0.133 cm / 28.033 deg`; `head` and `antenna_04_r` stayed at near-zero noise.

Isolated Trail source-vs-output sampler artifacts:

| Purpose | Path |
| --- | --- |
| Isolated Trail raw JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimNodePrePostTrailIsolatedTempSmoke_raw.json` |
| Isolated Trail summary JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimNodePrePostTrailIsolatedTempSmoke_Summary.json` |
| FakeVelocity matrix JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_AnimNodePrePostTrailIsolatedTempSmoke_FakeVelocityMatrix.json` |
| Temp cleanup check JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TrailIsolatedTempPostCheck.json` |

Isolated sampler interpretation:

- The no-FakeVelocity sample produced only floating-point noise, about `0.000005 cm`, because the Trail chain was static.
- A disposable `_MCP_Temp` duplicate of `ABP_Bot_Trail_Study` with `FakeVelocity=(0,0,80)` produced clear source-bypass vs post-node output.
- The strongest isolated Trail delta was `antenna_04_l`, about `21.948 cm` translation and `34.072 deg` rotation.
- Left antenna deltas increased toward the leaf (`antenna_02_l -> antenna_03_l -> antenna_04_l`), while `head` and `antenna_04_r` stayed at near-zero translation.
- The run cleaned all temp actors/assets and ended with `0` dirty content packages, `0` dirty map packages, and `0` assets under `/Game/_MCP_Temp/AnimNodePrePost`.
- This is isolated source-vs-output evidence. It still reports `runtime_graph_prepost=false` and `same_instance_prepost=false`; same-instance Trail attribution is now covered separately by `mode=pose_watch_capture` on the Post Process AnimInstance.

## Physics Pre/Post Evidence Synthesis

Physics synthesis artifacts:

| Purpose | Path |
| --- | --- |
| Synthesis Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Physics_PrePostEvidenceSynthesis.md` |
| Synthesis JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Physics_PrePostEvidenceSynthesis.json` |
| Synthesis CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_Physics_PrePostEvidenceSynthesis.csv` |
| Compiled node mapping summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphMapping_Summary.json` |
| Compiled node mapping raw | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphMapping_raw.json` |
| Compiled pose-link mapping summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphPoseLinks_Summary.json` |
| Compiled pose-link mapping raw | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_CompiledGraphPoseLinks_raw.json` |
| Trail PoseWatch same-instance pre/post summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TrailPoseWatchPrePost_Summary.json` |
| Trail PoseWatch same-instance pre/post raw | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_TrailPoseWatchPrePost_raw.json` |

Execution-map conclusion:

- Baddy RigidBody has active runtime evidence from SIE variants plus authored source-clip magnitude baselines.
- Bot Trail has active runtime evidence when the proof component explicitly overrides its Post Process AnimBP, isolated source-bypass vs post-node evidence through the `_MCP_Temp` sampler, and same-instance PoseWatch input/output capture through `anim_instance_source=post_process`.
- Both are sufficient for the current animation-learning baseline.
- `sample_anim_node_pre_post_runtime_pose(mode=compiled_graph_mapping)` now proves the selected `ABP_Baddy` RigidBody editor node maps to the live compiled `FAnimNode_RigidBody` instance in PIE: `same_anim_instance_node_mapping=true`, `runtime_node_instance_mapped=true`, `find_debug_anim_node_mapped=true`, and `pointer_match=true`.
- The same mode now reports runtime pose-link topology from the live node struct. The RigidBody smoke found `ComponentPose -> AnimGraphNode_LocalToComponentSpace` with `LinkID=11`, `SourceLinkID=1`, and `linked_pointer_match=true`.
- `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` now covers smoked same-instance paths for `ABP_Baddy` RigidBody and `ABP_Bot_Trail_Study` Post Process Trail. The Trail smoke resolved output link `4`, input link `1`, `ABP_Bot_Trail_Study_C`, and `same_instance_prepost=true`.
- RigidBody/Trail isolated source-vs-output subtraction remains covered by `sample_anim_node_pre_post_runtime_pose(mode=isolated_temp_components)`.

## Post Process Runtime/Static Comparison

Post Process comparison artifacts:

| Purpose | Path |
| --- | --- |
| SIE dynamic samples | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_RuntimeSamples.json` |
| Static comparison Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.md` |
| Static comparison JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.json` |
| Static comparison CSV | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.csv` |
| Static comparison chart | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_StaticPoseComparison.svg` |
| Pre/post isolation Markdown | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_PrePostPoseIsolation.md` |
| Pre/post isolation JSON | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcess_PrePostPoseIsolation.json` |
| PoseWatch same-instance pre/post summary | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcessPoseWatchPrePost_Summary.json` |
| PoseWatch same-instance pre/post raw | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_PostProcessPoseWatchPrePost_raw.json` |

Interpretation:

- Use the static single-node `A_Bot_Idle` comparison as the exact isolation proof. The dynamic SIE comparison is only a smoke test because separate AnimInstances can drift in phase.
- `HeadPitch` rotates `head` by about `5.99 deg`; descendant antenna leaf sockets move about `8.6 cm`.
- `AntennaRoll` rotates only `antenna_04_l` by `12.0 deg roll`; sibling/right antenna and head remain unchanged within floating-point noise.
- Pre/post static isolation is now expressed as `main-only A_Bot_Idle at time 0.0 -> Post Process variant output`.
- Live same-instance PoseWatch capture now confirms the same Post Process node-level result in PIE: both variants report `runtime_graph_prepost=true`, `same_instance_prepost=true`, output link `3`, and input link `2`.
- Runtime proof actors should set Post Process AnimBP through component override, not only through mesh defaults.

## Sampling Checklist

Use this checklist when adding or validating another animation experiment.

| Question | Check |
| --- | --- |
| Is the node in the root-connected final pose path? | Inspect graph links from `Root` backward. |
| Is the node merely retained/disconnected? | Check input/output pose pins for empty links. |
| Does editor tick show movement? | If physics or runtime trace logic is involved, prefer SIE/PIE sampling. |
| Are required curves active? | Probe `AnimInstance.get_curve_value()` and `get_all_curve_names()`. |
| Are variables writable but ineffective? | Confirm the downstream gate, not just property echo. |
| Did the test map introduce gameplay noise? | Override GameMode to `/Script/Engine.GameModeBase` when possible. |
| Did the test dirty assets? | Check dirty package count before closing the editor. |

## Remaining Study Backlog

| Priority | Topic | Next useful action | Dependency |
| ---: | --- | --- | --- |
| Done/PoseWatch | Slot and LayeredBoneBlend | Inventory complete for `UpperBody`, `CashedPose_UpperBody`, branch filters, filename/class montage evidence, AssetRegistry-level interaction references, read-only Blueprint call topology, and PIE same-instance all-input PoseWatch rehearsal. | `BP_Bot` topology shows interact/grab component flow, not a direct montage/dynamic-slot playback call. New overlay branch authoring remains a candidate API only for future requests. |
| Done/Runtime metrics | State-machine transitions | No-C++ transition topology probing is complete; live current-state reading, state weights, transition progress, relevant anim timing, runtime property setting, per-case state resampling, and meaningful `ABP_Bot` driver sequences are captured. | Full K2 call topology still needs follow-up API work. |
| Done/SIE pose grid + MCP API | BlendSpace runtime pose grid and sample authoring | Source pose map, SIE game-world pose grid, reusable `sample_blendspace_runtime_pose_grid`, and sample-only `ensure_blendspace_sample_variant` are complete for `BS_Bot_WalkRunLean` style edits. | Study evidence can use `StackOBot_BlendSpace_SIEPoseGrid.*`; repeatable MCP audits should use `StackOBot_BlendSpaceRuntimePoseGridMCP_*`. Authoring must still stay under `_MCP_Sample` unless original mutation is explicitly approved. |
| Done/PoseWatch + forced-driver | Control Rig pre/post | Direct-gate MCP probe, sample ModifyCurve curve-forcing, sample ControlRig input-default forcing, combined forced-driver sample assembly, direct transient ControlRig pre/post solve probe, and compiled AnimGraph same-instance PoseWatch capture are complete. | Same-instance ControlRig evidence uses the safe `_MCP_Sample` forced-driver AnimBP, not the original inactive `ABP_Bot` gate state. |
| Done/PoseWatch | Post Process pre/post | Static single-input-pose pre/post isolation and live same-instance PoseWatch capture are complete for the two variants. | Use `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process)` for comparable Post Process node checks. |
| Done/PoseWatch + isolated + mapping | Physics pre/post | Evidence synthesis complete for learning baseline; RigidBody/Trail isolated source-vs-output sampling is implemented and live-smoked; compiled runtime-node mapping and pose-link preflight are implemented and live-smoked; `ABP_Baddy` RigidBody and `ABP_Bot_Trail_Study` Post Process Trail same-instance PoseWatch pre/post capture are implemented and live-smoked. | Broader unusual node classes beyond the current RigidBody/Trail/ModifyBone/LayeredBoneBlend/ControlRig coverage may still need lower-level taps. |

## Deferred API Work

Implemented APIs to keep available for future audits:

1. `controlrig_direct_gate_probe` - implemented, build-verified, and StackOBot live-smoked; current artifacts are `StackOBot_ControlRig_DirectGateMCPProbe.*`.
2. `ensure_anim_graph_modify_curve_demo` - implemented, build-verified, and StackOBot live-smoked; current artifacts are `StackOBot_ModifyCurveMCPEnsure.*`.
3. `set_anim_graph_controlrig_input_defaults` - implemented, build-verified, and StackOBot live-smoked; current artifacts are `StackOBot_ControlRigInputDefaultsMCPSet.*`.
4. `ensure_controlrig_forced_driver_animbp` - implemented, build-verified, and StackOBot live-smoked; current artifacts are `StackOBot_ControlRigForcedDriverMCPEnsure.*`.
5. `sample_controlrig_pre_post_runtime_pose` - implemented, build-verified, and StackOBot live-smoked; current artifacts are `StackOBot_ControlRigPrePostMCPProbe.*`. This is direct transient ControlRig evidence, not compiled AnimGraph node-stack instrumentation.
6. `inspect_anim_state_machine_transitions` - implemented, build-verified, and StackOBot live-smoked; keep for future transition audits.
7. `sample_skeletal_bones_in_sie` - implemented, build-verified, synced into StackOBot, and live-smoked in PIE/SIE against a transient `SKM_Bot` actor; current artifacts are `StackOBot_SkeletalBonesInSIE_MCPProbe.*`.
8. `inspect_anim_instance_runtime_state` - implemented, build-verified, synced into StackOBot, and live-smoked in PIE/SIE against a transient `SKM_Bot` actor using `ABP_Bot_C`; it now reports state weights, optional relevant animation timing, and transition progress. Current artifacts include `StackOBot_AnimInstanceRuntimeState_MCPInspect.*` and `StackOBot_AnimStateRuntimeMetrics_*`.
9. `set_anim_instance_runtime_property_for_probe` - implemented, build-verified, synced into StackOBot, and live-smoked against a transient `ABP_Bot_C` runtime instance; current artifacts are `StackOBot_AnimRuntimePropertyMCPSet.*`.
10. `sample_anim_state_machine_runtime_response` - implemented, build-verified, synced into StackOBot, and live-smoked with restored runtime property cases plus active transition metric capture; current artifacts include `StackOBot_AnimStateMachineRuntimeResponseMCPProbe.*` and `StackOBot_AnimStateRuntimeMetrics_*`.
11. `inspect_blueprint_graph_call_topology` - implemented, build-verified, synced into StackOBot, and live-smoked against `BP_Bot` plus `BPC_InteractionHandler`; current artifacts are `StackOBot_BlueprintCallTopology_*`.
12. `sample_anim_node_pre_post_runtime_pose(mode=compiled_graph_mapping)` - implemented, build-verified in UnrealMCP and StackOBot, synced into StackOBot, and live-smoked against `ABP_Baddy` RigidBody. It maps editor node GUID `81E779C34D36CC52F0125F91BF52BAF3` to live compiled property `AnimGraphNode_RigidBody` / `/Script/AnimGraphRuntime.AnimNode_RigidBody` with pointer parity against `FindDebugAnimNode`, and now reports runtime pose-link topology such as `ComponentPose -> AnimGraphNode_LocalToComponentSpace`.
13. `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` - implemented, build-verified in UnrealMCP and StackOBot, synced into StackOBot, and live-smoked against `ABP_Baddy` RigidBody, `ABP_Bot_Trail_Study` Post Process Trail, `ABP_Bot` LayeredBoneBlend all-input capture, and `_MCP_Sample` `ABP_Bot_ControlRig_ForcedDriver_Study` ControlRig. It uses transient debug-data PoseWatches to capture selected output and selected input pose links in the same runtime AnimInstance. RigidBody artifacts are `StackOBot_PoseWatchPrePost_*`; Trail artifacts are `StackOBot_TrailPoseWatchPrePost_*`; multi-input LayeredBoneBlend artifacts are `StackOBot_LayeredBlendPoseWatchAllInputs_*` and request-template rehearsal artifacts are `StackOBot_LayeredBlendTemplateRehearsal_*`; ControlRig artifacts are `StackOBot_ControlRigPoseWatchPrePost_*`.
14. `ensure_anim_graph_trail_demo` - implemented, build-verified, and StackOBot live-smoked; it creates/reuses safe `_MCP_Sample` Trail Post Process AnimBP chains and refuses original asset mutation by default.
15. `inspect_anim_graph_protected_topology` - implemented, build-verified in UnrealMCP and StackOBot, synced into StackOBot, and live-smoked against `ABP_Bot` ControlRig topology. Current artifacts are `StackOBot_AnimGraphProtectedTopology_ControlRig_*`.
16. `ensure_postprocess_anim_demo_variant` - implemented, build-verified in UnrealMCP and StackOBot, synced into StackOBot, and live-smoked against the existing `HeadPitch` Post Process sample. It duplicates/reuses sample-only AnimBP/SkeletalMesh targets, ensures the Modify Bone chain, compiles the AnimBP, assigns the SkeletalMesh Post Process AnimBlueprint, saves, and reports `original_assets_modified=false`. Current artifacts are `StackOBot_PostProcessDemoVariantEnsure_*`.
17. `sample_blendspace_runtime_pose_grid` - implemented, build-verified in UnrealMCP and StackOBot, synced into StackOBot, and live-smoked against `BS_Bot_WalkRunLean` plus `BS_Bot_RunIdleJump` in PIE/SIE. It uses a transient `SkeletalMeshActor`, fixed sample time, bounded forced ticks, and cleanup-only runtime sampling. Current artifacts are `StackOBot_BlendSpaceRuntimePoseGridMCP_*`.
18. `ensure_blendspace_sample_variant` - implemented in the StackOBot UnrealMCP plugin and sibling Python wrapper, build-verified with `UnrealEditor Win64 Development -Project=StackOBot.uproject`, and live-smoked against `BS_Bot_WalkRunLean`. It created `/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanWideStudy`, edited lean samples to `1.25/-1.25`, saved the target, reported `original_assets_modified=false`, and was followed by `sample_blendspace_runtime_pose_grid(require_pie_world=false)` with `valid_pose_count=3` and `input_changed_pose=true`.

### StackOBot UnrealMCP plugin sync boundary

- The active StackOBot animation-study command surface lives in `<workspace-parent>/SampleProject/StackOBot/Plugins/UnrealMCP`. That sample project is not a git repo, so command availability must be confirmed against that plugin copy before relying on it.
- `<workspace-parent>/CubelessStylized/Plugins/UnrealMCP` is not currently equivalent to the StackOBot plugin copy. Its `UnrealMCPBlueprintNodeCommands.cpp` is an older command surface and does not include the latest StackOBot animation-study helpers such as `ensure_blendspace_sample_variant`.
- Do not narrow-paste `ensure_blendspace_sample_variant` into the older Cubeless plugin as a quick fix; it depends on helper code from the newer animation-study command surface. Treat any Cubeless-side reuse as a deliberate plugin sync/port task with build verification.
- For future StackOBot animation requests, use the StackOBot editor plus StackOBot-local UnrealMCP plugin bridge unless the UnrealMCP plugin has been intentionally synced and verified in another project.

Remaining C++/UnrealMCP candidates if reusable tooling is explicitly resumed:

1. Expand same-instance AnimGraph pre/post capture only if a new unusual node class falls outside the smoked RigidBody/Trail/Post Process ModifyBone/LayeredBoneBlend/ControlRig paths.
2. Add `ensure_state_machine_sample_variant` only if runtime property-driver proofs are not enough and a future request needs a safe duplicated AnimBP with new state/transition authoring.
3. Add `ensure_layered_slot_overlay_sample` only if a future request needs a new overlay branch rather than the existing `UpperBody` slot/cached-pose/LayeredBoneBlend route.
4. Add `ensure_postprocess_physics_variant` only if the existing Bot Trail and Baddy RigidBody samples cannot express the requested secondary-motion behavior.
5. Add a target actor resolver only if repeated PoseWatch proof setup fails because of transient actor, component override, PIE duplicate matching, or editor-world fallback issues.
6. Add notify/curve/montage read-authoring support only when a concrete request depends on sequence notifies, sync markers, protected curve data, or montage internals. This should be a guarded native command, not broad Python reflection, because a direct Python montage-internals probe asserted in `AnimMontage.h:770`.

Implemented `sample_skeletal_bones_in_sie` detail:

- Scope: immediate read-only sampling from the current active PIE/SIE `SkeletalMeshComponent`, with editor-world fallback when no play world exists.
- Inputs: actor label/name/path, component name, bone names, socket names, and `prefer_pie_world`.
- Outputs: sampled world type/name, play-session state, actor/component metadata, world/component transforms for requested bones/sockets, warnings, and errors.
- Limitation: the command does not start SIE or tick frames by itself; use Python/editor orchestration to create or advance the runtime pose first.

Implemented `inspect_anim_instance_runtime_state` detail:

- Scope: immediate read-only inspection of the current `SkeletalMeshComponent` `AnimInstance`, preferring active PIE/SIE and falling back to the editor world.
- Inputs: actor label/name/path, component name, state-machine name filter, montage/curve/state inclusion flags, and bounded state-machine/state/curve limits.
- Outputs: sampled world type/name, play-session state, actor/component/AnimInstance metadata, current state-machine names/indexes/elapsed time, optional state metadata, state weights, optional relevant animation timing, transition progress, active montage summary, curve values, warnings, and errors.
- Runtime index safety: the implementation probes live `FAnimNode_StateMachine` instances and maps them back to baked class data through `StateMachineIndexInClass`. Runtime getter calls use the discovered `machine_instance_index`, not the baked class index.
- StackOBot smoke result: transient `SKM_Bot` plus `ABP_Bot_C` in SIE returned `AirLocomotion=Walk/Run`, `GroundLocomotion=Idle`, `read_only=true`, `asset_modified=false`, and `sampled_world_type=PIE`.

Implemented runtime property/state-response detail:

- `set_anim_instance_runtime_property_for_probe` sets supported reflected properties on the matched live `UAnimInstance` only. It supports booleans, numbers, strings/names, vectors, rotators, and transforms through the shared runtime property writer.
- `sample_anim_state_machine_runtime_response` applies case property maps, calls bounded `USkeletalMeshComponent::TickAnimation`, samples state-machine snapshots, and restores successful property changes per case by default.
- StackOBot smoke used `bUseMultiThreadedAnimationUpdate` as a safe base `UAnimInstance` bool property to verify set/echo/tick/snapshot/restore behavior: `success=true`, `runtime_only=true`, `asset_modified=false`, `sampled_world_type=PIE`, `case_count=2`, and `successful_case_count=2`.
- Meaningful `ABP_Bot` runtime driver matrix is now captured in `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ABP_Bot_RuntimeDriverMatrix.md`.
- Confirmed drivers: `GroundSpeed`, `IsInAir?`, `MovementInput?`, and `IsHovering`.
- Confirmed sequences: `GroundLocomotion Idle <-> Walk/Run`, `Walk/Run -> Jump -> Fall`, landing split to `LandIdle`/`LandRun`, and jetpack path `Fall -> StartJetpack -> JetpackHovering -> Fall`.
- Runtime metrics are captured in `StackOBot_AnimStateRuntimeMetrics_*`: `GroundSpeed=420` captured active `GroundLocomotion Idle -> Walk/Run` transition progress from `elapsed_fraction=0.0833` to `0.75`, with matching per-state weights.
- The metrics smoke also includes an `IsInAir?=true` zero-duration transition guard; inactive zero-crossfade transitions report `elapsed_fraction=0`.
- Limitation: full K2 call topology is not captured yet.

Implemented `sample_controlrig_pre_post_runtime_pose` and ControlRig PoseWatch detail:

- Scope: transient ControlRig instance only. It is read-only and does not save original StackOBot assets.
- Inputs: ControlRig path/class, bone/control names to sample, driver variables such as `InteractionWorldLocation` and `ShouldDoIKTrace`, forced curves such as `IKBlend_l` and `IK_blend_interact`, and execute events.
- Outputs: pre-solve pose, post-solve pose, per-bone/per-control deltas, active curve values, driver-variable echo, execution status, and artifact paths.
- Direct ControlRig command limitation: it reports `runtime_graph_prepost=false` because it samples a transient rig, not the compiled AnimGraph node stack.
- Compiled AnimGraph same-instance ControlRig source-vs-post is now covered by `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` against `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ForcedDriver_Study`. The smoke resolved ControlRig output link `42`, input `Source` link `45`, `runtime_graph_prepost=true`, `same_instance_prepost=true`, `debug_object_restored=true`, and `errors=[]` / `warnings=[]`.
- ControlRig PoseWatch artifacts: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPoseWatchPrePost_Summary.json` and `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPoseWatchPrePost_raw.json`.

`sample_anim_node_pre_post_runtime_pose` detail:

- Scope: selected AnimGraph physics, transform, blend, and ControlRig nodes such as RigidBody, Trail, Transform Modify Bone, LayeredBoneBlend, and ControlRig, using duplicate `/Game/_MCP_Sample/AnimStudy` assets or transient runtime components only.
- Inputs: AnimBP or Post Process AnimBP path, skeletal mesh path, node selector, runtime mode, driver setup, settle ticks, duration/rate, and bones.
- Outputs: pre-node pose, post-node pose, per-bone deltas, runtime state/curve values when available, cleanup status, dirty-package status, and artifact paths.
- Safety: read-only by default, no Python map loading, no original asset mutation, and explicit reporting when component-level Post Process override is required.

`inspect_blueprint_graph_call_topology` detail:

- Scope: read-only inspection of selected Blueprint assets such as `BP_Bot` and `BPC_InteractionHandler`.
- Inputs: Blueprint path/name, optional graph selector, optional graph/name/node/reference filters, and bounded graph/node/link/reference limits.
- Outputs: graph nodes, classified K2 node kinds, function/variable/event member references, Enhanced Input action references, object/asset/class paths, and normalized pin links.
- StackOBot smoke result: `BP_Bot` `Interact` references resolved to `BPI_TouchInterface.Interact`, `Potential Interact`, and the grab-init/clear/update path; `Montage` references returned zero nodes in the smoked `BP_Bot` topology.
- Limitation: this is static Blueprint topology only. It does not prove runtime branch execution; combine it with PIE/SIE runtime probes when execution evidence is required.
