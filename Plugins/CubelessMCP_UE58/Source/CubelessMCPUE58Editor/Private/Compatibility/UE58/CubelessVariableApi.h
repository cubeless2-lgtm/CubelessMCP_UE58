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
    const FString& TypeObject, bool bArray, const FString& MetadataJson, const FString& Guid, bool bEditable, bool bTransient = false);
FResult Inspect(const FString& BlueprintPath, const FString& Name, bool bMetadata);
FResult SetDefault(const FString& BlueprintPath, const FString& Name, const FString& ValueJson, bool bCompile, bool bSave, bool bDryRun);
}

namespace Cubeless::UE58::Variable
{
FResult SetMetadata(const FString& BlueprintPath, const FString& VariableName, const FString& MetadataJson,
    const FString& IsEditableJson, const FString& InstanceEditableJson, const FString& ExposeOnSpawnJson);
FResult SetOrder(const FString& BlueprintPath, const FString& VariableOrderJson, bool bStrict, bool bCompile, bool bSave);
}

namespace Cubeless::UE58::Variable
{
FResult Remove(const FString& BlueprintPath, const FString& VariableName);
}

namespace Cubeless::UE58::Variable
{
FResult SetCategorySorting(const FString& BlueprintPath, const FString& CategoryOrderJson, const FString& CategoryToFront, bool bAppendExisting, bool bSave, bool bCompile);
}

namespace Cubeless::UE58::Variable
{
FResult SetCustomEventMetadata(const FString& BlueprintPath, const FString& CustomEventName, const FString& Description,
    const FString& NodeId, const FString& EventName, const FString& Tooltip, const FString& CallInEditorJson,
    bool bSetNodeComment, bool bCompile, bool bSave);
}

namespace Cubeless::UE58::Variable
{
FResult AddLocal(const FString& BlueprintPath, const FString& VariableName, const FString& VariableType, const FString& DefaultJson,
    const FString& Category, const FString& Tooltip, const FString& TypeObject, bool bIsArray,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
}

// Private scalar-local source role, shared only by declaration/Get/Set.
class UScriptStruct;class UPackage;
namespace Cubeless::UE58::Variable
{
struct FLocalScalarUdsSource
{
 struct FState;TSharedPtr<FState> State;
 bool Load(const FString& Requested,TFunction<bool()> Current,TFunction<bool(const TCHAR*,const TCHAR*)> Failure);
 bool PlainCurrent();bool Query();bool FinalFile();bool OutcomeUnknown()const;
 UScriptStruct* Type()const;UPackage* Package()const;TSharedRef<FJsonObject> Snapshot()const;
};
}
