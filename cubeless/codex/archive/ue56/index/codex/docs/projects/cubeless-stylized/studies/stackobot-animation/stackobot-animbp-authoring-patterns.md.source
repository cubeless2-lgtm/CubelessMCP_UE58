# StackOBot AnimBP Authoring Patterns

This guide converts the StackOBot animation study evidence into reusable authoring rules.
Use it when a future request asks to create or modify an animation part without re-reading
the sample project from scratch.

For step-by-step execution on a concrete user request, use:
`docs/stackobot-animation-request-playbook.md`.

For the shortest request-to-route authoring templates, use:
`docs/stackobot-animation-authoring-templates.md`.

For compact MCP command parameter syntax, use:
`docs/stackobot-animation-mcp-command-syntax.md`.

Scope:

- Reference project: `<workspace-parent>/SampleProject/StackOBot`.
- Study artifacts: `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy`.
- Default safe authoring target: `/Game/_MCP_Sample/AnimStudy`.
- Original StackOBot assets are reference-only unless the user explicitly approves editing them.

## Runtime Grammar

StackOBot should be read as a layered runtime pose stack:

```text
Gameplay / Blueprint state
  -> Main AnimBP AnimInstance
  -> State machines, BlendSpaces, slots, cached poses, layered blends
  -> Optional late Control Rig correction
  -> AnimGraph root pose
  -> Optional SkeletalMesh Post Process AnimBP
  -> Final component-space pose
```

`ABP_Bot` main path:

```text
GroundLocomotion -> SaveCachedPose GroundLocoPose
GroundLocoPose -> AirLocomotion -> SaveCachedPose LocomotionPose
LocomotionPose -> UpperBody Slot -> SaveCachedPose CashedPose_UpperBody
LocomotionPose + CashedPose_UpperBody -> LayeredBoneBlend -> SaveCachedPose FullBodyPose
IsInactive ? A_Bot_Idle_Inactive : FullBodyPose -> ControlRig -> Root
```

`ABP_Baddy` physics path:

```text
New State Machine
  -> LocalToComponentSpace
  -> RigidBody
  -> ComponentToLocalSpace
  -> DefaultSlot
  -> Root
```

Post Process AnimBP path used by the study variants:

```text
LinkedInputPose
  -> LocalToComponentSpace
  -> Transform/Physics node
  -> ComponentToLocalSpace
  -> Root
```

## Request Classification

| User request shape | Primary authoring surface | StackOBot pattern |
| --- | --- | --- |
| Idle, walk, run, jump, land, hover behavior | Main AnimBP state machine and transitions | `GroundLocomotion`, `AirLocomotion`, variables such as `GroundSpeed`, `IsInAir?`, `MovementInput?`, `IsHovering` |
| Speed, lean, directional locomotion response | BlendSpace | `BS_Bot_WalkRunLean` and `BS_Bot_RunIdleJump` |
| Upper-body interaction layered over locomotion | Slot, cached pose, layered blend | `UpperBody` slot, `CashedPose_UpperBody`, `LayeredBoneBlend` |
| Foot correction, interaction IK, late pose cleanup | Control Rig in AnimGraph | `CR_Bot_Correction` through `AnimGraphNode_ControlRig` |
| Head, antenna, cosmetic late adjustment | Post Process AnimBP | `LinkedInputPose -> ModifyBone -> Root` variants |
| Secondary motion and physics reaction | RigidBody or Trail node | `ABP_Baddy` RigidBody, `_MCP_Sample` Bot Trail study |
| Debug question about before/after node contribution | Runtime PoseWatch pre/post | `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` |

## Route Token Pattern Map

Use this table after the request compiler chooses an exact route token. It maps
the token back to this document's pattern section or protected boundary, then to
the safest first command and required proof command.

| Route token | Pattern or boundary | First read or authoring command | Verification command |
| --- | --- | --- | --- |
| `Post Process ModifyBone` | `5. Post Process AnimBP Pattern` | `ensure_postprocess_anim_demo_variant` | `sample_anim_node_pre_post_runtime_pose` |
| `BlendSpace sample variant` | `2. BlendSpace Pattern` | `ensure_blendspace_sample_variant` | `sample_blendspace_runtime_pose_grid` |
| `Bot Trail sample` | `6. Physics And Secondary Motion Pattern` | `ensure_anim_graph_trail_demo` | `sample_anim_node_pre_post_runtime_pose` |
| `UpperBody Slot and LayeredBlend` | `3. Slot And Layered Blend Pattern` | slot/cached-pose inventory | `sample_anim_node_pre_post_runtime_pose` |
| `protected metadata boundary` | protected metadata inventory boundary | safe animation asset inventory | none for protected internals |
| `ControlRig gate probe` | `4. Control Rig Pattern` | `inspect_anim_graph_protected_topology`, then `controlrig_direct_gate_probe` | `sample_anim_node_pre_post_runtime_pose` |
| `state-machine runtime-driver proof` | `1. State Machine Pattern` | `inspect_anim_state_machine_transitions` | `sample_anim_state_machine_runtime_response` |
| `Baddy RigidBody` | `6. Physics And Secondary Motion Pattern` | `inspect_anim_graph_node_settings` | `sample_anim_node_pre_post_runtime_pose` |
| `node resolver plus same-instance pre/post proof` | Verification Gate / node contribution proof | `inspect_anim_graph_protected_topology` or compiled mapping | `sample_anim_node_pre_post_runtime_pose` |

