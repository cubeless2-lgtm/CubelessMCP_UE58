# StackOBot Animation Acceptance Checklist

Use this checklist before reporting a StackOBot animation task as complete. It
does not choose the route; it verifies whether the chosen route produced enough
evidence.

## Universal Pass Gate

Every route must report these fields:

- route classification and why it was chosen;
- assets created or reused;
- whether original StackOBot assets were modified;
- compile/save result for authored sample assets;
- runtime world used for proof, such as editor world, SIE, or PIE;
- evidence artifact paths under `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy`;
- command `errors` and `warnings`;
- dirty content and map package status;
- cleanup status for transient actors and play sessions;
- C++/API decision: `not needed`, `candidate`, or `implemented`.

Do not mark the task complete when the only evidence is that an asset exists.
Animation work needs either a route-specific runtime proof or a clear statement
that the request was read-only.

## Route Token Acceptance Map

Use this quick map before final delivery. The detailed pass/fail text remains in
`Route-Specific Pass Criteria`, but each row here keeps the first command and
verification command aligned with the request route.

| Route token | Minimum evidence strength | First read or authoring command | Verification command | Complete only when |
| --- | --- | --- | --- | --- |
| `Post Process ModifyBone` | Same-instance pre/post | `ensure_postprocess_anim_demo_variant` | `sample_anim_node_pre_post_runtime_pose` | Target bone delta matches the requested transform and original assets were not edited without approval. |
| `BlendSpace sample variant` | Runtime smoke | `ensure_blendspace_sample_variant` | `sample_blendspace_runtime_pose_grid` | Sample coordinates are reported and `input_changed_pose=true` when a visible response is expected. |
| `Bot Trail sample` | Same-instance pre/post | `ensure_anim_graph_trail_demo` | `sample_anim_node_pre_post_runtime_pose` | Component-level Post Process override was used and the target chain shows a Trail response. |
| `UpperBody Slot and LayeredBlend` | Same-instance pre/post | `slot/cached-pose inventory` | `sample_anim_node_pre_post_runtime_pose` | `BasePose` and `BlendPoses[0]` are captured and route proof is not mistaken for visible action proof. |
| `protected metadata boundary` | Read-only topology | `safe animation asset inventory` | `none for protected internals` | Readable fields and blocked protected fields are reported, or a guarded native API candidate is parked. |
| `ControlRig gate probe` | Same-instance pre/post | `inspect_anim_graph_protected_topology`, then `controlrig_direct_gate_probe` | `sample_anim_node_pre_post_runtime_pose` | Required gates are identified and compiled AnimGraph proof is separated from direct transient rig proof. |
| `state-machine runtime-driver proof` | Runtime smoke | `inspect_anim_state_machine_transitions` | `sample_anim_state_machine_runtime_response` | Runtime cases report state response and restore changed runtime properties. |
| `Baddy RigidBody` | Same-instance pre/post | `inspect_anim_graph_node_settings` | `sample_anim_node_pre_post_runtime_pose` | Runtime node mapping or pose deltas support the physics effect and source clip motion is separated. |
| `node resolver plus same-instance pre/post proof` | Same-instance pre/post | `inspect_anim_graph_protected_topology` plus `compiled mapping` | `sample_anim_node_pre_post_runtime_pose` | Target node selection, input/output links, and same-instance confirmation are all reported. |

## Route-Specific Pass Criteria

