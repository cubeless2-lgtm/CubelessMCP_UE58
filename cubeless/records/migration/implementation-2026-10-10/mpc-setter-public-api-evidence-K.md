# MPC setter: installed UE58 binary and public API evidence

2026-10-10. Read-only investigation; no engine tool calls, DLL load, asset mutation, plugin source implementation, gameplay changes or engine edits. Reproducible binary probe: `cubeless/records/verification/implementation-2026-10-10/probe_mpc_setter_exports.py`; result: `mpc-setter-binary-probe-K.json`. The result includes exact tool commands, exit status, input file SHA256, stdout hash, line counts and positive/negative symbol matches. Source inspection alone is not the basis for the unavailable-setter conclusion.

## Conclusion

The installed UE 5.8.3 Engine DLL and import library do not expose the named asset-default MPC setters. Generated UHT wrappers expose getters, not those setters. The official MaterialTools class exposes creation/editing of graph assets, not a specialized MPC default setter. **Inherited asset-default overrides currently have no verified callable public API in this installed build.** This is a narrow conclusion, not a claim that all MPC writes are impossible.

Own parameter defaults remain editable through public ScalarParameters/VectorParameters and official ObjectTools, already used in isolated I fixtures. Per-world runtime writes have exported native instance methods and Blueprint/Python MaterialLibrary methods. These paths can support a partial migration with clear ownership and persistence semantics; neither is equivalent to persistent inherited overrides.

## Binary and UHT observations

Probe inputs:

- `Engine/Binaries/Win64/UnrealEditor-Engine.dll`, `dumpbin /nologo /exports` (37086 output lines in the observed run).
- `Engine/Intermediate/Build/Win64/x64/UnrealEditor/Development/Engine/UnrealEditor-Engine.lib`, `dumpbin /nologo /linkermember:2` (110131 lines).
- `Engine/Intermediate/Build/Win64/UnrealEditor/Inc/Engine/UHT/MaterialParameterCollection.generated.h`.

| Boundary | Observed result |
| --- | --- |
| SetScalarParameterDefaultValue / ByInfo | No matching DLL export or import-library symbol |
| SetVectorParameterDefaultValue / ByInfo | No matching DLL export or import-library symbol |
| GetScalarParameterDefaultValue / GetVectorParameterDefaultValue | Exported on UMaterialParameterCollection, including import-library symbols |
| GetBaseParameterCollection | Exported |
| SetScalarParameterValue / SetVectorParameterValue on UMaterialParameterCollectionInstance | Exported |
| Generated UHT callable wrappers | execGetBaseParameterCollection, execGetVectorParameterDefaultValue, execGetScalarParameterDefaultValue, execGetVectorParameterNames, execGetScalarParameterNames; no asset-default setter exec wrappers |

`MaterialParameterCollection.h:77–107` declares a MinimalAPI class; the asset-default setter declarations have neither ENGINE_API nor UFUNCTION. `ParameterCollection.cpp:149–221` contains implementations that update own values or the child's private inherited override maps. Thus the desired logic exists inside Engine but cannot be linked normally from this editor plugin using the inspected DLL/import library, and has no generated ProcessEvent/Python UFUNCTION entry point. We did not perform a new link build: root alone owns builds, and the absence from both callable binary boundaries already narrows the obstacle. Existing arbitrary process-memory lookup, copied engine method definitions or private-field reflection are not acceptable public alternatives.

## Official tools and editor utility search

Installed `Plugins/Experimental/Toolsets/EditorToolset/Content/Python/editor_toolset/toolsets/material.py:66` provides `create_parameter_collection`; its other public functions handle materials/functions/expressions. The source method list and the actual prior I `MaterialTools` registered tool names are captured in the JSON evidence from `material-author-I.json`, not inferred solely from similar names. There is no specialized MPC default write tool in that catalog.

Official `ObjectTools.set_properties` (`object.py:81–97`) calls `ToolsetLibrary.set_object_properties`. `ToolsetLibrary.cpp:114` checks each property through `PropertyAccessUtil::CanSetPropertyValue`; `CoreUObject/Private/UObject/PropertyAccessUtil.cpp:723` rejects properties without CPF_Edit, CPF_BlueprintVisible or CPF_BlueprintAssignable. The MPC own arrays have EditAnywhere. Private ScalarParameterBaseOverrides/VectorParameterBaseOverrides only have bare UPROPERTY; they are not an approved editable route. Calling the generic setter does not convert private map access into a public supported inherited setter.

