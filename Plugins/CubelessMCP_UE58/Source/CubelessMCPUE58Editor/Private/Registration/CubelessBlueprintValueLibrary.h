#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintValueLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintValueLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_compare_node"))
    static FString AddBlueprintCompareNode(const FString& BlueprintName,const FString& Operation,const FString& ValueType,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_literal_node"))
    static FString AddBlueprintLiteralNode(const FString& BlueprintName,const FString& LiteralType,const FString& ValueJson,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
