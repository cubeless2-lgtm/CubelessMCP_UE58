#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraSimulationStageLibrary.generated.h"

/** Hidden Editor-only operations. Authored stage/script/nodes are native Niagara
 * objects and never reference this class or another Cubeless runtime symbol. */
UCLASS()
class UCubelessNiagaraSimulationStageLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString PrepareStageToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString InspectNiagaraSimulationStageTarget(const FString& SystemPath,
        const FString& EmitterIndexJson, const FString& EmitterName);
    /** Exact legacy Grid2D stage creation. Options are finite validated JSON;
     * compilation is asynchronous and a pending compile defers the exact save. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString AddNiagaraGridSimulationStage(const FString& SystemPath,
        const FString& EmitterIndexJson, const FString& EmitterName, const FString& StageName,
        const FString& DataInterfaceName, const FString& InsertIndexJson,
        bool bSkipIfDuplicate, bool bEnabled, const FString& OptionsJson,
        bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString InspectNiagaraSimulationStageSettingsTarget(const FString& SystemPath,
        const FString& EmitterIndexJson, const FString& EmitterName,
        const FString& StageIndexJson, const FString& StageName);
    /** Presence-aware existing-stage patch; no allocation, blocking wait or
     * unconditional RequestRecompile. Async compile may defer exact save. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString SetNiagaraSimulationStageSettings(const FString& SystemPath,
        const FString& EmitterIndexJson, const FString& EmitterName,
        const FString& StageIndexJson, const FString& StageName, const FString& OptionsJson,
        bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
};
