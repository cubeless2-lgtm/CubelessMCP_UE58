#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraParameterCollections
{
FString InspectValues(const FString& ParamsJson);
FString SetValues(const FString& ParamsJson);
FString RepairDefaultStore(const FString& ParamsJson);
FString RemapSystem(const FString& ParamsJson);
FString RemapScripts(const FString& ParamsJson);
FString RecompileScripts(const FString& ParamsJson);
}
