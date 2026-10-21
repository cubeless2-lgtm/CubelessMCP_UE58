#include "Registration/CubelessMapLibrary.h"
#include "Operations/CubelessMapOperations.h"

FString UCubelessMapLibrary::AddBlueprintMapFunctionNode(const FString& BlueprintPath, const FString& Operation,
    const FString& KeyType, const FString& ValueType, const FString& ParamDefaultsJson,
    const FString& NodePositionJson, const FString& GraphName, const FString& GraphId,
    const FString& GraphType, bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::AddBlueprintMapFunctionNode(BlueprintPath, Operation, KeyType, ValueType,
        ParamDefaultsJson, NodePositionJson, GraphName, GraphId, GraphType, bCreateGraphIfMissing);
}
