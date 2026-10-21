#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::PCGComponentGeneration
{
    // Original Native raw contracts only. Named external wait/fallback remain Python compatibility concerns.
    FString RefreshComponents(const FString& ParamsJson);
    FString BindGenerationSettings(const FString& ParamsJson);
}
