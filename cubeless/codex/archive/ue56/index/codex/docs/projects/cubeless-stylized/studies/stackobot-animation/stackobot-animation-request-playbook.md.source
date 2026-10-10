# StackOBot Animation Request Playbook

Use this playbook when the user asks for a specific animation part to be created,
changed, tested, or explained. It turns the learned StackOBot grammar into an
execution protocol.

Related docs:

- `docs/stackobot-animation-study-closeout.md`
- `docs/stackobot-animation-next-work-backlog.md`
- `docs/stackobot-animation-acceptance-checklist.md`
- `docs/stackobot-animation-quickstart.md`
- `docs/stackobot-animation-route-matrix.md`
- `docs/stackobot-animation-request-run-template.md`
- `docs/stackobot-animation-authoring-templates.md`
- `docs/stackobot-request-compiler-drills.md`
- `docs/stackobot-animation-tivret-handoff-templates.md`
- `docs/stackobot-cpp-api-decision-matrix.md`
- `docs/stackobot-physics-request-grammar.md`
- `docs/stackobot-animbp-authoring-patterns.md`
- `docs/stackobot-animation-execution-map.md`
- `docs/stackobot-animation-study.md`
- `docs/stackobot-animation-mcp-command-syntax.md`
- `docs/stackobot-sample-asset-manifest.md`

## Default Rule

Start sample-only unless the user explicitly approves original asset mutation.

Default target paths:

