#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::BlueprintValue
{
Function::FResult AddCompare(const FString& BlueprintPath,const FString& Operation,const FString& ValueType,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
Function::FResult AddLiteral(const FString& BlueprintPath,const FString& LiteralType,const FString& ValueJson,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
}
