# X: guarded replacement of one occupied execution link

Status: the initial read-only investigation below led to an implemented narrow X contract. Root subsequently built and executed it on UE 5.8.3; see the root evidence section at the end. W sources are archived separately, original UE 5.6 and Epic sources remain unchanged. Full legacy connection/host parity is still incomplete.

## Finding: current source and historical pipeline contract differ

Original root: `C:/Git/UnrealMCP5_6_1`; inspected HEAD `1e74770bf54be3df54247da07d06a161ccf56637`. Working source SHA256:

- `Python/tools/node_tools.py`: `97542378C2FF1380048F6F1A9AF7C5C6D82924306612E854336B3C0C796E6CA5`.
- `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintNodeCommands_Part1.cpp`: `5EAB87D22258206E7ECE29EA9DC2EFD94C44054458AE5A3E57318908AA2BF113`.

The current public wrapper at `node_tools.py:791–820` exposes required blueprint/source node+pin/target node+pin; optional graph_name/id/type, `allow_pin_link_replacement=False`, `compile=False`, `save_asset=False`. It does **not** expose any `expected_*` argument. The active route at `Part1.cpp:35` calls `HandleConnectBlueprintGraphPins`; its complete implementation `:1229–1423` does not read any expected-link/schema guard or return `replacement_guard_verified`.

However, real pipeline sources pass the four expected fields through raw commands and require guard success. Examples: `Python/scripts/analysis/build_cube_fluid_ninja_mouse_timer_host.py:21–23`, `build_cube_fluid_ninja_mouse_event_host.py:38–41`, and `execute_cube_fluid_ninja_live_functions.py:2207–2238`. The latter binds source and old destination GUIDs in the command plan. Its test `test_execute_cube_fluid_ninja_live_functions.py:401–426` explicitly asserts the missing C++/public-wrapper markers, so this is an observable source/test inconsistency, not evidence that the current implementation enforces the contract.

`Docs/Analysis/CubeFluidNinjaLive_NinjaLiveFunctions_CleanRoom.md:1602–1624` documents a real motivating failure: default FunctionEntry.then→FunctionResult.execute scaffold caused `break_others_a` at the first intended authoring edge. Its promised remedy requires source count exactly one, exact reciprocal old destination, schema BREAK_OTHERS_A, old≠new, old destination input count exactly one, then new reciprocal edge present and old reciprocal edge absent. Required results include `replacement_guard_verified`, `expected_existing_link_removed`, and `replaced_source_link_count=1`.

Read-only `git log --all -S expected_existing_source_link_count -- '*.cpp' '*.h'` found no matching implementation in the available Git refs; the same search restricted to active Part1 also returned none. No claim is made about unreachable Git objects, another workstation, unrecorded dialogue or historical binaries. Original files were not repaired. X should implement the recoverable **documented pipeline contract**, while recording that current source does not contain its enforcement.

No `expected_existing_target_link*` input was found in the inspected original `.py/.cpp/.h/.md/.json` files. Do not invent these as historical public arguments. The old destination's reciprocal-count requirement comes from the document above; the **new** destination's empty-input requirement below is an explicit safety restriction for X.

## Complete active handler branches

| Branch | Current source behavior |
|---|---|
| Required blueprint/name/node/pin missing | Error before graph mutation. PIE/SIE is rejected. Blueprint, selected graph, both nodes, output/input pins and schema must resolve (`Part1.cpp:1231–1302`). |
| Input defaults | `allow_pin_link_replacement=false`; `compile=false`; `save_asset` falls back to raw `save=false` (`:1304–1306`). Public Python forwards save_asset only. |
| Already linked | Tests target.LinkedTo contains source. It skips dispatch and unsafe-response rejection, but still runs requested compile/save (`:1308–1338`). Reciprocal symmetry is only required by final readback. |
| Not linked, safe schema response | Dispatch permitted. Installed UE58 `CanSafeConnect()` accepts MAKE **and promotion**, so that predicate must not be treated as “no structural side effects.” Exact 5.6 enum behavior should not be inferred from 5.8 alone. |
| Not linked, unsafe schema response | If replacement permission is false or response is DISALLOW: error with schema message and pin snapshots (`:1315–1323`). With permission true, other responses can reach TryCreateConnection, including replacement/conversion; no expected-old-edge comparison exists. |
| Dispatch | Modifies source/target pins, calls schema TryCreateConnection, returns a bare error on false without a new graph readback; on true notifies graph and marks Blueprint modified (`:1326–1338`). |
| Compile | Optional AnimNodeProbeCompileBlueprint and its success result (`:1352–1365`). |
| Save | Optional SaveLoadedAsset is attempted even if requested compilation failed (`:1367–1375`). This is not the desired new failure policy and must not be silently claimed equivalent. |
| Verification | Re-resolves selected graph, node IDs and pins; requires new reciprocal link plus requested compile/save success. Counts source/target links, but does not establish which previous edges disappeared or preserve unrelated edges (`:1377–1403`). |
| Result | Legacy mutation_result_v1, already_connected, schema_response/message, before/after link counts, delta updated/skipped=1, failed from postcondition (`:1340–1349,1405–1423`). |

