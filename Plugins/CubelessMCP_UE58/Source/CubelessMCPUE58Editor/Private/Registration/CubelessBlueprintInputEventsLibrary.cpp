#include "Registration/CubelessBlueprintInputEventsLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintInputEventsApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintInputEventsLibrary::AddBlueprintInputAxisEventNode(const FString& BlueprintName,const FString& AxisName,const FString& PositionJson,bool bConsumeInput,bool bExecuteWhenPaused,bool bOverrideParentBinding,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintInputEvents::AddAxis(BlueprintName,AxisName,PositionJson,bConsumeInput,bExecuteWhenPaused,bOverrideParentBinding,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_input_axis_event_node"));
}
FString UCubelessBlueprintInputEventsLibrary::AddBlueprintEnhancedInputActionNode(const FString& BlueprintName,const FString& InputAction,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintInputEvents::AddEnhanced(BlueprintName,InputAction,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_enhanced_input_action_node"));
}
