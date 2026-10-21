#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraEmitterConfiguration
{
FString Inspect(const FString& ParamsJson);
FString SetEnabled(const FString& ParamsJson);
FString SetLocalSpace(const FString& ParamsJson);
FString SetSimTarget(const FString& ParamsJson);
FString SetFixedBounds(const FString& ParamsJson);
FString SetPersistentIds(const FString& ParamsJson);
FString Remove(const FString& ParamsJson);
}
