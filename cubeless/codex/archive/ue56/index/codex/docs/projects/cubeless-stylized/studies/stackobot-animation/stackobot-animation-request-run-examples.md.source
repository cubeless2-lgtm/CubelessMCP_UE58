# StackOBot Animation Request Run Examples

Use these dry-run examples with
`docs/stackobot-animation-request-run-template.md`. They are not execution
results. They show how to fill the record before Tivret touches assets.

## Example 1: Bot Head Yaw

```text
user_request:
Bot head should look 5 degrees to the right after the main animation.

target_character: Bot
target_body_area: head
timing_type: static late additive rotation
runtime_layer: Post Process AnimBP
route: Post Process ModifyBone sample
sample_target:
/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadYawPlus5Study
first_read_or_authoring_command:
ensure_postprocess_anim_demo_variant
verification_command:
sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture,
anim_instance_source=post_process, prefer_pie_world=false)
expected_evidence:
runtime_graph_prepost=true, same_instance_prepost=true, head yaw about 5 degrees
handoff_template:
Post Process ModifyBone
cxx_api_status:
not needed
ask_user_first:
false
route_matrix_checked:
true
route_token_document_map_checked:
true
route_token_acceptance_map_checked:
true
route_matrix_notes:
Matches the Post Process ModifyBone route classification, execution, and evidence/approval rows.
```

Acceptance focus:

- sample Post Process AnimBP compiles;
- sample SkeletalMesh uses the sample Post Process AnimBP;
- PoseWatch samples the Post Process instance, not only the main AnimInstance;
- original `ABP_Bot`, `SKM_Bot`, and maps remain untouched.

## Example 2: Wider Run Lean

```text
user_request:
Make Bot run lean wider left and right.

target_character: Bot
target_body_area: locomotion body response
timing_type: continuous BlendSpace axis response
runtime_layer: main AnimBP source BlendSpace
route: BlendSpace sample variant
sample_target:
/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanWideStudy
first_read_or_authoring_command:
ensure_blendspace_sample_variant
verification_command:
sample_blendspace_runtime_pose_grid
expected_evidence:
valid_pose_count matches the requested grid and input_changed_pose=true
handoff_template:
BlendSpace Sample Variant
cxx_api_status:
not needed
ask_user_first:
false
route_matrix_checked:
true
route_token_document_map_checked:
true
route_token_acceptance_map_checked:
true
route_matrix_notes:
Matches the BlendSpace sample variant route classification, execution, and evidence/approval rows.
```

Acceptance focus:

- original `BS_Bot_WalkRunLean` is read-only;
- edited sample coordinates are explicit;
- pose grid compares center, lean-left, and lean-right inputs;
- deltas are treated as controlled evidence, not exact visual approval.

## Example 3: Antenna Lag

```text
user_request:
Make the Bot antenna lag behind movement.

target_character: Bot
target_body_area: antenna_04_l chain, mirrored only if requested
timing_type: secondary motion / follow-through
runtime_layer: Post Process AnimBP physics-style node
route: Bot Trail sample
sample_target:
/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study
first_read_or_authoring_command:
ensure_anim_graph_trail_demo or reuse existing sample
verification_command:
sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture,
anim_instance_source=post_process, prefer_pie_world=true)
expected_evidence:
same-instance Trail input/output and target chain delta in SIE/PIE
handoff_template:
Trail Or Secondary Motion
cxx_api_status:
not needed for current Trail sample
ask_user_first:
false
route_matrix_checked:
true
route_token_document_map_checked:
true
route_token_acceptance_map_checked:
true
route_matrix_notes:
Matches the Bot Trail sample route classification, execution, and evidence/approval rows.
```

Acceptance focus:

- do not reactivate the disconnected original `ABP_Bot` Trail node;
- proof actor uses component-level Post Process override;
- moving physics-style proof prefers SIE/PIE;
- static editor tick alone is not final proof for moving lag behavior.

## Example 4: Upper Body While Moving

