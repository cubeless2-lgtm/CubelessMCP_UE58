#include "Registration/CubelessBlueprintEditorSubsystemLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintEditorSubsystemApi.h"
#include "Operations/CubelessFunctionOperations.h"
FString UCubelessBlueprintEditorSubsystemLibrary::AddBlueprintGetEditorSubsystemNode(const FString& BlueprintName,const FString& SubsystemClass,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing)
{
    return Cubeless::Operations::EncodeFunctionResult(Cubeless::UE58::BlueprintEditorSubsystem::AddEditorSubsystem(BlueprintName,SubsystemClass,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing),TEXT("add_blueprint_get_editor_subsystem_node"));
}
