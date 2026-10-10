# UE58 MPC inheritance read correctness

2026-10-10. Implementation is frozen. Build G succeeded: `cubeless/records/verification/implementation-2026-10-10/build-material-G.log`, 9.33 seconds total / 7.06 seconds local executor. Official-MCP runtime verification remains separately recorded; this implementation record does not certify its result. No MPC write tool or Custom-node tool is introduced by this slice.

## Change and exact scope

`CubelessMaterialApi.cpp` includes a new `CubelessMaterialCollectionRead.inc`. The original extracted UE56 helper file is unchanged, preserving provenance. Native registration signatures, Python registration schemas and `Build.cs` are unchanged.

`get_material_parameter_collection_values` now reads effective inherited scalar/vector names, GUIDs and default values. The implementation first walks `GetBaseParameterCollection()` with cycle detection, a maximum depth of 32, and the existing parameter-count budget applied to the whole chain. Invalid/duplicate names or GUIDs are explicit preflight errors. Only after this validation does it call the engine's recursive public name getters, typed lookups and effective-default getters. This prevents a successful response containing only the leaf's local parameters.

Existing fields and filter/include flags are retained. Additions include `base_chain` (queried leaf to root order), `effective_parameter_count_before_filter`, `collection_semantics_version="ue58_effective_collection_v1"`, and per-parameter `declared_in`/`inherited`. The legacy `asset_default` now means the effective value at the queried collection, including derived base overrides, as returned by the public default getter. The implementation does not read or mutate the private override maps. `include_asset_defaults=False` omits this value.

Runtime values remain exact observations from the selected world's `UMaterialParameterCollectionInstance` getter. The response identifies that world and explicitly separates this observation from rendered uniform-buffer proof. It does not substitute an asset default when the instance lookup fails. `include_runtime=False` performs no world-instance lookup and reports unresolved runtime values. Non-finite observed values fail rather than producing invalid JSON.

`list_material_collection_parameter_nodes` resolves each direct CollectionParameter node against the effective typed parameter lookup. It preserves legacy missing/name/ID counts, but valid inherited nodes are no longer falsely counted as missing. Added fields identify declaration origin, base chain and lookup scope. A stale GUID is reported as a mismatch; a read does not repair it. This tool's node scope remains direct `UMaterialExpressionCollectionParameter`, explicitly excluding CollectionTransform semantics and nested function graphs.

General `list_material_nodes` and `analyze_material_graph` add `ue58_collection_semantics`. Direct CollectionParameter and CollectionTransform references have their base chains inspected, including a combined budget across unique chain collections. Material Instance analysis uses its base material graph. Metadata explicitly says that effective values, expression-ID validity, transform semantics, nested function collections and rendered runtime behavior are **not** established by these general graph reads. Invalid direct chains fail preflight; they do not silently produce a complete-looking graph interpretation. Read-before/read-after snapshots incorporate the relevant chain/node observations.

## Installed API evidence

Paths are relative to `C:/Program Files/Epic Games/UE_5.8/Engine/Source/`:

- `Runtime/Engine/Public/Materials/MaterialParameterCollection.h`: exported public `GetBaseParameterCollection`, scalar/vector name and default getters, typed parameter lookup and ID lookup. Private base-override maps remain unaccessed.
- `Runtime/Engine/Private/Materials/ParameterCollection.cpp:555,586`: effective default lookup walks bases and applies derived override maps internally.
- `ParameterCollection.cpp:620,734,762,780`: public base getter, recursive names and typed declaration lookup.
- `Runtime/Engine/Public/Materials/MaterialParameterCollectionInstance.h:39` and `ParameterCollection.cpp:1047–1078`: runtime instance setters/getters are separate from asset defaults.
- `Runtime/Engine/Public/Materials/MaterialExpressionCollectionTransform.h:38`: public referenced Collection field, used only to qualify graph metadata.

## Executable verification fixture

