#include "Registration/CubelessBlueprintNetworkingLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintNetworkingApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessBlueprintNetworkingLibrary::InspectBlueprintNetworkingSettings(const FString& BlueprintName)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlueprintNetworking::Inspect(BlueprintName),TEXT("inspect_blueprint_networking_settings"));
}
FString UCubelessBlueprintNetworkingLibrary::SetBlueprintNetworkingSettings(const FString& BlueprintName,const FString& ReplicatesJson,const FString& ReplicateMovementJson,const FString& NetDormancyJson,const FString& AlwaysRelevantJson,const FString& OnlyRelevantToOwnerJson,const FString& NetUpdateFrequencyJson,const FString& MinNetUpdateFrequencyJson,const FString& NetPriorityJson,const FString& NetCullDistanceSquaredJson,bool bDryRun)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlueprintNetworking::Set(BlueprintName,ReplicatesJson,ReplicateMovementJson,NetDormancyJson,AlwaysRelevantJson,OnlyRelevantToOwnerJson,NetUpdateFrequencyJson,MinNetUpdateFrequencyJson,NetPriorityJson,NetCullDistanceSquaredJson,bDryRun),TEXT("set_blueprint_networking_settings"));
}
