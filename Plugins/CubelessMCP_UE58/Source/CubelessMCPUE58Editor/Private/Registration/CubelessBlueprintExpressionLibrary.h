#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintExpressionLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintExpressionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_math_node"))
    static FString AddBlueprintMathNode(const FString& BlueprintName,const FString& Operation,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_boolean_node"))
    static FString AddBlueprintBooleanNode(const FString& BlueprintName,const FString& Operation,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
