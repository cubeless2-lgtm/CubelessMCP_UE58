#include "Registration/CubelessBlueprintDispatcherLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintDispatcherApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintDispatcherLibrary::AddBlueprintEventDispatcher(const FString& BlueprintName,const FString& DispatcherName,const FString& InputsJson,const FString& Category,const FString& Tooltip,const FString& FriendlyName)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintDispatcher::AddDispatcher(BlueprintName,DispatcherName,InputsJson,Category,Tooltip,FriendlyName),TEXT("add_blueprint_event_dispatcher"));
}
FString UCubelessBlueprintDispatcherLibrary::AddBlueprintEventDispatcherCallNode(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintDispatcher::AddCall(BlueprintName,DispatcherName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_event_dispatcher_call_node"));
}
FString UCubelessBlueprintDispatcherLibrary::AddBlueprintEventDispatcherBindNode(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintDispatcher::AddBind(BlueprintName,DispatcherName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_event_dispatcher_bind_node"));
}
