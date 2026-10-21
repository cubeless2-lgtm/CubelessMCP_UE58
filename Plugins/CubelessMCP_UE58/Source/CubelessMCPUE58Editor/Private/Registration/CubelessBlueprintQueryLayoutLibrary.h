#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintQueryLayoutLibrary.generated.h"
UCLASS()
class UCubelessBlueprintQueryLayoutLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="find_blueprint_nodes"))
    static FString FindBlueprintNodes(const FString& BlueprintName,const FString& NodeTypeJson,const FString& EventTypeJson,const FString& GraphName,const FString& GraphId,const FString& GraphType);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="set_blueprint_node_positions"))
    static FString SetBlueprintNodePositions(const FString& BlueprintName,const FString& GraphName,const FString& UpdatesJson,const FString& GraphId,const FString& GraphType,bool bDryRun,bool bCompile,bool bSave);
};
