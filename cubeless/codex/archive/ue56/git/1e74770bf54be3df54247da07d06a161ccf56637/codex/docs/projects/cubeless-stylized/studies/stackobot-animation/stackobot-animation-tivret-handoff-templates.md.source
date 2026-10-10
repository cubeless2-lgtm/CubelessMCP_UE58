# StackOBot Animation Tivret Handoff Templates

Use these blocks when Ieta has classified a StackOBot animation request and must
show the exact instruction before Tivret executes asset or editor work.

Default rule: keep original StackOBot assets read-only and work under
`/Game/_MCP_Sample/AnimStudy` unless the user explicitly approves original asset
mutation. Always report C++/API status separately.

Before showing a block to the user, replace bracket placeholders with the current
request details. If a value is unknown but non-blocking, state the assumption in
the block; if it changes asset mutation, billing, or C++ scope, ask first.

## Route Token To Handoff

| Route token | Handoff section | Notes |
| --- | --- | --- |
| `Post Process ModifyBone` | `Post Process ModifyBone` | Sample Post Process AnimBP and same-instance PoseWatch. |
| `BlendSpace sample variant` | `BlendSpace Sample Variant` | Sample BlendSpace edit and pose-grid proof. |
| `Bot Trail sample` | `Trail Or Secondary Motion` | Bot antenna Trail sample route. |
| `UpperBody Slot and LayeredBlend` | `UpperBody Slot And LayeredBlend` | Route proof first; visible action source remains separate. |
| `protected metadata boundary` | `Notify, Curve, Sync Marker, Or Montage Internals` | Safe inventory first; guarded native API only if blocked. |
| `ControlRig gate probe` | `ControlRig Late Correction` | Direct gate probe, then forced-driver sample if needed. |
| `state-machine runtime-driver proof` | `State Machine Or Runtime Driver` | Read/runtime proof before graph authoring. |
| `Baddy RigidBody` | `Trail Or Secondary Motion` | RigidBody setting read or sample tuning. |
| `node resolver plus same-instance pre/post proof` | `no authoring handoff` | Instrumentation route; use final report fields unless a sample actor setup is needed. |

## Post Process ModifyBone

```text
티브렛에게 전달할 지시

StackOBot 원본 `ABP_Bot`, `SKM_Bot`, 맵, 원본 애셋은 수정/저장하지 말 것.
요청 목적은 [target bone/body area]에 대한 late Post Process bone adjustment다.

`ensure_postprocess_anim_demo_variant`로 `/Game/_MCP_Sample/AnimStudy` 아래에
샘플 Post Process AnimBP와 샘플 SkeletalMesh를 만들거나 재사용할 것.
대상 bone은 `[BoneName]`, additive rotation/translation/scale intent는
`[TransformIntent]`로 적용한다.

검증은 transient actor로 진행하고, 필요 없으면 SIE를 시작하지 않는다.
`sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture,
anim_instance_source=post_process)`로 target node input/output을 같은
AnimInstance에서 캡처한다.

완료 조건: `runtime_graph_prepost=true`, `same_instance_prepost=true`,
target bone delta가 의도와 맞음, `errors=[]`, 원본 asset mutation 없음,
transient actor cleanup 완료.
```

## BlendSpace Sample Variant

```text
티브렛에게 전달할 지시

StackOBot 원본 BlendSpace는 수정하지 말 것. 요청 목적은 `[BlendIntent]`다.

소스 BlendSpace `[SourceBlendSpace]`를 기준으로
`/Game/_MCP_Sample/AnimStudy/[SampleBlendSpace]` 샘플 변형을 만들거나 재사용한다.
`ensure_blendspace_sample_variant`로 axis edit `[AxisEdits]`와 sample edit
`[SampleEdits]`를 적용하고, 저장 대상은 샘플 BlendSpace만 허용한다.

검증은 `sample_blendspace_runtime_pose_grid`로 `[PoseGridInputs]`를 샘플링한다.
원본 asset 변경 여부, 유효 pose 수, 입력별 pose delta를 summary로 남긴다.

완료 조건: `original_assets_modified=false`, `valid_pose_count`가 예상값,
`input_changed_pose=true`가 필요한 입력에서 확인됨, dirty package 없음.
```

## State Machine Or Runtime Driver

