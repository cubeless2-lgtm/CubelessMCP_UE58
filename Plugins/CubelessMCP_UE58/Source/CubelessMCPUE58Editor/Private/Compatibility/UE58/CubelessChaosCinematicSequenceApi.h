#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::ChaosCinematicSequence
{
    FString CreateSequence(const FString& ParamsJson);
    FString BindCache(const FString& ParamsJson);
    FString ConfigureTiming(const FString& ParamsJson);
    FString InspectPipeline(const FString& ParamsJson);
}