## Authoring Patterns

### 1. State Machine Pattern

Use this for locomotion mode changes, jump/land/hover flow, and compact two-state behavior.

Expected workflow:

1. Identify the owning state machine and the variables that already drive it.
2. Add or reuse a state with a SequencePlayer or BlendSpacePlayer.
3. Connect transitions with simple variable gates or automatic completion rules.
4. Keep the graph readable before adding helper functions.
5. Verify static topology and then runtime state response.

Useful StackOBot evidence:

- `ABP_Bot` ground locomotion is `Idle <-> Walk/Run`, gated by `GroundSpeed`.
- `ABP_Bot` air locomotion uses `IsInAir?`, `MovementInput?`, and `IsHovering`.
- `ABP_Baddy` is the compact physics sample: `A_Baddy_Idle <-> A_Baddy_Walk`, gated by `Is Moving`.
- The request-template runtime-driver rehearsal proved the read-only route end to end in editor-world: `GroundSpeed=0` sampled `GroundLocomotion=Idle`, `GroundSpeed=500` sampled `Walk/Run`, and a final `GroundSpeed=0` returned to `Idle`, with `asset_modified=false` and `saves_assets=false`.

Preferred verification:

- `inspect_anim_state_machine_transitions`.
- `inspect_anim_instance_runtime_state`.
- `sample_anim_state_machine_runtime_response`.
- Runtime bone/socket sampling only after the state transition is proven.

Known pitfall:

- Treat old plain-Python state-machine notes as API-gap history. Current runtime-driver verification should use the MCP commands above, not direct Python `AnimInstance` wrapper calls.

### 2. BlendSpace Pattern

Use this when the request is a continuous pose response, such as speed, lean, or authored
axis-driven pose mixing.

Expected workflow:

1. Read authored axis ranges and sample coordinates.
2. Confirm which source clips carry the visible motion.
3. Modify or create the BlendSpace only in a sample path first with
   `ensure_blendspace_sample_variant`.
4. Runtime-sample the grid through SIE/PIE style ticking.
5. Compare new pose deltas against the source pose-map baseline.

Useful StackOBot evidence:

- `BS_Bot_WalkRunLean` has neutral walk/run samples plus left/right lean samples.
- `GroundSpeed` controls the walk/run scale, while `LeanAmount` controls side bias.
- `BS_Bot_RunIdleJump` has near-overlapping authored jump samples, so state-machine timing matters more than broad speed interpolation.

Preferred verification:

- Source pose map artifacts for authored sample meaning.
- `sample_blendspace_runtime_pose_grid`.
- Do not trust the old non-SIE full-editor `AnimationSingleNode` path that produced `0.0` pose deltas.

Current tooling status:

- `ensure_blendspace_sample_variant` creates/reuses `_MCP_Sample` BlendSpace
  variants, edits axis ranges or sample coordinates, validates/resamples, and saves
  only the target asset.
- `sample_blendspace_runtime_pose_grid` remains the runtime evidence command.
- Smoke asset: `/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanWideStudy`.
- Request-template proof asset:
  `/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanTemplateRehearsal`.

### 3. Slot And Layered Blend Pattern

Use this when a request wants an overlay, such as interaction arms over locomotion.

Expected workflow:

1. Preserve the locomotion base pose in a cached pose.
2. Route the overlay through a named slot.
3. Save the overlay as a cached pose if it is reused.
4. Blend base and overlay with explicit branch filters.
5. Verify all relevant inputs, not just the final output.

Useful StackOBot evidence:

- `UpperBody` slot is in `DefaultGroup`.
- Base pose is `LocomotionPose`.
- Overlay pose is `CashedPose_UpperBody`.
- `LayeredBoneBlend` uses branch filters around `pelvis`, `thigh_r`, and `thigh_l` to exclude leg branches.
- Request-template proof artifacts:
  `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_LayeredBlendTemplateRehearsal_Topology_Summary.json`
  and `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_LayeredBlendTemplateRehearsal_PoseWatch_Summary.json`.
