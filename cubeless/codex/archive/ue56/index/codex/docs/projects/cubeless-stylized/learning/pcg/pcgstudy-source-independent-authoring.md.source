# PCGStudy Source-Independent Authoring Notes

## Goal

Learn the PCG authoring grammar from `/Game/Cubeless/PCG/PCGStudy` so new PCG systems can be rebuilt without depending on the original PCGStudy source assets.

PCGStudy is the reference textbook. It should not become the runtime or recreation dependency. For test recreations, use replacement assets under `/Game/DreamscapeSeries`.

Default priority: use only PCGStudy as the PCG authoring grammar and system-design reference. Do not use Electric Dreams as a default PCG reference for now because that reference set is unfinished; bring it back only when the user explicitly re-enables it for a specific request.

## Safety Rules

- Treat `/Game/Cubeless/PCG/PCGStudy` as read-only analysis input.
- Do not use PCGStudy meshes, materials, Blueprints, maps, or saved point data as required sources for new work.
- Use `/Game/DreamscapeSeries` as the replacement mesh pool for recreation tests.
- Create validation drafts under `/Game/_MCP_Temp` first.
- Default PCG deliverable shape is a Blueprint Actor that owns a child PCGComponent. The BP is the user-facing authoring surface; the PCG graph lives under that actor/component, not as a bare graph-only deliverable.
- Expose user-tunable PCG parameters as BP variables by default, and keep BP variable names exactly matched to the PCG Actor Property parameter names. Example: BP variable `DungeonRoomCount` should be read by PCG as `DungeonRoomCount`, not an alias or differently cased name.
- Avoid Python map switching. Use native safe level commands if a map open is ever required.
- Save only after a recreated graph compiles and behaves as intended.

## Fast PCG Authoring Mode

Use this mode by default until a stable reusable PCG template asset exists.

1. Build or rebuild PCG graphs in batches. Prefer one Unreal Python/MCP operation that creates nodes, wires pins, sets editor properties, applies actor-property bindings, and marks assets dirty over many single-node calls.
2. Keep PCG regeneration off the hot path while authoring. Do not repeatedly trigger Construction Script regeneration or `Cleanup -> Generate` after each small edit; compile and regenerate only at grouped checkpoints.
3. Validate with data before screenshots. First read mesh/category instance counts, branch presence, spline containment, Landscape hit status, and radial density numbers. Use screenshots after the numeric gate passes or when visual framing is explicitly needed.
4. Capture review images sparingly. Prefer one final viewport screenshot or generated top-down distribution image, then run the alpha hook so the user-facing review copy has alpha `[255, 255]`.
5. Separate draft and final passes. Draft passes may use lower sampling density or simplified validation to prove graph structure; final passes restore requested densities, projection/normal alignment, scale/randomization, compile/save, regenerate once, and run the numeric plus visual QA gate.

Template cloning can be added later, but it is not part of the current default workflow.

### Fast PCG Side-Effect Gate

Fast PCG authoring trades immediate viewport feedback for fewer editor operations. Before a fast-authored PCG result is accepted, run these checks:

1. Verify the live `PCGComponent` and its `PCGGraphInstance` point at the intended graph; a reflection-level assignment alone is not enough.
2. Regenerate once at the final or major checkpoint and treat pre-regeneration viewport output as stale evidence.
3. Compare BP/CDO defaults against placed actor overrides for every PCG-facing actor property used by the graph.
4. Confirm spline shape, closed-state, bounds, and Landscape sampler contact after regeneration, not just after asset editing.
5. Run a deliberate parameter delta test, such as changing density or mesh on one category, and confirm that category count or mesh assignment changes as expected.
6. When judging density falloff, account for random seed noise by comparing aggregate center/mid/edge metrics rather than only individual instance positions.
7. If a batch edit fails or output is category-missing, inspect branch presence and node wiring/layout before adding more nodes.

If these checks are skipped, a fast-authored PCG can look unchanged, partially generated, or falsely correct until the next explicit refresh.

### Finished PCG Live Refresh Mode

Use fast authoring while the graph/BP is being built, then switch finished user-facing PCG Blueprints into live refresh mode when the numeric gate passes.

