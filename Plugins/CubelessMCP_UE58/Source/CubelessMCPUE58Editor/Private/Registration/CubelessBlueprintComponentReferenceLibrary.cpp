#include "Registration/CubelessBlueprintComponentReferenceLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintComponentReferenceApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessBlueprintComponentReferenceLibrary::AddBlueprintGetSelfComponentReference(const FString& BlueprintName,const FString& ComponentName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlueprintComponentReference::Add(BlueprintName,ComponentName,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_get_self_component_reference"));
}
