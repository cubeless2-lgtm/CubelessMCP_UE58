#include "Registration/CubelessBlueprintSelectionLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintSelectionApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintSelectionLibrary::AddBlueprintSelectNode(const FString& BlueprintName,const FString& ValueType,const FString& PositionJson,
    const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintSelection::AddSelect(BlueprintName,ValueType,PositionJson,
        GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_select_node"));
}
FString UCubelessBlueprintSelectionLibrary::AddBlueprintSwitchEnumNode(const FString& BlueprintName,const FString& EnumType,bool bHasDefaultPin,const FString& PositionJson,
    const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintSelection::AddSwitchEnum(BlueprintName,EnumType,bHasDefaultPin,PositionJson,
        GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_switch_enum_node"));
}
