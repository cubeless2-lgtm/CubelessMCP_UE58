#include "Registration/CubelessMakeArrayLibrary.h"
#include "Operations/CubelessMakeArrayOperations.h"

FString UCubelessMakeArrayLibrary::AddBlueprintMakeArrayNode(const FString& BlueprintPath,
    const FString& ElementType, const FString& InputCountJson, const FString& ValuesJson,
    const FString& TypeObject, const FString& EnumType, const FString& NodePositionJson,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::AddBlueprintMakeArrayNode(BlueprintPath, ElementType,
        InputCountJson, ValuesJson, TypeObject, EnumType, NodePositionJson, GraphName,
        GraphId, GraphType, bCreateGraphIfMissing);
}