`cubeless/tools/pipelines/mpc_inheritance_smoke.py` is an external client of the existing official MCP connection. It discovers and checks live schemas, then uses only named official MaterialTools/ObjectTools/AssetTools and Cubeless read tools. It does not launch or stop Unreal or host another MCP server.

The script is restricted to the exact isolated `cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject` path and verifies the connected editor's path, UE5.8.3 version, game-thread/PIE state and empty dirty-package set. The real user project is rejected before network access. All four UUID-owned fixture paths must be absent before the first creation. A new `/Game/CubelessMCPTests/MPCInheritance_<uuid>/` folder holds Base, Mid and Leaf collections plus one Material. Base owns scalar/vector parameters, Mid adds a scalar, Leaf adds a vector, and two expressions reference inherited parameters through Leaf.

The script independently reads Leaf's local arrays/Base through official ObjectTools, proving the tested inherited values are absent from Leaf's local arrays. It then checks all chain levels, scalar/vector defaults, inherited declaration paths, parameter GUIDs, include flags, filtering, collection-node ID/name matching and general graph scope metadata. Author performs one explicit save and checks clean state. Reload requires a new editor instance, matches all four saved package SHA-256 hashes, repeats reads, compares exact effective parameter identities/values against the author report, and verifies files remained unchanged. Each stage is recorded before execution and persisted with flush/fsync/replace; mutations are never retried after uncertainty. This is not atomic asset rollback.

```powershell
python -B cubeless/tools/pipelines/mpc_inheritance_smoke.py --endpoint <official-UE-MCP-URL> --expected-project C:/Git/UnrealMCP5_8_3/UnrealMCP5_8_3/cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject --phase author --output <new-author-report.json>
python -B cubeless/tools/pipelines/mpc_inheritance_smoke.py --endpoint <new-editor-official-UE-MCP-URL> --expected-project C:/Git/UnrealMCP5_8_3/UnrealMCP5_8_3/cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject --phase reload --parent-report <author-report.json> --output <new-reload-report.json>
```

The runtime owner chooses the endpoint, launches the new editor and executes these commands. No editor/network execution was performed by the implementation agent.

## Counterexamples and remaining proof

Offline fixture assertions in `test_mpc_inheritance_fixture.py` passed twelve tests (0.006 seconds): valid inherited data; legacy local-only omission; wrong declaring collection; wrong inherited default; duplicate row; zero GUID; runtime observation despite disabled flag; failed native outcome; rejecting the actual user project before client construction; accepting the exact isolated project up to a mocked client; all four files remaining within isolated Content; and traversal/user-asset path rejection. These are validator tests, not native-engine tests.

The initial external fixture draft incorrectly named the real user project in its local admission constant and disk-manifest root. The runtime owner invoked it with the isolated project, so the mismatch failed before network/client construction or asset creation. Both roots were corrected to the isolated project before any allowed execution. The new project/path counterexamples specifically prevent recurrence. This error and correction did not change the native read implementation.

Actual asset-base override values are **not yet runtime tested**. The installed collection's override maps lack Edit/BlueprintVisible flags; official ObjectTools uses `PropertyAccessUtil::CanSetPropertyValue`, which rejects them (`PropertyAccessUtil.cpp:723`). The default setters on the MinimalAPI collection are not reflected and are not marked `ENGINE_API`. The fixture therefore does not claim that private maps are writable through Python, bypass property permissions, or fake override evidence. Existing fixture assets with real derived overrides, an authorized editor UI fixture, or a separately verified public authoring API are needed to exercise that branch. Public effective getter source proves intended implementation behavior but is not a replacement for runtime proof.

Additional unexecuted cases remain explicit: over-depth/cyclic or ambiguous malformed chain rejection; missing/stale node GUID counters; live world runtime overrides; CollectionTransform parameter semantics; nested Material Functions; Material Instance graph metadata; render/cook/package behavior. Corruption fixtures must be isolated and must not rely on engine public setters accepting invalid cycles, since those setters may sanitize the requested data. None of these branches is classified complete by the simple chain fixture.
