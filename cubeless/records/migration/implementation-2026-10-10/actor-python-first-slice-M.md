# Actor Python first slice: implementation contract, not runtime completion

Date: 2026-10-10. Original repository remains read-only. The inventory and UE58 API investigation are recorded in `editor-actor-port-plan-K.md` and `editor-actor-source-inventory-K.json` in this directory. Those sources cover all 35 public editor tools; this implementation covers only the six entries below plus the native-only alias. The remaining 29 public tools are still pending.

## Files and registration boundary

Three new files separate the public registry class, domain actions and installed-engine bindings:

- `Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58/registration/actor_toolset.py`: `CubelessActorTools`, seven individually named tools.
- `.../operations/actor.py`: input validation, shared action cores, durable journal and partial-result semantics.
- `.../compatibility/ue58/actor.py`: six positional calls into the authorized editor-only `CubelessActorLibrary` native helper being implemented separately.

`registration/__init__.py` has deliberately not been changed. No Actor tool is yet advertised as registered, callable, UE behavior verified, saved/reloaded, or packaged. The six positional bindings were reconciled with the newly written `Private/Registration/CubelessActorLibrary.h/.cpp` before Python source freeze; the native build and actual reflection still require verification. No game module, content asset, native Source, existing operation, or batch allowlist was changed by this Python implementation.

## Legacy mapping and explicit boundary changes

| Legacy Python/public name | Native UE56 route/handler | New public entry | Preserved semantics and explicit differences |
| --- | --- | --- | --- |
| `get_actors_in_level` | `get_actors_in_level` / `HandleGetActorsInLevel` | same | Loaded editor-world actor name/class/location/rotation/scale list, including loaded sublevels. Bounded at 10000 loaded actors; no claim to list unloaded WP cells. Explicit editor world replaces ambiguous `GWorld`. |
| `find_actors_by_name` | `find_actors_by_name` / `HandleFindActorsByName` | same | Case-insensitive object-name substring and empty-pattern match-all. Does not silently substitute display-label matching. |
| `get_actor_properties` | `get_actor_properties` / `HandleGetActorProperties` | same | The source actually returns the five identity/transform fields, despite its broad description. Adds actor/world/level paths. Does not claim arbitrary reflected property enumeration. Exact case-sensitive object name; duplicate names across loaded levels fail instead of choosing the first. |
| `spawn_actor` | `spawn_actor` / `HandleSpawnActor` | same | Five original native classes. Zero XYZ/rotation, unit scale defaults. Original wrapper uppercasing versus mixed-case C++ dispatch defect is fixed through one case-insensitive canonical map. JSON-string `location_json`/`rotation_json` replace reflected arrays. `scale_json` exposes native scale support as an additive public argument. |
| no public wrapper | `create_actor` / `HandleSpawnActor` | `create_actor` | Distinct named alias and durable operation identity, same spawn action core/native helper. This alias is not counted as a sixth/seventh separate spawn implementation. |
| `delete_actor` | `delete_actor` / `HandleDeleteActor` | same | Exact object-name lookup and independent absence check. Native partial result retained. Structured errors replace the old wrapper's occasional empty dictionary on exception. |
| `set_actor_transform` | `set_actor_transform` / `HandleSetActorTransform` | same | Independently optional world location, Pitch/Yaw/Roll, scale. JSON string `"null"` preserves each component. All-null becomes verified no-op, instead of calling a redundant setter. Transform readback tolerance is 0.01 in native core, subject to actual verification. |

Common additions: optional nullable `expected_level_path`, and mutation-only nullable `operation_id`. Nonempty string JSON defaults avoid the official UE58 empty-string default metadata issue and the reflected-array converter issue. The scalar parameters are genuinely strings; callers must not pass a JSON array directly. Actual JSON null for a vector parameter is not the string `"null"` and is not an accepted contract. `expected_level_path` and `operation_id` intentionally accept omission or null.

