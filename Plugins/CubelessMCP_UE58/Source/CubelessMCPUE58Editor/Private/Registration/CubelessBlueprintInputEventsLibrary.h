#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintInputEventsLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintInputEventsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_input_axis_event_node"))
    static FString AddBlueprintInputAxisEventNode(const FString& BlueprintName,const FString& AxisName,const FString& PositionJson,bool bConsumeInput,bool bExecuteWhenPaused,bool bOverrideParentBinding,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_enhanced_input_action_node"))
    static FString AddBlueprintEnhancedInputActionNode(const FString& BlueprintName,const FString& InputAction,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
