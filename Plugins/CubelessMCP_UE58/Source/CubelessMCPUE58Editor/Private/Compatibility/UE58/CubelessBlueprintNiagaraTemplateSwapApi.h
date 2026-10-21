#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::BlueprintNiagaraTemplateSwap
{
    FString InspectTemplate(const FString& ParamsJson);
    FString DryRunSwap(const FString& ParamsJson);
    FString ExecuteCanary(const FString& ParamsJson);
    FString ExecuteDurable(const FString& ParamsJson);
}
