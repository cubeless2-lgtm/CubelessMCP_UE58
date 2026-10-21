#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraModuleLibrary.generated.h"

/** Internal Editor-only stock-module stack operations. Registration stays in
 * Python; generated assets reference the stock NiagaraScript, never this class. */
UCLASS()
class UCubelessNiagaraModuleLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Find-only finite selector/stack observation; no compile/save or asset load. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString InspectNiagaraStockModuleTarget(const FString& SystemPath, const FString& ModuleScriptPath,
        const FString& TargetUsage, const FString& TargetUsageId, const FString& TargetEmitterIndexJson,
        const FString& TargetEmitterName);
    /** Existing stack only; one public native insertion, no rollback. Async
     * compile requested at most once; outstanding compilation defers saving. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString AddNiagaraStockModuleToStack(const FString& SystemPath, const FString& ModuleScriptPath,
        const FString& TargetUsage, const FString& TargetUsageId, const FString& TargetEmitterIndexJson,
        const FString& TargetEmitterName, const FString& TargetIndexJson, const FString& SuggestedName,
        bool bSkipIfDuplicate, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
    /** Ordered finite RapidIteration batch. No graph override/User parameter alias.
     * Blocking waits and save-while-pending remain explicit unsupported scopes. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="set_niagara_module_inputs_batch"))
    static FString SetNiagaraModuleInputsBatch(const FString& SystemPath, const FString& EditsJson, const FString& Operation,
        bool bOverwriteExisting, bool bContinueOnError, bool bAllowSourceEdit, bool bSave, bool bRequestCompile,
        bool bWaitForCompletion, double TimeoutSeconds, double PollIntervalSeconds);
    /** Loaded-only bounded current RI and compile-state observation, never a writer. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="inspect_niagara_module_rapid_iteration"))
    static FString InspectNiagaraModuleRapidIteration(const FString& SystemPath);
};