```text
티브렛에게 전달할 지시

원본 `ABP_Bot`과 맵은 수정/저장하지 말 것. 이번 작업은 `[StateIntent]`가
현재 state-machine/runtime-driver grammar로 표현되는지 확인하는 읽기/런타임
검증이다.

먼저 `inspect_anim_state_machine_transitions`로 `[StateMachineName]` topology를
읽는다. 그 다음 transient `SKM_Bot + ABP_Bot_C` actor를 만들고
`sample_anim_state_machine_runtime_response`로 cases `[RuntimeCases]`를 적용한다.

각 case는 runtime AnimInstance property만 수정하고, 성공 후 원래 값으로 복구한다.
필요하면 SIE/PIE를 사용하되 원본 asset 저장은 하지 않는다.

완료 조건: 각 case의 current state/transition metric이 의도와 맞음,
runtime-only 결과, `asset_modified=false`, `saves_assets=false`, cleanup 완료.
```

## ControlRig Late Correction

```text
티브렛에게 전달할 지시

원본 `ABP_Bot`, `CR_Bot_Correction`, 맵은 수정/저장하지 말 것. 요청 목적은
`[ControlRigIntent]`이며, ControlRig gate와 compiled AnimGraph 결과를 분리해서
검증한다.

먼저 `inspect_anim_graph_protected_topology`로 ControlRig node가 root-connected인지
확인한다. `controlrig_direct_gate_probe`로 `[ControlRigCases]`를 실행해
`ShouldDoIKTrace`, `InteractionWorldLocation`, `IK_blend_interact`, `IKBlend_l`
같은 gate 반응을 확인한다.

gameplay gate가 transient actor에서 활성화되지 않으면
`ensure_controlrig_forced_driver_animbp`로 `/Game/_MCP_Sample/AnimStudy` 샘플
AnimBP를 만들거나 재사용한다. SIE 시작 후
`inspect_anim_instance_runtime_state(require_pie_world=true)`를 polling해서
PIE AnimInstance가 살아난 뒤 PoseWatch를 실행한다.

완료 조건: direct probe로 gate 반응 확인, forced-driver sample이 original asset을
수정하지 않음, ControlRig PoseWatch에서 `runtime_graph_prepost=true`,
`same_instance_prepost=true`, cleanup 완료.
```

## UpperBody Slot And LayeredBlend

```text
티브렛에게 전달할 지시

StackOBot 원본 `ABP_Bot`, `BP_Bot`, 맵은 수정/저장하지 말 것. 요청 목적은
이동 중 상체 액션 `[UpperBodyIntent]`를 기존 `UpperBody` Slot/LayeredBlend
route로 해석할 수 있는지 확인하는 것이다.

먼저 `StackOBot_SlotLayeredBlend_Inventory.md`와 기존 topology를 기준으로
`LocomotionPose -> UpperBody Slot -> CashedPose_UpperBody -> LayeredBoneBlend`
경로를 확인한다.

transient `SKM_Bot + ABP_Bot_C` actor를 만들고, 필요하면 SIE/PIE에서
`sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture,
input_pose_mode=all)`를 실행한다. 대상 node는 `ABP_Bot`의 LayeredBoneBlend
`A6513D7A4006C58E2BC82AADE84F15F6`를 우선 사용한다.

완료 조건: `BasePose`가 `LocomotionPose`, `BlendPoses[0]`가
`CashedPose_UpperBody`로 같은 AnimInstance에서 캡처됨. 실제 action source가
없으면 near-zero delta는 route proof로만 해석하고, visible action proof라고
말하지 않는다.
```

## Trail Or Secondary Motion

```text
티브렛에게 전달할 지시

원본 `ABP_Bot`의 disconnected Trail node를 직접 활성화하지 말 것. 요청 목적은
`[SecondaryMotionIntent]`이며, 안전한 샘플 Post Process route를 우선 사용한다.

Bot antenna 계열이면 `/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study`와
`SKM_Bot_Trail_Study`를 `ensure_anim_graph_trail_demo`로 만들거나 재사용한다.
Trail target은 `[TrailBone]`, base joint는 `[BaseJoint]`로 설정한다.

proof actor는 main `ABP_Bot_C`와 sample Post Process AnimBP를 함께 사용한다.
component-level `set_override_post_process_anim_bp(..., true)`가 가능하면 명시적으로
적용한다. SIE/PIE에서 live AnimInstance를 확인한 뒤
`sample_anim_node_pre_post_runtime_pose(anim_instance_source=post_process)`를 실행한다.

완료 조건: target chain에서 의도한 translation/rotation delta 확인,
right/left 반대편이나 root 계열의 불필요한 delta가 과하지 않음,
`errors=[]`, cleanup 완료, 원본 asset mutation 없음.
```

