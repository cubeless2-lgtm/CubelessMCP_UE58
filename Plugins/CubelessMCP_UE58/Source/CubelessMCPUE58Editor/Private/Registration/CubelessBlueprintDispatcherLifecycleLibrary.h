#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintDispatcherLifecycleLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintDispatcherLifecycleLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_event_dispatcher_assign_node"))
    static FString AddBlueprintEventDispatcherAssignNode(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_event_dispatcher_unbind_node"))
    static FString AddBlueprintEventDispatcherUnbindNode(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_event_dispatcher_clear_node"))
    static FString AddBlueprintEventDispatcherClearNode(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_custom_event_signature"))
    static FString AddBlueprintCustomEventSignature(const FString& BlueprintName,const FString& CustomEventName,const FString& PositionJson,const FString& SignatureSourceDispatcherName,const FString& SignatureSourceClass,const FString& SignatureSourceDelegateName,const FString& NativeDelegateName,bool bCallInEditor,bool bHasCallInEditor,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