```text
user_request:
Play an upper-body action while locomotion continues.

target_character: Bot
target_body_area: upper body
timing_type: overlay action over locomotion
runtime_layer: Slot / LayeredBoneBlend in main AnimBP
route: existing UpperBody Slot and LayeredBlend route proof
sample_target:
none for route proof; future sample overlay only if action source is required
first_read_or_authoring_command:
slot/cached-pose inventory, then all-input PoseWatch
verification_command:
sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture,
input_pose_mode=all)
expected_evidence:
BasePose from LocomotionPose and BlendPoses[0] from CashedPose_UpperBody in the same AnimInstance
handoff_template:
UpperBody Slot And LayeredBlend
cxx_api_status:
candidate only if a new visible action source or overlay branch is required
ask_user_first:
false for route proof, true before original asset mutation
route_matrix_checked:
true
route_token_document_map_checked:
true
route_token_acceptance_map_checked:
true
route_matrix_notes:
Matches the UpperBody Slot and LayeredBlend route classification, execution, and evidence/approval rows.
```

Acceptance focus:

- near-zero pose delta is only route proof when no action source is played;
- do not report visible action proof unless an action source actually runs;
- a new overlay/action source may trigger `ensure_layered_slot_overlay_sample`;
- original `ABP_Bot` graph editing still requires explicit approval.

## Example 5: Notify Or Montage Timing

```text
user_request:
Read the notify timing for this action and align another event to it.

target_character: Bot or Baddy, depending on the named asset
target_body_area: animation source metadata
timing_type: notify / curve / sync marker / Montage metadata
runtime_layer: animation asset metadata, not pose graph
route: protected metadata boundary
sample_target:
none until a guarded native API is approved/implemented
first_read_or_authoring_command:
safe animation asset inventory and AssetRegistry-level scan only
verification_command:
none for protected internals with current tooling
expected_evidence:
clear report of readable fields and blocked protected fields
handoff_template:
Notify, Curve, Sync Marker, Or Montage Internals
cxx_api_status:
candidate guarded native API for concrete metadata requests
ask_user_first:
true before implementing or using new guarded native API
route_matrix_checked:
true
route_token_document_map_checked:
true
route_token_acceptance_map_checked:
true
route_matrix_notes:
Matches the protected metadata boundary route classification, execution, and evidence/approval rows.
```

Acceptance focus:

- do not broad-probe Montage internals with generic Python;
- do not guess protected notify, curve, or sync-marker data;
- if safe inventory is insufficient, stop and park the guarded API candidate;
- mention the known `AnimMontage.h:770` crash boundary.

## Example 6: ControlRig Foot Interaction

```text
user_request:
Make the Bot foot reach an interaction point.

target_character: Bot
target_body_area: foot IK / interaction reach
timing_type: late correction gated by runtime inputs and curves
runtime_layer: ControlRig inside the main AnimBP
route: ControlRig gate probe, then forced-driver sample if gameplay gates are inactive
sample_target:
/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ForcedDriver_Study
first_read_or_authoring_command:
inspect_anim_graph_protected_topology, then controlrig_direct_gate_probe
verification_command:
sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)
expected_evidence:
ControlRig is root-connected, required gates are identified, and same-instance pre/post delta appears in the forced-driver sample
handoff_template:
ControlRig Late Correction
cxx_api_status:
not needed unless the requested gate or pin cannot be driven by existing commands
ask_user_first:
false for sample proof, true before editing original ABP_Bot or CR_Bot_Correction
route_matrix_checked:
true
route_token_document_map_checked:
true
route_token_acceptance_map_checked:
true
route_matrix_notes:
Matches the ControlRig gate probe route classification, execution, and evidence/approval rows.
```

Acceptance focus:

- direct ControlRig solve is not the same as compiled AnimGraph proof;
- gate names and input values must come from topology/probe results, not guesses;
- gameplay-gated behavior should use the forced-driver sample when transient play does not activate the route;
- original ControlRig and original AnimBP remain read-only.

## Example 7: Hover Transition Timing