The separate older `connect_blueprint_nodes` wrapper (`node_tools.py:732–783`) and handler (`Part1.cpp:1120–1226`) support a permission bool but lack graph-pin handler's compile/save and final mutation contract. They remain separate public identities; a shared implementation must not count one alias as two independently proven behaviors.

## UE 5.8 public API and target-side effects

Paths below are relative to `C:/Program Files/Epic Games/UE_5.8/Engine/Source/`.

- `Runtime/Engine/Classes/EdGraph/EdGraphSchema.h:43–55,465–467,775,828` declares exact response enums, CanSafeConnect, CanCreateConnection and exported virtual TryCreateConnection.
- `Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:2052–2093` chooses BREAK_OTHERS_A when A is an occupied **exec output**. An occupied **data input** produces BREAK_OTHERS_B in the usual output-A/input-B order, except multiple-self special cases. An exec input may have other incoming links without forcing B replacement. Therefore BREAK_OTHERS_A alone says nothing about whether the new exec target already has incoming flow.
- `EdGraphSchema_K2.cpp:2173–2291` also rejects invalid/same nodes/orphaned pins, consults node connection policies, checks direction/type, and may return promotion/conversion. X must require the exact standard K2 schema and exact response, not a localized message or boolean compatibility result.
- `Runtime/Engine/Private/EdGraph/EdGraphSchema.cpp:474–526` recomputes the response, performs `BreakAllPinLinks(true)` before `MakeLinkTo` for A/B/AB, and notifies the new endpoint nodes. `EdGraph/EdGraphPin.cpp:705–720` notifies each old linked node during break. Callbacks can run between removal and addition. There is **no atomic rollback** guarantee.
- `EdGraphSchema_K2.cpp:2294–2305` marks the owning Blueprint modified after successful dispatch; no explicit compile/save is done here.
- Public reflected `BlueprintGraphPin.TryCreateConnection` (`Editor/BlueprintEditorLibrary/Private/BlueprintEditorLibrary/BlueprintGraphPin.cpp:92–102`) uses the default K2 schema. Its `CanCreateConnection` (`:105–115`) reduces every non-DISALLOW response to true and is insufficient for replacement guards. This also means a custom graph schema must not be planned through its override and then executed through a different default schema.

All required comparison and mutation APIs are public. No private engine-header import, source patch, manual LinkedTo-array assignment or generic execution tool is needed. The resulting links join existing standard graph nodes and introduce no Cubeless runtime asset/type/function reference.

## Proposed narrow X contract

Implement the named legacy `connect_blueprint_graph_pins` surface with its source_pin/target_pin names and graph selectors, forwarding to the existing graph operation core. Preserve existing `connect_graph_pins` positional arguments, defaults, operation identity and journal fingerprint for ordinary calls. Add replacement options at the end or only to the legacy-named adapter; do not turn a formerly safe call into implicit replacement.

Recover these historically requested optional fields together:

| Field | Initial X interpretation |
|---|---|
| allow_pin_link_replacement | Strict bool; true alone is insufficient. |
| expected_existing_source_link_count | Strict int exactly 1 for this one-edge cohort; bool is invalid. |
| expected_existing_source_link_target_node_id | Required bounded exact GUID or canonical node-path selector, resolved by the existing node resolver in the same graph. |
| expected_existing_source_link_target_pin | Required exact non-orphan input pin name. |
| expected_schema_response | Exact stable value `break_others_a`; translated from the public enum, never localized message matching. |

Reject a partial expected-field group, expected fields with permission false, and unguarded permission true **before dispatch**. Broader permissive legacy behavior is intentionally not reproduced and remains outside full parity. Normal unoccupied MAKE and existing-link no-op behavior retain their current path. When guards are present, validate them before treating a requested link as already connected: replay of a completed operation ID returns its journaled result, but a **new** ID with stale old-edge expectations must fail unchanged, not falsely claim a verified replacement.

For replacement, require:

