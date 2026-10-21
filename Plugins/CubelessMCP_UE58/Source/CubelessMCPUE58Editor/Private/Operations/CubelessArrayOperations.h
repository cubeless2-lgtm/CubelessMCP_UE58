#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
FString InspectArrayNativeSupport();
FString AddBlueprintArrayFunctionNode(const FString& BlueprintPath, const FString& Operation, const FString& ElementType,
    const FString& ParamDefaultsJson, const FString& TypeObject, const FString& EnumType, const FString& NodePositionJson,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
}
