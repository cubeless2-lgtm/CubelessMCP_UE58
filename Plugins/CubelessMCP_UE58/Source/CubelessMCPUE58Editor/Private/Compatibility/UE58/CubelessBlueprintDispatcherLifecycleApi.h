#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::BlueprintDispatcherLifecycle
{
Function::FResult AddAssign(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
Function::FResult AddUnbind(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
Function::FResult AddClear(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
Function::FResult AddCustomEventSignature(const FString& BlueprintName,const FString& CustomEventName,const FString& PositionJson,const FString& SignatureSourceDispatcherName,const FString& SignatureSourceClass,const FString& SignatureSourceDelegateName,const FString& NativeDelegateName,bool bCallInEditor,bool bHasCallInEditor,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
}
