#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraRuntimeInspection
{
FString InspectRuntimeState(const FString& ParamsJson);
FString InspectParameterStore(const FString& ParamsJson);
FString InspectGrid2DRuntime(const FString& ParamsJson);
FString SampleRenderTargetRegionStats(const FString& ParamsJson);
}