1. Exact loaded, normal/idle owning Blueprint and standard K2 graph, game thread, no PIE/SIE; current existing graph budgets and GUID/path resolution.
2. Source output, requested new input and expected old input are standard compatible exec pins in that graph; old and new differ. No data replacement, conversion, promotion, wildcard or custom schema in X.
3. Source has exactly one outgoing reference, the exact expected old input; both directions agree. Old input has exactly one incoming reference, this source. Reject malformed one-sided/duplicated links; do not repair them.
4. **New input has zero incoming links.** This explicitly narrower rule prevents unexpected exec fan-in even though stock schema allows it. Other old/new target pins and unrelated graph connections may exist and must be preserved.
5. Current schema response is exactly BREAK_OTHERS_A. Repeat these endpoint checks immediately in the native dispatch helper after durable Python checkpoint; the helper must not accept stale caller-provided counts as truth.

Do not auto-reconnect the old path from a Sequence output inside this single-edge tool. The pipeline explicitly performs that next named connection. A later failure can leave an incomplete intended chain and must stop/report it; a batch is not an atomic transaction.

## Common-core reuse and failure stages

- Reuse `operations/graph.py:244–294`: existing graph selection, canonical node/pin resolution, full before/after snapshot, `_edges()` with both directions, `_contract`, standalone mutation journal and batch checkpoint callback. Extend its internal action instead of adding an unrelated graph mutation framework.
- Reuse `compatibility/ue58/graph.py` node/pin/graph APIs. `CubelessGraphApi.cpp:165–191` already supplies exact schema enum, source/target counts, types and reciprocal already-linked result. An additive response-name field is useful; existing fields and ordinary MAKE behavior stay intact.
- A small hidden native guarded replacement helper should share the loaded graph/pin resolver and compare all three endpoints immediately before public `TryCreateConnection`. Return `mutation_attempted`, `change_state_known`, changed bool/null, old/new reciprocal observations and actual schema result. Python owns the full-graph durable checkpoint and independent after snapshot; do not duplicate the full graph traversal natively.
- Stages: `preflight` → `checkpoint` → `replace_exec_link` → `readback` → optional explicit compile/save → `complete`. Checkpoint failure means no dispatch. Before dispatch, record intent without claiming an actual change; after dispatch set unknown until re-observed. A false engine return or exception must still attempt readback and preserve available partial observations; do not immediately discard them in an error wrapper.
- After condition: exactly remove both directed representations of source↔old and add both representations of source↔new. Old input has zero links, source/new each have the intended one; node/GUID set, coordinates, pin identities/types/defaults and all unrelated edges remain unchanged. Keep actual removed/added sets and affected endpoints in data. API true alone is not verified success.
- Distinguish known partial mismatch from unobservable state. Failure can have changed=true/known=true when the actual delta is available, or changed=null/known=false when endpoints/after snapshot cannot be resolved. Neither is retryable automatically. No Undo/transaction/compensation is described as rollback.
- Preserve legacy expected guard/result keys as data, including `replacement_guard_requested`, `replacement_guard_verified`, `expected_existing_link_removed`, `replaced_source_link_count`, expected old node/pin and schema decision. Do not misuse the older delta `removed_count=0` to hide a removed edge: retain its node-oriented compatibility meaning if needed and add explicit edge counts/sets with clear units.

Compile/save options must be accounted for explicitly. The immediate host pipeline requires both false; initial implementation can reject true with a clear pending-option error if integration cannot remain small. If included, reuse `operations/blueprint.py:93` compilation/SkipSave orchestration within the same owning operation and checkpoint, then re-resolve identities after compilation. Save-only requires its own confirmed shared save path/PreSave guard; do not call the compile helper merely to satisfy save=true. Never save after a failed requested compile. These differences must be tracked against the original raw handler rather than hidden under a parity claim.

## Representative root verification

Use one owned W clone: inspect a known existing exec edge, request replacement with a stale old endpoint and require unchanged graph/dirty state, then use the correct expected edge to insert Sequence/gated flow and explicitly reconnect the old branch. Check the exact three-endpoint state and full expected edge delta. The same operation ID should replay; a new ID with stale expectations should reject without reapplying. Compile/save once after the authored chain is complete, and reuse the existing gated timer runtime proof where behavior changes. Data replacement, multi-edge source, occupied new target, A/B/AB alternatives, and full old host integration remain explicitly unverified/unsupported until deliberately expanded.

No new verification framework, project or packaging pass is proposed. Root owns build, official registration and all actual engine execution.

## Native X candidate contract (source implementation, not runtime proof)

`CubelessGraphLibrary.replace_exec_link(graph_path, source_node_path, source_pin_name, target_node_path, target_pin_name, expected_old_target_node_path, expected_old_target_pin_name, expected_source_link_count, expected_schema_response)` is an internal `BlueprintCallable, AIIgnore` helper. The new `CubelessConnectionOperations` serializes the new UE58 `CubelessConnectionApi`; no new public generic tool or runtime module is introduced. Existing Function/Sequence implementations and Build.cs are unchanged.

