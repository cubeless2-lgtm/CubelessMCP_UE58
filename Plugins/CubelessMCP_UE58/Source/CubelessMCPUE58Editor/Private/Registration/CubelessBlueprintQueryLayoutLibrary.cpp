#include "Registration/CubelessBlueprintQueryLayoutLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintQueryLayoutApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessBlueprintQueryLayoutLibrary::FindBlueprintNodes(const FString& BlueprintName,const FString& NodeTypeJson,const FString& EventTypeJson,const FString& GraphName,const FString& GraphId,const FString& GraphType)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlueprintQueryLayout::Find(BlueprintName,NodeTypeJson,EventTypeJson,GraphName,GraphId,GraphType),TEXT("find_blueprint_nodes"));
}
FString UCubelessBlueprintQueryLayoutLibrary::SetBlueprintNodePositions(const FString& BlueprintName,const FString& GraphName,const FString& UpdatesJson,const FString& GraphId,const FString& GraphType,bool bDryRun,bool bCompile,bool bSave)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlueprintQueryLayout::SetPositions(BlueprintName,GraphName,UpdatesJson,GraphId,GraphType,bDryRun,bCompile,bSave),TEXT("set_blueprint_node_positions"));
}
