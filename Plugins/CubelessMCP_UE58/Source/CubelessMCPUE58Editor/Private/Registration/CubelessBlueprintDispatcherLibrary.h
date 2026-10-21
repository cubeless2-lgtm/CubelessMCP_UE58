#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintDispatcherLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintDispatcherLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_event_dispatcher"))
    static FString AddBlueprintEventDispatcher(const FString& BlueprintName,const FString& DispatcherName,const FString& InputsJson,const FString& Category,const FString& Tooltip,const FString& FriendlyName);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_event_dispatcher_call_node"))
    static FString AddBlueprintEventDispatcherCallNode(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_event_dispatcher_bind_node"))
    static FString AddBlueprintEventDispatcherBindNode(const FString& BlueprintName,const FString& DispatcherName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
