#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::BlueprintDispatcher
{
Function::FResult AddDispatcher(const FString& BlueprintName,const FString& DispatcherName,const FString& InputsJson,const FString& Category,const FString& Tooltip,const FString& FriendlyName);
Function::FResult AddCall(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
Function::FResult AddBind(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
}
