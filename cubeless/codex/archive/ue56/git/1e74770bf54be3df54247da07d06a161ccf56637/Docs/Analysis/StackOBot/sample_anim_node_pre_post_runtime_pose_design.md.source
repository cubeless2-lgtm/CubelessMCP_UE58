# sample_anim_node_pre_post_runtime_pose Design

## Goal

Provide a conservative UnrealMCP route for StackOBot animation study to compare a selected AnimGraph node's source pose against its output pose without mutating original project assets.

Primary study targets:

- `AnimGraphNode_RigidBody` in `ABP_Baddy`
- `AnimGraphNode_Trail` in safe `_MCP_Sample` Bot Trail variants
- later, `AnimGraphNode_ControlRig` source-vs-output attribution inside a compiled AnimGraph

This command is intended to close the remaining "exact pre/post attribution" gap in the StackOBot study. It is not meant to replace existing broad runtime probes such as `sample_skeletal_bones_in_sie`, `inspect_anim_instance_runtime_state`, or `sample_controlrig_pre_post_runtime_pose`.

## Important Distinction

There are two different levels of evidence:

1. **Isolated source-vs-output sampler, MVP**
   - Build or use temporary `_MCP_Temp` probe assets/components.
   - Run a source branch and a node-under-test branch with the same skeletal mesh, driver setup, tick count, and sample bones.
   - Report `same_instance_prepost=false` and `runtime_graph_prepost=false` because this is not tapping a compiled node inside one live AnimInstance.

2. **Compiled graph node instrumentation, later**
   - Tap pre-node and post-node poses inside the compiled AnimGraph node stack for one live component/frame.
   - Report `same_instance_prepost=true` and `runtime_graph_prepost=true`.
   - This may require custom proxy/instrumentation work and should not be assumed safe for the first implementation.

The MVP should be explicit about this limitation instead of pretending to provide same-frame internal node data.

## Proposed Command

`sample_anim_node_pre_post_runtime_pose`

### Initial Parameters

Target selection:

- `anim_blueprint` or `blueprint_name`: AnimBP or Post Process AnimBP path/name.
- `skeletal_mesh`: SkeletalMesh path used by the transient probe component.
- `node_id`: preferred exact editor node GUID.
- `node_type`: fallback class filter, for example `AnimGraphNode_RigidBody`, `AnimGraphNode_Trail`, or `AnimGraphNode_ControlRig`.
- `title_contains`: fallback node title substring.
- `graph_name`, `graph_id`, `graph_type`: same graph resolver pattern as existing node tools.

Runtime setup:

- `sample_bones`: bone names to compare. Default should include compact StackOBot/Baddy study bones, not the whole skeleton.
- `sample_sockets`: optional socket names.
- `tick_count`: bounded forced animation tick count, default `1`, clamp `0..240`.
- `tick_delta_time`: default `1/30`, clamp `0..1`.
- `settle_tick_count`: optional pre-sample settle ticks, default `0`, clamp `0..240`.
- `require_pie_world`: fail instead of using editor-world fallback for live-component modes.
- `driver_properties`: reflected AnimInstance property values, same JSON writer as runtime property probes.
- `curve_values`: optional curve values for curve-gated graphs.

Safety and artifact control:

- `mode`: `isolated_temp_components` for MVP; reserve `compiled_graph_instrumentation` for later.
- `dry_run`: default `false`; when true, only resolve target node and report feasibility.
- `cleanup`: default `true`; remove transient actors/components and disposable packages when possible.
- `allow_original_asset_modification`: default `false`; MVP should refuse true original mutation unless a later explicit workflow needs it.
- `temp_root`: default `/Game/_MCP_Temp/AnimNodePrePost`.

### Initial Response Fields

Top-level:

- `success`
- `read_only`
- `asset_modified`
- `runtime_only`
- `mode`
- `runtime_graph_prepost`
- `same_instance_prepost`
- `original_assets_modified`
- `temp_assets_created`
- `cleanup`
- `target_node`
- `sampled_world_type`
- `tick`
- `settle_tick`
- `errors`
- `warnings`

Per case or per sample:

- `source_pose`: bone/socket transforms before the selected node effect, or equivalent source branch output.
- `post_pose`: bone/socket transforms after the selected node effect.
- `deltas`: per-bone location distance and rotation angle delta.
- `driver_echo`: applied AnimInstance properties and curves.
- `artifact_paths`: optional JSON/CSV/Markdown paths written by orchestration scripts, not required from the C++ command itself.

## MVP Implementation Shape

### Phase 1: Read-only target resolver

Add a resolver that:

