#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::ChaosGeometryAuthoring
{
FString Create(const FString& ParamsJson);
FString ConfigureFracture(const FString& ParamsJson);
FString ConfigureAnchor(const FString& ParamsJson);
}
