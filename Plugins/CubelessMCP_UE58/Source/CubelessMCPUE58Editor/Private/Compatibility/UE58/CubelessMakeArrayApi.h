#pragma once
#include "CoreMinimal.h"
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::MakeArray
{
    /** Node-only MakeArray: existing types plus loaded runtime object/class/Rotator/Transform/ScriptStruct descriptors. Original loaders/short aliases remain incomplete. Null JSON preserves omitted count/values. */
    Function::FResult AddNode(const FString& BlueprintPath, const FString& ElementType,
        const FString& InputCountJson, const FString& ValuesJson, const FString& TypeObject,
        const FString& EnumType, const FString& NodePositionJson, const FString& GraphName,
        const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
}
