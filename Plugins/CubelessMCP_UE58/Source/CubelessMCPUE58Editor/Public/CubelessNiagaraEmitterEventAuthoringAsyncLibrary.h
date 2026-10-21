#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraEmitterEventAuthoringAsyncLibrary.generated.h"
class UCubelessNiagaraEmitterEventAuthoringJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraEmitterEventAuthoringAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_set_niagara_emitter_persistent_ids_job"))
    static UCubelessNiagaraEmitterEventAuthoringJob* StartSetNiagaraEmitterPersistentIdsJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_add_niagara_event_handler_job"))
    static UCubelessNiagaraEmitterEventAuthoringJob* StartAddNiagaraEventHandlerJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_add_niagara_grid_simulation_stage_job"))
    static UCubelessNiagaraEmitterEventAuthoringJob* StartAddNiagaraGridSimulationStageJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_set_niagara_simulation_stage_settings_job"))
    static UCubelessNiagaraEmitterEventAuthoringJob* StartSetNiagaraSimulationStageSettingsJob(const FString& ParamsJson);
};
