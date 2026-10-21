#pragma once

#include "CoreMinimal.h"

namespace Cubeless::UE58::BlueprintResolutionConnection
{
    FString Resolve(const FString& ParamsJson);
    FString Connect(const FString& ParamsJson);
    FString ConnectGraphPins(const FString& ParamsJson);
}
