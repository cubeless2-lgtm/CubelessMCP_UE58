#include "Operations/CubelessArrayOperations.h"
#include "Compatibility/UE58/CubelessArrayApi.h"
#include "Operations/CubelessFunctionOperations.h"

namespace Cubeless::Operations
{
FString InspectArrayNativeSupport()
{ return EncodeFunctionResult(UE58::Array::InspectNativeSupport(), TEXT("inspect_array_native_support")); }
FString AddBlueprintArrayFunctionNode(const FString& BlueprintPath, const FString& Operation, const FString& ElementType,
    const FString& ParamDefaultsJson, const FString& TypeObject, const FString& EnumType, const FString& NodePositionJson,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing)
{
    return EncodeFunctionResult(UE58::Array::AddNode(BlueprintPath, Operation, ElementType, ParamDefaultsJson, TypeObject,
        EnumType, NodePositionJson, GraphName, GraphId, GraphType, bCreateGraphIfMissing), TEXT("add_blueprint_array_function_node"));
}
}
