#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::PCGInspection
{
FString ResolveGraph(const FString& ParamsJson);
FString ListNodes(const FString& ParamsJson);
FString InspectCustomHlsl(const FString& ParamsJson);
FString InspectComponentOutputs(const FString& ParamsJson);
}
