#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
    FString InspectLoopNativeSupport(bool bAllowStockMacroLoad);
    FString AddBlueprintLoopNode(const FString& BlueprintPath, const FString& LoopType, const FString& FirstIndexJson,
        const FString& LastIndexJson, const FString& NodePositionJson, const FString& GraphName,
        const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing,
        const FString& ElementType, const FString& TypeObject, const FString& EnumType);
}
