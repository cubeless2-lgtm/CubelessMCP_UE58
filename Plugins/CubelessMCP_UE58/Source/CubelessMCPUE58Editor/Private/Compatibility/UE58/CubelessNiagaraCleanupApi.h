#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraCleanup
{
FString RemoveModules(const FString& ParamsJson);
FString RemoveMapSetNodes(const FString& ParamsJson);
FString RemoveOrphanMapGetNodes(const FString& ParamsJson);
FString RemoveUserParameters(const FString& ParamsJson);
FString RemoveSimulationStage(const FString& ParamsJson);
FString RemoveScratchPadScript(const FString& ParamsJson);
}