1. Expose an editor-facing auto-refresh control such as `AutoRegenerateInEditor`. Keep it off during MCP/scripted batch authoring, then default it on for final artist-facing assets unless the graph is known to be too heavy.
2. Use debounce or a dirty flag. Refresh after meaningful changes to spline shape, graph assignment, actor-property parameters, mesh overrides, scale, density, or falloff settings; do not run `Cleanup -> Generate` on every tick or every tiny node-authoring step.
3. Provide a deterministic manual refresh path for heavy graphs or uncertain editor states. If a Blueprint `Call In Editor` route is not safely available, use an editor utility or UnrealMCP refresh command instead of adding C++ by default.
4. Prefer a separate `PreviewDensityScale` or equivalent preview-quality control for expensive graphs, so artists can get quick feedback without forcing full-density regeneration during editing.
5. Validate live refresh before handoff: change one density/falloff parameter, one spline shape, and one mesh override; confirm regenerated category counts, containment, Landscape hits, graph assignment, and latest log health remain acceptable.

## Core Grammar

The repeated grammar is:

1. Read actor/BP parameters and actor tags.
2. Collect landscape, spline, actor, or stored point input.
3. Sample surface or spline into points.
4. Apply branch toggles from BP parameters.
5. Apply mask and exclusion utility graphs.
6. Apply density, noise, bounds, transform, projection, and self-pruning.
7. Assign mesh and optional material override attributes.
8. Spawn by point attributes.

The most common node families seen in the scan were:

- `PCGSubgraphSettings`
- `PCGGetActorPropertySettings`
- `PCGBranchSettings`
- `PCGAttributeFilteringSettings`
- `PCGStaticMeshSpawnerSettings`
- `PCGDifferenceSettings`
- `PCGSelfPruningSettings`
- `PCGBoundsModifierSettings`
- `PCGTransformPointsSettings`
- `PCGProjectionSettings`
- `PCGSplineSamplerSettings`
- `PCGGetSplineSettings`

This means the system is not mainly a hardcoded asset pack. It is a parameterized PCG graph grammar driven by actor properties and tag contracts.

## Required Contracts

### Actor And Component Tags

These tags are part of the authoring contract:

- `cliff`: cliff or steep-surface input.
- `road500`: road-width or road spline mask input.
- `del`: generic deletion or eraser actor input.
- `inner` / `outer`: water/shore/interior spline component regions.
- `ForestPathBP`: path spline used to cut forest placement.
- `herbicide_all`, `herbicide_tree`, `herbicide_grass`, `herbicide_bush`, `herbicide_debris`: selective exclusion/mask volumes.
- `herbicide_*_mesh`: mesh-based exclusion variants.
- `priority 0` through `priority 4`: priority masks for conflict resolution.

Use consistent lowercase/spacing when recreating. The original content mixes cases such as `Priority 0` and `priority 0`, so recreated graphs should normalize tag usage where possible and document aliases when needed.

### Point Attributes

The key spawner contract is attribute-based:

- `Meshes`: StaticMesh attribute used by most recreated spawners.
- `Mesh`: StaticMesh attribute used by some older/simple graphs.
- `Override Materials`: optional material override attribute.

Most large templates use `PCGMeshSelectorByAttribute` with `attribute_name = Meshes`, plus `Override Materials` when material override support is enabled. This is the most important source-independent pattern: if the point metadata carries the right mesh asset, the original PCGStudy mesh source is not needed.

## Dreamscape Replacement Pool

Use these replacement families for recreation tests:

- Trees: `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Trees/SM_Conifer_*`
- Ground vegetation: grass, fern, flower, and ground leaf meshes under `Meshes/Foliage`
- Rocks and cliffs: `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Stones/Rocks/SM_Rock_*` and `Meshes/Stones/Cliff/SM_Cliff_*`
- Structure/path pieces: ruin walls, pillars, stairs, door frames under `Meshes/Props/Ruin`
- Debris: branch, wood, trunk, cart, boat, torch meshes under `Meshes/Props/Objects`

## Rebuild Recipe: Forest / Basic Vegetation

Reference patterns:

- `/Game/Cubeless/PCG/PCGStudy/PCGGraph/PCG_Forest`
- `/Game/Cubeless/PCG/PCGStudy/PCGGraph/PCG_Base_01`
- instance graphs such as `IG_community_tree`, `IG_forest`, and grass variants.

Observed structure:

- Landscape is collected with `GetLandscape` and sampled with `SurfaceSampler`.
- Attribute filters commonly threshold density around low-to-mid values such as `0.2`.
- `ForestPathBP` spline input can be used to cut or reduce placement near a path.
- Multiple point streams are named by intent, such as tree point merge, group foliage, debris, and basic point group.
- `Difference`, `Merge`, `Collapse`, `TransformPoints`, `Projection`, and `SelfPruning` are used to clean overlaps and place points on the surface.
- Spawners use `Meshes` or older `Mesh` point attributes.

