# StackOBot Animation Authoring Templates

Use this as the first routing sheet when a future request asks to make a
specific StackOBot animation behavior without providing a sample. It condenses
the current learned grammar into authoring templates, verification gates, and
C++/API escalation rules.

Related docs:

- `docs/stackobot-animation-request-playbook.md`
- `docs/stackobot-request-compiler-drills.md`
- `docs/stackobot-animation-tivret-handoff-templates.md`
- `docs/stackobot-animation-acceptance-checklist.md`
- `docs/stackobot-physics-request-grammar.md`
- `docs/stackobot-animbp-authoring-patterns.md`
- `docs/stackobot-animation-mcp-command-syntax.md`
- `docs/stackobot-animation-execution-map.md`

Default rule: start sample-only under `/Game/_MCP_Sample/AnimStudy` unless the
user explicitly approves original StackOBot asset mutation.

When asset/editor work is needed, show the matching handoff block from
`docs/stackobot-animation-tivret-handoff-templates.md` before Tivret executes.

## Routing Table

| User intent | Route | First authoring/proof command | Verification gate | C++/API status |
| --- | --- | --- | --- | --- |
| Head/neck/antenna static offset after animation | Post Process ModifyBone | `ensure_postprocess_anim_demo_variant` | Post Process PoseWatch, usually no SIE | Covered |
| Run/lean/speed response change | BlendSpace sample variant | `ensure_blendspace_sample_variant` | `sample_blendspace_runtime_pose_grid` | Covered |
| Idle/walk/run/jump/landing/hover response to variables | State machine/runtime driver | `inspect_anim_state_machine_transitions`, then `sample_anim_state_machine_runtime_response` | Per-case state and transition metrics | Inspect/probe covered; new state authoring remains candidate |
| Foot placement or interaction IK | ControlRig late correction | `controlrig_direct_gate_probe`, then `ensure_controlrig_forced_driver_animbp` when gates are inactive | Direct ControlRig probe plus same-instance ControlRig PoseWatch | Covered |
| Upper-body action over locomotion | Existing `UpperBody` Slot and LayeredBlend route | `StackOBot_SlotLayeredBlend_Inventory.md`, then all-input PoseWatch | `BasePose` and `BlendPoses[0]` same-instance capture | Existing route covered; new overlay authoring remains candidate |
| Antenna lag, spring, or secondary motion | Trail/RigidBody style Post Process sample | `ensure_anim_graph_trail_demo` or existing Baddy RigidBody evidence; see `docs/stackobot-physics-request-grammar.md` | SIE Post Process PoseWatch or isolated source-vs-output | Covered for Bot Trail and Baddy RigidBody; broader physics authoring remains candidate |
| Notify, sync marker, curve, or Montage internals | Protected animation-source metadata | Existing asset inventory/read probe only | Do not broad-probe Montage internals with Python | Native guarded API only for concrete request |

## Route Token Template Map

| Route token | Template card | First read or authoring command | Verification command |
| --- | --- | --- | --- |
| `Post Process ModifyBone` | `Post Process ModifyBone` | `ensure_postprocess_anim_demo_variant` | `sample_anim_node_pre_post_runtime_pose` |
| `BlendSpace sample variant` | `BlendSpace Sample Variant` | `ensure_blendspace_sample_variant` | `sample_blendspace_runtime_pose_grid` |
| `Bot Trail sample` | `Secondary Motion Or Physics` | `ensure_anim_graph_trail_demo` | `sample_anim_node_pre_post_runtime_pose` |
| `UpperBody Slot and LayeredBlend` | `UpperBody Slot And LayeredBlend` | slot/cached-pose inventory | `sample_anim_node_pre_post_runtime_pose` |
| `protected metadata boundary` | `Notify, Curve, Sync Marker, And Montage Internals` | safe animation asset inventory | none for protected internals |
| `ControlRig gate probe` | `ControlRig Late Correction` | `inspect_anim_graph_protected_topology`, then `controlrig_direct_gate_probe` | `sample_anim_node_pre_post_runtime_pose` |
| `state-machine runtime-driver proof` | `State Machine Or Runtime Driver` | `inspect_anim_state_machine_transitions` | `sample_anim_state_machine_runtime_response` |
| `Baddy RigidBody` | `Secondary Motion Or Physics` | `inspect_anim_graph_node_settings` | `sample_anim_node_pre_post_runtime_pose` |
| `node resolver plus same-instance pre/post proof` | `Node Contribution Proof` | `inspect_anim_graph_protected_topology` or compiled mapping | `sample_anim_node_pre_post_runtime_pose` |

