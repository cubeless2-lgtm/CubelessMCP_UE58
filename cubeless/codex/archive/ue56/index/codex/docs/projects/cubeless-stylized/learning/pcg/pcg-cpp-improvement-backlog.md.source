# PCG C++ Improvement Backlog

This backlog collects places where current PCG, screenshot, or UnrealMCP
workflows rely on fragile Python/editor scripting workarounds. Do not implement
these as C++ changes one by one during PCG tuning. Batch them later after the
workarounds prove which API surface is worth making durable.

## Rules

- Keep using Blueprint, PCG graph, editor scripting, and MCP Python first.
- Add an item here when the workaround becomes slow, error-prone, or hard to
  validate.
- Do not modify non-exception project C++ from this backlog without explicit
  user approval.
- UnrealMCP plugin C++ remains an allowed exception, but still prefer batching
  related bridge/API changes.
- Each C++ item should include the current workaround, the desired durable API,
  and the verification gate that would prove it works.

## Current Candidates

### 1. Screenshot Capture Route API

- Current workaround: Python sets viewport camera locations, uses temporary
  validation cameras, active viewport capture, optional existing bookmark
  capture, or OS/window capture, and then waits for screenshot files.
- Pain: bookmark recall can read the wrong/stale viewport buffer, keyboard
  shortcut recall can be blocked by the Windows session, and capture-route
  failures are hard to distinguish from actual PCG visual failures.
- Latest evidence: `AutomationLibrary.take_high_res_screenshot` produced the
  first validation PNG but later capture requests reported scheduled tasks
  without writing files. The fallback `PrintWindow` OS capture succeeded, but
  it captures the whole editor window and can show a stale viewport if the
  camera update path fails.
- Desired API: UnrealMCP command that captures a screenshot from an explicit
  route: active viewport by default, explicit camera/transform when supplied,
  or existing bookmark slots only when requested. It must wait until the final
  image is written and report resolution, path, capture source, camera
  transform, file size, image hash, and dirty-package delta.
- Partial implementation: `Plugins/UnrealMCP` now exposes
  `list_viewport_bookmarks` and `capture_viewport_bookmark_screenshot`.
  The capture command can jump to an existing bookmark slot without overwriting
  it, forces bounded viewport redraws, writes PNG through native viewport
  pixel readback, and returns resolution, file size, capture mode, viewport
  transform, and dirty package summary. The sibling MCP `editor_tools.py`
  exposes both commands.
- Current verification: in the current editor world, `list_viewport_bookmarks`
  returned `max_bookmark_count=10` and `existing_indices=[1,2,3]`.
  `capture_viewport_bookmark_screenshot(bookmark_index=1)` wrote
  `Saved/MCP_Screenshots/mcp_bookmark1_cpp_test.png` at `990x553`,
  `734258` bytes, with `capture_mode=bookmark`. Active viewport capture also
  wrote a PNG successfully. Bookmark slot `5` correctly returned a structured
  "No bookmark exists" error. A follow-up capture returned
  `dirty_package_count=1` for pre-existing temp level
  `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_IntentGallery_MCP`, proving the command
  now reports dirty state instead of hiding it.
- Sequence verification: captured bookmark slots `1` and `2` in sequence to
  `mcp_bookmark1_qa_sequence.png` and `mcp_bookmark2_qa_sequence.png`.
  Both captures succeeded, returned different view locations/rotations, and
  produced different SHA-256 image hashes
  (`3ee90e18...` vs `b99a363c...`). A later bookmark 1 capture reported
  `dirty_package_count_before=1`, `dirty_package_count_after=1`, and
  `dirty_package_added_count=0`, proving the capture command did not introduce
  new dirty packages.
- Full QA batch evidence: added `Tools/Unreal/run_pcg_bookmark_visual_qa.py`,
  which now defaults to active viewport screenshot capture, optionally captures
  explicitly requested existing bookmark slots, parses current level PCG/ISM
  counts, writes a JSON report under `Saved/MCP_PCG`, and separates capture
  route health from visual-density approval. On
  `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_IntentGallery_MCP`, the pass completed
  in about `1.2s`, wrote both screenshots, reported different SHA-256 hashes,
  and `dirty_package_added_count=0` for both captures.
- Current visual result: `capture_qa_pass=true`, but `qa_pass=false` because
  the current validation/intent-gallery level has only `128` grass instances
  against the default visual-density target of `1000`. This is an expected
  quality failure for the current level rather than a capture API failure.
- Landscape QA verification: after saving the dirty intent-gallery temp map,
  the runner was used on
  `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`. The repaired final
  pass wrote
  `Saved/MCP_PCG/pcg_bookmark_visual_qa_landscape_repaired_report.json` and
  `Saved/MCP_Screenshots/landscape_pcg_repaired_bookmark1_visual_qa.png`,
  with `capture_qa_pass=true`, `visual_density_pass=true`, and `qa_pass=true`.
  Bookmark `2` does not exist in that map, so it was recorded as skipped rather
  than created or overwritten.
- Remaining verification gate: use the same runner on the production-selected
  field map after the next art-placement pass and require both
  `capture_qa_pass=true` and `visual_density_pass=true`.

### 1a. Safe Editor Map Transition API

- Current workaround: Python scripts call `load_level` or `load_map` after trying
  to clear Python references and run GC.