- Sample content root: `/Game/_MCP_Sample/AnimStudy`
- Evidence root: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy`
- Reference project: `<workspace-parent>/SampleProject/StackOBot`

Do not edit original StackOBot assets for a first pass. Use original assets as
reference inputs and duplicate or create safe sample variants.

For authoring commands, keep `allow_non_sample=false` unless original asset
mutation is explicitly approved. Do not broad-probe Montage internals with
generic Python.

## Intake

For every request, extract these fields before doing asset work:

For natural-language compilation drills and filled examples, use
`docs/stackobot-request-compiler-drills.md` before selecting the handoff block.

| Field | Meaning | Example |
| --- | --- | --- |
| Target character | Which skeleton/mesh/AnimBP owns the behavior | Bot, Baddy |
| Body area | Bone chain or system affected | head, antenna, foot IK, upper body |
| Timing type | Static offset, state change, continuous axis, overlay, physics response | head yaw, run lean, jump transition |
| Runtime layer | Main AnimBP, ControlRig, Post Process AnimBP, physics node | Post Process ModifyBone |
| Authoring target | Sample-only, original asset, or project implementation | `/Game/_MCP_Sample/AnimStudy` |
| Verification depth | Compile/load only, runtime smoke, same-instance PoseWatch | PoseWatch pre/post |
| Approval state | Whether original assets or non-exception C++ are approved | sample-only by default |

If the request is vague, choose the narrowest reversible sample-only path and record
the assumption. Ask only when the choice could mutate original assets, require
non-exception C++, or materially change the intended visual result.

## Classification Matrix

| Request wording | Route | First sample action | Proof |
| --- | --- | --- | --- |
| "make the head turn/look/tilt after animation" | Post Process AnimBP | `ensure_postprocess_anim_demo_variant` | no-SIE or SIE PoseWatch on ModifyBone |
| "make antenna wobble/follow/lag" | Post Process AnimBP physics | `ensure_anim_graph_trail_demo` or ModifyBone sample | PoseWatch on Trail/ModifyBone |
| "make idle/walk/run/jump/hover transition" | Main AnimBP state machine | inspect first; sample graph only if tooling exists | `inspect_anim_state_machine_transitions`, runtime state response |
| "change speed/lean response" | BlendSpace | `ensure_blendspace_sample_variant` in `_MCP_Sample`; sample grid | `sample_blendspace_runtime_pose_grid` |
| "upper body action over movement" | Slot/layered blend | inspect slot/cached pose/branch filters | all-input PoseWatch on LayeredBoneBlend |
| "foot placement/IK interaction" | ControlRig | forced-driver sample if gameplay gate is inactive | direct ControlRig probe plus AnimGraph PoseWatch |
| "physics jiggle/secondary body motion" | RigidBody/Trail | use Baddy RigidBody or Bot Trail sample; see `docs/stackobot-physics-request-grammar.md` | compiled mapping plus PoseWatch |
| "which node caused this?" | Instrumentation | no asset edit; target node resolver first | `sample_anim_node_pre_post_runtime_pose` |

## Route Token Playbook Map

Use this table after intake has produced a route token. It ties the playbook to
the route matrix, handoff templates, command syntax, and acceptance checklist.

| Route token | Starting point | Handoff | First read or authoring command | Verification command |
| --- | --- | --- | --- | --- |
| `Post Process ModifyBone` | Post Process static ModifyBone | `Post Process ModifyBone` | `ensure_postprocess_anim_demo_variant` | `sample_anim_node_pre_post_runtime_pose` |
| `BlendSpace sample variant` | BlendSpace sample coordinate/range edit | `BlendSpace Sample Variant` | `ensure_blendspace_sample_variant` | `sample_blendspace_runtime_pose_grid` |
| `Bot Trail sample` | Post Process physics or secondary motion | `Trail Or Secondary Motion` | `ensure_anim_graph_trail_demo` | `sample_anim_node_pre_post_runtime_pose` |
| `UpperBody Slot and LayeredBlend` | Slot, cached pose, LayeredBoneBlend overlay | `UpperBody Slot And LayeredBlend` | slot/cached-pose inventory | `sample_anim_node_pre_post_runtime_pose` |
| `protected metadata boundary` | Notify, curve, sync marker, or Montage metadata | `Notify, Curve, Sync Marker, Or Montage Internals` | safe animation asset inventory | none for protected internals |
| `ControlRig gate probe` | ControlRig late correction with gameplay gates | `ControlRig Late Correction` | `inspect_anim_graph_protected_topology`, then `controlrig_direct_gate_probe` | `sample_anim_node_pre_post_runtime_pose` |
| `state-machine runtime-driver proof` | Main AnimBP state-machine/runtime-driver behavior | `State Machine Or Runtime Driver` | `inspect_anim_state_machine_transitions` | `sample_anim_state_machine_runtime_response` |
| `Baddy RigidBody` | RigidBody/Trail secondary body motion | `Trail Or Secondary Motion` | `inspect_anim_graph_node_settings` | `sample_anim_node_pre_post_runtime_pose` |
| `node resolver plus same-instance pre/post proof` | Instrumentation only | no authoring handoff | `inspect_anim_graph_protected_topology` or compiled mapping | `sample_anim_node_pre_post_runtime_pose` |

## Execution Protocol

1. Classify the request with the matrix above.
2. Copy `docs/stackobot-animation-request-run-template.md` for requests that
   touch editor state or create sample evidence.
3. State the target route in a `티브렛에게 전달할 지시` block before asset work.
4. Confirm the target is sample-only unless the user approved original asset edits.
5. Run static topology or existing evidence first.
6. Create or modify the sample using the narrowest existing MCP command.
7. Compile/save sample assets only when the route is an authoring command.
8. Verify runtime behavior with the cheapest reliable proof:
   - Post Process static ModifyBone: editor-world no-SIE PoseWatch is acceptable.
   - Physics, state machine, BlendSpace, gameplay-driven gates: prefer SIE/PIE.
   - Multi-input blend: use all-input PoseWatch.
9. Write artifacts under `Saved/MCP/AnimStudy`.
10. Check the pass criteria in `docs/stackobot-animation-acceptance-checklist.md`.
11. Check dirty packages and never save dirty original maps just to clean up a proof actor.
12. Update Cubeless docs/work-log, then commit only relevant docs or tooling files.

## Tivret Instruction Templates

Use one of these blocks as the visible `티브렛에게 전달할 지시` starting point before
asset work. Replace bracketed fields and keep the target sample-only unless the user
explicitly approved original asset mutation.

### Post Process ModifyBone

```text
티브렛에게 전달할 지시

