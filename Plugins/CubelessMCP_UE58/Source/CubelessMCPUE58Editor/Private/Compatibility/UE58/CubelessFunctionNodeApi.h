#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::FunctionNode
{
// Original function-node9 adapter; the existing validated AddCall core owns node work.
Function::FResult Add(const FString& BlueprintPath, const FString& Target, const FString& FunctionName,
    const FString& ParamsJson, const FString& PositionJson, const FString& GraphName,
    const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
}