- The PIE rehearsal captured both `BasePose` and `BlendPoses[0]` in the same runtime AnimInstance with `runtime_graph_prepost=true` and `same_instance_prepost=true`.
- If no montage or action clip is played, near-zero pose deltas are normal; treat that case as route/instrumentation proof, not as visual overlay proof.

Preferred verification:

- `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, input_pose_mode=all)`.
- Confirm `BasePose` and `BlendPoses[0]` against the same post-node output.
- Use the existing `UpperBody` route first. Add or request a new slot/layered-blend authoring command only when a future request needs a new overlay branch rather than the existing StackOBot route.

### 4. Control Rig Pattern

Use this for late IK/correction, contact adjustment, and interaction-driven pose cleanup.

Expected workflow:

1. Treat the active Control Rig as a late correction layer, not as the source of locomotion arcs.
2. Confirm the ControlRig node is in the root-connected path.
3. Identify required input variables and curve gates.
4. If gameplay does not naturally activate the branch, use a sample forced-driver AnimBP.
5. Verify both direct ControlRig solve behavior and compiled AnimGraph same-instance behavior.

Useful StackOBot evidence:

- Active class: `/Game/StackOBot/Characters/Bot/Rig/CR_Bot_Correction.CR_Bot_Correction_C`.
- Inputs include `InteractionWorldLocation` and `ShouldDoIKTrace`.
- Important curves include `IK_blend_interact` and `IKBlend_l`.
- Direct probes show `IK_blend_interact=1` is the important interaction gate.
- Same-instance PoseWatch evidence exists for `/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ForcedDriver_Study`.

Preferred verification:

- `inspect_anim_graph_protected_topology`.
- `controlrig_direct_gate_probe`.
- `ensure_anim_graph_modify_curve_demo`.
- `set_anim_graph_controlrig_input_defaults`.
- `ensure_controlrig_forced_driver_animbp`.
- `sample_controlrig_pre_post_runtime_pose` for direct transient rig evidence.
- `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` for compiled AnimGraph evidence.

Known pitfall:

- Original `ABP_Bot` runtime setup may keep the interaction branch gated off. Use the forced-driver sample when the goal is to prove the node contribution.

### 5. Post Process AnimBP Pattern

Use this for late cosmetic or corrective bone adjustments after the main AnimBP final pose.

Expected workflow:

1. Duplicate or reuse a sample skeletal mesh under `/Game/_MCP_Sample`.
2. Build a Post Process AnimBP from `LinkedInputPose`.
3. Add a small transform or physics node chain.
4. Assign the duplicated mesh's Post Process AnimBP class.
5. Use component-level override in proof actors when runtime scripts need guaranteed activation.

Useful StackOBot evidence:

- Original `SKM_Bot` has no Post Process AnimBP assigned.
- Study variants prove parent and leaf behavior:
  - `HeadPitch` rotates `head` by about `6.0 deg` and moves antenna leaves by about `8.6 cm`.
  - `AntennaRoll` rotates only `antenna_04_l` by about `12.0 deg`.
- `HeadYawAuthoringPattern` proves the current sample-only authoring route can create a new Post Process Modify Bone variant with `head` yaw `8.0 deg`, compile/save it, assign the duplicated SkeletalMesh Post Process AnimBlueprint, reload it, and end with `dirty_package_count=0`.
- The same variant now has no-SIE editor-world PoseWatch proof: `head` rotates about `8.0 deg`, antenna leaf bones move about `11.74 cm`, and non-descendant bones remain at floating-point noise.
- `HeadYawPlus5Study` proves the reusable request-template path end to end: dry-run, sample-only create/save, editor-world no-SIE PoseWatch, and cleanup. It rotates `head` by about `5.0 deg`, moves antenna leaf bones about `7.34 cm`, keeps pelvis/neck at floating-point noise, and reports `original_assets_modified=false`.

Preferred verification:

- `ensure_postprocess_anim_demo_variant`.
- `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process)`.
- Static single-pose isolation when phase drift between separate AnimInstances would confuse the result.

Current caveat:

- Do not repeat the failed generic `execute_python` SIE actor setup; it crashed the hidden editor with `EXCEPTION_INT_DIVIDE_BY_ZERO`.
- For static Post Process ModifyBone proofs like `HeadYawAuthoringPattern`, the safer route is editor-world transient actor setup without SIE, then `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process, prefer_pie_world=false)`.
- If component-level Post Process override cannot be set through Python, relying on the duplicated SkeletalMesh `post_process_anim_blueprint` assignment is acceptable when the setup artifact confirms the sample mesh points to the expected Post Process AnimBP class.

### 6. Physics And Secondary Motion Pattern

Use this for RigidBody, Trail, spring-like antenna motion, stalk motion, and other secondary
motion requests.

Expected workflow:

1. Prefer Post Process AnimBP samples for isolated late secondary motion.
2. Keep component-space conversion nodes explicit when the physics node requires them.
3. Start with a real skeleton bone chain, not a disconnected or virtual-only setting.
4. Use SIE/PIE runtime proof; editor tick alone may not show meaningful physics motion.
5. Capture same-instance input/output where possible.

Useful StackOBot evidence:

- `ABP_Baddy` RigidBody is active and has same-instance PoseWatch evidence.
- Original `ABP_Bot` Trail node is disconnected and uses `VB VBHead`, which is not a valid clean skeleton-chain target for the sample.
- Active Bot Trail study uses `antenna_04_l` and requires explicit Post Process override for reliable scripted proof.
- The `StackOBot_TrailTemplateRehearsal_*` proof confirmed the reusable request route: `SKM_Bot_Trail_Study`, main `ABP_Bot_C`, explicit `ABP_Bot_Trail_Study_C` component override, SIE, then `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture, anim_instance_source=post_process)`.

Preferred verification:

- `ensure_anim_graph_trail_demo`.
- `sample_anim_node_pre_post_runtime_pose(mode=compiled_graph_mapping)`.
- `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)`.
- Runtime comparison only after the node is proven connected and active.

## MCP Command Map

| Need | Command |
| --- | --- |
| Static AnimGraph topology and protected node links | `inspect_anim_graph_protected_topology` |
| State-machine transition inventory | `inspect_anim_state_machine_transitions` |
| Runtime current state, weights, timing, transitions | `inspect_anim_instance_runtime_state` |
| Runtime property driver matrix | `sample_anim_state_machine_runtime_response` |
| BlendSpace runtime pose grid | `sample_blendspace_runtime_pose_grid` |
| BlendSpace sample variant authoring | `ensure_blendspace_sample_variant` |
| Control Rig direct gate proof | `controlrig_direct_gate_probe` |
| Sample ModifyCurve before ControlRig | `ensure_anim_graph_modify_curve_demo` |
| Set ControlRig input defaults in a sample | `set_anim_graph_controlrig_input_defaults` |
| Build forced-driver ControlRig sample | `ensure_controlrig_forced_driver_animbp` |
| Direct transient ControlRig pre/post solve | `sample_controlrig_pre_post_runtime_pose` |
| Post Process ModifyBone sample variant | `ensure_postprocess_anim_demo_variant` |
| Trail Post Process sample | `ensure_anim_graph_trail_demo` |
| Same-instance node pre/post capture | `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)` |

## C++ And API Escalation Rules

Use existing MCP commands and editor asset workflows first. Add or modify C++ only when the
current API surface cannot safely express or verify the requested behavior.

Keep these as candidate API needs, not immediate work:

1. Expand same-instance AnimGraph pre/post capture only if a new unusual node class falls outside the smoked RigidBody, Trail, ModifyBone, LayeredBoneBlend, and ControlRig paths.
2. Add narrower authoring commands when a requested graph edit requires protected pin routing or node configuration that Python/editor scripting cannot safely do.
3. Add runtime probes only when the existing state, BlendSpace, ControlRig, Post Process, and PoseWatch commands cannot produce the needed evidence.

Approval boundary:

- C++ inside the UnrealMCP plugin may be edited when justified by the requested MCP tooling work.
- Non-UnrealMCP project C++ still requires explicit user approval before editing.
- Asset-only requests should stay in Blueprint, AnimBP, ControlRig, Post Process AnimBP, or MCP/editor scripting unless code is explicitly requested.

## Verification Gate

Do not treat an authored animation change as complete until the relevant checks pass:

1. Static graph topology proves the new node or state is connected to the intended root path.
2. Sample assets compile with `compile_error_count=0`.
3. Runtime SIE/PIE smoke confirms the expected state, curve, or pose change.
4. Same-instance PoseWatch pre/post is used for node contribution claims when the command supports that node type.
5. Dirty package checks are reported, and original StackOBot assets remain unmodified unless explicitly approved.
6. Artifacts are written under `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy`.

## Safety Notes

- Do not use generic Unreal Python map creation or map loading through `execute_python`.
- Do not call `unreal.SystemLibrary.quit_editor()` from MCP Python cleanup routes.
- Prefer native `open_editor_level` or `safe_new_preview_map` for map work, with dry-run first.
- For SIE proof actors, create or identify the editor-world actor before starting SIE if the command needs to match the PIE duplicate by label.
- For ControlRig same-instance PoseWatch, do not call the capture command immediately after `editor_play_simulate`. Poll `inspect_anim_instance_runtime_state(require_pie_world=true)` until the matched PIE actor reports a live AnimInstance, then run `sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)`.
- Treat `editor_is_playing`, `refresh_bone_transforms`, and similar Python wrapper methods as version-dependent; failed helper calls should not be confused with final command failures.
- Keep `_MCP_Sample` learning assets disposable and gitignored unless the user explicitly asks to version a specific sample asset.