StackOBot 원본 애셋은 수정하지 않는다. Bot의 [bone/chain]에 [rotation/translation/scale]
late adjustment를 샘플 전용 Post Process AnimBP로 만든다. 타깃은
`/Game/_MCP_Sample/AnimStudy/[SampleName]` 아래에 둔다.

Use route:
1. `ensure_postprocess_anim_demo_variant`로 샘플 AnimBP와 샘플 SkeletalMesh를 만들거나 재사용한다.
2. 샘플 SkeletalMesh의 Post Process AnimBlueprint가 생성된 클래스인지 확인한다.
3. 정적 ModifyBone 검증이면 SIE 없이 editor-world transient actor를 사용한다.
4. `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process, prefer_pie_world=false)`로 같은 인스턴스 pre/post를 검증한다.
5. 원본 StackOBot 애셋, 원본 맵, 원본 SkeletalMesh는 저장하지 않는다.
```

### BlendSpace Sample Variant

```text
티브렛에게 전달할 지시

StackOBot 원본 BlendSpace는 수정하지 않는다. `[SourceBlendSpace]`를 기준으로
`/Game/_MCP_Sample/AnimStudy/[SampleBlendSpace]` 샘플 변형을 만들고,
[axis/sample coordinate/compatible animation] 변경만 적용한다.

Use route:
1. StackOBot editor가 `<workspace-parent>/SampleProject/StackOBot/Plugins/UnrealMCP` 플러그인 복사본을 쓰는지 확인한다.
2. `ensure_blendspace_sample_variant`로 샘플 BlendSpace를 만들거나 재사용한다.
3. 명시된 축 범위와 샘플 좌표만 바꾸고, skeleton/animation compatibility 실패 시 중단한다.
4. `sample_blendspace_runtime_pose_grid`로 입력 변화가 실제 pose delta를 만드는지 검증한다.
5. 결과에는 `original_assets_modified=false`, 저장된 샘플 경로, pose-grid 핵심 delta를 포함한다.
```

### Control Rig Forced Driver

```text
티브렛에게 전달할 지시

원본 `ABP_Bot`은 직접 수정하지 않는다. Control Rig 또는 IK 요청은 기존
`CR_Bot_Correction` 경로를 먼저 읽고, gameplay gate가 비활성일 경우 샘플 forced-driver
AnimBP로 검증한다.

Use route:
1. `inspect_anim_graph_protected_topology`로 ControlRig 노드가 root-connected인지 확인한다.
2. 필요한 curve/input gate를 `controlrig_direct_gate_probe` 결과와 비교한다.
3. `ensure_controlrig_forced_driver_animbp`로 `_MCP_Sample` forced-driver 샘플을 만들거나 재사용한다.
4. `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)`로 ControlRig input/output 같은 인스턴스 delta를 검증한다.
5. 원본 AnimBP와 원본 맵은 저장하지 않는다.
```

### Physics Or Secondary Motion

```text
티브렛에게 전달할 지시

secondary motion 요청은 먼저 기존 Baddy RigidBody 또는 Bot Trail 샘플 경로를 사용한다.
원본 AnimBP의 disconnected Trail 노드를 바로 활성화하지 않는다.

Use route:
1. RigidBody면 `ABP_Baddy` 증거 경로를 우선 사용하고, Trail이면 `_MCP_Sample` Bot Trail 샘플을 사용한다.
2. `sample_anim_node_pre_post_runtime_pose(mode=compiled_graph_mapping)`로 editor node와 runtime node 매핑을 확인한다.
3. `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)`로 같은 인스턴스 input/output을 캡처한다.
4. physics성 움직임이면 SIE/PIE proof를 우선하고, static editor tick만으로 완료 판정하지 않는다.
5. 원본 애셋 저장 없이 dirty package 상태를 보고한다.
```

### State Machine Or Runtime Driver

```text
티브렛에게 전달할 지시