- Pain: loading another map from inside `execute_python` can still leave the old
  world package referenced by `FPyReferenceCollector`, causing Unreal's `World
  Memory Leaks` fatal during editor map transition.
- Desired API: UnrealMCP command that performs map transition outside the active
  Python execution frame, clears Python exception/reference state, runs Unreal
  GC at the right boundary, and returns a structured success/failure result.
- Partial implementation: `Plugins/UnrealMCP` now exposes
  `open_editor_level`. The command normalizes a long package/object/`.umap`
  path, reports the current editor world, target filename, target existence,
  dirty package state, `can_load`, `blocked_reasons`, `load_attempted`, and
  `loaded`. It defaults to `dry_run=true`, and real transitions are blocked by
  default when dirty packages exist.
- Tooling integration: sibling `Python/tools/editor_tools.py` exposes
  `open_editor_level(level_path, dry_run=true, allow_dirty_packages=false, ...)`.
- Current verification: Live Coding build and sync succeeded. A dry-run for the
  already-open `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_IntentGallery_MCP`
  returned `already_open=true` and `load_attempted=false`. A protected real-load
  request for `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP` returned
  `can_load=false`, `blocked_reasons=["dirty_packages_present"]`,
  `load_attempted=false`, and preserved structured details instead of hiding
  the blocker behind a generic MCP error.
- Real transition verification: the dirty intent-gallery temp map was saved,
  clearing dirty packages from `1` to `0`, then `open_editor_level` loaded
  `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP` with `loaded=true`,
  `dirty_package_added_count=0`, and no new `World Memory Leaks`, `Fatal
  error`, `Assertion failed`, `Unhandled Exception`, or `Error:` lines in the
  latest log tail.
- Remaining verification gate: repeat this protected transition on a non-temp
  production-selected map before replacing all remaining Python map-transition
  workarounds.

### 2. PCG Regeneration Completion and Readback

- Current workaround: Python calls `cleanup`, `generate`, `generate_local`,
  schedules ticker retries, and polls ISM components until output appears.
- Pain: delayed PCG output causes immediate validation false negatives and
  forces scripts to carry retry loops.
- Latest evidence: a whole-Landscape candidate actor layer spawned `161` PCG
  actors but produced `0` ISM instances on
  `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`. The validation pass
  had to fall back to appending transforms to existing runtime PCG-generated
  ISM components.
- Desired API: UnrealMCP command/tool path that refreshes selected PCG
  components, lets the editor continue ticking, waits for generation
  completion or timeout from outside the editor thread, then returns generated
  component counts and package dirty state.
- Partial implementation: `Plugins/UnrealMCP` now has a compiled
  `refresh_pcg_components` bridge command that cleans, refreshes, generates,
  and reports PCG component state in one call. The command deliberately does
  not sleep-poll on the game thread; it returns `single_frame_readback` state
  and the sibling MCP Python tool performs `external_mcp_poll` when
  `wait_until_complete=true`.
- Tooling integration: sibling `Python/tools/pcg_tools.py` now tries the native
  command first, supports `max_components`, polls native readback externally,
  and falls back to the older Unreal Python refresh path when the running editor
  still reports `Unknown command`.
- Current verification: Live Coding loaded the new bridge code into the running
  editor. A direct bridge generate request with `wait_until_complete=true`
  returned in `0.328s` with `wait_mode=single_frame_readback` instead of the
  previous `30s` timeout. The MCP Python tool then completed
  `external_mcp_poll` against `MCP_ForestRoad_Instancer_00` with
  `initial_generate_count=1`, `component_count=1`, `wait_completed=true`,
  `wait_timed_out=false`, and `wait_iterations=3` in about `1.4s`.
- Remaining verification gate: run on the production-selected PCG actor with
  known tree, grass, and rock outputs; validate count changes after changing an
  exposed actor property and include package dirty state in the returned
  summary.

### 3. Safe PCG Data Introspection

- Current workaround: Python avoids broad reflection after an unsafe
  `PCGBasePointData.GetDensityBounds` probe crashed the editor.
- Pain: readback is limited to manually known methods, so diagnostics are slow
  and easy to under-report.
- Desired API: read-only C++ wrapper for PCG point data that validates ranges
  before reading counts, density bounds, transforms, metadata attribute names,
  and selected numeric/string/object attributes.
- Verification gate: malformed or empty point data returns structured errors
  instead of asserting or crashing.

### 4. Native Road Clearance and Overlap Filtering

- Current workaround: Python validates nearest-route clearance and overlap
  after generation while native `PCGDistanceSettings` and attribute/density
  filter semantics remain diagnostic.
- Pain: the active quality guarantee lives partly outside the graph, so visual
  tuning and native graph promotion can drift apart.
- Latest evidence: the spline road-mask clearing validation on
  `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP` used
  `MCP_PCG_RoadMaskSpline_ClearForest_Test.Road_SourceSpline` to remove
  `5,948` PCG-generated ISM instances from an already populated forest:
  `5,815` grass, `97` trees, and `36` rocks. Post-validation reported
  `grass_core=0`, `tree_clearance=0`, and `rock_clearance=0`, but the clearing
  behavior is still a Python post-process over PCG-owned ISM components rather
  than native graph-owned filtering.
- Follow-up evidence: after the spline was manually moved, nothing updated
  automatically because the current validation path is one-shot. The script was
  fixed to preserve and read the existing editor spline, and reapplying the
  moved spline removed another `2,749` instances with all current-route
  clearance violations returning to `0`. The old cleared corridor still cannot
  refill without regenerating the source forest, proving that the durable
  solution must be graph-owned regeneration instead of destructive ISM removal.
- Latest refill evidence: the whole-Landscape refill path now also resolves the
  existing editor spline before falling back to a generated route. On
  `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`, refill used
  `MCP_PCG_RoadMaskSpline_ClearForest_Test.Road_SourceSpline` (`8` points,
  `258,816.69cm`) and the follow-up road clear removed `2,577` instances with
  `grass_core=0`, `tree_clearance=0`, and `rock_clearance=0`.
- Current rule-repair evidence: added
  `Tools/Unreal/validate_pcg_landscape_quality_rules.py` and
  `Tools/Unreal/repair_pcg_landscape_quality_rules.py`. The validator initially
  found `2` tree road-clearance violations, `8` rock road-clearance violations,
  and `32` rock tilt violations. The repair pass removed those clearance
  offenders and clamped the rock transforms; the final report
  `Saved/MCP_PCG/pcg_landscape_quality_rules_report.json` returned
  `quality_pass=true`, `grass_normal_alignment.pass=true`, and zero tree/rock
  tilt or road-clearance violations.
- Desired API or graph support: durable native PCG rule path for category
  clearance and footprint overlap, or a focused C++ PCG element if built-in
  nodes cannot express the exact road/tree/grass/rock rules reliably.
- Verification gate: native output matches Python validator targets for gravel,
  stone, embankment, tree, grass, and rock clearance with zero hard-overlap
  violations.

### 4a. High-Density PCG Validation Scatter

- Current workaround: Python resets previous supplements by removing thousands
  of ISM instances one by one, line-traces Landscape height for every candidate,
  filters road clearance and object spacing in Python, then appends `180k+`
  validation instances to PCG-owned ISM components.
- Pain: this pass works for validation, but it is slow enough to interrupt
  iteration, depends on editor-exposed component mutation, and does not prove
  the native PCG graph can reproduce the same full-Landscape density.
- Latest evidence: the full-coverage validation pass scanned `186,275`
  instances, reset `186,200` previous supplements, and added `180,000` grass
  instances plus tree/rock supplements through Python.
- Follow-up evidence: the existing-spline refill pass produced `186,300`
  instances before road clear and `183,723` after road clear, then re-aligned
  `177,649` grass instances to Landscape normals with `normal_alignment_pass=true`.
- Latest QA evidence: after rule repair, the Landscape validation map retained
  dense coverage with `177,649` grass, `5,075` trees, and `997` rocks. The
  repaired bookmark QA report returned `qa_pass=true`, and the final rule
  validator returned `quality_pass=true`.
- Desired API or graph support: native PCG element or UnrealMCP helper that can
  apply deterministic density, Landscape projection, category spacing, and road
  clearance in one native generation path rather than as a post-process.
- Verification gate: regenerate the validation Landscape to the same target
  counts with `0` road violations and `0` tilt violations without Python
  per-instance reset/scatter loops.

### 4b. Safe Road Surface Visual Authoring

- Current workaround: road readability is restored through
  `Tools/Unreal/build_pcg_road_procedural_mesh_visual.py`, which spawns one
  `ProceduralMeshActor` and one terrain-following road mesh section from the
  current validation spline.
- Pain: the first static-mesh ribbon approach used
  `StaticMesh.BuildFromStaticMeshDescriptions` to rebuild a road surface asset
  and crashed the editor with a RenderResource/Array assert on
  `SM_Cubeless_PCG_RoadSurface_ShoulderVisualQA`. The cube-segment fallback was
  safer but created `746` actors. The SplineMesh pass reduced this to `44`
  actors but still showed segment/ribbon artifacts. The procedural mesh pass is
  much cheaper to manage, but it remains a Python-authored validation surface
  rather than a production road system.
- Latest evidence: on
  `/Game/_MCP_Temp/PCG/LVL_PCG_LandscapeValidation_MCP`, the final procedural
  pass used `MCP_PCG_RoadMaskSpline_ClearForest_Test.Road_SourceSpline` to
  create `1` road actor with `1` mesh section, `274` spline samples, `1,370`
  vertices, `2,184` triangles, `trace_misses=0`, and no dirty packages.
- Desired API or graph support: UnrealMCP helper or native PCG graph path for
  spline road visual output using a safe procedural mesh bridge, SplineMesh,
  decal, Runtime Virtual Texture, or Landscape layer blending without rebuilding
  StaticMesh assets from Python.
- Verification gate: regenerate the same road visual after moving the spline,
  report actor/component counts and clearance, capture overview/corridor
  screenshots, and complete without editor crash or dirty leftovers.

### 5. PCG Graph Authoring Helpers

- Current workaround: Python creates PCG nodes, sets editor properties by name,
  and relies on several engine-exposed classes whose property behavior varies
  by UE version.
- Pain: graph authoring scripts are verbose, and failures often appear only
  after live graph generation.
- Latest evidence: true-material grass, rock, and tree graphs had to be
  regenerated across `102` PCGGraph assets to add the same actor-property mesh
  override branch that base graphs already had. The first runtime validation
  failed only for rocks because `MaterialMood=2` routed to a true-material rock
  graph that still spawned the default mesh.
- Additional evidence: the native `SpawnSplineMesh` fixture for an open
  `2` point spline only accepted actor-property mesh override after
  `GetActorProperty.bForceObjectAndStructExtraction=false`, `CopyAttributes`
  copied the resulting soft-object-path attribute onto the polyline data, and
  `SplineMeshOverrideDescriptions` targeted `StaticMesh`. Connecting data to a
  visible `Overrides` pin was not sufficient because `PCGSpawnSplineMesh`
  initializes descriptor overrides from the default polyline input data.
- Desired API: typed UnrealMCP helpers for common PCG graph operations: create
  node, set settings with validation, wire pins, configure actor property
  getters, configure static mesh spawners with attribute selectors, and save
  with compile/validation reporting.
- Verification gate: rebuild a small fixture graph with actor-property mesh
  overrides and verify BP variable changes alter spawned meshes.
- Audit evidence: `Tools/Unreal/audit_pcg_static_mesh_spawner_actor_property_overrides.py`
  scanned `/Game/Cubeless/PCG` and found `275` PCG graph assets, `106` graphs
  with StaticMeshSpawner nodes, `190` total StaticMeshSpawner nodes, and `103`
  weighted/static mesh spawners in `100` graphs that still need review before
  production promotion. The next production priority is
  `/Game/Cubeless/PCG/Runtime/Graphs/PCG_Cubeless_ForestRoadRuntime_NativeSkeleton`,
  which has `3` weighted runtime spawners. Most remaining review candidates are
  learning/preset graphs, so those should be handled by generator/template rules
  rather than hand-editing every asset.

### 5a. Actor-Property Mesh Override Promotion

- Current workaround: validation can directly replace PCG-generated ISM
  component meshes with `SM_Grass_Medium01` after generation to make the ground
  read more like a grass carpet.
- Pain: direct component mesh replacement is useful for visual validation but
  bypasses the intended PCG rule that Static Mesh Spawner mesh choices should
  be driven through Blueprint actor properties.
- Latest evidence: the Landscape validation pass changed leaf/fern grass
  components to `SM_Grass_Medium01` and excluded flower components from the
  high-density scatter. It improved the output but remains a Python
  post-process.
- Latest validation: runtime Blueprint variables were added to
  `/Game/Cubeless/PCG/Runtime/Blueprints/BP_Cubeless_PCG_EcosystemRuntime`, and
  the two-phase validation report
  `Saved/MCP_PCG/pcg_runtime_actor_property_override_validation_report.json`
  passed for grass, tree, and rock. Generated ISM rows used `SM_Fern_01`,
  `SM_Conifer_08`, and `SM_SmallRock_02` from actor properties with no direct
  component mesh replacement.
- Desired API or graph support: make the production/runtime PCG graphs expose
  grass/tree/rock mesh selector inputs through actor properties by default, and
  provide a validation command that proves the generated ISM mesh paths match
  those Blueprint values after regeneration.
- Verification gate: keep the current grass/tree/rock runtime override report
  passing after future graph rebuilds, including true-material graph routes.

### 5b. Viewport Camera Control Reliability

- Current workaround: Python calls `set_level_viewport_camera_info`, invalidates
  viewports, then uses HighResShot, Automation screenshot tasks, or OS
  `PrintWindow` fallback.
- Pain: camera changes and screenshot output can lag or capture a stale
  viewport. This makes close/far visual QA less trustworthy than the numeric
  reports.
- Latest evidence: after setting a close validation camera, OS capture still
  showed the editor viewport from a higher/wider view, while console
  `HighResShot` did not always write a file immediately.
- Desired API: a native editor viewport command that applies a camera transform,
  waits for the target viewport to render that exact transform, captures either
  viewport-only or whole-editor output, and returns the actual camera transform
  used for the image.
- Verification gate: capture three different camera transforms in sequence and
  confirm image hashes and reported transforms differ as expected.

### 5c. Runtime Spline Component Sync Reliability

- Current workaround: Python sets `Road_SourceSpline` points on runtime
  Blueprint actors, then re-queries the actor and retries if the edit landed on
  a transient `TRASH_SplineComponent_*` or if the persistent spline still has
  the wrong point count or route length.
- Pain: native PCG road generation depends on the runtime spline being exact.
  A stale `2` point / `100cm` runtime spline can make road module sizes collapse
  to `1~2cm` and can cause roadside PCG branches to generate zero instances.
- Latest evidence: the ExampleMap native road visual review initially produced
  road spline mesh output but no roadside instances because the sync modified a
  transient component. After Python re-query/retry was added, the same route
  validated with `spline_mesh_component_count=288`, `instanced_instance_total=293`,
  and `roadside_clearance_violation_count=0`.
- Additional evidence: the native road shape suite in
  `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_IntentGallery_MCP` found both the
  authoring and runtime `Road_SourceSpline` components present but stale at
  `2` points / `100cm`. This made the suite pick a bad source and fail density
  checks until Python rejected unusably short sources, repaired the authoring
  handle, and restored `8` points / `51,681.76cm`.
- Desired API: a native UnrealMCP helper for setting SplineComponent points on a
  Blueprint actor instance that targets the persistent named component, waits
  for Blueprint component reinstancing/construction side effects to settle, and
  returns the final point count, spline length, and max point delta.
- Partial implementation: `Plugins/UnrealMCP` now has a compiled
  `set_spline_component_points` bridge command that targets a named/tagged
  `SplineComponent`, ignores `TRASH_` components by default, sets world/local
  points, optionally sets closed-loop state, and reports final point count,
  spline length, candidate components, and max point delta. The running editor
  must load the new plugin code by Live Coding apply or editor restart before
  the command is available.
- Tooling integration: sibling `Python/tools/pcg_tools.py` exposes
  `set_spline_component_points` with native-first behavior and a Python fallback
  for older running editor sessions.
- Verification gate: sync an authoring spline into a runtime actor, regenerate
  native PCG, and confirm the runtime spline length matches the authoring route
  within `1cm` before PCG generation starts.

### 5d. StaticMesh Block-Tag Exclusion

- Current workaround: field look-polish and canopy-boost Python passes scan
  actor/component tags containing `block`, approximate tagged StaticMesh bounds
  with 2D AABBs, and skip or report generated instances that would land inside
  those bounds.
- Pain: the current field level had `block_tagged=0`, so this is only a
  preflight path. Production PCG still needs a real tag-based exclusion fixture
  instead of relying on per-script Python bounds checks.
- Latest evidence: `CubelessFieldLookPolish_Report.json` reported
  `block_tagged_component_count=0` and `block_overlap_violations=0`; the canopy
  boost also reported `block_tagged_component_count=0` while preparing to skip
  future tagged bounds.
- Fixture evidence: `Tools/Unreal/validate_pcg_block_tag_staticmesh_exclusion.py`
  placed a tagged StaticMesh blocker inside the closed-spline grass area. The
  raw native graph output detected `1` block-tagged component but still produced
  `9` grass overlaps, so `native_graph_exclusion_pass=false`. The temporary
  Python prune removed those `9` instances and the final validation passed with
  `block_overlap_violation_count=0` and `generated_instance_total=119`.
- Tooling fix: the closed-spline validator now falls back to
  `StaticMeshComponent.get_local_bounds()` when `component.bounds` is not
  exposed in UE Python, so block-tagged StaticMesh actors are detected in this
  validation path.
- Native graph attempt: the next block-aware fixture tried to duplicate
  `PCG_Cubeless_ClosedSplineGrassArea_MCP` into
  `PCG_Cubeless_ClosedSplineGrassArea_BlockTagNative_MCP` and insert
  `PCGDataFromActorSettings(block) -> PCGDifferenceSettings` before the Static
  Mesh Spawner. Deleting/reduplicating the target graph while a `PCGComponent`
  referenced it caused an editor prompt loop and blocked the MCP bridge command
  loop. The script has been changed to update the existing temp graph in place,
  and the editor was restarted to clear the stale callback.
- Native graph validation: after the in-place graph update, the block-aware temp
  graph passed without Python pruning. The final report had
  `native_graph_exclusion_pass=true`, `block_tagged_component_count=1`,
  `block_overlap_violation_count=0`, `python_prune.total_removed=0`,
  `generated_instance_total=111`, and cleanup leftover `0`.
- Mesh override interaction: when actor-property StaticMesh override was added
  after `PCGDifferenceSettings`, the block-aware graph produced zero spawned
  instances even though the graph pins were connected. The working temp graph
  now copies `GrassMeshOverride` into `DynamicMeshPath` before the Difference
  operation, then routes `Difference.Out` into a `PCGMeshSelectorByAttribute`
  spawner. Latest validation passed with `native_graph_exclusion_pass=true`,
  raw/final overlap `0`, Python removed `0`, and `generated_instance_total=111`.
- Desired API or graph support: a native UnrealMCP/PCG helper that collects
  StaticMesh actor/component bounds by tag, converts them into PCG exclusion
  shapes or metadata masks, and applies the same exclusion to trees, rocks,
  grass, and road-edge scatter.
- Verification gate: place a tagged StaticMesh blocker in a validation level,
  regenerate the target PCG, and prove that generated tree/rock/object
  instances are `0` inside the blocker bounds while grass behavior follows the
  requested per-category overlap policy.

### 5e. Closed-Spline Area Generation

- Current workaround: `Tools/Unreal/validate_pcg_closed_spline_grass_area.py`
  creates a closed spline fixture and validates generated grass positions
  against the spline polygon.
- Important rule: open splines, including `2` point splines, are valid and must
  remain supported for roads, fences, guide lines, borders, clear masks, density
  gradients, and other linear placement. Closed splines with at least `3`
  points are the separate area-mask case for grass/groundcover generation.
- Latest evidence: the reused `BP_Cubeless_PCG_EcosystemCandidate` path is not
  sufficient for this feature. During PCG setup/generation it reverted the test
  spline to `2` points and produced `16` grass instances at local/template
  positions outside the requested world polygon, so the validation report failed
  with `outside_violation_count=16`.
- Latest validation: the fixture now separates the spline source actor from a
  bounded `PCGVolume`. On
  `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_IntentGallery_MCP`,
  `Tools/Unreal/validate_pcg_closed_spline_grass_area.py` generated `128`
  grass instances inside the closed `6` point polygon with
  `outside_violation_count=0`, `block_overlap_violation_count=0`, and
  `pitch_roll_violation_count_after=0`.
- StaticMesh override validation: the closed-area source Blueprint now exposes
  `UseGrassMeshOverride` and `GrassMeshOverride`. The base graph keeps a weighted
  default branch when override is disabled, and a separate actor-property branch
  copies `GrassMeshOverride` into `DynamicMeshPath` for
  `PCGMeshSelectorByAttribute`. The default-off report generated `128`
  `SM_Grass_Medium01` instances; the override-on report generated `128`
  `SM_Fern_01` instances, both inside the closed polygon with no rotation or
  outside violations.
- Coexistence validation: `Tools/Unreal/validate_pcg_open_closed_spline_intent_coexistence.py`
  now keeps the closed `6` point area fixture and open `2` point linear fixture
  in the same validation level. The report
  `Saved/MCP_PCG/CubelessSplineIntentCoexistence_Report.json` passed with
  closed grass `generated_instance_total=128`, `outside_violation_count=0`, and
  the open spline still at `spline_point_count=2` with `19` spline mesh
  components.
- Stability note: running the open fixture after the closed fixture can leave
  the closed source spline stale at `2` points through Blueprint/component
  reinstancing side effects. The coexistence validation explicitly reapplies
  both spline intents before generation and readback until a durable native MCP
  spline-sync helper exists.
- Desired graph support: add a dedicated closed-spline area graph path using
  the spline as an area/surface source, then sample points only inside the
  closed polygon before the Static Mesh Spawner. Mesh choices should follow the
  actor-property override rule rather than hard-coded spawner meshes.
- Verification gate: place one open `2` point linear spline and one closed
  `6` point grass-area spline in the same validation level, regenerate PCG, and
  prove that the open spline remains available for linear intent while closed
  area grass has `outside_violation_count=0`, `generated_instance_total>0`, no
  block-tag overlap, and the required rotation/normal alignment limits.

### 5f. Open 2-Point Spline Linear Mesh Generation

- Current workaround: `Tools/Unreal/validate_pcg_two_point_open_spline_fence.py`
  creates a tagged open `2` point spline and applies fence-like
  `SplineMeshActor` segments using the existing
  `/Game/AI_Generated/Meshes/SM_Ieta_RoadFence_A` mesh.
- Important rule: open `2` point splines must stay valid for fences, guides,
  road edges, borders, masks, gradients, and other linear placement. They must
  not be forced into the closed-spline area-mask behavior.
- Latest validation: on
  `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_IntentGallery_MCP`, the fixture passed
  with `spline_closed_loop=false`, `spline_point_count=2`,
  `spline_length=8532.88`, `18` `SplineMeshActor` fence segments,
  `18` `SplineMeshComponent` outputs, and `mesh_mismatch_count=0`.
- Native graph validation: `Tools/Unreal/validate_pcg_two_point_open_spline_fence_native_graph.py`
  now builds `/Game/_MCP_Temp/PCG/Graphs/PCG_Cubeless_TwoPointOpenFenceNative_MCP`
  with a tagged source spline actor and separate `PCGVolume`. The graph passed
  with `spline_closed_loop=false`, `spline_point_count=2`,
  `spline_length=8532.88`, `19` generated `SplineMeshComponent` outputs, and
  every output mesh resolved from the source actor property
  `FenceMeshOverride=/Game/AI_Generated/Meshes/SM_Ieta_RoadFence_A`.
- Endpoint-edit validation:
  `Tools/Unreal/validate_pcg_two_point_open_spline_fence_native_graph_moved_endpoint.py`
  moved the local spline endpoints from the baseline `(-4200,-750)` /
  `(4200,750)` pair to `(-5200,-1500)` / `(4800,2100)`. The graph remained
  open with exactly `2` source points, length updated to `10628.26cm`,
  generated `23` spline mesh components, kept actor-property mesh override
  passing, and still avoided descriptor fallback.
- Coexistence validation:
  `Tools/Unreal/validate_pcg_open_closed_spline_intent_coexistence.py` confirmed
  the native open fixture and closed grass-area fixture can coexist in
  `/Game/_MCP_Temp/PCG/LVL_Cubeless_PCG_IntentGallery_MCP`. The open path stayed
  `closed_loop=false`, `spline_point_count=2`, generated `19`
  `SplineMeshComponent` outputs, and kept `actor_property_mesh_override_pass=true`
  while the closed area path stayed `6` points and generated `128` grass
  instances inside its polygon.
- Implementation note: `PCGSpawnSplineMeshSettings.SplineMeshOverrideDescriptions`
  should use a `StaticMesh` property target fed by a polyline metadata
  attribute. `GetActorProperty` must not force object/struct extraction for
  static mesh object references, otherwise the expected mesh attribute is not
  emitted.
- Desired graph support: keep this native `SpawnSplineMesh` path as the
  baseline linear mesh rule and fold it into reusable authoring helpers instead
  of hard-coding validation meshes per script.
- Verification gate: regenerate after moving either of the two spline points
  and prove the output remains open, has exactly `2` source points, updates its
  segment count/length, keeps actor-property mesh override passing, and does
  not affect closed-spline grass area generation.

### 5g. Runtime Road Native StaticMesh Override Validation

- Current status: `/Game/Cubeless/PCG/Runtime/Graphs/PCG_Cubeless_ForestRoadRuntime_NativeSkeleton`
  now follows the actor-property mesh override rule for gravel, stone, and
  embankment roadside StaticMeshSpawner branches.
- Implementation pattern: each category keeps a weighted default spawner for
  override-off, reads `UseRockMeshOverride` and `RockMeshOverride` from the
  tagged runtime road actor, copies `RockMeshOverride` into `DynamicMeshPath`,
  and sends override-on points through `PCGMeshSelectorByAttribute`.
- Blueprint contract: `BP_Cubeless_PCG_ForestRoadRuntime` exposes
  `UseRockMeshOverride` and `RockMeshOverride`. Future PCG spawner meshes should
  follow this same Blueprint-variable-to-PCG-actor-property naming pattern.
- Latest smoke evidence: sibling deferred prepare/verify passed with
  `288` spline mesh components, roadside counts `gravel=238`, `stone=48`,
  `embankment=7`, total instances `293`, `roadside_clearance_violation_count=0`,
  no material mismatches, and no temp actor leftovers.
- Shape-suite evidence: `start_runtime_road_native_graph_shape_suite_smoke_test`
  passed all `4` route shapes and restored the source spline. Baseline produced
  `288` spline meshes / `293` instances, compact curve `288` / `98`, tight
  switchback `278` / `202`, and long sweep `293` / `355`; all had
  `clearance_violations=0`, no material mismatches, and no new log `Error:`
  lines. Non-baseline exact count mismatches remain diagnostic because the suite
  validates route-scaled density ranges instead.
- Regression evidence: sibling wrappers
  `prepare_cubeless_pcg_runtime_road_native_shape_suite.py` and
  `verify_cubeless_pcg_runtime_road_native_shape_suite.py` are registered in
  `run_pcg_study_regression.py`; targeted `deferred_prepare` and
  `deferred_verify` runner executions passed.
- Verification rule: exact learned counts stay in the report as diagnostics,
  but pass/fail uses `5%` or minimum `3` instance tolerance so route-selection
  drift does not mask the actual quality gates: generated output, spline mesh
  count, clearance, material state, and cleanup.
- Descriptor override finding: `SpawnSplineMesh` logged errors when
  `RoadStartOffset`, `RoadEndOffset`, `RoadStartScale`, and `RoadEndScale`
  were bound through descriptor overrides, because those attributes were not on
  the control-point data domain used by the override reader. The current graph
  generator keeps those attributes as diagnostic candidates and disables the
  descriptor overrides, preserving the verified `288` spline mesh output while
  eliminating new `LogPCG Error` lines.
- Audit result: the static-mesh spawner audit reports this graph with
  `spawner_count=6`, `needs_actor_property_review_count=0`,
  `covered_split_default_count=3`, actor-property nodes present, and
  copy-attribute nodes present.
- C++ note: no C++ was needed for this pass. A future helper could still reduce
  Python boilerplate by creating a reusable "actor mesh property to
  DynamicMeshPath" PCG authoring operation, a safe spline-mesh descriptor
  override/domain validator, and a native report summarizer.

### 5h. Runtime Material Override Actor Property Validation

- Current status: `BP_Cubeless_PCG_EcosystemRuntime` now exposes editable
  material override controls matching the mesh override naming pattern:
  `UseTreeMaterialOverride`, `TreeMaterialOverride`,
  `TreeMaterialOverrideSlot1`, `UseGrassMaterialOverride`,
  `GrassMaterialOverride`, `UseRockMaterialOverride`, and
  `RockMaterialOverride`.
- Native PCG route: material overrides must use
  `PCGMeshSelectorByAttribute` with `use_attribute_material_overrides=true` and
  `material_override_attributes` such as `DynamicMaterialSlot0`. This is the
  engine-supported path for actor-property material selection.
- Latest evidence:
  `Tools/Unreal/apply_pcg_material_override_actor_properties.py` built the
  disposable `_MCP_Temp` graph
  `PCG_MCP_MaterialOverrideActorPropertyValidation`. Deferred validation passed
  with `generated_instances=2`; the generated `SM_Grass_Medium01` ISM reported
  material slot 0 as
  `/Game/DreamscapeSeries/DreamscapeMountains/Materials/Foliage/Plants/MI_Fern.MI_Fern`.
- Array probe: a disposable child Blueprint confirmed that
  `MaterialInterface[]` actor properties can be exposed and populated. Directly
  feeding one array attribute into `material_override_attributes` applies the
  first array item to material slot 0, but it does not expand later array items
  to slots 1+. Attempting selector strings such as `ArrayName[0]` and
  `ArrayName[1]` did not produce slot attributes. For multi-slot production
  material arrays, use explicit per-slot attributes or add a helper that expands
  a BP array into `DynamicMaterialSlot0`, `DynamicMaterialSlot1`, and so on.
- Production caution: the existing true-material Electric Dreams graphs still
  use fixed `descriptor.override_materials` entries for preset variants. For
  arbitrary BP-driven material replacement, production graphs should be
  promoted to by-attribute mesh/material metadata instead of mutating shared
  graph assets or trying to patch weighted selector descriptors per actor.

### 5i. Runtime Single-Mesh Material Override Promotion

- Current status: single-mesh runtime graph families now support BP-driven
  material override through explicit per-slot attributes.
- Graph families covered:
  - Base tree profile graphs for `CompactConifer` and `ColumnConifer` across
    `Solo`, `Sparse`, and `LightGrove`.
  - Base style amount graphs for `ClassicGrass` and `TallGrass` across ground
    and ditch `Sparse`, `Normal`, and `Dense`.
  - True-material tree profile graphs for `CompactConifer` and `ColumnConifer`
    with `DarkPine` and `SoftPine` variants.
- Pattern: add a `Use*MaterialOverride` split outside the existing
  `Use*MeshOverride` split. If material override is off, the graph keeps the
  old weighted/default and actor mesh override behavior. If material override
  is on, the graph sends either the default single mesh or the actor mesh
  override to `DynamicMeshPath`, copies material actor properties to
  `DynamicMaterialSlot0` and optional `DynamicMaterialSlot1`, then spawns with
  `PCGMeshSelectorByAttribute.material_override_attributes`.
- Latest evidence:
  `Tools/Unreal/apply_pcg_runtime_single_mesh_material_overrides.py` passed
  deferred validation. `ClassicGrass_GroundOnly_GroundDense` produced `16`
  instances using BP `GrassMaterialOverride=MI_Fern`, and
  `CompactConifer_Solo` produced `1` instance with slot 0 `MI_Fern` and slot 1
  `MI_Rock_01`.
- Follow-up status: the multi-mesh weighted families were handled in the next
  pass. See `5j`.

### 5j. Runtime Weighted Material Override Promotion

- Current status: weighted multi-mesh runtime graph families now support
  BP-driven material override without collapsing weighted mesh variation.
- Graph families covered:
  - Base tree profiles for `CompactConifer`, `ColumnConifer`, and
    `MixedConifer`.
  - Base style amount graphs for `ClassicGrass`, `TallGrass`, `MixedGrass`,
    `GroundFoliage`, and `SmallRocks`.
  - True-material style amount and style matrix graphs for
    `GroundFoliage`/`SmallRocks`.
  - True-material tree profile graphs including `MixedConifer`.
- Pattern: when `Use*MaterialOverride=false`, the graph preserves the existing
  weighted default and actor mesh override branches. When
  `Use*MaterialOverride=true` and mesh override is off, the weighted selector
  stays weighted but enables `use_attribute_material_overrides=true` and reads
  explicit slot attributes such as `DynamicMaterialSlot0`. When both mesh and
  material override are on, the actor mesh path uses `DynamicMeshPath` through
  `PCGMeshSelectorByAttribute` and applies the same slot attributes.
- Latest evidence:
  `Tools/Unreal/apply_pcg_runtime_single_mesh_material_overrides.py` produced
  `Saved/MCP_PCG/pcg_runtime_weighted_material_overrides_report.json` with
  `validation_pass=true`. Built counts were `base_tree=9`,
  `base_style_amount=30`, `true_style_amount=24`, `true_style_matrix=60`, and
  `true_tree=18`.
- Validation coverage: `MixedGrass` generated `100` instances across `2`
  unique meshes with all slot 0 materials overridden; `SmallRocks` generated
  `26` instances across `2` unique meshes with all slot 0 materials overridden;
  `MixedConifer` generated `3` instances across `3` unique tree meshes with all
  slot 0 materials overridden; the actor mesh override branch forced
  `SM_Conifer_05` and applied both slot 0 and slot 1 overrides.
- C++ note: UE 5.7 already exposes weighted-selector material override
  attributes, so no native fix is required for correctness. A future UnrealMCP
  helper could still package this repeated graph-authoring pattern into one
  command to avoid large Python scripts and reduce iteration time.

### 5k. StaticMeshSpawner Actor-Property Audit Hardening

- Current status: the audit now understands the weighted material override
  graph pattern introduced by `5j`.
- Update: `Tools/Unreal/audit_pcg_static_mesh_spawner_actor_property_overrides.py`
  records weighted selector material override flags and treats
  `WeightedMaterialOverride` / `TrueMaterial Default` spawners as covered when
  they are paired with same-prefix by-attribute actor mesh/material override
  branches.
- Latest evidence:
  `Saved/MCP_PCG/CubelessPCGStaticMeshSpawnerActorPropertyAudit_Report.json`
  reports `355` StaticMeshSpawner nodes, `165` covered weighted/default
  branches, and only `19` review spawners in `18` graphs after false-positive
  removal.
- Classification update: the audit now separates remaining review items from
  production blockers. The latest run reports
  `production_graphs_needing_actor_property_review=0` and
  `production_review_spawner_count=0`.
- Remaining scope: the `19` review spawners are classified as referenced
  learning assets (`9`), unreferenced cleanup candidates (`7`), a temp-referenced
  cleanup candidate (`1`), and an empty unreferenced runtime cleanup candidate
  with two null-mesh spawners (`2`). Treat this as asset cleanup/archive work,
  not a C++ implementation task.
- Cleanup caution: deleting or archiving these assets is intentionally not part
  of this audit pass. Amount/prototype graphs still have learning graph
  referencers, and one material override preset is referenced by `_MCP_Temp`
  external actors.
- Policy manifest update:
  `Tools/Unreal/pcg_static_mesh_spawner_audit_policy.json` now carries the
  non-destructive review policy: production path prefixes, the
  ElectricDreamsLearning legacy allowlist, and explicit cleanup/archive
  candidate groups. The audit report includes policy load/version fields plus
  `actionable_graphs_needing_actor_property_review` and
  `actionable_review_spawner_count`.
- Latest policy evidence: after cleanup archive, UnrealMCP audit execution
  loaded policy version `1` with `9` legacy allowlist assets and
  `cleanup_candidate_count=0`. It reported
  `actionable_graphs_needing_actor_property_review=0`,
  `actionable_review_spawner_count=0`,
  `production_graphs_needing_actor_property_review=0`, and
  `production_review_spawner_count=0`. Active cleanup candidates are now also
  `0` graphs / `0` spawners.
- Cleanup disposition: `Tools/Unreal/archive_pcg_static_mesh_spawner_cleanup_candidates.py`
  first archived `9` confirmed cleanup graph assets to
  `/Game/Cubeless/_Archive/PCG_StaticMeshSpawnerActorPropertyAudit_20260612`.
  The archive pass reported `archived_count=9`, `blocked_count=0`,
  `failed_count=0`, and `pass=true`. Afterward the archived empty
  `RuntimeGrass/NewPCGGraph` copy was deleted because it produced an
  `AssetCheck` missing soft reference error for
  `/Game/DynamicGrassSystem/Meshes/Bush1_SM`. The final disposition is `8`
  archived ElectricDreamsLearning material override graphs and `1` deleted
  empty RuntimeGrass graph. The policy records these under
  `archived_candidates` and `deleted_candidates`.
- Regression runner update: sibling
  `Docs/Analysis/ElectricDreams/run_pcg_study_regression.py` now includes
  `static_mesh_spawner_actor_property_audit_verify`, backed by
  `verify_cubeless_pcg_static_mesh_spawner_actor_property_audit.py`. The
  targeted UnrealMCP runner execution passed in `0.186s` with
  `pcg_study_regression_pass=True`.
- Log-size fix: `AUDIT_PRINT_FULL_REPORT=False` lets the runner emit a compact
  audit summary while direct audit runs keep the full graph report by default.
- C++ note: no native change is needed for audit correctness. A future native
  PCG graph introspection command could make this report faster and less noisy,
  but the current Python audit is adequate.

### 5l. Per-Branch PCG Actor-Property Parameter Binding

- Current workaround: `Tools/Unreal/validate_pcg_roadside_ecosystem_falloff.py`
  manually creates many Blueprint member variables, configures matching
  `PCGGetActorPropertySettings` nodes, and wires each property into typed PCG
  pins such as `DistanceIncrement`, `OffsetMin`, and `OffsetMax`.
- Latest evidence: the spline ecosystem falloff graph now exposes
  `<Branch>DensitySpacingCm`, `<Branch>SpawnOffsetMin`, and
  `<Branch>SpawnOffsetMax` for `17` grass/tree/rock branches. The final
  validation passed with `edge_error_count=0`, grass counts `467/338/67/11`,
  tree counts `12/10/0/2`, rock counts `39/26/0/2`, and screenshot QA passing.
- Replacement evidence: the branch-heavy graph was later replaced by a
  grid-gradient graph with coarse actor properties
  `EcosystemGridExtents`, `EcosystemGridCellSize`, `EcosystemWidthCm`,
  `EcosystemGrassSpawnRatio`, `EcosystemTreeSpawnRatio`, and
  `EcosystemRockSpawnRatio`. Validation passed with grass `1059`, tree `55`,
  rock `104`, `external_road_clearance_violations=0`, and
  `endpoint_cluster_pass=true`.
- Pain: this pattern is correct but verbose, easy to mistype, and version
  sensitive. One earlier vector-variable attempt used the wrong Blueprint type
  helper and had to be recovered by reloading the package before saving.
- Additional API pitfall: in the UE 5.7 editor session,
  `BlueprintEditorLibrary.get_basic_type_by_name("float")` produced an `int`
  pin type. Fractional Blueprint actor properties had to be created with
  `get_basic_type_by_name("real")`; the accidentally-created integer
  `*DensityScale` variables were hidden and replaced with real-typed
  `*SpawnRatio` variables.
- Desired API: a typed UnrealMCP/PCG authoring helper that can add or update
  Blueprint actor properties, set default values, expose them for instance
  editing, create matching `GetActorProperty` nodes, validate pin existence and
  data type, connect the property node to a named PCG settings pin, and report
  all edge errors before saving.
- Desired API extension: query Blueprint member variable pin types and either
  change a variable type safely when Unreal permits it or report that a
  recreate/manual cleanup path is required. The helper should explicitly map
  requested scalar `float`/`double`/`real` semantics to the UE version's real
  pin category instead of trusting display names.
- Verification gate: rebuild a small fixture graph with float and vector actor
  properties, set different per-branch values on one placed actor, regenerate
  through `refresh_pcg_components`, and prove output counts/ranges change while
  `edge_error_count=0`, `dirty_package_added_count=0` for screenshot capture,
  and no new log `Error:` lines appear.
- Partial implementation: `Plugins/UnrealMCP` now exposes
  `set_blueprint_variable_metadata`, and the sibling MCP Python layer exposes a
  matching `set_blueprint_variable_metadata(...)` wrapper. This fixes existing
  variable metadata such as `ClampMax`/`UIMax` without recreating Blueprint
  variables.
- Latest verification: `BP_Cubeless_PCG_EcosystemCandidate.PresetType`
  metadata was updated to `ClampMin=1`, `ClampMax=6`, `UIMin=1`, `UIMax=6`,
  and the running editor accepted `PresetType=6` after `LiveCoding.CompileSync`.
  `refresh_pcg_components` then regenerated the selected validation actor with
  graph
  `/Game/Cubeless/PCG/ProductionCandidates/Graphs/PCG_Cubeless_EcosystemCandidate_SplineEcosystemFalloff`.
- Remaining C++ note: the metadata setter is done. The larger helper that
  creates BP variables, creates matching PCG property nodes, and wires typed
  pins is still a later maintainability/API candidate.

### 5m. Spline-Local Ribbon / Area Scatter Authoring Helper

- Current workaround: preset-6 `SplineEcosystemFalloff` is authored in Python by
  chaining stock PCG nodes: `PCGCreatePointsGridSettings` for a low-density
  local candidate volume, `PCGSplineSampler(OnSpline)` for source-spline
  distance reference points, `PCGDistanceSettings`, density remap, and spawn
  filters. This keeps the user-facing shape as a spline-distance falloff volume
  and avoids both the older `PCGDuplicatePoint` relative-transform crash path
  and the `OnHorizontal Fill` spline-scale mesh stretching path.
- Latest evidence: the repaired local-grid graph passes low-density validation
  with grass `75`, tree `5`, rock `9`, `edge_error_count=0`,
  `mesh_override_pass=true`, `material_override_pass=true`, and
  `surface_height_pass=true`. After manually moving the validation spline's last
  point from local `[4200,0,0]` to `[5100,1050,0]` and running `generate(true)`,
  output updated to `82` total instances with grass/tree/rock still nonzero and
  no new PCG assert/crash. The validation screenshots are
  `Saved/MCP_Screenshots/spline_ecosystem_grid_low_density_review_v2.png` and
  `Saved/MCP_Screenshots/spline_ecosystem_grid_low_density_after_spline_move.png`.
- Historical evidence: a denser spline-local row graph passed with grass `768`,
  tree `40`, rock `73`, but later spline editing exposed stock-node transform
  and cache issues. Keep that result as a reference, not the current safe route.
- Crash evidence: moving or extending a preset-6 spline point with the older
  `PCGDuplicatePoint` row graph produced `IsRotationNormalized()` asserts in
  `UnrealEditor-PCG` after repeated `Invalid metadata key` warnings. Replacing
  the duplicate-row graph with `OnHorizontal Fill` removed the relative
  transform accumulation path. The active `PCG_Style` component remains
  on-demand while artists edit the spline.
- Pain: building a spline-following scatter ribbon from scalar controls still
  requires non-obvious authoring rules. Stock PCG can fill a ribbon from spline
  point scale, but the artist-facing width lives on the BP actor, so tooling must
  keep spline point scale and actor properties synchronized and must reassign
  the graph before generation to invalidate stale spline sample caches.
- Desired API/tooling: add a graph-authoring helper, or a small native PCG node,
  that emits a spline-local ribbon point field from `Spline`, `StepCm`,
  `WidthCm`, optional planar subdivisions, and `Falloff` settings without
  relying on hidden spline point scale side effects. It should preserve open
  2-point spline support, output one point data stream, and keep density or a
  named distance attribute suitable for grass/tree/rock spawn probability. It
  should normalize or reconstruct point rotation and scale internally so
  interactive spline editing cannot propagate invalid transforms or width-scale
  mesh stretching into stock PCG spawners.
- Desired validation helper: report spline-following health, including projected
  coverage along the source spline, max lateral distance, endpoint cluster
  counts, and whether the output point bounds rotate with the spline/actor.
- Verification gate: create a two-point rotated spline and a bent multi-point
  spline fixture, regenerate through `refresh_pcg_components`, and require
  nonzero center/inner/mid/far counts, decreasing falloff, no endpoint cap
  cluster, and screenshots from both clean perspective and top-down overlay
  active viewport captures.
- Latest reproduction: preset-6 live editing now uses an editor tick watcher
  with debounced `GenerateOnDemand` regeneration because native live regenerate
  can crash while dragging spline points. Plain `generate(true)` can leave the
  old spline sample cache alive after point insertion/removal. The current
  working route is: sync spline width scale, reassign the same graph on change,
  then run three deferred generate passes at `0.9s` intervals; each deferred pass
  reassigns the graph again before `notify_properties_changed_from_blueprint()`
  and `generate(true)`. The graph must reset sampled point scale before spawning;
  otherwise `EcosystemWidthCm` stored on spline point `Scale.Y` stretches grass
  meshes into giant strips. This passed 2-point baseline, 6-point expansion,
  3-point reduction, and a latest endpoint move from `[34200,38600,0]` to
  `[25500,43000,0]` where output bbox max followed to `[27550.1,43705.3,0]`
  with `last_error=null`. There is still no native safe "spline component
  changed, invalidate PCG spline sample cache, regenerate resources, and
  preserve spline instance data" command.
- MCP helper gap: in the current bridge session, native `set_spline_component_points`
  was unavailable and the Python fallback selected a `TRASH_SplineComponent_50`
  when asked for component name `Spline`. A production-safe spline edit helper
  should resolve exact component object paths/tags, ignore transient `TRASH_*`
  components, return the final target component path, and verify final point
  count/length before reporting success.
- RegenerateInEditor test: enabling `PCG_Style` as
  `GenerationTrigger=GenerateOnLoad` with `RegenerateInEditor=true` is likely
  the correct production direction and can regenerate when actor properties
  change. However, direct Python `set_location_at_spline_point()` did not emit
  the same editor spline-handle notification as a user drag; with the watcher
  stopped, moving the endpoint to `[30000,50000,0]` left the previous output
  bounds unchanged, and `refresh_pcg_runtime_component()` alone did not update
  the generated resources. Add a native validation/edit helper that performs the
  real editor transaction/PostEdit path for spline point edits, so automated
  tests can distinguish "PCG does not support this" from "Python did not emit the
  editor change event."
- Grid-bound gap: the current safe graph no longer depends on spline point
  `Scale.Y`, but the candidate grid is still an actor-local finite volume. The
  graph now exposes `EcosystemGridExtents` and wires it directly into
  `PCGCreatePointsGridSettings.GridExtents`; expanding that property fixed the
  immediate "added spline point does not spawn" review case. A native/helper
  route should still derive the candidate grid bounds automatically from the
  edited spline bounds plus `EcosystemWidthCm`, then invalidate/regenerate PCG
  output through the same editor transaction path used by viewport spline
  handles.
- Editable tangent gap: validation initially forced preset-6 splines to
  `SplinePointType.LINEAR`, hiding tangent handles. The Python selector now
  converts only linear/constant points to `CURVE_CUSTOM_TANGENT`, but a native
  spline authoring helper should preserve user-edited point types and custom
  arrive/leave tangents explicitly when rebuilding, validating, or copying
  spline fixtures.
- Additional graph-authoring gap: `PCGAttributeFilteringSettings` could not be
  used as a dynamic `SplineDistance <= EcosystemWidthCm` filter from Python
  graph construction. Supplying actor scalar data through the `Filter` pin, or
  copying the actor scalar onto points and comparing `threshold_attribute`,
  produced zero spawned output. Keep the density-band filter for now and add a
  native helper or documented PCG pattern for dynamic scalar threshold filters.

### 6. Long Command Transport and Response Framing

- Current workaround: large Python payloads previously stressed the bridge;
  `MCPServerRunnable` has already been hardened to buffer JSON chunks and send
  complete responses.
- Pain: any future large binary/report payload could regress this class of
  error, and long editor-side commands can still block the bridge command loop
  until the hard `120s` timeout. The closed-spline PCG graph pin probe left the
  editor process alive and port `127.0.0.1:55557` listening, but both MCP Slate
  status and direct socket `ping` received no response while the command loop
  was blocked.
- Latest reproduction: field volume-owned grass staging attempted to create a
  Blueprint subclass of `PCGVolume` through `execute_python`. The command timed
  out after `120s` before writing a report or temp asset, and subsequent
  `execute_python` plus a native `get_actors_in_level` bridge command also
  timed out while the editor window remained responsive. The staging script was
  patched to avoid `PCGVolume` Blueprint subclassing, but the bridge still needs
  an independent stuck-command recovery/cancel path.
- Additional reproduction: the current editor bridge did not recognize native
  `refresh_pcg_components`, so preset-6 validation had to fall back to
  `execute_python` for PCG generation/count polling. Keep the parent plugin and
  sibling Python command registry in sync, and expose a command-list/version
  probe so validation scripts can choose native commands without guessing.
- Desired API: keep transport chunking covered by an explicit bridge regression
  test, avoid sending huge source strings when file execution is available, and
  add a lightweight bridge health/recovery path that can report or cancel a
  stuck command without requiring an editor restart.
- Verification gate: execute a large but harmless file-based command, a large
  response command, and a deliberately slow command; verify that timeout is
  reported cleanly, the next `ping` returns `pong`, and the bridge accepts a new
  short command afterward.

### 7. PCG-Native Block Mask / Difference Authoring

- Current workaround: the field volume-owned grass staging layer detects
  `block` tagged StaticMesh actor bounds and prunes overlapping generated grass
  instances after PCG async generation settles.
- Pain: `PCGDataFromActor -> Difference` detected the block actor in the
  review graph, but after async regeneration it over-subtracted the whole review
  volume and produced zero grass. Leaving block exclusion as Python post-prune is
  acceptable for a non-production review layer, but it is not the desired final
  PCG ownership model.
- Desired API/tooling: add a reliable MCP/editor helper or documented graph
  pattern that builds a PCG-native block mask from actor/component tags without
  over-subtracting unrelated volume data. It should handle actor tags and
  component tags, preserve generated grass outside the block bounds, and expose a
  structured validation report with pre/post instance counts.
- Verification gate: run the field volume-owned grass staging graph with a
  visible `block` tagged cube, verify nonzero grass output, `block_overlap=0`,
  road clearance violations `0`, and no Python instance prune required.

## Keep In Python For Now

- One-off temp validation level setup.
- Disposable `_MCP_Temp` actor cleanup.
- Report JSON writing under `Saved/MCP_*`.
- Local visual tuning experiments that are not yet stable production behavior.
- Small asset inspections where Unreal Python exposes a safe, documented API.
