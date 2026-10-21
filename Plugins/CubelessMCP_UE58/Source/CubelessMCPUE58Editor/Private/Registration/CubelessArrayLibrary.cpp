#include "Registration/CubelessArrayLibrary.h"
#include "Operations/CubelessArrayOperations.h"

FString UCubelessArrayLibrary::InspectNativeSupport()
{ return Cubeless::Operations::InspectArrayNativeSupport(); }
FString UCubelessArrayLibrary::AddBlueprintArrayFunctionNode(const FString& BlueprintPath, const FString& Operation,
    const FString& ElementType, const FString& ParamDefaultsJson, const FString& TypeObject, const FString& EnumType,
    const FString& NodePositionJson, const FString& GraphName, const FString& GraphId, const FString& GraphType,
    bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::AddBlueprintArrayFunctionNode(BlueprintPath, Operation, ElementType, ParamDefaultsJson,
        TypeObject, EnumType, NodePositionJson, GraphName, GraphId, GraphType, bCreateGraphIfMissing);
}
