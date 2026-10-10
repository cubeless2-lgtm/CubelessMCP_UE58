# StackOBot Physics Request Grammar

Use this note when a user asks for animation-side physics behavior without a
sample clip: antenna lag, spring follow, body jiggle, stalk motion, soft tails,
or "make it feel physical" requests.

Related docs:

- `docs/stackobot-animation-authoring-templates.md`
- `docs/stackobot-animation-request-playbook.md`
- `docs/stackobot-animation-mcp-command-syntax.md`
- `docs/stackobot-animation-execution-map.md`

Default rule: original StackOBot assets stay read-only. First implementation or
proof work should use `/Game/_MCP_Sample/AnimStudy` or runtime-only actors.

## Route Matrix

| User intent | Preferred route | Current proof | First command | C++/API status |
| --- | --- | --- | --- | --- |
| Antenna lag, drag, follow-through, springy tip | Bot Trail sample in Post Process AnimBP | `ABP_Bot_Trail_Study` PoseWatch pre/post | `ensure_anim_graph_trail_demo`, then `sample_anim_node_pre_post_runtime_pose` | Covered |
| Body, stalk, tail, or soft creature jiggle | Baddy RigidBody study variants | Baddy SIE metrics and source-vs-runtime synthesis | `inspect_anim_graph_node_settings`, optional `set_anim_graph_rigidbody_settings` on sample assets | Covered for existing sample variants |
| Physics strength tuning | Existing sample variant edit | AlphaHalf, ForceZ, WorldSpace variants | `set_anim_graph_rigidbody_settings` with `allow_non_sample=false` | Covered narrowly |
| Exact source-vs-post physics subtraction | Runtime node contribution probe | RigidBody and Trail PoseWatch smokes | `sample_anim_node_pre_post_runtime_pose` | Covered when node maps cleanly; keep as candidate if runtime setup fails |
| New Bot body physics chain using RigidBody | New sample graph authoring | Not implemented as a one-command route | None yet | Candidate: `ensure_anim_graph_rigidbody_demo_variant` |
| PhysicsAsset constraint design or constraint internals | PhysicsAsset analysis, not AnimBP only | Constraint counts and solver settings only | Read-only evidence first | Candidate: guarded constraint inspection API |
| World physics, constraints, destructibles, platforms | Blueprint/world physics route | Existing world-side examples only | Not an AnimBP route | Separate request family |

## Physics Route Token Map

Use this table after request compilation when the route is animation-side
physics. It keeps the physics grammar aligned with the route matrix, command
syntax, sample manifest, and acceptance checklist.

| Route token | Physics surface | First read or authoring command | Verification command |
| --- | --- | --- | --- |
| `Bot Trail sample` | Post Process Trail node on the Bot antenna chain | `ensure_anim_graph_trail_demo` | `sample_anim_node_pre_post_runtime_pose` |
| `Baddy RigidBody` | RigidBody node in `ABP_Baddy` or a duplicated sample variant | `inspect_anim_graph_node_settings` | `sample_anim_node_pre_post_runtime_pose` |

## Decision Rules

1. If the request names a Bot antenna or head-mounted appendage, start with the
   Trail route.
2. If the request names Baddy stalk, tail, soft body, wobble, jiggle, or body
   secondary motion, start with the RigidBody route.
3. If the user asks for "more/less physics", treat it as parameter tuning:
   `Alpha`, `ExternalForce`, `SimulationSpace`, or world geometry collision.
4. If the user asks for a new physical chain that does not already exist in a
   sample AnimBP, do not modify the original AnimBP. Park the new graph authoring
   helper as a C++/API candidate.
5. If the request is about collision, constraints, platforms, or destructible
   objects in the level, classify it as world physics instead of animation
   physics.

## Known Evidence

Bot Trail:

- Original `ABP_Bot` contains a retained Trail node titled
  `Trail controller / Bone: VB VBHead`, but it is disconnected from the root
  pose chain.
- Safe sample asset:
  `/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study`.
- Safe sample mesh:
  `/Game/_MCP_Sample/AnimStudy/SKM_Bot_Trail_Study`.
- Runtime proof requires explicit component-level Post Process AnimBP override
  on transient proof actors.
- Strongest same-instance PoseWatch smoke delta was on `antenna_04_l`.

Baddy RigidBody:

- Original active chain:
  `New State Machine -> LocalToComponentSpace -> RigidBody -> ComponentToLocalSpace -> DefaultSlot -> Root`.
- RigidBody node id from prior smokes:
  `81E779C34D36CC52F0125F91BF52BAF3`.
- Settings of interest: `SimulationSpace=ComponentSpace`, `Alpha=1.0`, no
  external force, no world geometry, default skeletal mesh PhysicsAsset.
- Safe study assets:
  `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study`,
  `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study_AlphaHalf`,
  `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study_ForceZ`,
  `/Game/_MCP_Sample/AnimStudy/ABP_Baddy_RigidBody_Study_WorldSpace`.
- WorldSpace made stalk motion reach walk-scale in the existing SIE metrics,
  while head and tail remained closer to idle-scale.

## Safe Command Patterns

Read a RigidBody node:

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

Tune a sample RigidBody variant:

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

Trail sample request:

```json
{
  "command": "ensure_anim_graph_trail_demo",
  "params": {
    "blueprint_name": "/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study.ABP_Bot_Trail_Study",
    "trail_bone": "antenna_04_l",
    "base_joint": "head",
    "chain_length": 4,
    "chain_bone_axis": "X",
    "fake_velocity": "[0, 0, 0]",
    "replace_existing": true,
    "allow_non_sample": false
  }
}
```

## C++/API Parking Lot

Keep these as candidates only until a concrete request needs them:

- `ensure_anim_graph_rigidbody_demo_variant`: create a connected sample
  RigidBody chain under `/Game/_MCP_Sample/AnimStudy`.
- `sample_anim_physics_variant_matrix`: spawn baseline and variants, drive
  movement, capture comparable bone metrics, and clean up in one command.
- `inspect_physics_asset_constraints_guarded`: read body/constraint details
  without broad Python reflection.
- Broader Trail parameter editing, if future requests need relaxation curves,
  limits, rotation behavior, or debug display beyond the current command.

## Final Response Checklist

For physics requests, report:

- Classification: Trail, RigidBody, source-vs-output proof, or world physics.
- Assets: sample path or read-only original path.
- Runtime proof: SIE/PIE/editor-world, node id, input/output links, key bone
  deltas, errors, warnings.
- Mutation status: original assets modified or not.
- C++/API decision: not needed, deferred candidate, or approved implementation.
