#include "Registration/CubelessBlueprintExpressionLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintExpressionApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintExpressionLibrary::AddBlueprintMathNode(const FString& BlueprintName,const FString& Operation,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintExpression::AddMath(BlueprintName,Operation,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_math_node"));
}
FString UCubelessBlueprintExpressionLibrary::AddBlueprintBooleanNode(const FString& BlueprintName,const FString& Operation,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintExpression::AddBoolean(BlueprintName,Operation,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_boolean_node"));
}