| Route token | Route | Pass criteria | Fail or incomplete when |
| --- | --- | --- | --- |
| `Post Process ModifyBone` | Post Process ModifyBone | Sample Post Process AnimBP compiles; sample SkeletalMesh has the intended Post Process AnimBlueprint; PoseWatch reports `runtime_graph_prepost=true` and `same_instance_prepost=true`; target bone delta matches the requested transform. | Only the graph was created, target bone did not move, Post Process instance was not sampled, or original assets were edited without approval. |
| `BlendSpace sample variant` | BlendSpace sample variant | Sample BlendSpace is under `_MCP_Sample`; edited axis/sample coordinates are reported; runtime pose grid returns expected `valid_pose_count`; `input_changed_pose=true` when a visible response is expected. | Original BlendSpace was edited by accident, skeleton/animation compatibility failed, pose grid has no meaningful delta for a requested visual change, or only offline coordinates were reported. |
| `state-machine runtime-driver proof` | State-machine runtime driver | Transition topology was read; runtime cases are explicit; current state, state names, weights, transition progress, or relevant animation timing match the request; restored runtime properties are reported. | Only variables were set without state evidence, the wrong AnimInstance/world was sampled, or a graph authoring request is claimed complete through runtime-driver proof alone. |
| `ControlRig gate probe` | ControlRig late correction | ControlRig node is root-connected; direct gate probe identifies the required gates; forced-driver sample is used if gameplay gates are inactive; compiled AnimGraph PoseWatch proves same-instance pre/post. | Direct transient ControlRig solve is treated as compiled graph proof, gate values are guessed, or the request depends on gameplay execution that was not sampled. |
| `UpperBody Slot and LayeredBlend` | UpperBody Slot/LayeredBlend | Existing `UpperBody` slot/cached-pose route is confirmed; all-input PoseWatch captures `BasePose` and `BlendPoses[0]` in the same AnimInstance; report clearly distinguishes route proof from visible action proof. | Near-zero delta is reported as a visible action, no action/montage/source clip exists for the request, or branch filters were not checked. |
| `Bot Trail sample` | Bot Trail secondary motion | Trail sample or reused sample is under `_MCP_Sample`; component-level Post Process override is applied for proof actors; SIE/PIE is preferred for motion; same-instance Trail PoseWatch reports target chain delta and no unexpected root/body drift. | The disconnected original Trail node is activated, static editor tick is used as final physics proof for moving behavior, or no same-instance Post Process sample is captured. |
| `Baddy RigidBody` | Baddy RigidBody physics | Existing RigidBody settings or sample tuning are reported; mapped runtime node, PoseWatch/source comparison, or pose deltas support the effect; physics interpretation is separated from source clip motion. | World physics is confused with AnimBP RigidBody, only source animation deltas are shown, or PhysicsAsset details are inferred without guarded inspection. |
| `protected metadata boundary` | Notify/curve/sync-marker/Montage metadata | Safe asset inventory reports readable fields and blocked protected fields, or the result explicitly parks a guarded native API candidate. | Generic Python reflection is used against Montage internals, protected fields are guessed, or a concrete metadata edit is claimed without guarded tooling. |
| `node resolver plus same-instance pre/post proof` | Node contribution proof | Target node selection is unambiguous; compiled mapping or PoseWatch pre/post gives same-instance confirmation; input/output links or sampled bones explain the contribution. | Node selection is ambiguous, only static graph topology is shown, or unsupported node class is silently treated as covered. |

## Evidence Strength Levels

Use the strongest feasible level for the request:

| Level | Meaning | Acceptable for |
| --- | --- | --- |
| Read-only topology | Graph, node, state-machine, or asset metadata was inspected. | Explanations, route selection, and protected metadata boundary reports. |
| Sample compile/load | A sample asset was generated or reused and compiled/loaded. | Authoring smoke only; not enough for final visual behavior. |
| Runtime smoke | Runtime instance exists and responds at a broad level. | State-machine and BlendSpace behavior checks when exact node proof is not needed. |
| Same-instance pre/post | Input and output of the target node are captured on the same AnimInstance. | Final proof for ModifyBone, Trail, RigidBody, ControlRig, LayeredBoneBlend, and node contribution requests. |

## When To Stop And Escalate

Stop before final delivery and mark C++/API as `candidate` when:

- the current command surface cannot author the requested sample graph;
- the current command surface cannot verify the result with route-specific proof;
- protected notifies, curves, sync markers, or Montage internals are required;
- target actor or AnimInstance resolution fails repeatedly;
- a visible action request needs a source clip, Montage, Slot path, or overlay
  sample that does not exist.

Do not escalate just because the request is visually complex. Escalate only when
the existing safe route is blocked.

## Final User Report Checklist

The user-facing result should say:

- what was made or inspected;
- where the sample or evidence lives;
- whether original StackOBot assets were untouched;
- the runtime proof result in one or two concrete metrics;
- whether C++/API was unnecessary or parked;
- any residual risk that affects the next request.
