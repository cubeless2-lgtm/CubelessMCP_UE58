#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
    FString AddBlueprintMakeArrayNode(const FString& BlueprintPath, const FString& ElementType,
        const FString& InputCountJson, const FString& ValuesJson, const FString& TypeObject,
        const FString& EnumType, const FString& NodePositionJson, const FString& GraphName,
        const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
}
