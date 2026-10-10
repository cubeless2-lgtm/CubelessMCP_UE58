# Editor/Actor UE58 migration plan and source coverage

2026-10-10. Read-only source investigation, no new Actor tool implementation/registration or engine mutation. `editor-actor-source-inventory-K.json` covers all **35** decorated tools in original `Python/tools/editor_tools.py`, all **35** matches in captured SDK runtime registration, their complete input schemas, command routes, handler line numbers and lexical engine/helper call references. No missing routes were found. Native-only `create_actor` is a deprecated alias to the same HandleSpawnActor implementation, not a 36th distinct operation. The capture is runtime decorator registration without tool-body execution.

Reproduction: `build_editor_actor_inventory.py`. Original paths and SHA256 are in the JSON. Lexical call extraction is a navigation aid, not proof that every helper is public or source-compatible with UE58. The initial core below has been read through its Python→native→engine path; remaining editor domains are explicitly incomplete, not lost or counted as official replacements.

## First implementation slice

Keep six named legacy public tools: get_actors_in_level, find_actors_by_name, spawn_actor, delete_actor, set_actor_transform, get_actor_properties. Preserve the hidden/native create_actor command identity as an explicit compatibility alias using the same spawn core. If it is exposed on official MCP, count it as an alias with identical semantics and its own description; do not pretend it is a new independent feature. Next Actor slice is spawn_blueprint_actor plus typed set_actor_property after dependency/constructed-actor side effects are reviewed.

Use `CubelessActorTools` Python registration and shared operation/journal envelope, with editor-only native helpers where exact UObject naming/spawn parameters/world ownership require C++. Suggested separation:

- registration/actor_toolset.py: six specialized schemas plus documented native-alias surface.
- operations/actor.py: query/mutation orchestration, argument adapters, durable before-intent, actual readback and legacy fields.
- compatibility/ue58/actor.py: exact native adapters.
- Native UE58 Actor API compatibility: resolve world/level, enumerate, object-name resolution, class allowlist, actor snapshot and mutation functions.
- Shared Actor operation core: one preflight, one snapshot, one mutation and independent after lookup; hidden BlueprintCallable+AIIgnore helpers, no new external server or runtime game dependency.

Reuse existing operation gate/journaling rather than inventing a second Actor TCP/request layer. If future batch support includes these operations, factor action cores before adding allowlist entries. Current Blueprint batch does not automatically support Actor commands.

## Legacy contracts and differences

Original native handler source is `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp`; common serialization is `UnrealMCPCommonUtils.cpp:1696–1765`.

