#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::BlueprintNetworking
{
Variable::FResult AuditLoadedDuplicateSource(UObject* SourceObject);
Variable::FResult Inspect(const FString& BlueprintName);
Variable::FResult Set(const FString& BlueprintName,const FString& ReplicatesJson,const FString& ReplicateMovementJson,const FString& NetDormancyJson,const FString& AlwaysRelevantJson,const FString& OnlyRelevantToOwnerJson,const FString& NetUpdateFrequencyJson,const FString& MinNetUpdateFrequencyJson,const FString& NetPriorityJson,const FString& NetCullDistanceSquaredJson,bool bDryRun);
Variable::FResult InspectRawInput(const FString& ParamsJson);
Variable::FResult SetRawInput(const FString& ParamsJson);
}
