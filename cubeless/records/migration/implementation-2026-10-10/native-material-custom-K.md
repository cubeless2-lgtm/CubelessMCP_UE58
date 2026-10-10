# Specialized Custom Material node: native slice K

Status: implemented; root's build K passed (UBT 16.74 seconds, executor 12.58 seconds, `build-material-K.log`). Official MCP execution remains pending at this update. Python registration, operation/journal wrapper and offline tests are owned by the Python agent; external fixture and independent verifier by the operations agent. No native code is counted as migrated merely because it compiles.

## Existing core reused

`UCubelessMaterialLibrary::AddCustomMaterialNode` is a named `BlueprintCallable` + `AIIgnore` helper, not an additional directly registered MCP tool. Python adds the named public legacy tool. The hidden positional contract is:

```text
add_custom_material_node(material_path, code, output_type, inputs_json,
    description, graph_type, node_x, node_y, additional_defines_json,
    include_file_paths_json, max_graph_nodes) -> native JSON envelope
```

The public wrapper documents JSON-string adapters for legacy collection/Any fields. Defaults remain code/material required, output `CMOT_Float3`, empty inputs/defines/includes/description, auto graph and zero position. This boundary is an explicit schema adaptation, not an unreported identical-signature claim.

The new `AddCustomNode` operation performs specialized preflight and then shares the existing Add-node path: exact `/Game` Material/Function resolution, graph budget, runtime Engine class/reference checks, CDO-based property conversion before creation, undo/durable-journal boundary, one creation, assignment/readback, failed-new-node removal, independent after capture and postcondition checks. Failure cleanup proves only observed removal from the graph; no atomic package rollback is claimed. Existing Add/Set/collection read behavior is retained.

Specialized parameters become seven exact properties: Code, OutputType, Description, Desc, Inputs, AdditionalDefines and IncludeFilePaths. Input/define arrays are first built as native typed `FCustomInput`/`FCustomDefine` arrays. Their public reflected array properties export the real values as UE text, which the shared property converter imports and verifies. User strings are not interpolated into handwritten UE array import syntax. Empty arrays use UE's equivalent `()` import. Combined encoded properties must still fit the Add core's 65536 UTF-16-unit budget.

After all Custom fields are assigned, public `UMaterialExpressionCustom::RebuildOutputs` runs once; an existing GraphNode is reconstructed once. This follows the local output/pin updates in installed `MaterialExpressions.cpp:12842-12894` while avoiding the Super PostEditChangeProperty path. The inherited Custom defer path remains in use. No explicit compile/save occurs. Engine create/dirty/graph notifications can still have engine-side effects; the native envelope's compile/save semantics remain unchanged.

The independent after check verifies exact converted property values plus Custom input count/name/address and a single rebuilt output. That is loaded graph proof, not shader validity, shader-job completion, render behavior, Material Function caller compilation, disk reload or package proof.

## Legacy provenance and preserved validation

`CubelessMaterialCustomLegacy.inc` extracts nine parsing helpers (292 original lines) from `C:/Git/UnrealMCP5_6_1/MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPMaterialCommands.cpp`. Source SHA256 is `8c41e028d0f6b27e0d5c2d85cad37216bab8d2234e61189c812e6a463b0d5a91`; the source hash was rechecked after extraction and the original repository was not changed. Original comments remain; no license notice existed in that source file. The JSON-object key iteration is adapted to UE58 FStringType/ToView.

The original HLSL identifier/keyword rejection, case-insensitive duplicate input/define checks, output aliases, input string or `name`/`input_name` object forms, define `name`/`define_name` and `value`/`define_value` forms are retained. Define number/bool conversion preserves the original SanitizeFloat and `True`/`False` text semantics; this is not proof that every macro value is valid HLSL in every usage.

## Explicit limits and incomplete branch

- Code: nonempty after trim, at most 65536 UTF-16 code units. Description: at most 4096.
- Each JSON string: the existing one-value parser limit 65536. Arrays: inputs 64, defines 64, include entries 32. Input names must be shorter than the engine's NAME_SIZE=1024 before FName construction. The combined encoded property limit may reject an otherwise individually valid large input before creation.
- Coordinates: int32 at the reflected native boundary; Python rejects booleans/fractions/out-of-range values before dispatch.
- **Include/preprocessor support remains incomplete.** Nonempty include paths and any directive marker (`#`, `%:`, `??=`) in Code or define-value text are rejected before asset capture/creation with `custom_shader_includes_unsupported`, conservatively including markers inside comments/strings. This deliberately rejects Code-local `#define` as well as includes; ordinary AdditionalDefines name/value entries remain available. It is not an HLSL preprocessor. Shader virtual directory mapping/transitive include dependency validation has not been implemented. The tool must not be counted as full legacy parity while these branches remain unsupported.
- The same class-specific check runs on converted Custom Code/AdditionalDefines/IncludeFilePaths and input/output names in generic Add/Set before assignment. Using the generic tool cannot bypass this restriction. Existing unsafe values are not silently rewritten, and unrelated fields such as Desc are not blocked because of an existing Code value. Generic failures retain their existing preflight error envelope; shader-include functionality remains explicitly incomplete in those branches too. A separate known generic FName/raw-import error boundary is recorded in `generic-material-fname-boundary-K.md` and is not fixed by this shader guard.
- Native adapter/parser/identifier/output failures use `custom_material_input_invalid`, stage `preflight`, `mutation_attempted=false`. Shared target/property failures retain the existing Material error codes. No uncertain mutation is retried.

Successful native data adds `created_custom_node`, `input_count`, `requested_output_type`, `custom_outputs_rebuilt`, `custom_graph_node_reconstructed`, `custom_shader_include_scope` and `custom_shader_code_compiled=false` alongside existing expression details. The native `postcondition_passed` comes from after readback, not merely the engine creation return value.

## Required root proof

Build/registry/schema/omitted defaults, invalid identifier/reserved names, case-insensitive duplicate names, invalid output aliases, JSON-type and size rejections, explicit unsupported includes, default float3/no-input creation, named inputs/defines/description/position, exact official property readback, connections, separate compile/save and a new-process reload. Compile success with NullRHI/no material resource must retain the existing unknown shader-resource scope. Any failed author run is reconciled rather than automatically retried.

No Build.cs dependency or engine/gameplay source changed for this slice. The original Material public total stays 21: this is the twelfth named implementation candidate, with nine still unimplemented and include/other unverified branches still tracked separately.