| Tool | Original inputs and observable behavior | UE58 candidate / parity limits |
| --- | --- | --- |
| get_actors_in_level | No inputs. GWorld + GetAllActorsOfClass(AActor), actual loaded world actors rather than strictly one current ULevel. Native returns actors; Python adds success, actor_count, unreal_response. Actor fields are name, class, location[x,y,z], rotation[pitch,yaw,roll], scale[x,y,z]. | Explicit editor UWorld + public GameplayStatics/TActorIterator. Do not silently replace with GetAllLevelActors: UE58 editor subsystem filters WorldSettings, builder brush, uneditable/outliner-hidden/transient actors. Preserve legacy actor data and add explicit world/level/path metadata. |
| find_actors_by_name | pattern required, actor object-name substring via FString.Contains. Empty pattern matches all. Same world enumeration, same actor JSON and Python actor_count wrapper. | Filter object GetName, not GetActorLabel. UE58 FString Contains defaults IgnoreCase; declare/test case semantics explicitly. Official SceneTools.find_actors searches labels, so it is not equivalent. |
| spawn_actor | name/type required, location/rotation default [0,0,0]. Python validates 3-entry lists, converts float, then sends type.upper(). C++ accepts only StaticMeshActor/PointLight/SpotLight/DirectionalLight/CameraActor using case-sensitive mixed-case comparisons. Native also accepts hidden scale input default [1,1,1]. Exact UObject name via SpawnParams.Name, duplicate object-name check over editor world. Returns actor fields, spawned, postcondition_verified and before/after/delta mutation contract. No save/compile. | Existing public wrapper/native type case mismatch must be corrected through one canonical five-type lookup; preserve all five supported classes and native hidden scale. Do not copy the broken uppercase forwarding unchanged. Public UWorld::SpawnActor with explicit editor level/name mode; independently re-find exact name and compare class/transform. |
| delete_actor | name required, exact object-name first-match lookup in editor world. Calls Actor.Destroy; then relookup must be absent. Reports deleted_actor snapshot, deleted bool, postcondition_verified, before/after and counts. Missing actor is an error. No save. | Prefer editor-aware public deletion with selected/layers bookkeeping, after validating one unambiguous target/world/level. Verify object invalid/absent and actual level/actor-package dirtiness. Selection can legitimately change on deleting a selected actor; record it. Undo is not atomic rollback. |
| set_actor_transform | name required; optional location/rotation/scale default None; omitted components preserve current values. No requested fields still invokes setter in old handler. Native SetActorTransform bool + transform.Equals tolerance 0.01; reports updated/skipped counts, set_accepted, postcondition_verified. | Build one complete target transform from one before observation, use editor API once, and independently compare actual transform. Canonical quaternion comparison avoids Euler-wrap false failures. A no-op can skip actual engine mutation only if output/count/failure semantics are documented and validated. |
| get_actor_properties | name required, exact object-name first match in GWorld. Doc says all/detailed, but serializer ignores bDetailed and actually returns only five basic fields. Errors/not-found forwarded; some Python exceptions historically return {}. | Preserve five existing fields, add explicit identity/ownership separately. Do not claim all reflection properties. Official ObjectTools requires an object ref and caller-selected properties, so not an equivalent drop-in. Return structured errors rather than preserving silent {} failures. |

Source ranges: list :5969, find :5989, spawn :6015, delete :6142, transform :6194, inspect :6263; world/name helpers :241–283. HandleCommand :2331 adds one FScopedTransaction for mutations but the low-level handlers do not consistently Modify/mark/save package state. New verified dirty evidence must come from actual object/package observation, not assume a transaction saved anything. Read-only original queries use GWorld while mutations use GEditor->GetEditorWorldContext().World; UE58 new tools must explicitly identify the editor world, reject/segregate PIE, and document this correction.

## Verified installed public API candidates

Engine references below are under `C:/Program Files/Epic Games/UE_5.8/Engine/Source/`.

- `Runtime/Engine/Classes/Kismet/GameplayStatics.h:98` exports and reflects GetAllActorsOfClass; explicitly pass the captured editor world. It scans loaded actors, not unloaded World Partition actor descriptors.
- `Editor/UnrealEd/Public/Subsystems/UnrealEditorSubsystem.h` provides the editor world; native GEditor context is also available. No editor viewport widget is needed for querying actor transforms.
- `Runtime/Engine/Classes/Engine/World.h:437,496–515,3765–3775`: FActorSpawnParameters.OverrideLevel, NameMode Required_ErrorAndReturnNull/Required_ReturnNull, ObjectFlags and public SpawnActor overloads. Use RF_Transactional, nonfatal exact-name failure, captured explicit ULevel and collision handling stated in results. A collision/name failure may still execute engine code; always inspect actual changes.
- `EditorActorSubsystem.cpp:97–158` spawn helpers route through FLevelEditorViewportClient::TryPlacingActorFromObject and set GEditor ClickLocation/ClickPlane. They do not give a requested UObject name parameter and may create multiple actors via a factory. Official SceneTools then sets the human label. This is unsuitable as an unqualified exact-name replacement.
- `EditorActorSubsystem.cpp:369–394` GetAllLevelActors intentionally filters its results; do not use it as a parity shortcut for the old all-loaded-world query.
- `EditorActorSubsystem.h:236` DestroyActor and cpp:546–625 DestroyActors provide editor validation, selection removal, layer disassociation, World.EditorDestroyActor and delegates. There is no atomic rollback and the Layers subsystem assumption must be checked in headless/minimal-editor conditions. Direct World.EditorDestroyActor (`World.h:3731`) is another public route, but would need the editor bookkeeping scope made explicit.
- `EditorActorSubsystem.h:366/cpp:2686` SetActorTransform delegates to EditorElementSubsystem.SetElementTransform, returning actual bool. This route depends on available editor typed-element subsystems; preflight availability rather than inventing UI fallback. Native Actor.SetActorTransform is public, but must provide Modify/Pre/PostEdit/dirty semantics deliberately if used instead.
- `Actor.h:2742,3371` SetActorLabel and GetLevel are distinct APIs. A label is neither UObject name nor unique identity. GetPathName, world/level package and external actor package must accompany results to disambiguate loaded levels.

