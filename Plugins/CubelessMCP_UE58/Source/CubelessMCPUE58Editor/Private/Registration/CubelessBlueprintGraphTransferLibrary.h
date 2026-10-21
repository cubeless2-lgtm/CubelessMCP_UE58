#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintGraphTransferLibrary.generated.h"
UCLASS()
class UCubelessBlueprintGraphTransferLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="import_blueprint_graph_nodes_from_text"))
    static FString ImportBlueprintGraphNodesFromText(const FString& BlueprintName,const FString& Text,const FString& TextFilePath,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bClearGraph,bool bKeepRootNode,bool bAllowNonSample,bool bValidateOnly,bool bDryRun,bool bCompile,bool bSaveAsset);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="repair_blueprint_pin_links"))
    static FString RepairBlueprintPinLinks(const FString& BlueprintName,bool bAllowNonSample,bool bValidateOnly,bool bDryRun,bool bCompile,bool bSaveAsset);
};
