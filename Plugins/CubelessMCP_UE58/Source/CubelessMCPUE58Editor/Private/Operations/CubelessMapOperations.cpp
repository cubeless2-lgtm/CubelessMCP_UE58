#include "Operations/CubelessMapOperations.h"
#include "Compatibility/UE58/CubelessMapApi.h"
#include "Operations/CubelessFunctionOperations.h"

namespace Cubeless::Operations
{
FString AddBlueprintMapFunctionNode(const FString& BlueprintPath, const FString& Operation,
    const FString& KeyType, const FString& ValueType, const FString& ParamDefaultsJson,
    const FString& NodePositionJson, const FString& GraphName, const FString& GraphId,
    const FString& GraphType, bool bCreateGraphIfMissing)
{
    return EncodeFunctionResult(UE58::Map::AddNode(BlueprintPath, Operation, KeyType, ValueType,
        ParamDefaultsJson, NodePositionJson, GraphName, GraphId, GraphType, bCreateGraphIfMissing),
        TEXT("add_blueprint_map_function_node"));
}
}
