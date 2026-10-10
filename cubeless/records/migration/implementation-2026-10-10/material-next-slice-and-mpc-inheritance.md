# Next native Material slice and UE58 MPC inheritance review

2026-10-10. Read-only design; **no Source edits**. Build F2 remains frozen for runtime testing. Original Material public tool count is 21: first slice 9, Add/Set native slice 2, other tools 10. A native implementation count is not a completed migration or branch-coverage count.

## First priority: make inherited MPC inspection explicit

Installed engine source establishes that UE58 Material Parameter Collections can have a `Base` and private scalar/vector override maps. Public access exists without a private-field workaround:

| Evidence in installed UE58 source | Meaning |
| --- | --- |
| `Runtime/Engine/Public/Materials/MaterialParameterCollection.h:138` | Exported `GetBaseParameterCollection()` returns the direct base. |
| `Runtime/Engine/Private/Materials/ParameterCollection.cpp:620` | The getter returns `Base`. |
| `ParameterCollection.cpp:734` | `GetParameterNames` recursively includes base names before local names. The public scalar/vector name getters call it at lines 542/549. |
| `ParameterCollection.cpp:555,586` | Scalar/vector default getters search the chain and consult derived overrides, unlike reading a base parameter struct's `DefaultValue`. |
| `ParameterCollection.cpp:625,658,762,780` | ID/name and typed parameter lookup traverse the base chain. |
| `ParameterCollection.cpp:1047–1078` | World-instance getters resolve values through the instance API; their result is a world-instance observation, not a guarantee that all inherited overrides or rendered output are correct. |

These paths are under `C:/Program Files/Epic Games/UE_5.8/Engine/Source/`. The finding is verified against this installed version; this record does not claim which earlier engine release introduced inheritance.

Current migrated source only enumerates the queried collection's own arrays in `CubelessMaterialLegacy.inc:2353,2385`. Inherited values can therefore disappear from `get_material_parameter_collection_values` while it reports success. `list_material_collection_parameter_nodes` similarly scans only local arrays at lines 2028/2042 and can incorrectly report an inherited node parameter as missing or mismatched. These are unimplemented UE58 compatibility branches, even if base-free fixtures pass.

`list_material_nodes` and `analyze_material_graph` serialize graph/property/reference observations rather than a complete effective MPC-value evaluation. They do not need to reject all valid graph structure because a referenced MPC is derived. Their response must, however, explicitly identify that effective collection values/parameter-ID validation were not performed. Analyze of a Material Instance must inspect its base material graph for this qualification. Traversal of nested Material Functions is a separate coverage scope and must not be inferred from direct-node inspection.

### Immediate admission option

Before a broader inherited-values implementation, reject `get_material_parameter_collection_values` when its canonical target has `GetBaseParameterCollection()!=nullptr`, with `success=false`, `stage=preflight`, `error_code=mpc_inheritance_not_supported`, `base_collection`, and no incomplete `scalars`/`vectors` result represented as a success. For `list_material_collection_parameter_nodes`, inspect referenced collections before legacy validation and use the same explicit unsupported outcome rather than emit false missing/ID-mismatch counts. Preserve the observed node/reference identities in diagnostic data.

Additive metadata for general graph reads can use:

```json
{
  "ue58_collection_semantics": {
    "status": "not_evaluated",
    "scope": "direct_graph_collection_references",
    "inherited_collections": [
      {"collection": "/Game/...", "base_collection": "/Game/..."}
    ],
    "effective_values_verified": false,
    "nested_function_collections_verified": false
  }
}
```

This does not change existing tool input schemas. Python must preserve the native error/metadata. Pure graph-structure readback may still pass, while effective MPC verification remains incomplete.

### Preferred subsequent effective-values implementation

1. Walk the public base getter iteratively with a visited set and explicit maximum chain depth (e.g. 32) **before** calling recursive engine getters. Apply the collection parameter budget to the whole unique chain, not just local arrays. Invalid cycles/duplicate ambiguous names are explicit errors.
2. Obtain effective scalar/vector names through exported public getters. Resolve type and stable GUID with public typed parameter lookup/`GetParameterId`; record the declaring collection separately from the queried collection.
3. Obtain `asset_default` through `GetScalarParameterDefaultValue`/`GetVectorParameterDefaultValue` with the found flag checked. A base struct default must not substitute for the derived effective default. Keep legacy fields and add `base_chain`, `declared_in`, `inherited`, and `asset_default_semantics="effective_at_queried_collection"`.
4. For requested runtime readings, call the current explicit world's collection instance API and report its exact return/readback. Keep asset-default and world-instance values distinct. Verify inherited override behavior with actual UE58 fixtures before claiming they coincide with rendered uniform-buffer data.
5. Validate a collection expression's name/type/ID against the effective lookup, not the local arrays. Report whether the name exists, whether the ID matches, and the declaring collection. Node JSON should never silently rewrite a stale ID.
6. Test a three-level base chain, inherited scalar and vector values, derived override, same-name/type ambiguities or sanitized names, stale expression ID, base-free regression, budget/cycle preflight and include-runtime false. Save/reload fixtures in a new process and compare API results. Any engine-internal inheritance behavior that differs remains documented, not hidden by a fallback.

