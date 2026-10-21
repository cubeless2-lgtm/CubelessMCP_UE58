#include "Operations/CubelessMakeArrayOperations.h"
#include "Compatibility/UE58/CubelessMakeArrayApi.h"
#include "Operations/CubelessFunctionOperations.h"

namespace Cubeless::Operations
{
FString AddBlueprintMakeArrayNode(const FString& BlueprintPath, const FString& ElementType,
    const FString& InputCountJson, const FString& ValuesJson, const FString& TypeObject,
    const FString& EnumType, const FString& NodePositionJson, const FString& GraphName,
    const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing)
{
    return EncodeFunctionResult(UE58::MakeArray::AddNode(BlueprintPath, ElementType,
        InputCountJson, ValuesJson, TypeObject, EnumType, NodePositionJson, GraphName,
        GraphId, GraphType, bCreateGraphIfMissing), TEXT("add_blueprint_make_array_node"));
}
}
