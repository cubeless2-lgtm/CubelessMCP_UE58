#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::BlueprintObjectDelegate
{
Function::FResult AddNativeBind(const FString& BlueprintName,const FString& DelegateOwnerClass,const FString& DelegateName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
Function::FResult AddCreateDelegate(const FString& BlueprintName,const FString& FunctionName,const FString& ObjectNodeId,const FString& DelegateTargetNodeId,const FString& ObjectPinName,const FString& DelegateTargetPinName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
Function::FResult AddMessage(const FString& BlueprintName,const FString& InterfaceClass,const FString& FunctionName,const FString& TargetNodeId,const FString& TargetPinName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
}
