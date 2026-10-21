#pragma once

#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::StateTreeRead
{
Variable::FResult List(const FString& Directory, bool bRecursive, int64 MaxResults, int64 Offset);
Variable::FResult Inspect(const FString& AssetPath, int64 MaxStates);
}
