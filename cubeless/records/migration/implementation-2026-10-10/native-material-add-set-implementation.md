# Material Add/Set native implementation — 2026-10-10

## Status and scope

Two legacy Material operations now have native UE 5.8.3 implementations: `add_material_node` and `set_material_node_property`. Incremental build F2 passed (8.99 seconds, `cubeless/records/verification/implementation-2026-10-10/build-material-F2.log`). Python registration and actual official-MCP tests are owned by the corresponding runtime/registration agents and are not certified by this implementation record. This is **not an assertion of completed migration**. The original Material toolset has 21 public tools; the preceding nine-tool slice and these two implementations leave ten other public tools outside this implementation slice. Individual branches of these two operations also remain restricted or unverified, as detailed below.

The pre-existing nine Material tool input contracts remain unchanged. The actual registration architecture remains Python `CubelessMaterialTools` named tools → hidden reflected `UCubelessMaterialLibrary` functions → operations → UE58 compatibility implementation. The native library is a `UBlueprintFunctionLibrary` with `BlueprintCallable`/`AIIgnore` helpers, not a new public generic MCP dispatcher. The plugin module is Editor-only. `Build.cs` is unchanged by this slice.

## Native adapter contract

The reflected Python bindings have the following positional arguments, with all return values JSON strings containing the existing native Material outcome envelope:

```python
unreal.CubelessMaterialLibrary.add_material_node(
    material_path, expression_class, graph_type,
    node_x, node_y, selected_asset, properties_json, max_graph_nodes)

unreal.CubelessMaterialLibrary.set_material_node_property(
    material_path, node_id, property_name, value_json,
    graph_type, max_graph_nodes)
```

`properties_json` represents a JSON object; an empty string is treated as an empty object. `value_json` represents one JSON value, including quoted JSON strings or JSON null. Python must encode the legacy `Any` value with `json.dumps`, not pass the string representation of a Python object. Both encoded inputs have a maximum of 65,536 characters; property objects have at most 64 entries. Node coordinates are reflected `int32`; any adapter from the legacy position array must document and validate its conversion. Graph capacity is checked before adding a node. Mutation paths remain canonical `/Game` assets. The usual wrapper limit is 2,048 graph nodes.

No public Python defaults are declared by this document; the registration owner must compare and implement the actual legacy defaults. Before/after snapshots, dirty flags, mutation attempt, changes, postcondition result and failure stage use the existing native envelope. Requested property values additionally have full reflected text readback under `requested_property_readback` so graph snapshots do not conceal scalar-property changes.

## Operations, validation and restrictions

Add uses `UMaterialEditingLibrary::CreateMaterialExpressionEx`. Set prepares a scratch property value using `FDefaultConstructedPropertyElement`; type conversion and dependency checks precede `Modify` and copying into the asset. Postconditions check the new node's identity, class and coordinates, exact node-count delta, or the existing node's retained identity. Reflected `FProperty::Identical` compares actual property values with the prepared requested values after engine updates.

Only concrete native Engine expression classes are admitted in this first slice. All native class ancestors and referenced native classes must be from `/Script/Engine` or `/Script/CoreUObject`. Unknown plugin/native/generated types are rejected as unsupported even if their package lacks `PKG_EditorOnly`; package flags alone do not establish runtime safety. This restriction applies to Add/Set and does not silently change the earlier nine tools' admission rules.

Editable properties are admitted only when `CPF_Edit` is present and edit-const, transient, deprecated, instanced/exported-object flags are absent. Fixed reflected arrays are not supported. Primitive types are checked strictly; numeric values must be finite and integers exactly representable and in range. Enums, strings, text, names, class/object references, GUIDs and supported structured values use their respective reflected type handling. Other import-text paths operate on scratch memory and require complete input consumption. This intentionally narrows the original unrestricted reflected setter. Unsupported properties/types remain incomplete parity items, not removed legacy functionality.

Object references are checked recursively across reflected persistent properties and hard/soft Asset Registry package dependencies, with bounded budgets. Same-graph expression references must have the same owning material/function. External asset references must be runtime-class assets under `/Game` or `/Engine`; plugin mounts, tool/editor paths, editor-only/transient assets and unaudited script modules are rejected. Material Function expressions are inspected explicitly. Asset-reference inspection can synchronously load dependencies. The checks constrain introduced references; they are not a proof of every cook configuration or packaged runtime.

Selected assets are currently supported only for verified engine pairings: texture expressions with textures, Material Function Call with a Material Function, and collection expressions with a Material Parameter Collection. Arbitrary plugin selected-asset behavior is unsupported. Shader text cannot introduce unaudited `/Plugin/` include paths. Custom/external-code graph refresh is deferred using the legacy protection; custom shader correctness is not established by scalar readback.

When Add creates a node and a later property application fails, the implementation attempts to delete the new expression and records its actual absence/presence, the created key and properties applied before failure. `transient_graph_changes_occurred` records that creation happened even when the final graph matches its earlier shape. This cleanup is **not atomic rollback**: dirty state, transaction history and implicit engine side effects may remain. Set does not claim rollback after a failed update. The operations layer records actual before/after snapshots and verification outcomes on failures as well as successes. No automatic retry occurs.

## Compilation observation

`analyze_material_graph` preserves its earlier fields and adds `ue58_compile_observation`. This observes the current editor shader platform resource via public `GetMaterialResource(GMaxRHIShaderPlatform)`, `IsCompilationFinished`, `IsGameThreadShaderMapComplete`, `HasValidGameThreadShaderMap` and `GetCompileErrors`.

The field reports platform, NullRHI state, resource availability, compilation/shader-map completion, errors and scope. Missing resources produce `status="unknown"` with nullable compilation fields, never a positive compilation claim. A Material Function has no standalone material render resource and similarly reports the separate verification requirement. This is current editor platform/resource evidence only; it is not saved/reloaded/cooked/rendered coverage. Polling `IsCompilationFinished` can finalize ready cache work; this is disclosed in the result.

Add/Set contain no explicit recompile, save, `FinishCompilation`, blocking polling loop or wait. Existing engine create/property-update APIs can still trigger implicit editor updates or compilation work, so the envelope continues to disclose `implicit_compile_possible`. The separate asynchronous compile/save design remains research and unimplemented; see `async-material-compile-plan.md`.

## Source and build evidence

The source extraction manifest is `native-material-mutation-extraction.json`. Six required helpers were extracted with original source ranges and provenance into `CubelessMaterialMutationLegacy.inc`; adapted operations and guards are in `CubelessMaterialMutation.inc`. The original UE56 repository was not edited. Its inspected Material command source SHA-256 remained `8c41e028d0f6b27e0d5c2d85cad37216bab8d2234e61189c812e6a463b0d5a91`.

Build F failed on a local enum variable shadow, UE58 JSON shared-string key conversions, and an undefined array serialization helper. The source fixes use actual UE58 `FJsonObject` shared-string keys (`ToView`/`FString` and `TryGetField`); they do not enable the legacy JSON-key compatibility macro. Build F2 passed after these fixes; runtime evidence must be recorded by the runtime owner separately. Material Function, custom-code properties, object-reference success/failure branches, cleanup failure and packaged independence of newly authored materials remain unverified until executed.
