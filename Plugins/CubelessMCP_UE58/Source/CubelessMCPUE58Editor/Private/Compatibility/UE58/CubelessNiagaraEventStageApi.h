#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::NiagaraEventStage
{
    FString InspectSimulationStages(const FString& ParamsJson);
    FString InspectEventHandlers(const FString& ParamsJson);
    FString AddEventHandler(const FString& ParamsJson);
}
