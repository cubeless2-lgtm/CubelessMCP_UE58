#include "Registration/CubelessBlueprintDispatcherLifecycleLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintDispatcherLifecycleApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintDispatcherLifecycleLibrary::AddBlueprintEventDispatcherAssignNode(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintDispatcherLifecycle::AddAssign(BlueprintName,DispatcherName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_event_dispatcher_assign_node"));
}
FString UCubelessBlueprintDispatcherLifecycleLibrary::AddBlueprintEventDispatcherUnbindNode(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintDispatcherLifecycle::AddUnbind(BlueprintName,DispatcherName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_event_dispatcher_unbind_node"));
}
FString UCubelessBlueprintDispatcherLifecycleLibrary::AddBlueprintEventDispatcherClearNode(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintDispatcherLifecycle::AddClear(BlueprintName,DispatcherName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_event_dispatcher_clear_node"));
}
FString UCubelessBlueprintDispatcherLifecycleLibrary::AddBlueprintCustomEventSignature(const FString& BlueprintName,const FString& CustomEventName,const FString& PositionJson,const FString& SignatureSourceDispatcherName,const FString& SignatureSourceClass,const FString& SignatureSourceDelegateName,const FString& NativeDelegateName,bool bCallInEditor,bool bHasCallInEditor,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintDispatcherLifecycle::AddCustomEventSignature(BlueprintName,CustomEventName,PositionJson,SignatureSourceDispatcherName,SignatureSourceClass,SignatureSourceDelegateName,NativeDelegateName,bCallInEditor,bHasCallInEditor,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_custom_event_node"));
}
