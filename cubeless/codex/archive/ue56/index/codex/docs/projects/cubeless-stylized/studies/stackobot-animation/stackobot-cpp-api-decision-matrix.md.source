# StackOBot C++ API Decision Matrix

Use this matrix before adding or modifying UnrealMCP C++ for StackOBot animation
work. The default answer is still no new C++: use existing sample assets,
runtime probes, and live read commands first.

Related docs:

- `docs/stackobot-animation-quickstart.md`
- `docs/stackobot-animation-route-matrix.md`
- `docs/stackobot-animation-request-playbook.md`
- `docs/stackobot-request-compiler-drills.md`
- `docs/stackobot-physics-request-grammar.md`
- `docs/stackobot-live-read-drill-2026-06-19.md`

## Current Rule

Do not add C++ just because a requested animation behavior is complex. Add or
modify UnrealMCP C++ only when one of these is true:

- the current command surface cannot safely author the sample graph;
- the current command surface cannot verify the runtime result;
- generic Python would need protected AnimGraph/Montage internals;
- repeated manual setup has become the risky part and should be replaced by a
  guarded native command.

UnrealMCP plugin C++ is approved by project rules when genuinely needed, but the
decision still has to be tied to a concrete blocked request.

## Covered, Do Not Rebuild

| Capability | Current route | Decision |
| --- | --- | --- |
| Post Process ModifyBone sample | `ensure_postprocess_anim_demo_variant` plus Post Process PoseWatch evidence | Covered |
| Bot Trail sample | `ensure_anim_graph_trail_demo` plus Post Process PoseWatch evidence | Covered |
| Baddy RigidBody setting read/tuning | `inspect_anim_graph_node_settings`, `set_anim_graph_rigidbody_settings` on sample assets | Covered narrowly |
| BlendSpace sample variant | `ensure_blendspace_sample_variant`, then `sample_blendspace_runtime_pose_grid` | Covered |
| ControlRig gate and forced-driver sample | `controlrig_direct_gate_probe`, `ensure_controlrig_forced_driver_animbp`, PoseWatch evidence | Covered |
| Existing UpperBody route proof | Slot/cached-pose inventory and all-input LayeredBoneBlend PoseWatch | Covered for route proof |
| State-machine read/runtime response | `inspect_anim_state_machine_transitions`, `inspect_anim_instance_runtime_state`, `sample_anim_state_machine_runtime_response` | Covered for read/probe |
| General node contribution proof | `sample_anim_node_pre_post_runtime_pose` when node mapping succeeds | Covered for smoked node classes |

## Route Token Decision Map

Use this table after the request route is compiled. It keeps the exact route
tokens aligned with the route matrix and request-run examples.

| Route token | Current C++/API decision | If blocked |
| --- | --- | --- |
| `Post Process ModifyBone` | not needed for current sample and PoseWatch proof | Revisit only if sample graph authoring cannot be expressed by the current command. |
| `BlendSpace sample variant` | not needed for current sample variant and pose-grid proof | Revisit only if BlendSpace sample editing or grid verification is blocked. |
| `Bot Trail sample` | not needed for current Trail sample and Post Process PoseWatch proof | Revisit only if a future Trail parameter cannot be authored through the current command surface. |
| `UpperBody Slot and LayeredBlend` | candidate only if a visible overlay source or branch must be authored | Prove the existing Slot/LayeredBlend route first, then park `ensure_layered_slot_overlay_sample` if source authoring is required. |
| `protected metadata boundary` | candidate guarded native API for concrete metadata requests | Use safe inventory first; do not broad-probe Montage internals with generic Python. |
| `ControlRig gate probe` | not needed for direct gate probe or forced-driver sample proof | Revisit only if a requested gate or pin cannot be driven by existing commands. |
| `state-machine runtime-driver proof` | candidate only if a new state, sequence player, BlendSpace player, or transition rule must be authored | Run runtime-driver proof first, then park `ensure_state_machine_sample_variant` if graph authoring is required. |
| `Baddy RigidBody` | not needed for setting reads or sample tuning; candidate for deeper PhysicsAsset inspection | Use existing RigidBody settings and pose deltas first, then park guarded PhysicsAsset inspection if body/constraint details are required. |
| `node resolver plus same-instance pre/post proof` | not needed while compiled mapping and PoseWatch can identify the node | Revisit only for unsupported node classes or repeated actor/AnimInstance resolution failure. |

## Current Candidate Shortlist

Keep this as the latest "consider C++ only if blocked" list:

