# Material array defaults: UE58 compatibility slice I

Status: implemented and independently reviewed. Root's build I passed (`build-material-I.log`, UBT 17.91 seconds, executor 13.01 seconds). Actual official MCP validation remains root-owned and pending at this update; the build does not establish omitted-argument behavior.

## Scope and separation

The official `FToolsetRegistry::RegisterConverter` API registers one converter named `CubelessMCP_UE58.MaterialToolArrayParameters.v1`. Its predicate matches only the generated owner class `/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/material_toolset_PY.CubelessMaterialTools`, exact input function/property names, `CPF_Parm` without output/reference flags, and the exact array inner type:

- `add_material_node.node_position`: `FArrayProperty<FIntProperty>`, exactly two int32 numbers, default `[0,0]`.
- `get_material_parameter_collection_values.parameter_names`: `FArrayProperty<FStrProperty>`, default `[]`.

All other properties are unhandled. There is no global array override, new public MCP tool, gameplay/module dependency, engine-source edit, extra compile/save call, or Build.cs dependency. Python signatures remain plain lists. `Compatibility/UE58/CubelessMaterialArrayConverter.*` owns UE property/schema adaptation; `Registration/CubelessJsonConverters.*` owns registry lifetime; `Registration/CubelessRegistrationLibrary.*` prepares the generated class before its first cached tool-call schema is built.

The expected generated owner path follows installed `PyUtil.cpp:1416-1495`; the preparation report includes the actual path. Actual startup validation must compare it, rather than assume the source-derived path is runtime proof.

## Why both the converter and preparation are needed

Installed `JsonSchemaGeneratorEditor.cpp:278-415` handles only nonempty default text and has no built-in array default branch. The public custom callback is consulted first for supported nonempty defaults. `PropertyToDefault` parses UE text with `FArrayProperty::ImportText_Direct` into initialized scratch storage, checks complete consumption, exports the real element values, and validates their type and range. `PropertyToJsonSchema` also reads declared default metadata; it does not invent defaults for a parameter that has none.

Empty arrays export an empty string (`CoreUObject/Private/UObject/PropertyArray.cpp:1041-1120`), and empty string imports as an empty array (`:1164`). Merely placing a default on the full property schema is insufficient: `JsonSchemaGenerator.cpp:291-298` selects required inputs from editor metadata before processing that schema. The hidden `PrepareMaterialToolset` helper therefore confirms the exact class, both registered converter matches, both parameter types, declared defaults `[0,0]` and `[]`, and absence of an already-cached Material toolset. Only then it changes the in-memory `CPP_Default_parameter_names` value from empty to `()`. A preexisting `()` is accepted; any other nonempty string or different default fails without normalization. No UObject asset is saved.

The Python bootstrap must call this helper after class creation and before registry registration, and block Material registration if its structured `success` is false. If refreshing an existing Material registration, Python must explicitly unregister that class before preparation and then register it again so execution schemas are rebuilt. This is not an engine-wide reload guarantee.

## Actual call path and validation

`ObjectFunctionToolCall.cpp:293-299` caches the function schema in its constructor. Its required check (`:532-558`) and default injection (`:187-250`) use that cached schema before `JsonDataToStruct`. A fresh description alone cannot prove cached execution is fixed.

For non-null JSON arrays, every element is checked before any output argument storage is modified. Coordinates reject wrong length, booleans, fractions, nonfinite values and values outside int32. Names reject non-string elements. Validated input is filled into initialized scratch storage and copied to the call argument only after all checks. No generic JsonUtilities/toolset re-entry is used, avoiding recursion and another module dependency.

Important installed-engine limit: `JsonObjectConverter.cpp::JsonAttributesToUStructWithContainer` skips an explicit JSON null field before the property converter is called. A converter rejects null when invoked, but cannot promise every public call with null reaches it. The schema excludes null; MPC explicit null may arrive as the initialized empty array. This remains an observed-source limitation to probe and report separately; it is not counted as strict null rejection. Invalid declared defaults are blocked by bootstrap preparation, not trusted to a JSON-null fallback.

## Ordering and ownership

The editor module registers its converter at PostEngineInit before native toolset registration. G2 actual startup evidence was `isolated-material-array-G2.log:1742` native startup at 09:39:13.073, `:1844` init_unreal at 09:39:14.056, `:1855` Material tool registration at 09:39:14.093. I must confirm the same sequence with the new converter/preparation log.

The module retains its converter and a weak pointer to the original registry subsystem. Shutdown removes its registered converter from that subsystem if UObject state is still available, then releases its handles. It does not load another subsystem during shutdown. Official duplicate-name registration is refused; map ordering provides no converter priority. Preparation checks that both inputs resolve to our converter rather than silently relying on priority. Dynamic DLL reload and foreign interference with our unique converter name are outside this verified lifecycle.

## Required actual verification

1. Build I and clean editor startup; confirm converter registration precedes Python registration and preparation success includes the observed owner path.
2. Describe both inputs: array item types/coordinate constraints, defaults, and absence from required. Confirm unrelated toolset array schema remains unchanged.
3. Omitted position on a new isolated Material produces a node at `[0,0]`; explicit coordinates still work. Omitted MPC filter and explicit `[]` return the same effective parameter values; named filter returns only selected values.
4. Reject malformed non-null arrays before operation/journal/asset changes. Probe explicit null separately and record the installed engine limitation.
5. Read back the created graph and preserved MPC values, then use the existing save/reload pipeline. Successful schema description alone is not completion.

Independent review by the Python implementation agent checked owner/property/type matching against actual Python-generated reflection source, empty-default required ordering, metadata mutation scope, and JSON null skipping. Root reviewed the scoped converter and preparation before build.
