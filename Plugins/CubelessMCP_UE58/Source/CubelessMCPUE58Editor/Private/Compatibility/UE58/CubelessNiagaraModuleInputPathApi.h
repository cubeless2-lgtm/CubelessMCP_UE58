#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraModuleInputPaths
{
FString RemoveUnusedPath(const FString& ParamsJson);
FString BindUserParameter(const FString& ParamsJson);
FString BindArrayUserParameter(const FString& ParamsJson);
}