## Notify, Curve, Sync Marker, Or Montage Internals

```text
티브렛에게 전달할 지시

이번 요청은 `[MetadataIntent]` 때문에 animation source metadata가 필요한지 먼저
확인한다. 원본 애셋을 수정하지 말고, broad Unreal Python reflection으로
`AnimMontage` internals를 탐색하지 말 것.

시작 증거는 `StackOBot_AnimationAsset_Inventory.*`,
`StackOBot_AnimationAsset_ReadApiProbe.json`, AssetRegistry-level Montage scan,
그리고 runtime route evidence다. Python으로 안전하게 읽히는 timing, skeleton,
root-motion flag, BlendSpace sample, source pose sampling만 사용한다.

`AnimSequence.Notifies`, `raw_curve_data`, `authored_sync_markers`,
`AnimSequence.get_data_model`, Montage slot tracks/sections/branching points가
필요하면 현재 경로로 진행하지 말고 Ieta에게 guarded native MCP command 후보로
돌려보낸다.

완료 조건: 현재 safe read로 충분한지 또는 C++/API 후보가 필요한지 명확히 보고.
`AnimMontage.h:770` crash boundary를 재현하지 않는다.
```

## Route Token Final Report Map

Use this map when Tivret returns execution results to Ieta. The generic field
list stays in `Final Report Shape`; this table defines the route-specific proof
focus that must be present before Ieta reports completion to the user.

| Route token | Final proof focus | Must report | C++/API status cue |
| --- | --- | --- | --- |
| `Post Process ModifyBone` | Same-instance Post Process node proof. | `runtime_graph_prepost`, `same_instance_prepost`, target bone delta, and original asset mutation status. | `not needed` unless original asset mutation or unsupported node authoring is requested. |
| `BlendSpace sample variant` | Runtime pose-grid response for the sample BlendSpace. | `valid_pose_count`, `input_changed_pose`, sample coordinate changes, and original asset mutation status. | `not needed` for current sample coordinate edits. |
| `Bot Trail sample` | Same-instance Trail or Post Process secondary-motion proof. | `same-instance Trail`, `target chain delta`, component-level Post Process override, and cleanup status. | `not needed` unless a new secondary-motion graph cannot be authored safely. |
| `UpperBody Slot and LayeredBlend` | Existing slot/cached-pose/LayeredBoneBlend route proof. | `BasePose`, `BlendPoses[0]`, branch filter summary, and visible action proof caveat. | `candidate` before new overlay branch authoring. |
| `protected metadata boundary` | Safe metadata inventory or explicit parked API candidate. | `readable fields`, `blocked protected fields`, protected field names, and crash-boundary avoidance. | `candidate` when notifies, curves, sync markers, or Montage internals are required. |
| `ControlRig gate probe` | Gate identification plus compiled AnimGraph same-instance proof when needed. | `root-connected`, `required gates`, `same-instance pre/post`, direct-vs-compiled proof boundary, and cleanup status. | `not needed` for sample proof; ask before editing original ControlRig or AnimBP. |
| `state-machine runtime-driver proof` | Runtime state-machine response cases. | `current state`, `transition progress`, `restored runtime properties`, sampled world, and changed inputs. | `candidate` before original graph mutation or new state-machine authoring. |
| `Baddy RigidBody` | RigidBody settings plus runtime node or pose proof. | `RigidBody settings`, `mapped runtime node`, `pose deltas`, and source clip motion separation. | `not needed` for read/sample proof; `candidate` before original physics asset or AnimBP mutation. |
| `node resolver plus same-instance pre/post proof` | Instrumentation-only node contribution proof. | `target node selection`, `input/output links`, `same-instance confirmation`, and ambiguity if unresolved. | `not needed` while read-only; `candidate` only for unsupported node classes. |

## Final Report Shape

Tivret should return these fields to Ieta after execution:

```text
route:
assets_created_or_reused:
original_assets_modified:
runtime_world:
main_command_results:
pose_or_state_evidence:
errors:
warnings:
dirty_packages:
cleanup:
cxx_api_needed:
artifact_paths:
residual_risk:
```

Ieta then summarizes the result for the user and records durable project memory.
