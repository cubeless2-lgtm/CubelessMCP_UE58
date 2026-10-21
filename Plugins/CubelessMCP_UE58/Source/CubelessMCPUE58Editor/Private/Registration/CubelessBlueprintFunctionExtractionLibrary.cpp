#include "Registration/CubelessBlueprintFunctionExtractionLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintFunctionExtractionApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessBlueprintFunctionExtractionLibrary::CollapseBlueprintNodesToFunction(FString BlueprintName,FString GraphName,FString FunctionName,FString NodeIdsJson,FString GraphType,int64 ExpectedNodeCount,bool bAllowNonTemp,bool bDryRun,bool bCompile,bool bSaveAsset,FString PrivateContextJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlueprintFunctionExtraction::Collapse(BlueprintName,GraphName,FunctionName,NodeIdsJson,GraphType,ExpectedNodeCount,bAllowNonTemp,bDryRun,bCompile,bSaveAsset,PrivateContextJson),TEXT("collapse_blueprint_nodes_to_function"));
}
