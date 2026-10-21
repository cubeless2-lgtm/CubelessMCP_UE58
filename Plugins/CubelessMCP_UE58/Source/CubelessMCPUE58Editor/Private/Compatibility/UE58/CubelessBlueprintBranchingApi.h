#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::BlueprintBranching
{
Function::FResult AddEnumLiteral(const FString& BlueprintPath,const FString& EnumType,const FString& ValueJson,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
Function::FResult AddIsValid(const FString& BlueprintPath,const FString& ValueType,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
Function::FResult AddSwitchInt(const FString& BlueprintPath,int32 StartIndex,int32 CaseCount,const FString& CaseValuesJson,bool bHasDefaultPin,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
}
