# Consolidating the duplicate native Blueprint compiler route

2026-10-10, source prepared for build H. This removes one **new migration-era duplicate registry entry**, not any legacy public Python tool or compiler implementation. Runtime absence/presence and wrapper execution must be verified against the rebuilt DLL in a new editor; a source count alone is not registration evidence.

## Exact source change

`Plugins/CubelessMCP_UE58/Source/CubelessMCPUE58Editor/Private/Registration/CubelessNativeToolset.h` changes only `CompileBlueprintValidation` from `meta=(AICallable)` to `meta=(AIIgnore)` and describes its internal purpose. It remains `BlueprintCallable`, static, with the same name, parameters, defaults, return type and implementation. `GetEditorDiagnostics`, `AuditPackageDependencies` and `GetSkeletalMeshSections` remain public native tools. The previously internal `InspectBlueprintParent` remains unchanged.

The official registry actually supports this separation:

- Installed `ToolsetRegistry/Private/ToolsetRegistry/ToolsetDefinition.cpp:11–14` returns a successful `false` when a UFunction has `AIIgnore`, before checking `AICallable`.
- `FunctionLibraryToolset.cpp:114–126` adds tool calls only when that result is `true`. Direct `call_tool` looks up this registered tool map; no compatibility alias is automatically created.
- Installed `PythonScriptPlugin/Private/PyGenUtil.cpp:1615–1625` determines Python function exposure from BlueprintCallable/Event and other script-exposure flags. It does not exclude `AIIgnore`. Keeping BlueprintCallable therefore preserves `unreal.CubelessNativeToolset.compile_blueprint_validation(...)` for the internal Python adapter.

These are installed UE5.8.3 engine sources, not an assumed metadata convention. Engine files were not changed. Registry hiding is not a security boundary: this helper remains reflected and callable by authorized editor code.

## Preserved public routes and input migration

The three legacy named Python tools remain and still share `operations.blueprint.compile_blueprint` → `compatibility.ue58.blueprint.compile_validation` → the unchanged native helper. They retain durable operation journals, uncertain-outcome handling, native compiler diagnostics and before/after snapshots.

| Former direct native request | Current public route |
| --- | --- |
| `BlueprintPath=P, bSave=False, bRefreshNodes=False` | `CubelessBlueprintTools.compile_blueprint(blueprint_name=P, operation_id=ID)` |
| `BlueprintPath=P, bSave=True, bRefreshNodes=False` | `CubelessBlueprintTools.compile_and_save_blueprint(blueprint_name=P, save=True, operation_id=ID)` |
| Any explicit refresh choice and save choice | `CubelessBlueprintTools.compile_and_validate_blueprint(blueprint_name=P, save=bSave, refresh_nodes=bRefreshNodes, operation_id=ID)` |

Full object paths are accepted as `blueprint_name`. The direct native return was a JSON object string; public Python returns the established structured operation envelope. Compiler-specific fields are in its decoded `data`, with normalized `compile`/`save`/`validation` fields and journal identity. Clients must adapt the envelope rather than assume that the old top-level JSON shape is an alias. No silent automatic retry or alias is introduced.

## Client and verification inventory

Search of current `cubeless/tools/**/*.py` found no direct public `CompileBlueprintValidation`/`compile_blueprint_validation` call to migrate. The sole active plugin Python reference is the intended internal call at `compatibility/ue58/blueprint.py:93`, which remains unchanged.

- `verification/bootstrap_additional_calls.py:205` already calls public `compile_blueprint` and `compile_and_validate_blueprint` and checks the retained native diagnostic fields, refresh behavior, save=false and unchanged file hash.
- `pipelines/blueprint_mutation_smoke.py:245` already calls public `compile_and_save_blueprint` for its compile/save/reload evidence.
- The Blueprint batch allowlist already dispatches those public operations through shared guarded Python actions and never opens the native journal-bypassing route.
- `verification/capture_registration.py` accepts `--expected-count` rather than hard-coding 28. The new expected total is 27. Actual descriptions should explicitly show native three and all Python compiler wrappers before declaring success.

Historical registration/author reports through build G contain the old native tool in their captured schemas, and earlier direct native experiments remain historical evidence. They are not edited to claim the entry was absent earlier. `IMPLEMENTATION_STATUS.md` and other current aggregate reports should be updated by their owner after build H/runtime validation; this change record does not rewrite those shared files.

A static AST/header inventory after the change finds six toolsets: three native tools and 24 Python tools (Diagnostics 2, Blueprint 5, Graph 5, Material 11, Batch 1), total 27. The legacy public Python compile tool count remains three. Remaining migration scope is unchanged by deduplicating one new entry.

## Required runtime checks

1. Build H and copy a consistent source/Python/DLL snapshot to the isolated project, then start a new editor.
2. Official list/describe/search must retain six Cubeless toolsets and 27 unique public tools; native descriptions must include exactly diagnostics/dependency audit/skeletal sections. `CompileBlueprintValidation` must be absent.
3. An explicit old direct call must fail tool lookup without entering the helper or changing assets; compare dirty state and the isolated fixture before/after.
4. Exercise all three public wrappers on isolated fixtures, including no-save/refresh and save-on-success; retain operation IDs, compiler diagnostics, journal replay behavior and cold reload. Successful wrapper execution also proves the retained reflected native helper remains callable from Python.

## Build H companion correctness fix

The runtime owner observed `set_material_node_property(value_json="0.875")` fail preflight in G2 with `mutation_attempted=false`, `changed=false`. UE58 `JsonSerializer.h:495–617` creates a serializer state only for an object or array root; its FJsonValue overload does not admit a standalone primitive as assumed. `CubelessMaterialMutation.inc::ParseMutationJson` now wraps the encoded value in a one-element JSON array, deserializes with the actual array overload, requires exactly one element, and returns that element without changing its type. Object/array inputs continue to work. The reader rejects additional input (`JsonReader.h:74–85`), and comma-separated multiple values are rejected by the exact-one-element check.

This parser fix is independent of the compiler-route metadata change but ships in the same build H. Required actual tests include a scalar number, quoted string, boolean/null type paths, object and array values, malformed/trailing input and multiple comma-separated values. JSON validity does not imply that a particular property accepts that JSON type. Failed G2 mutations are not automatically retried; the runtime owner reconciles retained state and uses a fresh explicit test operation.