Source-independent rebuild:

1. Create an actor/BP with a PCG component and exposed variables for density, seed, offsets, slope/height limits, and mesh arrays.
2. Get landscape data and optionally a path spline by tag.
3. Surface sample base points.
4. Filter by density, slope, height, and path exclusion.
5. Add noise and offsets.
6. Self-prune points by category radius.
7. Assign `Meshes` from Dreamscape conifers, ground vegetation, rocks, and debris.
8. Spawn with `PCGMeshSelectorByAttribute`.

## Rebuild Recipe: Road / Path

Reference pattern:

- `/Game/Cubeless/PCG/PCGStudy/PCGGraph/PCG_Base_Road`

Observed structure:

- Large parameter surface: road width, road mesh toggle, center grass, center gravel, side gravel, side grass, structure spawn, lamp spawn, density, offsets, scale, fit-to-curve, and shadow/projection options.
- Uses `GetSpline` and several `SplineSampler` variants:
  - dense spline sampling around `10` units for close support points,
  - road path sampling around `200` units,
  - larger intervals around `1000`, `2000`, and `8000` for sparse objects.
- Uses many `Branch` nodes to turn modules on/off from BP actor properties.
- Uses `Difference`, `OuterIntersection`, `BoundsModifier`, and `SelfPruning` for road footprint cleanup.
- Road graphs still reference old `/Game/EL/...` decal materials. Treat those as design references only.
- Spawning is attribute-based with `Meshes` and optional `Override Materials`.

Source-independent rebuild:

1. Use a spline actor or BP component as the road backbone.
2. Expose road width, side-band width, center/side density, gravel/grass toggles, lamp/structure toggles, and seed.
3. Sample the road center spline.
4. Create road footprint masks with bounds expansion or spline interior sampling.
5. Use difference masks to remove tree/grass/debris inside the road footprint.
6. Generate center road, side gravel, side grass, lamps, and structures as separate point streams.
7. Assign Dreamscape ruin pieces, rocks, grass, and debris through `Meshes`.
8. Replace old decal dependencies with project-local decals or skip decals in the first recreation pass.

## Rebuild Recipe: Spline Grass

Reference pattern:

- `/Game/Cubeless/PCG/PCGStudy/PCGGraph/PCG_Splinegreass`

Observed structure:

- Medium-large graph with many branch toggles and subgraph utility calls.
- Exposes density, seed, base offset, additional offset, final density adjustment, grass spacing, grass mesh, and landscape-layer reaction controls.
- Uses road-width variants and herbicide masks to avoid road or deletion areas.
- `SplineSampler` commonly samples `OnSpline` by distance around `100` units.
- Spawns by `Meshes` plus optional `Override Materials`.

Source-independent rebuild:

1. Read spline input.
2. Sample along spline at a spacing parameter.
3. Optionally project points to landscape.
4. Apply road/herbicide exclusion masks.
5. Add density noise and XY/Z offsets.
6. Assign Dreamscape grass, fern, and flower meshes to `Meshes`.
7. Spawn through attribute selector.

## Rebuild Recipe: Waterside / Shore

Reference pattern:

- `/Game/Cubeless/PCG/PCGStudy/PCGGraph/PCG_waterside`
- `/Game/Cubeless/PCG/PCGStudy/PCG_CustomNode/SG_WaterRiverMask`
- `/Game/Cubeless/PCG/PCGStudy/PCG_CustomNode/SG_WaterLakeMask`

Observed structure:

- Uses `inner` and `outer` component tags to distinguish interior water/shore zones.
- Uses many branch/subgraph/filter operations.
- Interior spline sampling uses `OnInterior` with spacing commonly around `100`, `300`, `500`, or `600`.
- Outer/intersection logic separates river/lake mask areas from surrounding placement.
- Exclusion and priority masks are integrated with herbicide and priority subgraphs.
- Supports floor/bottom meshes, rocks, debris, grass, and shore/edge vegetation.

Source-independent rebuild:

1. Require a water/shore spline with `inner` and/or `outer` component tags.
2. Sample interior and boundary bands separately.
3. Generate bottom/shore point streams.
4. Use water masks to remove normal foliage from water interior.
5. Add rocks and debris along boundary zones.
6. Use priority masks to prevent water-edge items from colliding with road/cliff/vegetation systems.
7. Assign Dreamscape rocks, grass, ferns, and debris meshes through `Meshes`.