- loads the AnimBP as `UAnimBlueprint`;
- resolves the requested AnimGraph;
- finds exactly one `UAnimGraphNode_Base` by `node_id`, `node_type`, or title filter;
- reports node class, GUID, title, graph metadata, input pose pins, output pose pins, and whether the node type is supported by the MVP.

This phase should be read-only and useful by itself. If resolution is ambiguous, fail with candidate node summaries.

### Phase 2: Isolated temporary component sampler

For the first useful StackOBot pass:

- support RigidBody and Trail-style nodes only when the command can build a controlled temporary comparison from duplicated/sample assets;
- use disposable packages under `/Game/_MCP_Temp/AnimNodePrePost`;
- never modify `/Game/StackOBot/...` original assets;
- avoid Python map loading APIs;
- use existing safe map/open commands or run in an already active editor world;
- sample compact bone lists through the same transform JSON helpers used by `sample_skeletal_bones_in_sie`.

Expected flags:

- `runtime_graph_prepost=false`
- `same_instance_prepost=false`
- `read_only=false` only if temporary assets are created; `original_assets_modified=false`

### Phase 3: Live component integration

After the isolated sampler works, add a live-component mode that:

- reuses `FindAnimInstanceRuntimeTarget`;
- applies driver properties with the same runtime-only writer used by `set_anim_instance_runtime_property_for_probe`;
- ticks with the same bounded `TickSkeletalComponentForAnimRuntimeProbe`;
- samples final component pose.

This mode still does not prove internal node pre/post by itself. It is useful as a final-pose comparison against the isolated sampler.

### Phase 4: True compiled graph instrumentation

Only implement after the MVP proves useful.

Open questions:

- whether a safe UE 5.7 public API can tap intermediate `FPoseContext` data for arbitrary nodes without engine changes;
- whether a debug `UAnimInstance` subclass/proxy is acceptable for temporary sample assets;
- how to avoid stale raw node pointers during Live Coding, PIE teardown, and asset reload.

Until these are answered, exact same-instance node pre/post should remain a labeled future mode.

## Safety Rules

- Do not mutate original StackOBot assets.
- Do not use generic Python map loading or creation.
- Keep generated assets under `/Game/_MCP_Temp/AnimNodePrePost`.
- Return dirty package names before and after the run.
- Fail if target node resolution is ambiguous.
- Fail if sample bones are missing unless `allow_missing_bones=true` is explicitly provided.
- Clamp all tick counts and delta times.
- Do not keep raw UObject pointers across deferred callbacks.
- Report when the result is an isolated comparison rather than true compiled graph instrumentation.

## Verification Plan

For implementation:

1. `python -m py_compile Python/tools/node_tools.py Python/unreal_mcp_server.py`
2. `MCPGameProjectEditor Win64 Development -NoHotReload`
3. `StackOBotEditor Win64 Development -NoHotReload` after syncing plugin C++ into StackOBot
4. Dry-run target resolver on:
   - `ABP_Baddy` RigidBody node
   - safe Bot Trail study node
5. Runtime smoke with disposable `_MCP_Temp` output:
   - confirm original StackOBot asset dirty count stays unchanged;
   - confirm `runtime_graph_prepost=false` for MVP;
   - confirm deltas are nonzero for a known RigidBody/Trail sample.

## First Implementation Recommendation

Start with the read-only target resolver plus a dry-run response. Do not attempt full node instrumentation in the same commit.

The next implementation commit should add:

- C++ command registration and handler stub;
- Python wrapper;
- docs entry under `Docs/Tools/node_tools.md`;
- dry-run node resolver returning target node and feasibility data.

Only after that passes review should the temporary runtime sampler be added.

## Implementation Status

### 2026-06-18