## Template Cards

### Post Process ModifyBone

Use when the request is a late cosmetic bone adjustment: look direction, head
tilt, antenna pose, small additive correction.

Authoring grammar:

```text
source main pose -> SkeletalMesh Post Process AnimBP
LinkedInputPose -> Transform(Modify)Bone(target bone) -> Root
```

Default proof:

1. Create/reuse sample Post Process AnimBP and duplicated SkeletalMesh with
   `ensure_postprocess_anim_demo_variant`.
2. Spawn a transient actor using the sample mesh and main `ABP_Bot_C`.
3. Run `sample_anim_node_pre_post_runtime_pose` with
   `anim_instance_source=post_process`.
4. Verify `runtime_graph_prepost=true`, `same_instance_prepost=true`, target
   bone delta, and no dirty content packages.

Do not use C++ for this route unless the target node cannot be built or verified
with the existing command.

### BlendSpace Sample Variant

Use when the request changes continuous locomotion response, such as wider lean,
different walk/run input placement, or stronger run/jump blend response.

Authoring grammar:

```text
source BlendSpace -> sample duplicate -> axis/sample coordinate edits -> pose grid
```

Default proof:

1. Duplicate/reuse the source BlendSpace under `_MCP_Sample` with
   `ensure_blendspace_sample_variant`.
2. Keep original BlendSpaces read-only unless mutation is explicitly approved.
3. Run `sample_blendspace_runtime_pose_grid` against representative inputs.
4. Require `valid_pose_count` to match the requested grid and
   `input_changed_pose=true` when a visual response is expected.

### State Machine Or Runtime Driver

Use when the request changes state selection, transition response, or variable
driven behavior rather than a single final-pose node.

Authoring/proof grammar:

```text
AnimInstance variable cases -> state-machine runtime sample -> transition metrics
```

Default proof:

1. Read topology with `inspect_anim_state_machine_transitions`.
2. Spawn a transient `SKM_Bot + ABP_Bot_C` actor.
3. Run `sample_anim_state_machine_runtime_response` with explicit cases, such as
   `GroundSpeed=0`, `GroundSpeed=500`, and return-to-idle.
4. Verify current state, state weights, transition progress when present, and
   restored runtime properties.

Only add a state-machine authoring API when a concrete request needs a new state,
sequence player, BlendSpace player, or transition rule.

### ControlRig Late Correction

Use when the request is foot contact, interaction reach, IK correction, or a late
pose cleanup that depends on gameplay inputs.

Authoring/proof grammar:

```text
main AnimBP pose -> ControlRig node -> root
forced driver sample: upstream pose -> ModifyCurve -> ControlRig
```

Default proof:

1. Confirm the ControlRig node is connected with
   `inspect_anim_graph_protected_topology`.
2. Use `controlrig_direct_gate_probe` to identify active gates and useful driver
   values.
3. If gameplay gates are inactive in a transient actor, use
   `ensure_controlrig_forced_driver_animbp` under `_MCP_Sample`.
4. Start SIE, poll `inspect_anim_instance_runtime_state(require_pie_world=true)`
   until the PIE AnimInstance exists, then run ControlRig PoseWatch.

Do not treat a direct transient ControlRig probe as compiled AnimGraph proof; it
is gate discovery. Same-instance PoseWatch is the compiled graph proof.

### UpperBody Slot And LayeredBlend

Use when the request is an upper-body action while locomotion continues.

Existing StackOBot grammar:

```text
LocomotionPose -> UpperBody Slot -> CashedPose_UpperBody
LocomotionPose + CashedPose_UpperBody -> LayeredBoneBlend -> FullBodyPose
```

Default proof:

