#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::MaterialCollectionParameters
{
Variable::FResult Replace(const FString& MaterialPath, bool bUseRuntimeValues, bool bSave);
}
