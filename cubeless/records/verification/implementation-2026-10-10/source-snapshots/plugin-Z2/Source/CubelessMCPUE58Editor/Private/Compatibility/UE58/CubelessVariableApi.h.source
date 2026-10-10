#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Cubeless::UE58::Variable
{
struct FResult
{
    bool bSuccess = false, bMutationAttempted = false, bChanged = false, bChangeStateKnown = true, bPostconditionPassed = false;
    bool bCompileRequested = false, bCompileAttempted = false, bSaveRequested = false, bSaveAttempted = false, bSaved = false;
    TOptional<bool> Compiled;
    FString Stage = TEXT("preflight"), ErrorCode, Error;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FResult Add(const FString& BlueprintPath, const FString& Name, const FString& Type, bool bExposed,
    const FString& DefaultJson, const FString& Category, const FString& Tooltip, const FString& FriendlyName,
    const FString& TypeObject, bool bArray, const FString& MetadataJson, const FString& Guid, bool bEditable);
FResult Inspect(const FString& BlueprintPath, const FString& Name, bool bMetadata);
FResult SetDefault(const FString& BlueprintPath, const FString& Name, const FString& ValueJson, bool bCompile, bool bSave, bool bDryRun);
}
