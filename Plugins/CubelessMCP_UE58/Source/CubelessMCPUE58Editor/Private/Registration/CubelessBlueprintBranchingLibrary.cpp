#include "Registration/CubelessBlueprintBranchingLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintBranchingApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintBranchingLibrary::AddBlueprintEnumLiteralNode(const FString& BlueprintName,const FString& EnumType,const FString& ValueJson,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintBranching::AddEnumLiteral(BlueprintName,EnumType,ValueJson,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_enum_literal_node"));
}
FString UCubelessBlueprintBranchingLibrary::AddBlueprintIsValidNode(const FString& BlueprintName,const FString& ValueType,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintBranching::AddIsValid(BlueprintName,ValueType,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_is_valid_node"));
}
FString UCubelessBlueprintBranchingLibrary::AddBlueprintSwitchIntNode(const FString& BlueprintName,int32 StartIndex,int32 CaseCount,const FString& CaseValuesJson,bool bHasDefaultPin,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintBranching::AddSwitchInt(BlueprintName,StartIndex,CaseCount,CaseValuesJson,bHasDefaultPin,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_switch_int_node"));
}
