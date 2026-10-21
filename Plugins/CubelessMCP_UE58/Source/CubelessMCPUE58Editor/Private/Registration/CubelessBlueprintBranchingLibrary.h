#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintBranchingLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintBranchingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_enum_literal_node"))
    static FString AddBlueprintEnumLiteralNode(const FString& BlueprintName,const FString& EnumType,const FString& ValueJson,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_is_valid_node"))
    static FString AddBlueprintIsValidNode(const FString& BlueprintName,const FString& ValueType,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_switch_int_node"))
    static FString AddBlueprintSwitchIntNode(const FString& BlueprintName,int32 StartIndex,int32 CaseCount,const FString& CaseValuesJson,bool bHasDefaultPin,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
