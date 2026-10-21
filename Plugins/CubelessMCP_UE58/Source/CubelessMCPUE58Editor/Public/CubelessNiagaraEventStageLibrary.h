#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraEventStageLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraEventStageLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="inspect_niagara_simulation_stages"))
    static FString InspectNiagaraSimulationStages(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="inspect_niagara_event_handlers"))
    static FString InspectNiagaraEventHandlers(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="add_niagara_event_handler"))
    static FString AddNiagaraEventHandler(const FString& ParamsJson);
};
