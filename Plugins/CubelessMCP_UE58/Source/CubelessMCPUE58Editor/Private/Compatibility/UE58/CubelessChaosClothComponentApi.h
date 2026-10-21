#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::ChaosClothComponent
{
    // Dedicated operation functions; no command executor or server transport.
    FString Inspect(const FString& ParamsJson);
    FString Configure(const FString& ParamsJson);
}
