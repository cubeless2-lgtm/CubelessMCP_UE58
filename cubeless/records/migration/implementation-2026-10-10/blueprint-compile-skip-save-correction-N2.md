# Blueprint compile automatic-save correction N2

2026-10-10. Existing correctness defect found during the next component mutation API investigation. This is a source fix; its specific SaveOnCompile preference counterexamples require new runtime evidence.

`Private/Compatibility/UE58/CubelessEditorApi.cpp` previously called FKismetEditorUtilities::CompileBlueprint with EBlueprintCompileOptions::None, then reported saved only from its explicit bSave branch. Installed UE5.8 `Engine/Source/Editor/Kismet/Private/BlueprintCompilationManager.cpp:357,459–489` evaluates SkipSave; absent that flag, EditorSettings SaveOnCompile Always/SuccessOnly can save compiled assets after compilation. Thus compile-only requests could write disk while reporting saved=false. Earlier tests under their recorded editor configuration do not establish behavior under other preferences.

N2 always supplies EBlueprintCompileOptions::SkipSave, including when bSave=true. The helper's existing branch saves the requested Blueprint once only after compiler validation succeeds. The public method signature and three legacy Python wrapper identities are unchanged. No persistent user/editor configuration or engine source is changed. Do not infer absence of arbitrary third-party callback side effects from this flag; it controls the engine compilation-manager save path.

Actual source call chain: FKismetEditorUtilities::CompileBlueprint in UnrealEd/Private/Kismet2/Kismet2.cpp:798–807 invokes FBlueprintCompilationManager::CompileSynchronously. The latter flushes compile/reinstancing queues, performs GC and broadcasts before building PackagesToSave. SkipSave is a save policy control, not an asynchronous compilation API. Compilation remains synchronous and its duration must be measured honestly.

Required isolated evidence, owned by the root executor:

1. Record the isolated process/project and initial UBlueprintEditorSettings SaveOnCompile value. Test the current/default value, Always and SuccessOnly independently with fresh disposable Blueprint fixtures.
2. Change only the isolated process settings CDO using verified public reflection. Do not call SaveConfig or persist preferences. Restore the captured value in a finally path and report whether restoration was observed.
3. Before each compile-only request, make a known unsaved local change and record file hash/mtime, dirty state, node/property values and diagnostics. Compile with bSave=false through the legacy public wrapper, then assert compiler outcome and file bytes/mtime unchanged while unsaved change remains dirty. A new unsaved asset is also useful: no .uasset may appear.
4. For bSave=true under each mode, check compiler diagnostics and the one explicit save stage; compare the saved independent values and perform a cold reload. Instrumented save-call count and final disk state are different evidence and should both be recorded.
5. If practical, test a compiler-error fixture: no explicit save should occur even when SaveOnCompile=Always. Do not provoke engine fatal paths or alter user assets.

The old bug and source/frozen evidence remain visible. This correction does not complete deferred component add/set functionality or alter their legacy compile semantics.
