#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::BlueprintInputEvents
{
Function::FResult AddAxis(const FString& BlueprintPath,const FString& AxisName,const FString& PositionJson,bool bConsume,bool bPaused,bool bOverride,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreate);
Function::FResult AddEnhanced(const FString& BlueprintPath,const FString& InputAction,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreate);
}