The native implementation checks the already-loaded graph, existing parent guard plus audited Engine/CoreUObject native ancestor modules, idle Blueprint state, direct graph ownership and exact standard K2 schema. Source/old/new pins must be unique named live pins with valid identities and plain equal exec types. Expected count 1 and response `break_others_a` are mandatory. The only dispatched edit API is `Schema->TryCreateConnection`, once. Its public pin methods perform their own `Modify` and K2 marks the Blueprint modified; the helper does not add redundant Modify/Notify/compile/save calls.

The JSON envelope contains `success`, `stage`, `error_code`/`error` on failure, `mutation_attempted`, `change_state_known`, `changed` (boolean or null), `engine_return` (boolean or null before dispatch), `postcondition_passed`, and explicit-operation `compiled=false`, `saved=false`. `data.before` and `data.after` contain source/old_target/target endpoint identities, types and actual linked_to entries, plus graph GUID/count/schema and Blueprint dirty/status. Each link records reciprocity. Endpoints are resolved again after the API returns, including when it returns false; original pin pointers are not dereferenced after node callbacks. Missing/invalid/reconstructed-away identities yield incomplete readback and unknown changes. A fully observable mismatch retains its actual endpoint state and reports failure. Native `change_state_known` is explicitly restricted to these observations; Python owns full-graph preservation verification.

Argument errors use `connection_replacement_guard_invalid`; stale counts/targets/reciprocity/schema use `connection_replacement_guard_mismatch`. API false, incomplete readback and postcondition mismatch have distinct error codes. `replacement_guard_verified`, `expected_existing_link_removed`, `replaced_source_link_count`, `try_create_connection_count`, endpoint identity/type preservation and exclusive new-link checks are retained under data. No rollback/retry is provided. `ReadConnectionPlan` only adds stable `schema_response_name` and `exact_standard_k2_schema`, preserving all existing fields.

This is a source candidate until root completes build and actual official-MCP execution. Broader data/target replacement, conversion/promotion, occupied new targets and arbitrary native parent modules remain unsupported; arbitrary callback side effects are not exhaustively measured by endpoint observations.

## Root execution evidence: X, 2026-10-11 KST

The prior candidate status above is historical. `build-guarded-links-X1.log` passed in 18.65 seconds and `python-contract-tests-X.log` passed 101 tests in 4.019 seconds. The new named Python tool shares the ordinary connection core while preserving the previous `connect_graph_pins` signature and journal fingerprint. False API returns now retain independently observed partial state; lost responses remain uncertain even if graph readback succeeds. Focused offline checks include this distinction and historical journal replay; they are not injected engine failures.

`registered-schemas-and-diagnostics-X.json` records 8 Toolsets/54 public tools and 121 plugin files at isolated PID39104. There are 28 explicit batch capabilities. `source-snapshot-X.json` preserves the matching root/deployed source before Y work; binaries are hash-only. The native replacement helper remains hidden behind the named Python operation.

`blueprint-guarded-links-X.json` passed on a fresh clone of the W3 fixture. A wrong old endpoint failed unchanged; the correct source1/old1/new0 exec replacement succeeded; identical-ID replay did not repeat it; a new ID with the now-stale old edge failed unchanged. A separate direct connection restored the original flow, followed by one full compile and save. Final caller: 11 nodes/12 physical edges. Source, other graphs and defaults stayed unchanged. Single-trial loopback NullRHI observations: 25 named calls/34 HTTP, named-call sum 1.4361454 seconds, whole pipeline 2.3835839 seconds, excluding editor startup. These are no speedup estimate.

`actual-codex-guarded-links-X.json` confirms the actual connector schema, named existing-link no-op, exact saved graph, dependency audit and clean editor. A preliminary shell comparison lost Korean labels in Windows stdout encoding; reading the same UTF-8 report with ASCII JSON transport resolved that comparison without repeating an engine mutation.

`gated-timer-runtime-X.json` passed six cases on a SHA-matching saved copy in a fresh no-MCP PID23012: timer 0/0/0/.25/.75/.75, always-path LastDelta, CDO/file/Config/dirty preservation. Runtime body was 0.1356369 seconds; explicit compile/save zero, implicit load work unmeasured. `guarded-links-verification-X.json` joins 11 checks. This proves fresh no-MCP load and caller execution, not official-connector cold reload or Shipping. The full original host, inherited Tick, data replacement, conversion/promotion and connection compile/save=true options remain incomplete. No source-repository or engine edits, Git commit or push were performed.
