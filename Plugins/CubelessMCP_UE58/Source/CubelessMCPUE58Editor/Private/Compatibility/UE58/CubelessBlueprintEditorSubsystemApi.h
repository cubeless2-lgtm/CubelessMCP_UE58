#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::BlueprintEditorSubsystem
{
Function::FResult AddEditorSubsystem(const FString& BlueprintPath,const FString& SubsystemClass,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
}
