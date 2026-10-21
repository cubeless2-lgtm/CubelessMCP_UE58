#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::Map
{
// Exact String -> Vector/Double clear/find/add/contains/remove/keys/values/length.
// Empty defaults preserve the
// original unlinked wildcard shape. Key/Value literals use an explicit typed
// stock-pin profile; TargetMap still requires a separately requested real wire.
Function::FResult AddNode(const FString& BlueprintPath, const FString& Operation,
    const FString& KeyType, const FString& ValueType, const FString& ParamDefaultsJson,
    const FString& NodePositionJson, const FString& GraphName, const FString& GraphId,
    const FString& GraphType, bool bCreateGraphIfMissing);
// Internal exact AddCall wildcard opt-in. No requested key/value profile is inferred.
bool SupportsUntypedCall(const FString& FunctionClass,const FString& FunctionName);
Function::FResult AddUntypedCall(const FString& BlueprintPath,const FString& FunctionName,
    const FString& ParamDefaultsJson,const FString& NodePositionJson,const FString& GraphName,
    const FString& GraphId,const FString& GraphType);
}
