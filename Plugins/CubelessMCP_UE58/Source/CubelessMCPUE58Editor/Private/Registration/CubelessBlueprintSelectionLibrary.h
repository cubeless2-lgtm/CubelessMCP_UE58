#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintSelectionLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintSelectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_select_node"))
    static FString AddBlueprintSelectNode(const FString& BlueprintName,const FString& ValueType,const FString& PositionJson,
        const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_switch_enum_node"))
    static FString AddBlueprintSwitchEnumNode(const FString& BlueprintName,const FString& EnumType,bool bHasDefaultPin,const FString& PositionJson,
        const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
