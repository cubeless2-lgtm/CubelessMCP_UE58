# StackOBot Animation Route Matrix

Use this matrix before filling `docs/stackobot-animation-request-run-template.md`.
It is the compact grammar for turning a concrete animation request into a route,
safe first action, verification command, C++ decision, and approval boundary.

Source examples:

- `docs/stackobot-animation-request-run-examples.md`
- `docs/stackobot-animation-request-playbook.md`
- `docs/stackobot-animation-acceptance-checklist.md`
- `docs/stackobot-cpp-api-decision-matrix.md`

## Route Classification

| Route token | Target | Body area | Timing type | Runtime layer |
| --- | --- | --- | --- | --- |
| `Post Process ModifyBone` | Bot | head | static late additive rotation | Post Process AnimBP |
| `BlendSpace sample variant` | Bot | locomotion body response | continuous BlendSpace axis response | main AnimBP source BlendSpace |
| `Bot Trail sample` | Bot | antenna_04_l chain, mirrored only if requested | secondary motion / follow-through | Post Process AnimBP physics-style node |
| `UpperBody Slot and LayeredBlend` | Bot | upper body | overlay action over locomotion | Slot / LayeredBoneBlend in main AnimBP |
| `protected metadata boundary` | Bot or Baddy, depending on the named asset | animation source metadata | notify / curve / sync marker / Montage metadata | animation asset metadata, not pose graph |
| `ControlRig gate probe` | Bot | foot IK / interaction reach | late correction gated by runtime inputs and curves | ControlRig inside the main AnimBP |
| `state-machine runtime-driver proof` | Bot | locomotion state-machine behavior | state duration or transition condition | main AnimBP state machine |
| `Baddy RigidBody` | Baddy | stalk / body secondary motion | animation physics response | RigidBody node in the AnimBP |
| `node resolver plus same-instance pre/post proof` | Bot or Baddy, depending on the selected graph | target node output and affected bones | instrumentation only | compiled AnimGraph node contribution |

## Execution Matrix

| Route token | Sample target | First command | Verification command |
| --- | --- | --- | --- |
| `Post Process ModifyBone` | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadYawPlus5Study` | `ensure_postprocess_anim_demo_variant` | `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process, prefer_pie_world=false)` |
| `BlendSpace sample variant` | `/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanWideStudy` | `ensure_blendspace_sample_variant` | `sample_blendspace_runtime_pose_grid` |
| `Bot Trail sample` | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study` | `ensure_anim_graph_trail_demo` or reuse existing sample | `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process, prefer_pie_world=true)` |
| `UpperBody Slot and LayeredBlend` | none for route proof; future sample overlay only if action source is required | slot/cached-pose inventory, then all-input PoseWatch | `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, input_pose_mode=all)` |
| `protected metadata boundary` | none until a guarded native API is approved/implemented | safe animation asset inventory and AssetRegistry-level scan only | none for protected internals with current tooling |
| `ControlRig gate probe` | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ForcedDriver_Study` | `inspect_anim_graph_protected_topology`, then `controlrig_direct_gate_probe` | `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` |
| `state-machine runtime-driver proof` | none for first pass; future sample graph only if runtime-driver proof is insufficient | `inspect_anim_state_machine_transitions` | `sample_anim_state_machine_runtime_response` |
| `Baddy RigidBody` | `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study` for first proof; duplicate the same prefix only if tuning needs a new variant | `inspect_anim_graph_node_settings` | `sample_anim_node_pre_post_runtime_pose(mode=compiled_graph_mapping)`, then PoseWatch capture if runtime proof is needed |
| `node resolver plus same-instance pre/post proof` | none unless a controlled sample actor is needed for runtime proof | `inspect_anim_graph_protected_topology` or compiled mapping for the suspected node | `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` |

## Evidence And Approval Matrix

| Route token | Expected evidence | C++/API status | Approval boundary |
| --- | --- | --- | --- |
| `Post Process ModifyBone` | `runtime_graph_prepost=true`, `same_instance_prepost=true`, requested bone delta | not needed | `false` |
| `BlendSpace sample variant` | `valid_pose_count` for the requested grid and `input_changed_pose=true` | not needed | `false` |
| `Bot Trail sample` | same-instance Trail input/output and target chain delta in SIE/PIE | not needed for current Trail sample | `false` |
| `UpperBody Slot and LayeredBlend` | BasePose from `LocomotionPose` and `BlendPoses[0]` from `CashedPose_UpperBody` in the same AnimInstance | candidate only if a new visible action source or overlay branch is required | false for route proof, true before original asset mutation |
| `protected metadata boundary` | readable fields plus blocked protected fields | candidate guarded native API for concrete metadata requests | true before implementing or using new guarded native API |
| `ControlRig gate probe` | root-connected ControlRig, required gates, and same-instance pre/post delta in the forced-driver sample | not needed unless the requested gate or pin cannot be driven by existing commands | false for sample proof, true before editing original ABP_Bot or CR_Bot_Correction |
| `state-machine runtime-driver proof` | explicit driver cases with current state, transition progress, state weight, and restored runtime properties | candidate only if a new state, sequence player, or transition rule must be authored | false for read/runtime proof, true before original graph mutation or new authoring API |
| `Baddy RigidBody` | RigidBody settings, mapped runtime node, and source-vs-output or pre/post pose deltas for the stalk chain | not needed for narrow setting reads or sample tuning; candidate for deeper PhysicsAsset inspection | false for sample/read proof, true before original physics asset or AnimBP mutation |
| `node resolver plus same-instance pre/post proof` | target node selection, runtime/editor mapping when needed, input/output links, sampled bone deltas, and same-instance confirmation | not needed unless the node class is unsupported or actor/AnimInstance resolution repeatedly fails | false while the work is read-only instrumentation |

## Selection Rules

- Start with `Post Process ModifyBone` when the request is a late static bone offset after the main animation.
- Start with `BlendSpace sample variant` when the request changes an axis-driven locomotion response.
- Start with `Bot Trail sample` or `Baddy RigidBody` when the request is animation-side secondary motion or follow-through.
- Start with `UpperBody Slot and LayeredBlend` when locomotion must continue while an upper-body action is overlaid.
- Start with `protected metadata boundary` for notifies, curves, sync markers, Montage metadata, or timing reads that are not pose-graph behavior.
- Start with `ControlRig gate probe` when the request depends on IK, interaction reach, or late correction gates.
- Start with `state-machine runtime-driver proof` when the behavior is a state duration, transition condition, or state-machine response.
- Start with `node resolver plus same-instance pre/post proof` when the request asks which node caused a pose change.

## Stop Conditions

- If the route requires original asset mutation, stop and ask unless the user already approved that exact mutation.
- If the route needs protected metadata or Montage internals, stop before broad generic Python probing.
- If a sample route cannot produce same-instance or route-specific proof, report the blocker before expanding scope.
- If a concrete request needs a new non-exception C++ API, park it in `docs/stackobot-cpp-api-decision-matrix.md` before implementation.
