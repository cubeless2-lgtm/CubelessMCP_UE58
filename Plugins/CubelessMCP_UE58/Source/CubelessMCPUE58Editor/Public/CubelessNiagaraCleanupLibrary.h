#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraCleanupLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraCleanupLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Cleanup", meta=(ScriptName="remove_niagara_modules"))
    static FString RemoveNiagaraModules(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Cleanup", meta=(ScriptName="remove_niagara_parameter_map_set_nodes"))
    static FString RemoveNiagaraParameterMapSetNodes(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Cleanup", meta=(ScriptName="remove_niagara_orphan_parameter_map_get_nodes"))
    static FString RemoveNiagaraOrphanParameterMapGetNodes(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Cleanup", meta=(ScriptName="remove_niagara_user_parameters"))
    static FString RemoveNiagaraUserParameters(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Cleanup", meta=(ScriptName="remove_niagara_simulation_stage"))
    static FString RemoveNiagaraSimulationStage(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Cleanup", meta=(ScriptName="remove_niagara_scratch_pad_script"))
    static FString RemoveNiagaraScratchPadScript(const FString& ParamsJson);
};
