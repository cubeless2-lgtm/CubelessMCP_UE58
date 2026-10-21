#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
FString AddBlueprintMapFunctionNode(const FString& BlueprintPath, const FString& Operation,
    const FString& KeyType, const FString& ValueType, const FString& ParamDefaultsJson,
    const FString& NodePositionJson, const FString& GraphName, const FString& GraphId,
    const FString& GraphType, bool bCreateGraphIfMissing);
}