These are installed source declarations/implementations, not yet proof of new Cubeless runtime behavior. Root owns the future build and runtime probes.

## Official tool equivalence audit

Installed `editor_toolset/toolsets/scene.py:66–166,501–540` and `actor.py:106–152` were read. No candidate is classified as an equivalent official replacement yet:

- SceneTools.find_actors uses case-insensitive **label** search and its bounds/type filters; old name matching uses object names.
- SceneTools.add_to_scene_from_class/from_asset creates through editor factories, sets label after creation, and has optional parent/snap semantics. It does not preserve the old unique object-name contract or legacy error/result/verification envelope.
- SceneTools.remove_from_scene returns a bool with PIE/editability checks; it does not return before/after actor snapshots, dirty scope or durable idempotent accounting.
- ActorTools.set_actor_transform supports world/relative and Blueprint-template transforms, may compile a Blueprint for a template, and returns True after its world setter without independent transform-result validation. Old tool targets level actors with partial component patching. Retain a named compatibility wrapper over an appropriate direct API.
- ObjectTools.get_properties/set_properties are useful primitives for richer future typed property operations. They do not replace the current named query, per-operation selector, runtime dependency guard or postcondition contract.

## Identity, ownership, dirty and save rules

Capture editor world package, current ULevel, actor object path/name/class and level/external package state once before mutation. Do not base resolution on selection or label. Exact name may collide across streamed ULevels; reject ambiguous names and return candidate paths instead of editing the first match. Add a documented exact object-path selector or optional level package selector while keeping existing name requests valid when unambiguous.

The initial mutation slice should require a saved `/Game/` non-World-Partition, non-external-actor editable level, and report unsupported level ownership before mutation. This is a bounded initial scope, not full map parity. Test in fresh `/Game/CubelessMCPTests/Actor_<run>/` maps; the shipped tool body must work on any admitted project `/Game/` map, not hardcode the test path. Broaden to streaming/sublevels/external actor packages only with independent ownership/save/reload evidence. Reads can enumerate all loaded levels with explicit scope and a bounded result cap; hitting a cap must report incomplete/continuation state, never silently claim every actor was returned.

Do not save or compile during basic spawn/delete/transform. Report changed level and external actor packages, dirty before/after, selection side effects and inherited actor references. Save once at the pipeline boundary with exact package ownership. For a saved map the reloaded actor identity/value/deletion must be tested in a new editor process; disk save success alone is not full persistence evidence. A rootless actor rejecting transform or a locked/uneditable actor must fail without false success. Already-dirty unrelated packages remain untouched; no global save or clear-dirty fallback.

## Shared-core optimization

Use one snapshot serializer for list/find/get/spawn/set/delete output; only detailed identity extensions differ. One enumeration supplies list/find or exact-name resolution within a single preflight, with a local lookup map where several actors are processed. Independent after enumeration must remain after mutation; no cross-call cache and no request-provided state as evidence. create_actor and spawn_actor share one spawn action and class map. Do not instantiate a fresh full graph-like actor enumeration repeatedly merely to package result fields. Track traversals and actor counts so a later comparison can measure the actual savings.

