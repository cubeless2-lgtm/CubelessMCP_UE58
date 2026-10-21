#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraGraphCleanupAsyncLibrary.generated.h"
class UCubelessNiagaraGraphCleanupJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraGraphCleanupAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_remove_niagara_modules_job"))
    static UCubelessNiagaraGraphCleanupJob* StartRemoveNiagaraModulesJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_remove_niagara_parameter_map_set_nodes_job"))
    static UCubelessNiagaraGraphCleanupJob* StartRemoveNiagaraParameterMapSetNodesJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_remove_niagara_orphan_parameter_map_get_nodes_job"))
    static UCubelessNiagaraGraphCleanupJob* StartRemoveNiagaraOrphanParameterMapGetNodesJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_remove_niagara_simulation_stage_job"))
    static UCubelessNiagaraGraphCleanupJob* StartRemoveNiagaraSimulationStageJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_remove_niagara_scratch_pad_script_job"))
    static UCubelessNiagaraGraphCleanupJob* StartRemoveNiagaraScratchPadScriptJob(const FString& ParamsJson);
};