idle/walk/run/jump/hover 같은 main AnimBP 요청은 먼저 authoring보다 runtime driver를 읽는다.
원본 `ABP_Bot` graph 편집은 사용자가 명시적으로 승인하기 전까지 하지 않는다.

Use route:
1. `inspect_anim_state_machine_transitions`로 관련 state/transition/gate를 확인한다.
2. `inspect_anim_instance_runtime_state`로 현재 state, weights, transition progress를 읽는다.
3. `sample_anim_state_machine_runtime_response`로 `[driver cases]`가 의도한 state sequence를 만드는지 검증한다.
4. 기존 graph와 변수로 해결 가능한지 판단하고, graph authoring이 필요하면 별도 샘플/툴링 계획으로 분리한다.
```

## Dry-Run Request Scenarios

Use this table as the rehearsal check before touching assets. If a new request does
not fit one of these rows, classify it first and add the missing route before authoring.

| Example request | Classification | Starting template | First proof | C++/API status |
| --- | --- | --- | --- | --- |
| "Bot head should look 10 degrees to the right after the main animation." | Post Process static ModifyBone on `head` | Post Process ModifyBone | `ensure_postprocess_anim_demo_variant`, then no-SIE `sample_anim_node_pre_post_runtime_pose(anim_instance_source=post_process)` | Covered by existing commands |
| "Make run lean wider on left/right." | BlendSpace sample coordinate/range edit | BlendSpace Sample Variant | `ensure_blendspace_sample_variant`, then `sample_blendspace_runtime_pose_grid` | Covered, but requires StackOBot-local UnrealMCP plugin copy |
| "Make the feet react to an interaction point." | ControlRig late correction with gameplay gates | Control Rig Forced Driver | `inspect_anim_graph_protected_topology`, `controlrig_direct_gate_probe`, forced-driver PoseWatch | Covered by existing commands |
| "Make the antenna lag or trail behind movement." | Post Process physics or secondary motion | Physics Or Secondary Motion | Existing Bot Trail sample, compiled mapping, PoseWatch capture; prefer SIE/PIE for physics motion | Covered for current Trail sample; new physics parameter authoring remains candidate API |
| "Make hover transition stay longer or respond to a variable." | Main AnimBP state-machine/runtime-driver behavior | State Machine Or Runtime Driver | `inspect_anim_state_machine_transitions`, `inspect_anim_instance_runtime_state`, `sample_anim_state_machine_runtime_response` | Inspect/probe covered; new state/transition authoring remains candidate API |
| "Play an upper-body action while locomotion continues." | Slot, cached pose, LayeredBoneBlend overlay | Existing `UpperBody` route from authoring patterns | Slot/cached-pose inventory, then `sample_anim_node_pre_post_runtime_pose(input_pose_mode=all)`; request-template proof is `StackOBot_LayeredBlendTemplateRehearsal_*` | Existing overlay route and all-input proof covered; new overlay authoring remains candidate API |
| "Which node caused the pose change?" | Instrumentation only | No asset-authoring template | `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` or compiled mapping first | Covered by existing commands unless node class is unusual |

## Known Safe Routes

### Post Process ModifyBone

Best current route for a static late bone correction:

1. Create sample variant with `ensure_postprocess_anim_demo_variant`.
2. Confirm target SkeletalMesh points to the generated Post Process AnimBlueprint.
3. Spawn an editor-world transient actor without SIE if the proof is static.
4. Set main `ABP_Bot_C` and explicit component-level Post Process override.
5. Run:

```text
sample_anim_node_pre_post_runtime_pose(
  mode=pose_watch_capture,
  anim_instance_source=post_process,
  prefer_pie_world=false,
  require_pie_world=false
)
```

Evidence: `HeadYawAuthoringPattern` uses this route. It proved `head` yaw `8.0 deg`,
antenna leaf movement about `11.74 cm`, `runtime_graph_prepost=true`, and
`same_instance_prepost=true` without starting SIE.

### ControlRig

Best current route when gameplay gates do not fire naturally:

1. Inspect topology with `inspect_anim_graph_protected_topology`.
2. Use `controlrig_direct_gate_probe` to identify required curve/input gates.
3. Create or reuse the forced-driver sample through `ensure_controlrig_forced_driver_animbp`.
4. Verify with `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)`.

Evidence: `_MCP_Sample` `ABP_Bot_ControlRig_ForcedDriver_Study` proved same-instance
ControlRig pre/post with output link `42` and input `Source` link `45`.

### BlendSpace

Best current route for axis-driven animation authoring:

1. Inspect the source pose map and authored sample coordinates.
2. Create or reuse a sample variant with `ensure_blendspace_sample_variant`.
3. Edit only explicit axis/sample coordinates or compatible sample animations.
4. Use `sample_blendspace_runtime_pose_grid`.
5. Interpret deltas as controlled tooling evidence, not exact match to older async SIE artifacts.

Current route:

- The command duplicates or reuses a target under `/Game/_MCP_Sample/AnimStudy`,
  edits axis ranges/sample coordinates, validates/resamples, and saves only the
  sample target.
- `LeanWideStudy` smoke widened `A_Bot_Run_LeanLeft` to `Lean=1.25` and
  `A_Bot_Run_LeanRight` to `Lean=-1.25`; Unreal expanded the Lean axis to
  `-1.5..1.5`, saved the sample BlendSpace, and the runtime pose-grid fallback
  reported `input_changed_pose=true`.
- `LeanTemplateRehearsal` repeated the request-template route with
  `Lean=1.30/-1.30`; `sample_blendspace_runtime_pose_grid(require_pie_world=false)`
  reported `valid_pose_count=3`, `input_changed_pose=true`, and max delta
  `5.046 cm`.
- Do not mutate the original StackOBot BlendSpaces as a workaround.

### Physics

Best current route for secondary motion:

1. Prefer existing Baddy RigidBody or Bot Trail samples.
2. Use compiled mapping to prove the live node maps to the selected editor node.
3. Use PoseWatch capture for exact same-instance input/output proof.
4. For Bot Trail proof actors, set `set_override_post_process_anim_bp(ABP_Bot_Trail_Study_C, true)` on the component, start SIE, poll for a live PIE AnimInstance, then call PoseWatch with `anim_instance_source=post_process`.
5. Split cleanup into `editor_end_play` first and actor deletion second; do not send a combined cleanup payload that deletes actors while SIE is still active.

## Approval Gates

Proceed without asking again:

- Reading StackOBot/Cubeless/UnrealMCP code and docs.
- Creating or updating Cubeless documentation.
- Creating or modifying C++ inside the UnrealMCP plugin when justified by MCP tooling.
- Creating or modifying disposable sample assets under `/Game/_MCP_Sample/AnimStudy`.

Ask or require explicit approval:

- Modifying original StackOBot assets outside `_MCP_Sample`.
- Modifying Cubeless project gameplay/runtime C++ outside approved plugin exceptions.
- Saving original maps that became dirty during transient proof actor cleanup.
- Using billed/API routes, credentials, or non-local services.
- Promoting sample assets into production content.

## C++/API Escalation

Keep C++ as a candidate only when an existing MCP command cannot safely create,
wire, inspect, or verify the requested graph.

Escalate to UnrealMCP C++ when:

- Python wrappers cannot access required AnimGraph internals.
- Generic `execute_python` would need unsafe map switching, SIE setup, or protected pin edits.
- The requested proof needs reusable same-instance runtime instrumentation.
- A repeated manual setup pattern can be made safer as a native command.

Do not escalate when:

- Existing commands already cover the request.
- A sample-only asset can be created and verified with current commands.
- The only missing work is documentation or artifact summarization.

Candidate API parking lot:

| Candidate | Use only when | Prefer existing route first |
| --- | --- | --- |
| `ensure_state_machine_sample_variant` | A future request needs a new state, sequence player, or transition rule that cannot be represented by runtime property-driver tests. | `inspect_anim_state_machine_transitions`, `inspect_anim_instance_runtime_state`, `sample_anim_state_machine_runtime_response` |
| `ensure_layered_slot_overlay_sample` | A future request needs a new slot/layered-blend overlay sample rather than inspecting the existing `UpperBody` route. | Slot/cached-pose inventory and `sample_anim_node_pre_post_runtime_pose(input_pose_mode=all)` |
| `ensure_postprocess_physics_variant` | A future request needs authored Trail/RigidBody-like secondary motion parameters outside the existing Bot Trail and Baddy RigidBody samples. | `ensure_anim_graph_trail_demo`, compiled mapping, PoseWatch capture |
| `resolve_anim_posewatch_target_actor` | Repeated proof requests keep failing because transient actor setup, component override, PIE duplicate matching, or editor-world fallback is inconsistent. | Existing editor-world setup for static Post Process, existing SIE/PIE actor setup for gameplay/physics |
| `inspect_or_author_anim_notifies_curves` | A request depends on sequence notifies, sync markers, curve authoring, or Montage internals that remain protected or unsafe through current Python reads. | Current sequence inventory, BlendSpace sample inventory, ControlRig curve-gate probes, and AssetRegistry-level Montage scan |

Implemented route:

- `ensure_blendspace_sample_variant`: duplicate or reuse a source BlendSpace under
  `/Game/_MCP_Sample/AnimStudy`, apply explicit axis/sample-coordinate edits,
  refuse original asset mutation by default, validate skeleton/animation compatibility,
  save only the sample target, and then run or request `sample_blendspace_runtime_pose_grid`.
- Command-surface check: for StackOBot work, confirm the active editor is using
  `<workspace-parent>/SampleProject/StackOBot/Plugins/UnrealMCP`. The Cubeless plugin copy can lag behind
  the StackOBot-local study command surface, so do not assume `ensure_blendspace_sample_variant`
  exists outside the StackOBot bridge until that plugin copy is synced and build-verified.
- Existing `UpperBody` Slot/LayeredBlend route: inspect
  `StackOBot_SlotLayeredBlend_Inventory.md`, then verify the target node with
  `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, input_pose_mode=all)`.
  The request-template rehearsal artifacts are `StackOBot_LayeredBlendTemplateRehearsal_*`.
  This route proves StackOBot's existing overlay wiring; visible action proof still requires
  a montage/action source or a future sample overlay authoring command.
- Notify/curve/sync-marker requests: start with `StackOBot_AnimationAsset_Inventory.*`,
  `StackOBot_AnimationAsset_ReadApiProbe.json`, and runtime route evidence. Do not broad-probe
  `AnimMontage` internals through generic Unreal Python; the 2026-06-19 retry asserted in
  `AnimMontage.h:770`. Escalate to a guarded native MCP command only for a concrete request.

## Failure Handling

If a validation route fails, classify the failure before changing the plan:

| Failure | Interpretation | Next action |
| --- | --- | --- |
| Compile error | Sample graph invalid | Fix sample graph or command parameters |
| Dirty map after spawn/delete | Reversible proof actor dirtied map | Do not save; close editor externally |
| Python wrapper method missing | Wrapper exposure gap | Let C++ command handle tick/read path |
| SIE setup crash | Unsafe setup route | Avoid repeating; use no-SIE proof or native setup |
| No pose delta | Could be real behavior or inactive gate | Check topology, curves, runtime state, and input gates |
| No matching actor/AnimInstance | Runtime target setup issue | Recreate transient target or use a native command |

## Route Token Failure Map

Use this table when a route-specific validation fails. Keep the first retry on
the same safe route, and escalate to C++/API only when the documented safe proof
or authoring route is actually blocked.

| Route token | First retry or read command | Verification command | Escalate only when |
| --- | --- | --- | --- |
| `Post Process ModifyBone` | Re-run `ensure_postprocess_anim_demo_variant` against the sample target and confirm the target bone/transform inputs. | `sample_anim_node_pre_post_runtime_pose` | C++/API: `not needed` unless the sample Post Process graph cannot be authored or same-instance Post Process proof is unavailable. |
| `BlendSpace sample variant` | Re-run `ensure_blendspace_sample_variant` and inspect axis/sample coordinate edits. | `sample_blendspace_runtime_pose_grid` | C++/API: `not needed` unless the sample BlendSpace cannot be authored or runtime pose-grid proof cannot validate the requested response. |
| `Bot Trail sample` | Re-run or inspect `ensure_anim_graph_trail_demo`, then confirm component-level Post Process override setup. | `sample_anim_node_pre_post_runtime_pose` | C++/API: `not needed` unless the sample Trail/Post Process route cannot be authored or target chain proof remains unavailable. |
| `UpperBody Slot and LayeredBlend` | Re-check `slot/cached-pose inventory`, branch filters, and the existing `UpperBody` path before asking for new authoring. | `sample_anim_node_pre_post_runtime_pose` | C++/API: `candidate` only if a visible action requires a missing source clip, Montage, or new overlay branch. |
| `protected metadata boundary` | Re-run `safe animation asset inventory` and AssetRegistry-level reads only. | `none for protected internals` | C++/API: `candidate` only if the request requires protected notifies, curves, sync markers, or Montage internals. |
| `ControlRig gate probe` | Re-run `inspect_anim_graph_protected_topology`, then `controlrig_direct_gate_probe`, and confirm required curve/input gates. | `sample_anim_node_pre_post_runtime_pose` | C++/API: `not needed` unless the compiled ControlRig proof route cannot be sampled or original ControlRig/AnimBP mutation is required. |
| `state-machine runtime-driver proof` | Re-run `inspect_anim_state_machine_transitions` and verify the runtime case inputs are writable/restored. | `sample_anim_state_machine_runtime_response` | C++/API: `candidate` only if a graph authoring change is required beyond runtime-driver proof. |
| `Baddy RigidBody` | Re-run `inspect_anim_graph_node_settings` and separate source clip motion from physics response. | `sample_anim_node_pre_post_runtime_pose` | C++/API: `not needed` for sample proof; `candidate` only if original PhysicsAsset or AnimBP mutation is required, or RigidBody node proof cannot be sampled. |
| `node resolver plus same-instance pre/post proof` | Re-run `inspect_anim_graph_protected_topology` plus `compiled mapping` and report ambiguity if node selection remains unclear. | `sample_anim_node_pre_post_runtime_pose` | C++/API: `not needed` unless the suspected node class is outside the current mapping/PoseWatch coverage. |

## Delivery Shape

When finishing a requested animation part, report:

- `route:` exact route token and route matrix row used.
- `assets_created_or_reused:` sample assets, transient actors, and evidence files.
- `original_assets_modified:` explicit `false` unless the user approved original mutation.
- `runtime_world:` editor world, SIE/PIE world, or no-runtime proof context.
- `main_command_results:` authoring/read commands, compile/save result, and major errors.
- `pose_or_state_evidence:` pose deltas, state changes, or why proof stayed read-only.
- `errors:` command, compile, bridge, editor log, or validation errors.
- `warnings:` weak evidence, inactive gates, missing source clips, or sample-only limits.
- `dirty_packages:` packages dirtied, saved, reverted, or intentionally left unsaved.
- `cleanup:` transient actor, preview map, sample reuse, and artifact cleanup status.
- `cxx_api_needed:` `not needed`, `candidate`, or `implemented` plus the reason.
- `artifact_paths:` JSON, Markdown, screenshot, or log paths used as evidence.
- `residual_risk:` blocked route, manual editor follow-up, or next safe retry.
