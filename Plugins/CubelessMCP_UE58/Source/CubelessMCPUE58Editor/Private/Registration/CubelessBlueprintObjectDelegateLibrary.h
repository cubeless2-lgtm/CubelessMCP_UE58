#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintObjectDelegateLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintObjectDelegateLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_native_delegate_bind_node"))
    static FString AddBlueprintNativeDelegateBindNode(const FString& BlueprintName,const FString& DelegateOwnerClass,const FString& DelegateName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_create_delegate_node"))
    static FString AddBlueprintCreateDelegateNode(const FString& BlueprintName,const FString& FunctionName,const FString& ObjectNodeId,const FString& DelegateTargetNodeId,const FString& ObjectPinName,const FString& DelegateTargetPinName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_interface_message_node"))
    static FString AddBlueprintInterfaceMessageNode(const FString& BlueprintName,const FString& InterfaceClass,const FString& FunctionName,const FString& TargetNodeId,const FString& TargetPinName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
