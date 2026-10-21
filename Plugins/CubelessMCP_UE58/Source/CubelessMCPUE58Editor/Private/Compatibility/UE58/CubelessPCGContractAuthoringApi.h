#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
struct FPCGMeshSelectorWeightedEntry;

namespace Cubeless::UE58::PCGContractAuthoring
{
    // Shared existing stock descriptor planner only. Caller owns source resolution,
    // returned holds, current/SDK fences and persistence; this never dispatches a tool.
    bool SpawnerInputTruth(const TSharedPtr<FJsonValue>& Value);
    FString SpawnerInputString(const TSharedPtr<FJsonValue>& Value);
    TSharedPtr<FJsonObject> WeightedEntryObservation(const FPCGMeshSelectorWeightedEntry& Entry, bool PackagePaths);
    bool BuildWeightedEntryPlan(const TArray<TSharedPtr<FJsonValue>>& Specs,
        TArray<FPCGMeshSelectorWeightedEntry>& Entries,
        TFunctionRef<UObject*(const TSharedPtr<FJsonValue>&, bool)> Resolve,
        TFunctionRef<bool()> BeforeDescriptor, TFunctionRef<bool()> AfterDescriptor,
        TFunctionRef<bool()> CurrentFence, TFunctionRef<bool(const FPCGMeshSelectorWeightedEntry&)> Observe,
        TFunctionRef<bool(const FString&)> Fail);
    FString AuditGraphContract(const FString& ParamsJson);
    FString SetSpawnerEntries(const FString& ParamsJson);
}
