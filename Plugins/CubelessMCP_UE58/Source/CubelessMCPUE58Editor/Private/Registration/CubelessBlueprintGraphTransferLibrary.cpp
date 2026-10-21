#include "Registration/CubelessBlueprintGraphTransferLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintGraphTransferApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessBlueprintGraphTransferLibrary::ImportBlueprintGraphNodesFromText(const FString& BlueprintName,const FString& Text,const FString& TextFilePath,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bClearGraph,bool bKeepRootNode,bool bAllowNonSample,bool bValidateOnly,bool bDryRun,bool bCompile,bool bSaveAsset)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlueprintGraphTransfer::Import(BlueprintName,Text,TextFilePath,GraphName,GraphId,GraphType,bClearGraph,bKeepRootNode,bAllowNonSample,bValidateOnly,bDryRun,bCompile,bSaveAsset),TEXT("import_blueprint_graph_nodes_from_text"));
}
FString UCubelessBlueprintGraphTransferLibrary::RepairBlueprintPinLinks(const FString& BlueprintName,bool bAllowNonSample,bool bValidateOnly,bool bDryRun,bool bCompile,bool bSaveAsset)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlueprintGraphTransfer::Repair(BlueprintName,bAllowNonSample,bValidateOnly,bDryRun,bCompile,bSaveAsset),TEXT("repair_blueprint_pin_links"));
}
