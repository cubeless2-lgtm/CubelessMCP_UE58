#include "Registration/CubelessLoopLibrary.h"
#include "Operations/CubelessLoopOperations.h"

FString UCubelessLoopLibrary::InspectNativeSupport(bool bAllowStockMacroLoad)
{ return Cubeless::Operations::InspectLoopNativeSupport(bAllowStockMacroLoad); }
FString UCubelessLoopLibrary::AddBlueprintLoopNode(const FString& BlueprintPath, const FString& LoopType,
    const FString& FirstIndexJson, const FString& LastIndexJson, const FString& NodePositionJson,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing,
    const FString& ElementType, const FString& TypeObject, const FString& EnumType)
{
    return Cubeless::Operations::AddBlueprintLoopNode(BlueprintPath, LoopType, FirstIndexJson, LastIndexJson,
        NodePositionJson, GraphName, GraphId, GraphType, bCreateGraphIfMissing, ElementType, TypeObject, EnumType);
}