| Candidate area | Concrete trigger | Current decision |
| --- | --- | --- |
| Notify, curve, sync marker, or Montage metadata | Safe animation inventory and AssetRegistry-level scan cannot answer a concrete metadata request. | Candidate guarded native API; highest crash-risk boundary. |
| State-machine graph authoring | Runtime-driver proof is insufficient and the user needs a new state, sequence player, BlendSpace player, or transition rule. | Candidate `ensure_state_machine_sample_variant`. |
| UpperBody overlay source or branch authoring | Existing Slot/LayeredBlend route is proven but no compatible visible action source or overlay branch exists. | Candidate `ensure_layered_slot_overlay_sample`. |
| Deeper PhysicsAsset inspection | A request depends on bodies, constraints, limits, or solver details beyond exposed AnimBP node settings. | Candidate read-only guarded inspector first. |
| Unsupported node resolver or repeated AnimInstance resolution failure | Node contribution proof cannot map the requested node class, or repeated proof attempts fail because actor/component/PIE instance selection is fragile. | Candidate resolver extension or guarded target actor resolver. |

## Candidate Matrix

| Candidate | Implement when | Do first | Current status |
| --- | --- | --- | --- |
| `ensure_state_machine_sample_variant` | A user asks to add or change a real state, transition, sequence player, BlendSpace player, or transition rule and runtime-driver proof is insufficient. | Inspect transitions, run runtime response cases, write a non-C++ manual graph plan. | Parked |
| `ensure_layered_slot_overlay_sample` | A user asks for a visible upper-body action source that does not exist in the current `UpperBody` route. | Prove existing Slot/LayeredBlend route, identify source animation/montage need. | Parked |
| `ensure_anim_graph_rigidbody_demo_variant` | A user asks for Bot body physics or another new connected RigidBody chain not covered by Baddy samples. | Try existing Baddy RigidBody evidence or Bot Trail route; classify whether it is animation physics or world physics. | Parked |
| `sample_anim_physics_variant_matrix` | Repeated physics tuning requests need comparable baseline/variant SIE metrics in one safe command. | Use existing RigidBody/Trail evidence and single-node PoseWatch first. | Parked |
| `inspect_physics_asset_constraints_guarded` | A user request depends on PhysicsAsset bodies/constraints, limits, or solver details beyond exposed counts/settings. | Read current PhysicsAsset summary and decide if AnimBP physics is enough. | Parked |
| `inspect_or_author_anim_notifies_curves` | A user request needs sequence notifies, sync markers, curves, or Montage internals. | Use safe asset inventory and AssetRegistry-level Montage scan; do not broad-probe Montage Python. | Highest-risk parked candidate |
| `resolve_anim_posewatch_target_actor` | Repeated verification attempts fail because actor/component/PIE duplicated instance resolution is the fragile part. | Try documented transient actor setup and component-level Post Process override first. | Parked |
| `extend_anim_node_runtime_mapping` | A node contribution request targets an unsupported AnimGraph node class or editor/runtime node identity cannot be mapped with existing compiled mapping. | Try `inspect_anim_graph_protected_topology`, compiled mapping, and same-instance PoseWatch first. | Parked |
| Broader Trail parameter editor | A future request needs relaxation curves, rotation limits, planar/stretch limits, debug display, or fake velocity cases beyond current command inputs. | Use current `ensure_anim_graph_trail_demo` and isolated temp FakeVelocity evidence first. | Parked |
| `inspect_blueprint_graph_call_topology` | A future request depends on exact Blueprint call flow for interaction/action playback and Python graph traversal times out. | Use AssetRegistry references, variable/function inventory, and existing runtime route proof first. | Parked |

## Immediate Implementation Triggers

Start C++ implementation only if the active request matches one of these:

1. "Make a new state/transition" and runtime property cases cannot express the
   requested behavior.
2. "Play this specific upper-body action while moving" and no compatible action
   source/overlay sample route exists.
3. "Add body physics to Bot" where Trail is not semantically sufficient and
   Baddy RigidBody evidence is only a reference.
4. "Read or author notifies/curves/montage sections" where the safe inventory is
   not enough.
5. "The proof keeps failing because the target actor/AnimInstance cannot be
   resolved" across repeated attempts.
6. "Prove this specific node changed the pose" where the node class is
   unsupported or editor/runtime node identity cannot be mapped after the
   documented topology and compiled mapping reads.
7. "Inspect the physics asset limits/constraints" where exposed RigidBody node
   settings and sample pose deltas are insufficient.

If the request is only "make it stronger", "make it lean more", "turn head",
"antenna lag", or "show which node changed the pose", do not start new C++.

## Verification For Any New C++ API

Any new UnrealMCP C++ command must include:

- sample-root guard by default where it writes assets;
- `allow_non_sample=false` default for asset mutation commands;
- structured `success`, `errors`, `warnings`, and changed-asset report;
- clear dry-run path when feasible;
- compile/save reporting for sample assets;
- no generic Python map switching;
- live StackOBot bridge smoke on `127.0.0.1:55557`;
- sibling `unreal-mcp-cubeless` docs/wrapper update if the command belongs in
  the shared tool surface.

## Timing Decision

Current timing: do not implement more C++ yet.

Reason: the live read drill on 2026-06-19 confirmed the current request compiler
matches live nodes for Post Process ModifyBone, Bot Trail, Baddy RigidBody,
ControlRig, UpperBody LayeredBlend, and state-machine routes. The remaining C++
work should wait for a concrete user request that crosses one of the triggers
above.