All results use the existing structured `CubelessPythonOperationResult`: operation state and errors are top-level; exact legacy payload and expanded native observations are JSON in `data_json`. Legacy transport nesting such as `unreal_response` is not reconstructed. This schema difference is explicit, not a claim of byte-for-byte protocol parity.

## Initial mutation scope and incomplete behavior

The native agreement restricts spawn/delete/transform to a single saved `/Game` persistent level that is also the current level, without streaming levels, World Partition or external actor packages. Delete/transform initially admit exact instances of the five original native classes only. BP subclasses, other native classes, templates, child/attached actors, editor-only targets, rootless or locked targets are unsupported until their side effects and ownership behavior are ported and verified. Reads remain available for the loaded editor world. Transform components are finite with magnitude at most 1e12 (native rendering/editor bound); each public JSON vector string is at most 512 UTF-16 code units. Object selectors are at most 1023 units, search patterns at most 4096. Native checks are authoritative; Python does not guess engine class flags.

No explicit compile, map save, reload, viewport manipulation or selection is requested by Python. Native deletion may update editor selection/layer bookkeeping; native evidence must expose those actual effects. All modifications are in-memory until a separately authorized explicit save. Undo/editor transactions are not represented as atomic rollback. Runtime dependencies cannot be introduced by these wrappers: the first spawn types are native Engine actor classes, and no Cubeless class/function is added to an asset.

## Shared core, journal and performance

`spawn_actor` and `create_actor` call the same `_spawn` and `_spawn_action_from_arguments`; their journal fingerprints retain distinct operation names. Reusing an ID across aliases is a conflict. Transform and delete also expose action cores for a future explicitly reviewed batch allowlist. These actions do not invoke `mutate` themselves, so a future root batch can provide its checkpoint callback without nested leases. Actor support is not currently added to batch.

Each standalone mutation validates/normalizes inputs once, acquires the shared mutation lease and exclusive durable journal, captures existing state, writes a durable pre-dispatch checkpoint, then calls the native API once. The native API must revalidate its public boundary and read back the result; those checks are not removed as superficial duplicates. When the caller omits its level guard, the inspected persistent-level package is still pinned for native dispatch. A mismatch fails before dispatch.

Spawn's journal preserves world/level identity, loaded count and exact-name collision matches without persisting the entire unrelated actor list. This reduces JSON journal volume while retaining the relevant precondition; no measured runtime speedup is claimed. Native postcondition snapshots remain separate. Get/list/read operations do not acquire mutation journals or save/compile assets.

An identical completed request ID returns the historic result with `replayed=true` and does not call even the snapshot API. A started journal blocks automatic re-execution. If the process ends after the pre-dispatch checkpoint, that stored stage is only the last durable observation, not the actual crash stage or proof that no mutation happened. Mutation-call exceptions or malformed native results preserve an attempted/unknown change entry and before snapshot. Explicit native preflight failure can establish `mutation_attempted=false`; known partial failure preserves observed changes. Compile/save remain not requested. Journal checkpoint failure prevents dispatch.

## Validation and pending engine proof

`cubeless/tools/verification/test_actor_python_contracts.py` runs 22 synthetic-native tests against the real journal implementation. These cover exact six positional bindings and seven schemas, vector rejection before dispatch, five-class case normalization, partial transforms, no-op results, alias identity/replay/conflict, lease exclusion, durable compact before snapshots, checkpoint I/O failure, native preflight rejection, known partial failure, missing postcondition, malformed post-dispatch result, unknown basic change-state rejection, read errors, level guard mismatch, and interrupted-journal no-retry behavior. This is offline contract evidence only; it does not prove engine API behavior.

Required next steps: native header/build review; explicit registration activation; actual official registry schema and omission tests; isolated saved test-map authoring (five spawn types, transform/read/delete, name-vs-label, missing/duplicate/unsupported/level guards); mutation result/postcondition checks; explicit map save followed by a fresh editor process reload; test-map/reference dependency inspection. Existing runtime-independence evidence for other fixtures cannot be relabeled as Actor proof.
