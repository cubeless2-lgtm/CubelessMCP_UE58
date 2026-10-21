#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::Array
{
// Finite read-only evidence before constructing the legacy array family.
// No selectors, native invocations, nodes, loads, CDO creation, compile or save.
Function::FResult InspectNativeSupport();
Function::FResult AddNode(const FString& BlueprintPath, const FString& Operation, const FString& ElementType,
    const FString& ParamDefaultsJson, const FString& TypeObject, const FString& EnumType, const FString& NodePositionJson,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
}
