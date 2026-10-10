# StackOBot Animation MCP Command Syntax

Use this as the short syntax sheet for StackOBot animation requests. The full
tool source remains in `../unreal-mcp-cubeless/Python/tools/node_tools.py`
and `../unreal-mcp-cubeless/Docs/Tools/node_tools.md`.

For the shortest operating guide, start with
`docs/stackobot-animation-quickstart.md`.
For request-to-route authoring templates, start with
`docs/stackobot-animation-authoring-templates.md`.
For natural-language request compilation drills, use
`docs/stackobot-request-compiler-drills.md`.

Default safety rules:

- Start under `/Game/_MCP_Sample/AnimStudy` unless original asset mutation was explicitly approved.
- Keep `allow_non_sample=false` for authoring commands.
- Prefer read-only commands before authoring commands.
- Use the StackOBot-local UnrealMCP plugin copy for StackOBot animation-study commands:
  `<workspace-parent>/SampleProject/StackOBot/Plugins/UnrealMCP`.
- Commands here do not open maps. Do not use generic Python map switching as setup.

## Command Quick Map

| Request need | Command | Minimum useful params | Mutates assets |
| --- | --- | --- | --- |
| Static AnimGraph topology | `inspect_anim_graph_protected_topology` | `blueprint_name`, optional `node_type`/`node_id` | No |
| State-machine topology | `inspect_anim_state_machine_transitions` | `blueprint_name`, optional `state_machine_name` | No |
| Runtime AnimInstance state | `inspect_anim_instance_runtime_state` | `actor_label` or actor filter, optional `state_machine_name` | No |
| Runtime property response | `sample_anim_state_machine_runtime_response` | actor filter, `cases[]` with `properties` | No |
| Node pre/post contribution | `sample_anim_node_pre_post_runtime_pose` | `blueprint_name`, node selector, actor filter, `mode` | No for runtime modes; isolated temp uses `_MCP_Temp` |
| BlendSpace runtime pose grid | `sample_blendspace_runtime_pose_grid` | `skeletal_mesh`, `blendspace_path`/`blendspaces`, sample inputs | No |
| BlendSpace sample authoring | `ensure_blendspace_sample_variant` | `source_blendspace`, `variant_name`, `sample_edits`/`axis_edits` | Yes, sample target only |
| Post Process ModifyBone authoring | `ensure_postprocess_anim_demo_variant` | `source_blueprint_name`, `source_skeletal_mesh`, `variant_name`, `bone_name`, `rotation` | Yes, sample target only |
| ControlRig gate probe | `controlrig_direct_gate_probe` | `control_rig_path` or `control_rig_class`, optional `cases` | No |
| ControlRig forced-driver sample | `ensure_controlrig_forced_driver_animbp` | sample `blueprint_name`, optional `curve_values`, `input_defaults` | Yes, sample target only |
| Trail sample authoring | `ensure_anim_graph_trail_demo` | sample `blueprint_name`, `trail_bone`, `base_joint` | Yes, sample target only |
| RigidBody settings read | `inspect_anim_graph_node_settings` | `blueprint_name`, `node_type=RigidBody` | No |
| RigidBody sample tuning | `set_anim_graph_rigidbody_settings` | sample `blueprint_name`, optional `alpha`, `external_force`, `simulation_space` | Yes, sample target only |

## Route Token Command Map

Use this after the request compiler chooses a route token. The first command
column names the safest first read or sample authoring action; the verification
column names the proof command or explicit no-command boundary.

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

## Common StackOBot Asset Paths

