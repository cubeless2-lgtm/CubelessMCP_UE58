#pragma once
#include "CoreMinimal.h"
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::Loop
{
    /** Catalog only. The optional load is restricted to the exact installed stock asset. */
    Function::FResult InspectNativeSupport(bool bAllowStockMacroLoad);
    Function::FResult AddNode(const FString& BlueprintPath, const FString& LoopType, const FString& FirstIndexJson,
        const FString& LastIndexJson, const FString& NodePositionJson, const FString& GraphName,
        const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing,
        const FString& ElementType, const FString& TypeObject, const FString& EnumType);
    /** Find-only observation of exact GetDataTableRowNames.OutRowNames -> stock ForEachLoop.Array. No dispatch. */
    TSharedRef<FJsonObject> ReadNameForEachConnection(const FString& GraphPath, const FString& SourceNodePath,
        const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName);
    TSharedRef<FJsonObject> ConnectNameForEachPins(const FString& GraphPath, const FString& SourceNodePath,
        const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName);
}
