#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintFunctionExtractionLibrary.generated.h"
UCLASS(meta=(AIIgnore))
class UCubelessBlueprintFunctionExtractionLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="collapse_blueprint_nodes_to_function"))
    static FString CollapseBlueprintNodesToFunction(FString BlueprintName,FString GraphName,FString FunctionName,FString NodeIdsJson,FString GraphType,int64 ExpectedNodeCount,bool bAllowNonTemp,bool bDryRun,bool bCompile,bool bSaveAsset,FString PrivateContextJson);
};
