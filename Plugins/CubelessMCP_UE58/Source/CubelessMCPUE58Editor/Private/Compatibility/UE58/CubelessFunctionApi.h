#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
namespace Cubeless::UE58::Function
{
struct FResult
{
    bool bSuccess = false, bMutationAttempted = false, bChanged = false, bChangeStateKnown = true, bPostconditionPassed = false;
    FString Stage = TEXT("preflight"), ErrorCode, Error;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FResult Resolve(const FString& BlueprintPath, const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreate);
FResult AddParameter(const FString& BlueprintPath, const FString& Name, const FString& Type, const FString& Direction,
    const FString& DefaultJson, const FString& TypeObject, bool bArray, const FString& GraphName, const FString& GraphId,
    const FString& GraphType, bool bCreateGraph);
FResult AddCall(const FString& BlueprintPath, const FString& FunctionClass, const FString& FunctionName,
    const FString& DefaultsJson, const FString& PositionJson, const FString& GraphName, const FString& GraphId,
    const FString& GraphType, bool bCreateGraph, bool bLatent, bool bEditorOnly, bool bWildcard, bool bDeprecated, bool bInternal);
}
