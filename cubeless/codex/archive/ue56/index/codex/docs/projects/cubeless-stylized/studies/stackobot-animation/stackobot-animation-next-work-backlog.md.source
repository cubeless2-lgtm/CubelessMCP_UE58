# StackOBot Animation Next Work Backlog

Use this backlog after `docs/stackobot-animation-study-closeout.md` when deciding
what to do next. Items are trigger-based: do not implement them just because they
exist.

## Current Default

The next real animation request should start sample-only. Existing docs and MCP
commands are enough for the common requests:

- Post Process ModifyBone head/neck/antenna offsets;
- BlendSpace lean/speed response variants;
- ControlRig gate probes and forced-driver samples;
- existing UpperBody Slot/LayeredBoneBlend route proof;
- Bot Trail secondary motion;
- Baddy RigidBody read/sample tuning;
- state-machine read/runtime-driver proof;
- node contribution PoseWatch or compiled mapping.

No immediate C++ work is scheduled.

## Route Token Backlog Map

Use this table after request compilation. It tells whether the route is ready,
blocked, or only a future candidate, and keeps the first proof command aligned
with the route matrix and command syntax.

| Route token | Backlog posture | First read or authoring command | Verification command |
| --- | --- | --- | --- |
| `Post Process ModifyBone` | ready sample route | `ensure_postprocess_anim_demo_variant` | `sample_anim_node_pre_post_runtime_pose` |
| `BlendSpace sample variant` | ready sample route | `ensure_blendspace_sample_variant` | `sample_blendspace_runtime_pose_grid` |
| `Bot Trail sample` | ready sample route | `ensure_anim_graph_trail_demo` | `sample_anim_node_pre_post_runtime_pose` |
| `UpperBody Slot and LayeredBlend` | route proof ready; visible action source remains candidate | slot/cached-pose inventory | `sample_anim_node_pre_post_runtime_pose` |
| `protected metadata boundary` | guarded read/API candidate only | safe animation asset inventory | none for protected internals |
| `ControlRig gate probe` | ready direct probe and forced-driver sample route | `inspect_anim_graph_protected_topology`, then `controlrig_direct_gate_probe` | `sample_anim_node_pre_post_runtime_pose` |
| `state-machine runtime-driver proof` | read/runtime proof ready; graph authoring remains candidate | `inspect_anim_state_machine_transitions` | `sample_anim_state_machine_runtime_response` |
| `Baddy RigidBody` | read/sample tuning ready; deeper PhysicsAsset inspection remains candidate | `inspect_anim_graph_node_settings` | `sample_anim_node_pre_post_runtime_pose` |
| `node resolver plus same-instance pre/post proof` | read-only instrumentation ready for smoked node classes | `inspect_anim_graph_protected_topology` or compiled mapping | `sample_anim_node_pre_post_runtime_pose` |

## P0: Before Any New Asset Work

| Item | Trigger | Action |
| --- | --- | --- |
| Bridge readiness check | Any StackOBot editor/MCP work | Confirm the StackOBot bridge on `127.0.0.1:55557` and that the active plugin copy is `<workspace-parent>/SampleProject/StackOBot/Plugins/UnrealMCP`. |
| Sample boundary check | Any authoring request | Keep writes under `/Game/_MCP_Sample/AnimStudy` unless original mutation is explicitly approved. |
| Handoff visibility | Any asset/editor mutation | Show the matching `Tivret handoff` block before execution. |
| Dirty package check | Any live editor session | Report dirty content/map packages and do not save original maps just to clean up transient actors. |

## P1: Most Likely Future Work

| Backlog item | Implement when | Preferred first step | C++ status |
| --- | --- | --- | --- |
| Visible upper-body action sample | User asks for a real visible upper-body action while locomotion continues and no compatible action source is already available. | Re-prove existing `UpperBody` Slot/LayeredBlend route, then identify action source requirements. | Candidate `ensure_layered_slot_overlay_sample` only if needed. |
| State-machine sample authoring | User asks to add/change a state, transition, transition rule, sequence player, or BlendSpace player. | Inspect transitions and run runtime-driver proof cases first. | Candidate `ensure_state_machine_sample_variant` only after runtime proof is insufficient. |
| Guarded notify/curve/Montage support | User asks to read, author, or align sequence notifies, sync markers, curves, Montage sections, or branching points. | Use safe animation asset inventory and AssetRegistry-level Montage evidence. | Candidate guarded native API; do not broad-probe with Python. |
| Bot body physics beyond Trail | User asks for Bot body/stalk/tail-like RigidBody behavior where Trail is semantically wrong. | Compare to Baddy RigidBody evidence and classify animation physics vs world physics. | Candidate `ensure_anim_graph_rigidbody_demo_variant`. |

## P2: Reusable Tooling Only If Repeated

| Backlog item | Implement when | Notes |
| --- | --- | --- |
| Physics variant matrix command | Repeated physics tuning requests need comparable baseline/variant SIE metrics. | Use single-node PoseWatch and existing RigidBody/Trail evidence first. |
| PhysicsAsset guarded inspector | A request depends on bodies, constraints, limits, or solver details not visible through current summaries. | Keep read-only first; avoid unsafe arbitrary UV or map operations. |
| PoseWatch target actor resolver | Verification repeatedly fails from actor/component/PIE duplicate matching, not from graph behavior. | Try documented transient setup and component-level Post Process override first. |
| Broader Trail parameter editor | User needs relaxation curves, rotation limits, planar/stretch limits, debug display, or fake velocity cases beyond current command inputs. | Start from `ensure_anim_graph_trail_demo`. |
| Blueprint call-flow deepening | Exact gameplay action playback path is needed and current topology/runtime probes are insufficient. | Use `inspect_blueprint_graph_call_topology` style reads before adding new graph tooling. |

## P3: Maintenance

| Item | Trigger | Action |
| --- | --- | --- |
| StackOBot plugin command-surface sync | Need the StackOBot animation-study MCP commands in Cubeless or another project. | Treat as deliberate port/sync work with build verification; do not narrow-paste one command. |
| Sample asset regeneration | `_MCP_Sample` asset is missing or stale. | Regenerate with the route in `docs/stackobot-sample-asset-manifest.md`. |
| Evidence refresh drill | After significant plugin changes or editor upgrade. | Repeat a read-only drill similar to `docs/stackobot-live-read-drill-2026-06-19.md`. |
| Documentation compaction | The docs become hard to route through. | Keep quickstart and closeout as first pages; move details to route-specific notes. |

## Do Not Spend Time On Yet

- Do not add new C++ without a concrete blocked request.
- Do not mutate original StackOBot assets during study work.
- Do not use generic Python reflection for Montage internals.
- Do not reimplement covered routes such as Post Process ModifyBone, BlendSpace
  variants, current ControlRig forced-driver proof, Bot Trail, or Baddy
  RigidBody narrow tuning.
- Do not sync the entire UnrealMCP plugin into Cubeless just to satisfy a
  StackOBot-only study route.

## Practical Next Step When The User Wakes Up

If the user asks for a specific part, execute this sequence:

1. Open `docs/stackobot-animation-quickstart.md`.
2. Compile the sentence in `docs/stackobot-request-compiler-drills.md`.
3. Check this backlog only if the route is blocked or marked candidate.
4. Start with a sample-only implementation.
5. Escalate to C++ only when the active request hits a documented trigger.