1. Reuse the existing `UpperBody` route first.
2. Inspect `StackOBot_SlotLayeredBlend_Inventory.md`.
3. Run `sample_anim_node_pre_post_runtime_pose` with `input_pose_mode=all`.
4. Verify `BasePose` resolves to `LocomotionPose` and `BlendPoses[0]` resolves
   to `CashedPose_UpperBody` in the same runtime AnimInstance.

If no Montage/action source is played, near-zero deltas are normal and only prove
the route. Add `ensure_layered_slot_overlay_sample` only when a concrete request
needs a new overlay branch or a sample action source.

### Secondary Motion Or Physics

Use when the request is antenna lag, soft follow, spring response, or physical
secondary motion.

Default routes:

- Bot antenna Trail: `_MCP_Sample` Post Process Trail sample.
- Baddy body/stalk physics: existing `ABP_Baddy` RigidBody evidence.

Default proof:

1. Prefer `ensure_anim_graph_trail_demo` for Bot antenna-chain samples.
2. For Trail proof actors, explicitly set component-level Post Process override
   to the sample Post Process AnimBP class.
3. Prefer SIE/PIE, then PoseWatch with `anim_instance_source=post_process`.
4. For source-vs-output comparisons, use isolated temp mode under `_MCP_Temp`.

Do not reactivate the disconnected original Bot Trail node directly. It is a
reference node, not the safe authoring target.

### Node Contribution Proof

Use when the request asks which AnimGraph node changed the pose, or when a
route proof needs exact input/output attribution for a suspected node.

Proof grammar:

```text
static topology or compiled mapping -> target node selection -> same-instance pre/post PoseWatch
```

Default proof:

1. Start read-only with `inspect_anim_graph_protected_topology` or compiled
   mapping for the suspected node.
2. Run `sample_anim_node_pre_post_runtime_pose` only after the target node and
   runtime AnimInstance are selected clearly.
3. Report input/output links, sampled bone deltas, and same-instance
   confirmation.
4. If node identity or actor resolution is ambiguous after repeated attempts,
   stop and park a resolver/API candidate instead of editing assets.

Do not start this route by authoring or mutating assets. It is instrumentation
unless a separate route has already justified a sample actor setup.

### Notify, Curve, Sync Marker, And Montage Internals

Use this route only when the user request specifically depends on animation
source metadata.

Current safe reads:

- Sequence timing, skeleton, root-motion flags, and sampled source poses.
- BlendSpace axes, samples, notify trigger mode, and target interpolation speed.
- AssetRegistry/name-level Montage presence.

Unsafe or blocked reads:

- `AnimSequence.Notifies` is protected through Python.
- `raw_curve_data`, `authored_sync_markers`, and `get_data_model` are unavailable
  through the current StackOBot Python path.
- Broad Python reflection against `AnimMontage` internals asserted in
  `AnimMontage.h:770`.

Escalate to a guarded native MCP command only after a concrete request requires
these fields. Do not explore Montage internals through generic Python reflection.

## Completion Contract

Every authored or probed request should pass
`docs/stackobot-animation-acceptance-checklist.md` and finish with:

1. Classification and chosen route.
2. Exact sample asset paths or explicit statement that the pass is read-only.
3. Verification artifact paths under
   `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy`.
4. Runtime result summary: world type, node/input link ids when relevant, key
   deltas or state changes, errors, warnings, and dirty-package status.
5. Cleanup statement: transient actors removed, SIE ended, editor closed without
   saving when map dirtiness came only from reversible proof actors.
6. C++/API decision: not needed, deferred candidate, or explicitly approved.

## C++/API Escalation Gate

Do not add C++ just because a request is complex. Add or modify UnrealMCP C++
only when the current safe command surface cannot author or verify the behavior.

Valid escalation cases:

- New state/transition authoring cannot be represented with runtime driver
  probes.
- A new layered-slot overlay branch must be generated rather than using the
  existing `UpperBody` route.
- Trail/RigidBody parameter authoring exceeds the current Bot Trail and Baddy
  RigidBody samples.
- Repeated target setup failures show that actor/component resolution needs a
  native helper.
- Notify, sync-marker, curve, or Montage internals are required by a concrete
  request and are protected or unsafe through Python.

Until one of those cases is true, keep C++ as a parked candidate and proceed with
sample assets, runtime probes, and PoseWatch evidence.