| Meaning | Path |
| --- | --- |
| Bot main AnimBP | `/Game/StackOBot/Characters/Bot/ABP_Bot.ABP_Bot` |
| Bot source SkeletalMesh | `/Game/StackOBot/Characters/Bot/Mesh/SKM_Bot.SKM_Bot` |
| Bot ControlRig | `/Game/StackOBot/Characters/Bot/Rig/CR_Bot_Correction.CR_Bot_Correction` |
| Bot walk/run/lean BlendSpace | `/Game/StackOBot/Characters/Bot/Animations/BS_Bot_WalkRunLean.BS_Bot_WalkRunLean` |
| Baddy AnimBP | `/Game/StackOBot/Characters/Blobling/Anim/ABP_Baddy.ABP_Baddy` |
| Baddy SkeletalMesh | `/Game/StackOBot/Characters/Blobling/SKM_Baddy.SKM_Baddy` |
| Sample root | `/Game/_MCP_Sample/AnimStudy` |
| Evidence root | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy` |

## Authoring Syntax

### Post Process ModifyBone sample

```json
{
  "command": "ensure_postprocess_anim_demo_variant",
  "params": {
    "source_blueprint_name": "/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study.ABP_Bot_PostProcess_Study",
    "source_skeletal_mesh": "/Game/StackOBot/Characters/Bot/Mesh/SKM_Bot.SKM_Bot",
    "variant_name": "HeadYawExample",
    "target_blueprint_name": "/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadYawExample",
    "target_skeletal_mesh": "/Game/_MCP_Sample/AnimStudy/SKM_Bot_PostProcess_Study_HeadYawExample",
    "bone_name": "head",
    "rotation": [0, 8, 0],
    "replace_existing": true,
    "overwrite_existing": false,
    "compile": true,
    "save": true,
    "allow_non_sample": false
  }
}
```

Verify a static Post Process ModifyBone sample with editor-world PoseWatch when SIE is not needed:

```json
{
  "command": "sample_anim_node_pre_post_runtime_pose",
  "params": {
    "blueprint_name": "/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadYawExample.ABP_Bot_PostProcess_Study_HeadYawExample",
    "graph_name": "AnimGraph",
    "graph_type": "function",
    "node_type": "AnimGraphNode_ModifyBone",
    "actor_label": "MCP_PostProcess_HeadYawExample",
    "mode": "pose_watch_capture",
    "anim_instance_source": "post_process",
    "dry_run": false,
    "prefer_pie_world": false,
    "require_pie_world": false,
    "sample_bones": ["neck_01", "head", "antenna_04_l", "antenna_04_r"]
  }
}
```

### BlendSpace sample variant

```json
{
  "command": "ensure_blendspace_sample_variant",
  "params": {
    "source_blendspace": "/Game/StackOBot/Characters/Bot/Animations/BS_Bot_WalkRunLean.BS_Bot_WalkRunLean",
    "variant_name": "LeanWideExample",
    "sample_edits": [
      {"animation_name": "A_Bot_Run_LeanLeft", "x": 1.25},
      {"animation_name": "A_Bot_Run_LeanRight", "x": -1.25}
    ],
    "save": true,
    "allow_non_sample": false
  }
}
```

Runtime grid verification:

```json
{
  "command": "sample_blendspace_runtime_pose_grid",
  "params": {
    "skeletal_mesh": "/Game/StackOBot/Characters/Bot/Mesh/SKM_Bot.SKM_Bot",
    "blendspaces": [
      {
        "path": "/Game/_MCP_Sample/AnimStudy/BS_Bot_WalkRunLean_LeanWideExample.BS_Bot_WalkRunLean_LeanWideExample",
        "samples": [
          {"label": "run_center", "x": 0.0, "y": 500.0, "z": 0.0},
          {"label": "run_lean_left", "x": 1.25, "y": 258.546, "z": 0.0},
          {"label": "run_lean_right", "x": -1.25, "y": 259.420, "z": 0.0}
        ]
      }
    ],
    "sample_bones": ["pelvis", "foot_l", "foot_r", "head"],
    "prefer_pie_world": true,
    "require_pie_world": false,
    "cleanup": true
  }
}
```

### ControlRig forced-driver sample

```json
{
  "command": "ensure_controlrig_forced_driver_animbp",
  "params": {
    "blueprint_name": "/Game/_MCP_Sample/AnimStudy/ABP_Bot_ControlRig_ForcedDriver_Study.ABP_Bot_ControlRig_ForcedDriver_Study",
    "graph_name": "AnimGraph",
    "graph_type": "function",
    "control_rig_class": "CR_Bot_Correction",
    "curve_values": {
      "IK_blend_interact": 1.0,
      "IKBlend_l": 1.0
    },
    "input_defaults": {
      "ShouldDoIKTrace": true,
      "InteractionWorldLocation": [80, -40, 80]
    },
    "replace_existing": true,
    "disconnect_existing_links": true,
    "allow_non_sample": false
  }
}
```

For read-only gate exploration before authoring:

```json
{
  "command": "controlrig_direct_gate_probe",
  "params": {
    "control_rig_path": "/Game/StackOBot/Characters/Bot/Rig/CR_Bot_Correction.CR_Bot_Correction",
    "sample_elements": ["foot_l", "foot_r", "Control:IK_foot_L", "Control:IK_foot_R"],
    "cases": [
      {"name": "baseline", "should_trace": false, "loc": [0, 0, 0], "curves": {"IKBlend_l": 0, "IK_blend_interact": 0}},
      {"name": "interact_side", "should_trace": true, "loc": [80, -40, 80], "curves": {"IKBlend_l": 1, "IK_blend_interact": 1}}
    ]
  }
}
```

### State-machine runtime response

```json
{
  "command": "inspect_anim_state_machine_transitions",
  "params": {
    "blueprint_name": "/Game/StackOBot/Characters/Bot/ABP_Bot.ABP_Bot",
    "state_machine_name": "AirLocomotion",
    "include_pins": true,
    "include_rule_graph_nodes": true,
    "max_rule_graph_nodes": 64
  }
}
```

```json
{
  "command": "sample_anim_state_machine_runtime_response",
  "params": {
    "actor_label": "MCP_AnimState_Smoke",
    "state_machine_name": "GroundLocomotion",
    "cases": [
      {"name": "speed_0", "properties": {"GroundSpeed": 0.0}, "tick_count": 2},
      {"name": "speed_500", "properties": {"GroundSpeed": 500.0}, "tick_count": 4}
    ],
    "restore_after_case": true,
    "prefer_pie_world": true,
    "require_pie_world": false
  }
}
```

### Trail or secondary motion sample

```json
{
  "command": "ensure_anim_graph_trail_demo",
  "params": {
    "blueprint_name": "/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study.ABP_Bot_Trail_Study",
    "graph_name": "AnimGraph",
    "graph_type": "function",
    "trail_bone": "antenna_04_l",
    "base_joint": "head",
    "chain_length": 2,
    "chain_bone_axis": "X",
    "fake_velocity": [0, 0, 0],
    "replace_existing": true,
    "allow_non_sample": false
  }
}
```

For node contribution proof, use `sample_anim_node_pre_post_runtime_pose` in
`compiled_graph_mapping` mode first when the node selector is uncertain, then
`pose_watch_capture` after the runtime node is mapped.

### RigidBody settings and sample tuning

Read the active Baddy RigidBody node before changing any sample:

```json
{
  "command": "inspect_anim_graph_node_settings",
  "params": {
    "blueprint_name": "/Game/StackOBot/Characters/Blobling/Anim/ABP_Baddy.ABP_Baddy",
    "node_type": "RigidBody",
    "include_pins": true,
    "max_depth": 3
  }
}
```

Tune only a `_MCP_Sample` RigidBody AnimBP:

```json
{
  "command": "set_anim_graph_rigidbody_settings",
  "params": {
    "blueprint_name": "/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study_ForceZ.ABP_Baddy_RigidBody_Study_ForceZ",
    "alpha": "1.0",
    "external_force": "[0, 0, 350]",
    "simulation_space": "ComponentSpace",
    "enable_world_geometry": "false",
    "allow_non_sample": false
  }
}
```

Use `docs/stackobot-physics-request-grammar.md` to decide whether a request is
Trail, RigidBody, source-vs-output proof, or world physics.

## Result Checklist

Every animation command result should be summarized with:

- `success`, `errors`, and `warnings`.
- Whether `original_assets_modified=false` or `asset_modified=false`.
- Target sample asset paths for authoring commands.
- Compile/save result for sample assets.
- `sampled_world_type` and `is_play_session_active` for runtime probes.
- Key pose deltas or state-machine response.
- Dirty package status after transient actor work.
- Cleanup status for transient actors, play sessions, and temporary probe assets.
- Evidence artifact paths under `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy`.
- C++/API decision: `not needed`, `candidate`, or `implemented`.
- Residual risk that affects the next request.
