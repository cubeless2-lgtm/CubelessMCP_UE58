#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintConstructionLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintConstructionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_get_subsystem_from_pc_node"))
    static FString AddBlueprintGetSubsystemFromPCNode(const FString& BlueprintName,const FString& SubsystemClass,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_construct_object_node"))
    static FString AddBlueprintConstructObjectNode(const FString& BlueprintName,const FString& ObjectClass,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
