#include "Registration/CubelessBlueprintValueLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintValueApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintValueLibrary::AddBlueprintCompareNode(const FString& BlueprintName,const FString& Operation,const FString& ValueType,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintValue::AddCompare(BlueprintName,Operation,ValueType,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_compare_node"));
}
FString UCubelessBlueprintValueLibrary::AddBlueprintLiteralNode(const FString& BlueprintName,const FString& LiteralType,const FString& ValueJson,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintValue::AddLiteral(BlueprintName,LiteralType,ValueJson,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_literal_node"));
}
