#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::BlueprintSelection
{
Function::FResult AddSelect(const FString& BlueprintPath,const FString& ValueType,const FString& PositionJson,
    const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
Function::FResult AddSwitchEnum(const FString& BlueprintPath,const FString& EnumType,bool bHasDefaultPin,const FString& PositionJson,
    const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
}