Search of installed EditorScriptingUtilities and Blutility sources found no MPC-specific asset-default setter. MaterialEditingLibrary's MaterialInstance parameter setters target UMaterialInstanceConstant, a different asset type; names resembling scalar/vector defaults do not establish equivalence. Generated source `.gen.cpp` and PythonStub directories were absent at the inspected expected paths; the report relies on present generated `.h` wrappers and does not pretend an actual Python `dir()` inspection was performed.

## Available paths and their bounds

1. **Own defaults:** direct public editable arrays, preserving existing FName, GUID, order, parameter count and untouched values; call Modify/PreEditChange once for changed arrays and the appropriate PostEditChange event once after updates, then save only the requested collection if changed. Official ObjectTools already authored own scalar/vector arrays in the I Material/MPC fixtures. Replacing a whole array without preserving GUIDs is not the legacy setter contract. A named Cubeless wrapper still needs validation, per-entry results, durable intent, runtime/disk readback and cold reload before it is counted as migrated.
2. **Runtime instance values:** exported UMaterialParameterCollectionInstance setters return bool, with exported getter readback. The implementation (`ParameterCollection.cpp:969–1043`) looks up parameters through GetScalar/VectorParameterByName, which traverse Base (`:762` onward), so inherited parameter runtime overrides are supported in source. `KismetMaterialLibrary.h:29–43` exposes UFUNCTION scalar/vector setters/getters with WorldContext; Python name is `unreal.MaterialLibrary`. These modify per-world values, do not persist asset defaults, and log invalid-name errors instead of returning bool. Prefer native bool helpers plus explicit world identity and actual getter readback. No new runtime write was executed in this investigation.
3. **Inherited persistent overrides:** the real editor Details customization (`Editor/DetailCustomizations/Private/MaterialParameterCollectionDetails.cpp:25–196`) uses friend access to the private maps and invokes change notifications. That UI implementation proves the editor supports overrides, but is not a public script function our plugin can call. Manual use remains possible; UI automation has not been implemented or validated and must not be counted as a migrated direct-engine tool. Requesting an exported/UFUNCTION setter upstream or waiting for a supported engine API is an alternative; engine edits are outside this project scope.

Changing the ancestor's own default is not a substitute for a child override: it affects siblings and exceeds the target scope. Copying inherited parameters into a child own array changes ownership/GUID/layout semantics. Removing the Base relationship or flattening a collection is also not the setter contract. These are not automatic fallbacks.

## Proposed bounded implementation contract

Keep the original named tool, dictionary-to-JSON input adapters, default flags and per-entry result fields. Preflight all entries to distinguish own, inherited and missing names before any write. An inherited persistent override request must fail explicitly before changing any asset until an approved public setter is available; do not silently convert it to runtime-only. `set_asset_defaults=false, set_runtime=true` can admit inherited runtime names after source and runtime verification. `set_asset_defaults=true` can initially admit own names only. Partial valid-entry legacy behavior must be documented separately from whole-request unsupported preflight, with actual changed counts/errors/save states and no rollback claim.

Current mutation policy rejects PIE/SIE; the legacy tool chose PIE then editor world. First implementation should state editor-world-only support or add a deliberately reviewed runtime session policy; it must not silently imply PIE parity. No-world cases must distinguish asset-only updates from runtime failures. `PreEditChange` captures PreviousTotalVectorStorage (`ParameterCollection.cpp:224–229`); `PostEditChangeProperty` may rebuild uniforms, change StateId and affect referencing materials if layout changed. A value-only wrapper must preserve layout and report implicit engine work separately from verified shader completion. The old bSave default only saves when asset defaults changed; runtime-only requests should not claim saved asset state.

The full MPC setter remains **not implemented**. Persistent inherited writes and PIE support are explicit incomplete portions, while own-default and editor-world runtime paths have public implementation candidates. Root can choose that bounded slice without claiming full UE56/UE58 inheritance parity.
