# StackOBot Animation Quickstart

Use this as the first page for future StackOBot animation requests.

## Start Here

1. Confirm the primary bridge is reachable:
   `127.0.0.1:55557`.
2. Compile the user's natural-language request with
   `docs/stackobot-request-compiler-drills.md`; use
   `python Tools/Unreal/compile_stackobot_animation_request.py --summary --request "<request>"`
   as the first route-token smoke when helpful.
3. Compare the compiled route fields against
   `docs/stackobot-animation-route-matrix.md`.
4. For any request that touches editor state or creates sample evidence, copy
   `docs/stackobot-animation-request-run-template.md`.
5. Show the filled `티브렛에게 전달할 지시` block from
   `docs/stackobot-animation-tivret-handoff-templates.md`.
6. Keep the first pass sample-only under `/Game/_MCP_Sample/AnimStudy`.
7. Run the narrowest read or authoring command from
   `docs/stackobot-animation-mcp-command-syntax.md`.
8. Verify with the route-specific gate from
   `docs/stackobot-animation-authoring-templates.md`.
9. Record results in `docs/work-log.md`.
10. Before committing, run
   `python Tools/Unreal/run_stackobot_animation_local_checks.py --summary`
   and commit only relevant docs/tooling.

## Preflight Checklist

Before Tivret touches editor state or creates sample evidence, confirm these
items and record the result in the request run note:

- Local read-only preflight passes:
  `python Tools/Unreal/run_stackobot_animation_local_checks.py --summary`
- StackOBot project path exists at `<workspace-parent>/SampleProject/StackOBot`.
- Primary UnrealMCP bridge is reachable on `127.0.0.1:55557`.
- Active StackOBot plugin copy is
  `<workspace-parent>/SampleProject/StackOBot/Plugins/UnrealMCP`.
- Required animation-study command is exposed by that plugin copy; if not,
  treat it as a command-surface sync issue before adding C++.
- Pre-existing dirty packages are captured before transient actor setup.
- First authoring target stays under `/Game/_MCP_Sample/AnimStudy` and
  authoring commands keep `allow_non_sample=false`.
- Concrete `_MCP_Sample/AnimStudy` sample targets named in route matrix or
  request-run examples are listed in `docs/stackobot-sample-asset-manifest.md`.
- Evidence output target is
  `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy`.

Use `--require-bridge` when this is the final gate before live editor work.

## Route Shortcuts

| User asks for | Route | First proof |
| --- | --- | --- |
| Head, neck, antenna offset after animation | Post Process ModifyBone | Sample Post Process AnimBP, then Post Process PoseWatch |
| Run speed or lean response | BlendSpace sample variant | Sample BlendSpace, then pose grid |
| Idle/walk/run/jump/hover behavior | state-machine runtime-driver proof | State-machine inspect, then runtime cases |
| Foot IK or interaction reach | ControlRig gate probe | Direct gate probe, then forced-driver sample if needed |
| Upper body while moving | UpperBody Slot and LayeredBlend | Inventory, then all-input LayeredBlend PoseWatch |
| Antenna lag or spring follow | Bot Trail sample | Trail sample, then SIE Post Process PoseWatch |
| Baddy jiggle/stalk/tail physics | Baddy RigidBody | RigidBody settings read, then sample tuning if needed |
| Notify, curve, sync marker, Montage internals | protected metadata boundary | Safe inventory only; guarded native API if concrete request needs internals |
| Which node changed the pose | node resolver plus same-instance pre/post proof | Compiled mapping or PoseWatch pre/post |

## Route Token Quick Map

Use this table after the request compiler chooses an exact route token. It is the
fast path from the first page to the safest first command and the required proof
command.

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

## Do Not Do First

- Do not edit original StackOBot assets for the first pass.
- Do not save dirty original maps to clean up transient proof actors.
- Do not broad-probe Montage internals with generic Python reflection.
- Do not reactivate the disconnected original Bot Trail node directly.
- Do not add C++ unless the current command surface cannot express a concrete
  request.

## Main References

- Doc index: `docs/stackobot-animation-doc-index.md`
- Closeout/readiness: `docs/stackobot-animation-study-closeout.md`
- Next-work backlog: `docs/stackobot-animation-next-work-backlog.md`
- Acceptance checklist: `docs/stackobot-animation-acceptance-checklist.md`
- Request compiler: `docs/stackobot-request-compiler-drills.md`
- Route matrix: `docs/stackobot-animation-route-matrix.md`
- Request run template: `docs/stackobot-animation-request-run-template.md`
- Request run examples: `docs/stackobot-animation-request-run-examples.md`
- Request playbook: `docs/stackobot-animation-request-playbook.md`
- Handoff templates: `docs/stackobot-animation-tivret-handoff-templates.md`
- Authoring templates: `docs/stackobot-animation-authoring-templates.md`
- Physics grammar: `docs/stackobot-physics-request-grammar.md`
- Command syntax: `docs/stackobot-animation-mcp-command-syntax.md`
- C++ API decision matrix: `docs/stackobot-cpp-api-decision-matrix.md`
- Execution map: `docs/stackobot-animation-execution-map.md`
- Sample manifest: `docs/stackobot-sample-asset-manifest.md`
- Latest live read drill: `docs/stackobot-live-read-drill-2026-06-19.md`