Keep construction/spawn/transform/delete distinct action boundaries. Do not batch away user-requested save checkpoints or fold spawn+transform across an unknown failure. One API may run Blueprint construction scripts, attachment callbacks and editor delegates; first slice limits to the original five native classes, and the later Blueprint variant needs explicit class/CDO/runtime-dependency checks and side-effect accounting.

## Full editor source inventory: deferred independent targets

| Remaining group | Exact public tools | Reason it is separate / next API review |
| --- | --- | --- |
| Actor extension | spawn_blueprint_actor; set_actor_property | Generated-class/asset resolver and typed property guard. BP construction and object-reference ownership need runtime validation. |
| Physics/PIE input | line_trace_physical_material; inject_pie_input_key | World/trace channel/physical material and input user/device policy; runtime PIE scope differs from edit-only gate. |
| Viewport capture | focus_viewport; take_screenshot; list_viewport_bookmarks; set_active_viewport_realtime; capture_viewport_bookmark_screenshot | Active viewport/IBookmarkTypeTools/ReadPixels/PNG file output, selection/view restoration and renderer readiness; explicitly UI/render-dependent. |
| Level lifecycle | open_editor_level; get_editor_level_open_status; clear_dirty_package_flags; safe_new_preview_map | Existing ticker request/status, dirty-package safety and save/report policies. Clearing dirty flags is not saving or rollback; never use it as routine cleanup. |
| Niagara preview UI/lab | open_niagara_preview_player; get_niagara_preview_player_state; get_niagara_preview_lab_state; cleanup_niagara_preview_lab; capture_niagara_preview_lab_view; preview_niagara_system_in_preview_lab; sample_niagara_system_in_preview_lab | Original FNiagaraPreviewPlayerWindow, lab worlds/actors, renderer screenshots and sampling. Port editor-only helpers without introducing MCP game classes or copying runtime hard dependencies. |
| Niagara live inspection | inspect_niagara_component_runtime_state; inspect_niagara_component_parameter_store; inspect_niagara_component_grid2d_runtime; sample_render_target2d_region_stats | Runtime instance/system/parameter-store/data-interface/GPU readback APIs and readiness; loaded metadata is not simulation/render proof. |
| Niagara asset swaps | preview_niagara_component_asset_swap; inspect_blueprint_niagara_component_template; dry_run_blueprint_niagara_component_template_asset_swap; execute_blueprint_niagara_component_template_asset_swap_canary; execute_blueprint_niagara_component_template_asset_swap_durable | Composite inspect→dry-run→canary→durable contracts. Preserve separate cloning/deletion/compile/save/report and failure boundaries; helper reuse does not collapse tool identities. |

All 29 remaining public tools are in the JSON with exact source/schema/routes, status incomplete. No tool is removed because it resembles an official tool. UE58 API compatibility for the non-Actor groups is not established by lexical references; their existing main inventory and individual pipelines remain independent work items.

## Verification and estimate for the first slice

Before implementation, agree final exact path/level selector and array boundary adapter (JSON strings are proven; adding global array converters is unnecessary). Then native compile and actual registration/omitted-default probes; no broad generic executor counted as Actor migration.

Fresh map verification: five native spawn types; unique exact object name distinct from human label; list and case/empty substring search; partial transform update preserving omitted fields; missing/ambiguous/duplicate/invalid finite-vector errors; mutation replay and uncertain-outcome no-rerun; deletion after snapshot with absence and selection accounting; correct level dirty state, explicit one save, new-process reload, deleted actor remains absent. Counterexamples should include label-only matches, same object name in different levels, no root component, locked/unsupported level, readback failure and journal failure before dispatch. Supported-path success must not erase unsupported-path evidence.

Estimated engineering size, not a calendar promise: 6 public wrappers + 1 compatibility alias; shared native world/resolver/snapshot core; 3 core mutations; 1 author/reload pipeline + independent verifier and failure tests. This is larger than Custom because world/level ownership and persistent actor identity need new core proof. Runtime duration depends on build/editor restart/map persistence; do not infer a speedup from reducing tool calls without measurements.
