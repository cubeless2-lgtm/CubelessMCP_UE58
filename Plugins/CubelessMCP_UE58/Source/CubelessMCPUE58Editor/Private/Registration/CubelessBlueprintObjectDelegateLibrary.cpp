#include "Registration/CubelessBlueprintObjectDelegateLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintObjectDelegateApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintObjectDelegateLibrary::AddBlueprintNativeDelegateBindNode(const FString& BlueprintName,const FString& DelegateOwnerClass,const FString& DelegateName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintObjectDelegate::AddNativeBind(BlueprintName,DelegateOwnerClass,DelegateName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_native_delegate_bind_node"));
}
FString UCubelessBlueprintObjectDelegateLibrary::AddBlueprintCreateDelegateNode(const FString& BlueprintName,const FString& FunctionName,const FString& ObjectNodeId,const FString& DelegateTargetNodeId,const FString& ObjectPinName,const FString& DelegateTargetPinName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintObjectDelegate::AddCreateDelegate(BlueprintName,FunctionName,ObjectNodeId,DelegateTargetNodeId,ObjectPinName,DelegateTargetPinName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_create_delegate_node"));
}
FString UCubelessBlueprintObjectDelegateLibrary::AddBlueprintInterfaceMessageNode(const FString& BlueprintName,const FString& InterfaceClass,const FString& FunctionName,const FString& TargetNodeId,const FString& TargetPinName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintObjectDelegate::AddMessage(BlueprintName,InterfaceClass,FunctionName,TargetNodeId,TargetPinName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_interface_message_node"));
}
