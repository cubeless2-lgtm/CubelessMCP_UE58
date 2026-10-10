#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::Material
{
enum class EOperation : uint8
{
    ResolveGraph, ListNodes, AnalyzeGraph, ListCollectionNodes, GetCollectionValues,
    ConnectNodes, ConnectProperty, DeleteNode, LayoutNodes, AddNode, SetNodeProperty, AddCustomNode
};

struct FState
{
    TStrongObjectPtr<UObject> Asset;
    TStrongObjectPtr<UObject> SelectedExpression;
    TSharedPtr<FJsonObject> Snapshot;
    FString AssetPath;
    FString SelectedNodeKey;
    bool bDirty = false;
    int32 NodeCount = 0;
};

// Per native call observations, not estimates of engine-internal recursion.
// A nested call receives its own scope and restores the outer scope afterward.
class FReadMetricsScope final
{
public:
    FReadMetricsScope();
    ~FReadMetricsScope();
    FReadMetricsScope(const FReadMetricsScope&) = delete;
    FReadMetricsScope& operator=(const FReadMetricsScope&) = delete;
    void Increment(FName Name, int64 Amount = 1);
    TSharedRef<FJsonObject> ToJson() const;
private:
    FReadMetricsScope* Previous = nullptr;
    TMap<FName, int64> Counts;
    double StartedAt = 0.0;
};

bool IsMutation(EOperation Operation);
bool PrepareCustomNodeRequest(const TSharedPtr<FJsonObject>& Input, TSharedPtr<FJsonObject>& OutParams, FString& ErrorCode, FString& Error);
FString OperationName(EOperation Operation);
bool CaptureState(EOperation Operation, const TSharedPtr<FJsonObject>& Params, int32 MaxGraphNodes, FState& OutState, FString& OutError);
bool ValidateMutationTarget(const FState& State, FString& OutError);
bool ValidateMutationInputs(EOperation Operation, const TSharedPtr<FJsonObject>& Params, const FState& State, int32 MaxGraphNodes, FString& OutError);
TSharedPtr<FJsonObject> Execute(EOperation Operation, const TSharedPtr<FJsonObject>& Params);
// Only two read-only operations already have their exact data in the local capture.
// No caller-provided/global cache and no replacement of the independent after capture.
bool TryReuseCapturedReadData(EOperation Operation, const FState& Before, TSharedPtr<FJsonObject>& OutData);
bool VerifyMutation(EOperation Operation, const TSharedPtr<FJsonObject>& Params, const FState& Before, const FState& After, FString& OutMethod, FString& OutError);
TSharedPtr<FJsonObject> DescribeDelta(const FState& Before, const FState& After);
FString ToJson(const TSharedPtr<FJsonObject>& Object);
}
