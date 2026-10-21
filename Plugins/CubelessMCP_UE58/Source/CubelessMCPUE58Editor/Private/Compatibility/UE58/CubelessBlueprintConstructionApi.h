#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::BlueprintConstruction
{
Function::FResult AddSubsystemFromPC(const FString& BlueprintPath,const FString& SubsystemClass,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
Function::FResult AddConstructObject(const FString& BlueprintPath,const FString& ObjectClass,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
}
