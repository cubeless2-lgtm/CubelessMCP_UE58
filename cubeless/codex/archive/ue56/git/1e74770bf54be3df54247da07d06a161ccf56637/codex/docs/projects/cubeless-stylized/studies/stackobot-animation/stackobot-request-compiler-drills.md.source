# StackOBot Request Compiler Drills

Use this sheet when a user asks for a StackOBot animation behavior in ordinary
language. The goal is to compile the request into a safe route, concrete first
command, verification gate, and C++/API decision before Tivret touches assets.

Related docs:

- `docs/stackobot-animation-quickstart.md`
- `docs/stackobot-animation-request-playbook.md`
- `docs/stackobot-animation-authoring-templates.md`
- `docs/stackobot-animation-tivret-handoff-templates.md`
- `docs/stackobot-cpp-api-decision-matrix.md`
- `docs/stackobot-physics-request-grammar.md`
- `docs/stackobot-animation-mcp-command-syntax.md`
- `docs/stackobot-sample-asset-manifest.md`

Default rule: classify first, show the filled `티브렛에게 전달할 지시` block next,
then execute only the sample-safe route unless original asset mutation was
explicitly approved.

## Compiler Output

Every request should compile to this record:

```text
request:
assumptions:
target_character:
target_body_area:
runtime_layer:
route:
sample_target:
first_read_or_authoring_command:
verification_command:
expected_evidence:
handoff_template:
cxx_api_status:
ask_user_first:
```

Use `ask_user_first=true` only when the missing choice affects original asset
mutation, non-exception C++, billing/API use, map saving, or an important visual
direction. Otherwise state a reversible assumption and proceed sample-only.

## Local Compiler Tool

Use the local compiler for a quick route-token smoke before filling a request
run record:

```powershell
python Tools/Unreal/compile_stackobot_animation_request.py --summary --request "Bot 머리를 오른쪽으로 5도만 더 돌려줘."
```

The tool is read-only. It does not call Unreal, does not touch assets, and does
not replace Ieta's final judgment. Treat its output as the first classification
draft, then compare the route against `docs/stackobot-animation-route-matrix.md`
and the route-token maps before Tivret touches editor state.

## Signal Words

| Request signals | Route | Body/layer inference | First command |
| --- | --- | --- | --- |
| `머리`, `고개`, `head`, `look`, `tilt`, `turn`, `after animation` | Post Process ModifyBone | Bot `head` or `neck_01`; late cosmetic correction | `ensure_postprocess_anim_demo_variant` |
| `안테나`, `흔들`, `wobble`, `lag`, `trail`, `follow-through` | Trail or secondary motion | Bot antenna chain; Post Process Trail sample | `ensure_anim_graph_trail_demo` |
| `기울`, `lean`, `속도`, `speed`, `walk`, `run` | BlendSpace sample variant | `BS_Bot_WalkRunLean`; locomotion axis response | `ensure_blendspace_sample_variant` |
| `idle`, `walk`, `run`, `jump`, `hover`, `transition`, `state` | State machine/runtime driver | Main `ABP_Bot`; runtime variables | `inspect_anim_state_machine_transitions` |
| `발`, `foot`, `IK`, `interaction point`, `touch` | ControlRig late correction | `CR_Bot_Correction` gates and curves | `controlrig_direct_gate_probe` |
| `상체`, `upper body`, `attack`, `button`, `while moving` | UpperBody Slot/LayeredBlend | Existing `UpperBody` slot route | Inventory, then all-input PoseWatch |
| `말랑`, `jiggle`, `soft body`, `stalk`, `tail`, `rigidbody` | RigidBody physics | Baddy RigidBody or sample variant tuning | `inspect_anim_graph_node_settings` |
| `notify`, `curve`, `sync marker`, `montage section` | Protected metadata | Asset source metadata, not pose graph | Existing safe inventory only |
| `왜`, `which node`, `caused`, `pre/post` | Instrumentation | Resolve node contribution | `sample_anim_node_pre_post_runtime_pose` |

## Route Token Compiler Map

Use this table after reading the signal words. It maps the normalized route token
to the first command and verification command expected in the compiler output.

| Route token | First read or authoring command | Verification command |
| --- | --- | --- |
| `Post Process ModifyBone` | `ensure_postprocess_anim_demo_variant` | `sample_anim_node_pre_post_runtime_pose` |
| `BlendSpace sample variant` | `ensure_blendspace_sample_variant` | `sample_blendspace_runtime_pose_grid` |
| `Bot Trail sample` | `ensure_anim_graph_trail_demo` | `sample_anim_node_pre_post_runtime_pose` |
| `UpperBody Slot and LayeredBlend` | slot/cached-pose inventory | `sample_anim_node_pre_post_runtime_pose` |
| `protected metadata boundary` | safe animation asset inventory | none for protected internals |
| `ControlRig gate probe` | `inspect_anim_graph_protected_topology`, then `controlrig_direct_gate_probe` | `sample_anim_node_pre_post_runtime_pose` |
| `state-machine runtime-driver proof` | `inspect_anim_state_machine_transitions` | `sample_anim_state_machine_runtime_response` |
| `Baddy RigidBody` | `inspect_anim_graph_node_settings` | `sample_anim_node_pre_post_runtime_pose` |
| `node resolver plus same-instance pre/post proof` | `inspect_anim_graph_protected_topology` or compiled mapping | `sample_anim_node_pre_post_runtime_pose` |

