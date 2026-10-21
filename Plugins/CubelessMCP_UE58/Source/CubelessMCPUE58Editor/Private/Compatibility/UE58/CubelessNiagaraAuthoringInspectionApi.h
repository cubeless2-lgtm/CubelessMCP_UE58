#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraAuthoringInspection
{
FString InspectStack(const FString& ParamsJson);
FString InspectGraph(const FString& ParamsJson);
FString InspectGraphActions(const FString& ParamsJson);
FString InspectCustomHlsl(const FString& ParamsJson);
FString InspectScratchInterface(const FString& ParamsJson);
FString InspectCompileStatus(const FString& ParamsJson);
}