```text
user_request:
Make hover stay longer before returning to walk.

target_character: Bot
target_body_area: locomotion state-machine behavior
timing_type: state duration or transition condition
runtime_layer: main AnimBP state machine
route: state-machine runtime-driver proof before graph authoring
sample_target:
none for first pass; future sample graph only if runtime-driver proof is insufficient
first_read_or_authoring_command:
inspect_anim_state_machine_transitions
verification_command:
sample_anim_state_machine_runtime_response
expected_evidence:
explicit driver cases show current state, transition progress, state weight, and restored runtime properties
handoff_template:
State Machine Or Runtime Driver
cxx_api_status:
candidate only if a new state, sequence player, or transition rule must be authored
ask_user_first:
false for read/runtime proof, true before original graph mutation or new authoring API
route_matrix_checked:
true
route_token_document_map_checked:
true
route_token_acceptance_map_checked:
true
route_matrix_notes:
Matches the state-machine runtime-driver proof route classification, execution, and evidence/approval rows.
```

Acceptance focus:

- do not claim a new hover rule was authored from runtime-driver evidence alone;
- runtime properties must be restored after each case;
- the sampled world and AnimInstance must be reported;
- graph authoring stays parked until the concrete request proves runtime-driver proof is not enough.

## Example 8: Baddy Soft Stalk

```text
user_request:
Make the Baddy stalk feel softer and more delayed.

target_character: Baddy
target_body_area: stalk / body secondary motion
timing_type: animation physics response
runtime_layer: RigidBody node in the AnimBP
route: existing Baddy RigidBody read or sample tuning
sample_target:
/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study for first proof; duplicate the same prefix only if tuning needs a new variant
first_read_or_authoring_command:
inspect_anim_graph_node_settings
verification_command:
sample_anim_node_pre_post_runtime_pose(mode=compiled_graph_mapping), then pose_watch_capture if runtime proof is needed
expected_evidence:
RigidBody settings, mapped runtime node, and source-vs-output or pre/post pose deltas for the stalk chain
handoff_template:
Trail Or Secondary Motion
cxx_api_status:
not needed for narrow setting reads or sample tuning; candidate for deeper PhysicsAsset inspection
ask_user_first:
false for sample/read proof, true before original physics asset or AnimBP mutation
route_matrix_checked:
true
route_token_document_map_checked:
true
route_token_acceptance_map_checked:
true
route_matrix_notes:
Matches the Baddy RigidBody route classification, execution, and evidence/approval rows.
```

Acceptance focus:

- separate animation-physics behavior from world physics or collision requests;
- do not infer body or constraint limits that were not safely inspected;
- same-instance proof is preferred before reporting the effect as complete;
- original Baddy assets remain untouched unless explicitly approved.

## Example 9: Node Contribution Proof

```text
user_request:
Prove which node changed this pose.

target_character: Bot or Baddy, depending on the selected graph
target_body_area: target node output and affected bones
timing_type: instrumentation only
runtime_layer: compiled AnimGraph node contribution
route: node resolver plus same-instance pre/post proof
sample_target:
none unless a controlled sample actor is needed for runtime proof
first_read_or_authoring_command:
inspect_anim_graph_protected_topology or compiled mapping for the suspected node
verification_command:
sample_anim_node_pre_post_runtime_pose(mode=pose_watch_capture)
expected_evidence:
target node selection, runtime/editor node mapping when needed, input/output links, sampled bone deltas, and same-instance confirmation
handoff_template:
no authoring handoff unless the proof needs a sample actor setup
cxx_api_status:
not needed unless the node class is unsupported or actor/AnimInstance resolution repeatedly fails
ask_user_first:
false while the work is read-only instrumentation
route_matrix_checked:
true
route_token_document_map_checked:
true
route_token_acceptance_map_checked:
true
route_matrix_notes:
Matches the node resolver plus same-instance pre/post proof route classification, execution, and evidence/approval rows.
```

Acceptance focus:

- do not start by editing assets;
- identify the suspected node before interpreting pose deltas;
- use compiled mapping when editor node identity and runtime node identity could diverge;
- if node selection is ambiguous, report ambiguity instead of over-claiming causality.
