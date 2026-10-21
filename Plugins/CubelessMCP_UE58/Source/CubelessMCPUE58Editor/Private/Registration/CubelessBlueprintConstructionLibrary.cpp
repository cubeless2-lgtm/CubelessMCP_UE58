#include "Registration/CubelessBlueprintConstructionLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintConstructionApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintConstructionLibrary::AddBlueprintGetSubsystemFromPCNode(const FString& BlueprintName,const FString& SubsystemClass,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintConstruction::AddSubsystemFromPC(BlueprintName,SubsystemClass,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_get_subsystem_from_pc_node"));
}
FString UCubelessBlueprintConstructionLibrary::AddBlueprintConstructObjectNode(const FString& BlueprintName,const FString& ObjectClass,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintConstruction::AddConstructObject(BlueprintName,ObjectClass,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_construct_object_node"));
}