## Drill Table

| User request | Compiled route | Assumption | First command | Verification | C++/API |
| --- | --- | --- | --- | --- | --- |
| "Bot 머리를 오른쪽으로 5도만 더 돌려줘." | Post Process ModifyBone | Bot `head`, additive yaw +5, sample-only | `ensure_postprocess_anim_demo_variant` | no-SIE Post Process PoseWatch | Not needed |
| "안테나가 달릴 때 뒤로 살짝 끌리게 해줘." | Trail secondary motion | Bot `antenna_04_l`, `base_joint=head`, sample Trail | `ensure_anim_graph_trail_demo` | SIE Post Process PoseWatch | Not needed for current Trail |
| "달릴 때 좌우 기울기가 더 과장되면 좋겠어." | BlendSpace sample variant | `BS_Bot_WalkRunLean`, widen Lean samples | `ensure_blendspace_sample_variant` | `sample_blendspace_runtime_pose_grid` | Not needed |
| "점프에서 착지로 넘어가는 타이밍을 보고 싶어." | State machine/runtime driver | Read-only first; no state graph edit yet | `inspect_anim_state_machine_transitions` | runtime state response cases | Authoring candidate only if new rule is required |
| "상호작용 지점에 발이 닿게 만들어줘." | ControlRig late correction | Existing `CR_Bot_Correction`; forced driver if gates inactive | `controlrig_direct_gate_probe` | ControlRig same-instance PoseWatch | Not needed |
| "움직이면서 버튼 누르는 상체 동작이 나오게 해줘." | UpperBody Slot/LayeredBlend | Existing `UpperBody` route; no new montage source yet | Slot/layer inventory | all-input LayeredBoneBlend PoseWatch | New overlay/action source remains candidate |
| "Baddy 줄기 부분이 더 물렁하게 흔들렸으면 해." | RigidBody sample tuning | Baddy stalk bones; use sample variants | `inspect_anim_graph_node_settings` | SIE variant metrics or PoseWatch | Not needed for narrow tuning |
| "이 몽타주 notify 시점을 읽어서 맞춰줘." | Protected metadata | Montage internals needed | safe asset inventory only | report blocked fields | Candidate guarded native API |
| "어느 노드가 최종 자세를 바꿨는지 증명해줘." | Instrumentation | Select suspected node from topology first | compiled mapping | PoseWatch pre/post | Not needed unless node is unsupported |

## Filled Handoff Example

For "Bot 머리를 오른쪽으로 5도만 더 돌려줘":

```text
티브렛에게 전달할 지시

StackOBot 원본 ABP_Bot, SKM_Bot, 맵, 원본 애셋은 수정/저장하지 말 것.
요청 목적은 Bot head에 대한 late Post Process yaw +5 degree adjustment다.

ensure_postprocess_anim_demo_variant로 /Game/_MCP_Sample/AnimStudy 아래에
HeadYawPlus5-style sample Post Process AnimBP와 sample SkeletalMesh를 만들거나
재사용한다. 대상 bone은 head, additive transform intent는 yaw +5 degrees다.

검증은 transient actor로 진행하고 SIE 없이 가능한 정적 PoseWatch를 우선 사용한다.
sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture,
anim_instance_source=post_process, prefer_pie_world=false)로 target ModifyBone
input/output을 같은 AnimInstance에서 캡처한다.

완료 조건: runtime_graph_prepost=true, same_instance_prepost=true, head yaw delta
about +5 degrees, descendant antenna motion present, original asset mutation 없음,
cleanup 완료.
```

## Ambiguity Rules

- "살짝", "조금", "더" means choose a conservative sample value and report it.
  Example: head yaw `5 deg`, Trail `Alpha=1` with existing chain length, BlendSpace
  Lean sample movement around `0.25` to `0.30` beyond the original.
- If the user names a body area but not a character, default to Bot unless the
  body area is Baddy-specific such as stalk or Blobling tail.
- If the request asks for a visible action but no action source exists, prove the
  existing route first and park new action-source authoring as a candidate.
- If the request sounds like world collision or destructible physics, do not force
  it into AnimBP physics. Split it into a separate world-physics task.
- If metadata internals are required, do not broad-probe Montage or notifies with
  generic Python. Use safe inventory, then park a guarded native API candidate.

## C++/API Decision Quick Check

Use `not needed` when:

- The request maps to Post Process ModifyBone, BlendSpace variant, ControlRig
  forced driver, Bot Trail sample, Baddy RigidBody sample tuning, state runtime
  response, or current PoseWatch instrumentation.

Use `candidate, not now` when:

- The request needs a new state/transition graph, new UpperBody action source,
  new connected RigidBody graph for Bot, deeper Trail parameter editing, exact
  PhysicsAsset constraints, or protected notify/curve/Montage internals.

Use `ask first` when:

- The candidate would change original assets, write non-exception C++, save a
  real map, or use a billed/API route.