- Phase 1 dry-run target resolver is implemented and live-smoked on StackOBot `ABP_Baddy` RigidBody.
- A conservative runtime mode, `active_component_tick_delta`, is implemented for `dry_run=false`.
- `active_component_tick_delta` samples a matched live `SkeletalMeshComponent` before and after forced animation ticks and reports `pre_tick_pose`, `post_tick_pose`, and transform `deltas`.
- This runtime mode is useful evidence for final component pose response, but it is not the isolated source-vs-output sampler and not true compiled AnimGraph node instrumentation.
- Runtime responses therefore keep `runtime_graph_prepost=false` and `same_instance_prepost=false`.
- The isolated temporary component/asset sampler under `/Game/_MCP_Temp/AnimNodePrePost` is implemented for RigidBody/Trail-style single input/output pose nodes.
- `mode=isolated_temp_components` duplicates the AnimBP into source-bypass and post-node temp copies, rewires the source copy around the selected node, compiles both temp AnimBPs, samples separate transient components, and cleans up temp actors/assets by default.
- StackOBot `ABP_Baddy` RigidBody live smoke passed with nonzero stalk deltas, `runtime_graph_prepost=false`, `same_instance_prepost=false`, `original_assets_modified=false`, and no remaining dirty content/map packages after cleanup.
- StackOBot Bot Trail live smoke passed against a disposable `_MCP_Temp` duplicate of `ABP_Bot_Trail_Study` with `FakeVelocity=(0,0,80)`. The strongest isolated source-bypass vs post-node delta was `antenna_04_l`, about `21.948 cm` translation and `34.072 deg` rotation.
- The static no-FakeVelocity Trail case produced only floating-point noise, about `0.000005 cm`, confirming that controlled motion or fake velocity is required for a meaningful Trail isolated sample.
- Trail cleanup verification reported `0` dirty content packages, `0` dirty map packages, and `0` remaining assets under `/Game/_MCP_Temp/AnimNodePrePost`.
- A read-only compiled graph mapping preflight, `mode=compiled_graph_mapping`, is implemented to map an editor AnimGraph node GUID to the compiled/live `FAnimNode_*` property on the matched `AnimInstance`.
- `compiled_graph_mapping` reports `same_anim_instance_node_mapping=true` when the live node struct address is resolved and cross-checks the address against `UAnimGraphNode_Base::FindDebugAnimNode` when possible.
- `compiled_graph_mapping` also reports `runtime_pose_links` by reflecting runtime `FPoseLink` / `FComponentSpacePoseLink` fields from the live node struct. This captures `LinkID`, `SourceLinkID`, linked compiled property, linked visual node, and linked-node pointer parity.
- This preflight proves the editor-node-to-runtime-node address path needed for future instrumentation, but it still reports `runtime_graph_prepost=false` and `same_instance_prepost=false` because it does not sample true compiled graph input/output pose data.
- A same-instance runtime PoseWatch strategy has been selected for the next implementation step. The safe route is to temporarily set the AnimBP debug object, register transient `FAnimBlueprintDebugData` PoseWatch entries for the selected node's compiled output link ID and the first runtime input pose link ID, force a bounded tick, read `FAnimNodePoseWatch` bone transforms, then restore the debug object and remove only the transient watches.
- `mode=pose_watch_capture` implements that route. It does not append to `UAnimBlueprint::PoseWatches`, does not save assets, and reports `transient_pose_watches=true`, `debug_object_restored=true`, `pre_pose`, `post_pose`, and `deltas`.
- Live StackOBot smoke passed on `ABP_Baddy` RigidBody. It confirmed `runtime_graph_prepost=true`, `same_instance_prepost=true`, `transient_pose_watches=true`, `debug_object_restored=true`, `original_assets_modified=false`, output link `1`, input `ComponentPose` link `11`, and sampled stalk deltas from the same `ABP_Baddy_C` runtime instance.
- Runtime target selection now accepts `anim_instance_source=main` (default) or `anim_instance_source=post_process`. The Post Process path uses `USkeletalMeshComponent::GetPostProcessInstance()` and reports `requested_anim_instance_source` plus `anim_instance_source`.
- Live StackOBot smoke passed on `ABP_Bot_Trail_Study` as a Post Process AnimBP. It confirmed `runtime_graph_prepost=true`, `same_instance_prepost=true`, `transient_pose_watches=true`, `debug_object_restored=true`, output link `4`, input link `1`, and sampled `antenna_04_l` at about `0.110 cm` / `28.035 deg` from the same `ABP_Bot_Trail_Study_C` runtime instance.
- Live StackOBot smoke passed on `_MCP_Sample` `ABP_Bot_ControlRig_ForcedDriver_Study` as a main AnimBP. It confirmed `runtime_graph_prepost=true`, `same_instance_prepost=true`, `same_anim_instance_node_mapping=true`, `runtime_node_instance_mapped=true`, `find_debug_anim_node_mapped=true`, `transient_pose_watches=true`, `debug_object_restored=true`, ControlRig output link `42`, input `Source` link `45`, and sampled strong same-instance deltas from the same `ABP_Bot_ControlRig_ForcedDriver_Study_C` runtime instance.
- The ControlRig PoseWatch smoke artifacts are `D:/Git/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPoseWatchPrePost_raw.json` and `D:/Git/SampleProject/StackOBot/Saved/MCP/AnimStudy/StackOBot_ControlRigPoseWatchPrePost_Summary.json`.
- The current `pose_watch_capture` coverage now includes RigidBody, Trail, Post Process Modify Bone, LayeredBoneBlend multi-input capture, and ControlRig forced-driver capture. Future lower-level taps should be reserved for node classes that do not expose usable runtime pose links through this route.
