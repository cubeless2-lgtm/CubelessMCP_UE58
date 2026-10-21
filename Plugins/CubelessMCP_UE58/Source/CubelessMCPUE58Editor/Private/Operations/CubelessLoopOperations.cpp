#include "Operations/CubelessLoopOperations.h"
#include "Compatibility/UE58/CubelessLoopApi.h"
#include "Operations/CubelessFunctionOperations.h"

namespace Cubeless::Operations
{
FString InspectLoopNativeSupport(bool bAllowStockMacroLoad)
{ return EncodeFunctionResult(UE58::Loop::InspectNativeSupport(bAllowStockMacroLoad), TEXT("inspect_loop_native_support")); }
FString AddBlueprintLoopNode(const FString& BlueprintPath, const FString& LoopType, const FString& FirstIndexJson,
    const FString& LastIndexJson, const FString& NodePositionJson, const FString& GraphName,
    const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing,
    const FString& ElementType, const FString& TypeObject, const FString& EnumType)
{
    return EncodeFunctionResult(UE58::Loop::AddNode(BlueprintPath, LoopType, FirstIndexJson, LastIndexJson,
        NodePositionJson, GraphName, GraphId, GraphType, bCreateGraphIfMissing, ElementType, TypeObject, EnumType), TEXT("add_blueprint_loop_node"));
}
}