## Rebuild Recipe: Cliff / Talus / Grass On Cliff

Reference patterns:

- `/Game/Cubeless/PCG/PCGStudy/PCGGraph/PCG_Talus`
- `/Game/Cubeless/PCG/PCGStudy/PCGGraph/PCG_ForestPath`

Observed structure:

- Cliff/talus graph exposes density, seed, density adjustment, offset, landscape-layer reaction, soil-side slope 기준, rock A/B/C/D mesh groups, size, Z offset, spawn toggles, and Z limit.
- Uses `cliff` component tag.
- Uses `SelfPruning`, `Projection`, `TransformPoints`, `Difference`, `Branch`, and `Subgraph`.
- Grass-on-cliff pattern is smaller and uses many attribute filters and self-pruning operations. It can use `Mesh` rather than `Meshes`.
- Spline sampling for cliff grass can use `OnSpline` distance around `400`.

Source-independent rebuild:

1. Identify steep or cliff areas by landscape normal/slope, layer, or a tagged cliff spline/component.
2. Generate base points with density and slope limits.
3. Split points into large rock, medium rock, small rock, and ground-cover streams.
4. Apply Z limit and slope filters.
5. Add offsets and scale variation.
6. Self-prune each rock/grass stream by category radius.
7. Assign Dreamscape cliff, rock, and grass meshes via `Meshes` or `Mesh`.

## Utility Subgraphs To Recreate First

Prioritize these as source-independent utility building blocks:

- `SG_Herbicide_*`: get tagged spline/actor mask, sample interior at about `50` spacing, output exclusion density.
- `SG_priority0` through `SG_priority4`: same mask pattern, used for conflict resolution.
- `SG_WaterRiverMask` / `SG_WaterLakeMask`: spline mask and boundary/intersection helper.
- `ZoffsetNScale` / `ZoffsetNZscale`: transform helper driven by exposed Z offset and scale parameters.
- `SG_Transformby3vector`: vector-driven transform helper.
- `SG_Symmetry`: mirrored transform/merge helper.
- `HeightFiltering`: branch plus attribute filter for height thresholds.

These should be rebuilt as small, named subgraphs before rebuilding large recipe graphs.

## MCP Validation Sample

Created source-independent validation graph:

- `/Game/_MCP_Temp/PCGStudy_RebuildTest/Graphs/PCG_RebuildTest_SplineGrass_Min`

This graph is a disposable MCP validation asset, not a permanent library asset. It combines the first herbicide-mask pattern with a minimal spline-grass placement chain:

1. `GetSelfSpline` (`PCGGetSplineSettings`)
2. `SplineSampler_100cm` (`PCGSplineSamplerSettings`)
3. `HerbicideActors_Tag` (`PCGDataFromActorSettings`, all world actors tagged `herbicide_all_mesh`)
4. `Subtract_Herbicide` (`PCGDifferenceSettings`)
5. `Randomize_Grass` (`PCGTransformPointsSettings`)
6. `Spawn_Dreamscape_Grass` (`PCGStaticMeshSpawnerSettings`)

The spawner uses only Dreamscape replacement meshes:

- `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Grass/SM_Grass_Medium01`
- `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Foliage/Plants/SM_Fern_01`
- `/Game/DreamscapeSeries/DreamscapeMountains/Meshes/Stones/Rocks/SM_Rock_01`

Validation result:

- Direct dependencies: `/Script/PCG` and the three Dreamscape meshes above.
- Forbidden dependency check: no `/Game/Cubeless/PCG/PCGStudy` dependencies.
- The graph was laid out left-to-right with named nodes so it remains readable in the editor.

Known limitation: this is a graph-structure validation sample. It proves the source-independent node grammar, tag-mask contract, Dreamscape mesh substitution, and dependency isolation. Promotion to a permanent library graph should happen only after an actor-level generation smoke test with a spline owner and tagged herbicide mask actor.

## C++ / API Follow-Up List

Current status: no mandatory C++ change is required for the first source-independent PCGStudy rebuild sample. Unreal Python can create PCG graphs, add nodes, configure common settings, connect pins, save assets, and verify dependencies.

Implemented API follow-ups in sibling `../unreal-mcp-cubeless`:

- `create_pcg_graph_from_spec`
- `audit_pcg_graph_contract`
- `validate_pcg_source_independence`
- `set_pcg_static_mesh_spawner_entries`
- `read_pcg_node_contract`
- `promote_pcg_temp_graph`
- `pcg_actor_smoke_test`

These are Python FastMCP tools for now. They avoid immediate C++ expansion and keep the future C++ route limited to cases where Unreal Python reflection is too brittle or too slow.

Recommended API improvements:

1. `create_pcg_graph_from_spec`
   - Status: implemented as a Python FastMCP tool.
   - Add an UnrealMCP tool that accepts a JSON graph spec: asset path, node class, node title, node position, selected settings, edges, and static mesh spawner entries.
   - This should keep graph creation deterministic and avoid repeating brittle ad-hoc editor Python scripts.
   - Implement in Python first if practical; move to C++ only if protected PCG editor properties or graph refresh behavior require native access.

2. `audit_pcg_graph_contract`
   - Status: implemented as a Python FastMCP tool.
   - Add a read-only audit tool for a PCG graph asset.
   - Report node titles/classes, edges, spawner mesh selectors, actor tag selectors, exposed parameters, direct dependencies, and forbidden dependency prefix hits.
   - This overlaps with `audit_content_root_mcp`, but should be PCG-graph-specific and easier to read during authoring.

3. `validate_pcg_source_independence`
   - Status: implemented as a Python FastMCP tool.
   - Add a small guard that fails when a recreated graph references forbidden source prefixes such as `/Game/Cubeless/PCG/PCGStudy`.
   - Allow explicit replacement roots such as `/Game/DreamscapeSeries`.
   - This can remain a Python/MCP API unless it needs to run inside a faster native batch audit.

4. `pcg_actor_smoke_test`
   - Status: implemented as a guarded Python FastMCP dry-run/current-level existing-component smoke test.
   - Add a safe validation route that creates or uses a temporary actor under an approved preview/test context, attaches a spline and PCG component, runs generation, and reports instance/component counts.
   - Must not switch maps through Python. If map work is needed, use existing native safe map commands only.
   - Current Python version does not create actors or switch maps. It audits the graph and can dry-run or run cleanup/generate on existing current-level PCG components that already reference the graph.
   - Full temp actor creation, spline attachment, PCG component setup, refresh, cleanup, and dirty-package handling remain the first candidate that may benefit from C++.

5. PCG node readback helpers
   - Status: implemented as `read_pcg_node_contract` and supported by `audit_pcg_graph_contract`.
   - Add stable helpers to read pin labels, node titles, node positions, and selected PCG settings without hitting protected property or enum pythonization issues.
   - The current workaround reads `pin.properties.label`; enum fields such as pin usage can fail Python conversion.
   - C++ wrapper access would make graph diagnostics less fragile.

6. Static Mesh Spawner setter helper
   - Status: implemented as `set_pcg_static_mesh_spawner_entries`, including weighted entries and `by_attribute` selector setup.
   - Add a helper for setting `PCGStaticMeshSpawnerSettings` weighted mesh entries and attribute-based selectors.
   - Python can mutate `mesh_selector_parameters` in place, but the property wrapper itself is read-only and easy to misuse.
   - The helper should support both simple weighted Dreamscape replacements and the PCGStudy-compatible `Meshes` / `Override Materials` attribute contract.

7. Permanent library promotion command
   - Status: implemented as guarded dry-run-first `promote_pcg_temp_graph`.
   - Later, add an explicit promotion/copy command from `/Game/_MCP_Temp/...` to a permanent Cubeless PCG library path.
   - It should run dependency guard, naming checks, and optional graph contract audit before saving.
   - Do not auto-promote `_MCP_Temp` output by default.

## Minimal Validation Plan

1. Build one small utility first: `SG_Herbicide_all` equivalent under `/Game/_MCP_Temp`. Initial validation is embedded in `PCG_RebuildTest_SplineGrass_Min`.
2. Build one grass spline recipe using Dreamscape grass meshes. Initial validation is embedded in `PCG_RebuildTest_SplineGrass_Min`.
3. Build one road recipe that cuts grass around a spline and spawns side rocks/grass.
4. Build one cliff/talus recipe using Dreamscape rock meshes.
5. Verify each graph without referencing `/Game/Cubeless/PCG/PCGStudy` dependencies.
6. Only after that, promote reusable pieces into a permanent Cubeless PCG library.