## Next new-tool shortlist

Two candidates are recommended after the existing inspection gap above is addressed. They are suggestions, not approval to modify the frozen source.

1. **`add_custom_material_node`** — original handler lines 5196–5297 and Python schema lines 346–397. Its specialized input/define/include validation and direct `UMaterialExpressionCustom` field assignment can reuse the new Add-node boundary and cleanup/evidence handling. Preserve code/output type/input names/description/position/defines/includes; validate and bound all lists/text before creation. Reject unaudited plugin shader includes. Keep graph creation/readback separate from shader compile success. Proposed hidden helper: `AddCustomMaterialNode(MaterialPath, Code, OutputType, InputsJson, Description, GraphType, NodeX, NodeY, AdditionalDefinesJson, IncludeFilePathsJson, MaxGraphNodes) -> FString`. Public schema preserves the legacy named inputs and adds the established optional operation ID only. This is the clearest next bounded authoring tool.

2. **`set_material_parameter_collection_values`** — original handler lines 6200–6450 and Python schema lines 668–702. First establish correct inherited reads. A first write slice can then explicitly admit base-free existing parameters, validate all values before writing, retain the original asset/runtime/save booleans, and report unsupported inherited targets. Asset defaults need a proper `PreEditChange`/`PostEditChange` pair, exact parameter/name/ID readback, explicit world identity and one save attempt after durable before-save checkpoint. Runtime-only changes use exported `UMaterialParameterCollectionInstance::SetScalarParameterValue`/`SetVectorParameterValue`, followed by exact getters. No save for runtime-only updates. Proposed hidden phases: validated apply plus independent save-once helper, so Python can journal between them. The public tool retains `collection_path`, `scalars`, `vectors`, `set_asset_defaults=True`, `set_runtime=True`, `save=True`, and operation ID. A runtime-only subset must not silently ignore default `set_asset_defaults=True`/`save=True` arguments.

MPC value mutation is not free of global editor effects: `ParameterCollection.cpp:224` records previous uniform-buffer storage in `PreEditChange`; line 262 branches on storage changes; lines 284–347 recompile/dirty loaded material consumers and update worlds when layout changes; lines 348–366 update derived collections/world instances even without layout changes. The legacy setter calls `PostEditChange` without a matching `PreEditChange`; copying that sequence would risk a spurious layout-change path. The convenient collection default setters at header lines 98–108 are not marked `ENGINE_API` on a `MinimalAPI` class; do not assume they link from the project module. The instance setters are exported. A value-only tool must not create/delete parameters or modify GUIDs, and tests must capture additional dirty packages/world effects.

Two conditional follow-up candidates are `replace_material_texture_references` and `replace_material_collection_references`. Both have useful bounded replacement cores but the original tools also explicitly compile and optionally save. Their full contracts must wait for the compile/save lifecycle below. Publishing their no-compile core under the full legacy name and counting it complete would be incorrect.

## All ten remaining public tools

| Tool | Main remaining requirement |
| --- | --- |
| add_custom_material_node | Specialized validated node builder, exact graph readback, shader includes/class dependency guard. |
| set_material_parameter_collection_values | Effective inherited read foundation, bounded parameter-only changes, world and save evidence. |
| mirror_material_parameter_collection | Parameter/ID conflict and inherited semantics; structural changes can dirty/recompile every loaded consumer; not a small isolated mutation. |
| replace_material_collection_references | Replacement identity validation plus original compile/save contract and function-consumer scope. |
| replace_material_collection_parameters | Node replacement/consumer rewiring/runtime-value choice plus original compile/save. |
| replace_material_texture_references | Texture type/reference validation, replacement snapshot and original compile/save. |
| compile_and_save_material | True async lifecycle and resource proof; no synchronous finish disguised as async. |
| refresh_material_cached_expression_data | Original casts away const and mutates cached private-style data; public equivalent and cache/dependency proof required. |
| expand_material_function_calls | Complex duplication/rewire/defaults/cycle budget, partial cleanup and save semantics; original rollback claims require independent proof. |
| set_material_parameter_collection_sync | Persistent ticker ownership, weak assets/world/session changes, shutdown/cancel and journal/job semantics; one-shot mutation wrapper insufficient. |

## Async compile/save prerequisite

`async-material-compile-plan.md` remains the detailed source-based design. Official `UToolCallAsyncResultString` is a genuine supported result path, but the existing legacy `RecompileMaterial`/`FinishCompilation` sequence blocks. Public engine compile submission can itself perform synchronous translation/cache continuations; some scenarios force synchronous compilation. Full `compile_and_save_material` parity is therefore not established by a generic async return type.

An acceptable implementation needs explicit submission eligibility, public resource polling, strong promise lifetime with weak asset references, a mutation lease and per-asset serialization, durable before-submit/after-compile/before-save/completion records, one save attempt only after verified completion, and no automatic replay after uncertain outcomes. Transport cancellation does not currently prove cancellation of a native task. Own cancellation/deadline should prohibit saving and preserve an observable pending/unknown record without canceling shared shader jobs. Material Functions need caller/preview scope separately; a missing resource (including NullRHI) remains unknown. The new F2 `ue58_compile_observation` helps measure actual resource status but is not itself an async compiler or proof of shader output.
