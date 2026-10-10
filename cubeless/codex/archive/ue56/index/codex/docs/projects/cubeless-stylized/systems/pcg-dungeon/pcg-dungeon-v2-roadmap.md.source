# PCG Dungeon V2 Roadmap

This roadmap explains how the current PCG dungeon V1 can support a later V2
without treating gameplay as complete. V1 is a closed-ceiling, Geometry
Script-authored, native-PCG-spawned dungeon delivery under
`/Game/Cubeless/PCG/Dungeon`.

## Reusable From V1

The V1 module library is reusable:

- Geometry Script-baked Static Mesh modules under `/Game/Cubeless/PCG/Dungeon/Meshes`.
- Dungeon material set under `/Game/Cubeless/PCG/Dungeon/Materials`.
- Native PCG point-source graph:
  `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativePointSource`.
- Native PCG spawn graph:
  `/Game/Cubeless/PCG/Dungeon/Graphs/PCG_Cubeless_Dungeon_MVP_NativeIntegration`.
- Mesh-key and material-safe split contract exported in
  `Saved/MCP_Dungeon/CubelessDungeonMVP_PCGSpawnerContract.json`.
- Handoff/readiness gates:
  `check_pcg_dungeon_handoff_readiness.py`,
  `check_pcg_dungeon_delivery_preflight.py`, and
  `run_pcg_dungeon_delivery_closeout.py`.

These assets and reports mean V2 does not need to rediscover wall orientation,
ceiling grouping, mesh-key grouping, material split groups, asset manifest
coverage, or screenshot closeout rules.

## Replace Or Redesign In V2

V2 should redesign the layout generator before changing gameplay:

- Random room/corridor layout rules.
- Room shape/archetype distribution.
- Loop and branch policy.
- Lock/key route policy.
- Encounter/reward/shop placement policy.
- Native point-source generation strategy.

The current V1 random structure is still Python/export-backed before it becomes
native PCG point data. A V2 can keep the V1 module set and validation gates
while replacing the layout source.

## Candidate V2 Directions

### Native PCG Layout Promotion

Promote more layout assembly into native PCG graph logic. This keeps the output
closest to Unreal PCG but may require careful graph authoring and repeated audit
helpers.

Best reuse:

- Existing mesh-key spawner groups.
- Existing material-safe spawner branches.
- Existing handoff readiness and closeout gates.

Risk:

- Complex graph edits can become brittle through editor scripting.
- Some layout algorithms may be awkward as pure PCG nodes.

### Focused UnrealMCP Layout Helper

Keep a Python/UnrealMCP layout helper, but make its contract narrower and more
data-oriented. The helper would produce point data and tags while native PCG
owns final mesh spawning.

Best reuse:

- Current `NativePointSource` and `NativeIntegration` split.
- Current JSON contracts and report-based validation.

Risk:

- Still not fully native PCG for layout.
- Requires discipline to keep gameplay logic out of the generator.

### Room Template Library

Build authored room templates from Geometry Script modules, then let PCG select
and connect templates.

Best reuse:

- Existing module meshes and material palette.
- Existing preset/closeout workflow.

Risk:

- Needs a new room-template manifest.
- Requires collision/door alignment validation beyond current V1 checks.

## V2 Gate Proposal

Keep V2 PCG-only until generation quality is proven. A V2 gate should require:

- Native output actor exists and generates non-empty PCG output.
- Output counts match the active V2 final gate.
- Wall, door, connector, ceiling, and corner orientations pass.
- Mesh-key grouping has no unknown/missing mesh paths.
- Presets run and restore default.
- Top/oblique screenshots pass exposure checks.
- Asset manifest audit passes.
- Live dirty state is `0`.
- Delivery closeout passes.

Gameplay placeholders may remain as metadata, but they should not be promoted
to a V2 completion gate unless the active task explicitly becomes gameplay.

## Suggested First V2 Step

Create a separate V2 folder instead of rewriting V1 in place:

```text
/Game/Cubeless/PCG/DungeonV2
```

Recommended first implementation step:

1. Reuse the V1 Geometry Script mesh builders to create a V2 module set.
2. Copy the V1 closeout pattern and report names with a V2 prefix.
3. Prototype one alternate layout source while keeping the NativeIntegration
   spawn graph pattern.
4. Compare V1 and V2 through separate output actors and screenshots.
5. Only after V2 generation passes, decide whether to replace or coexist with
   the V1 delivery.

This keeps the current V1 deliverable stable while allowing a different dungeon
algorithm to evolve safely.
