#include "Registration/CubelessBlueprintInputSelfLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintInputSelfApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintInputSelfLibrary::AddBlueprintInputActionNode(const FString& BlueprintName,const FString& ActionName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintInputSelf::AddInputAction(BlueprintName,ActionName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_input_action_node"));
}
FString UCubelessBlueprintInputSelfLibrary::AddBlueprintSelfReference(const FString& BlueprintName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintInputSelf::AddSelfReference(BlueprintName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_self_reference"));
}
