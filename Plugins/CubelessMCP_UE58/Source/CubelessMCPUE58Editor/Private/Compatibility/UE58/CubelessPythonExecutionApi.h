#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/Function.h"

namespace Cubeless::UE58::PythonExecution
{
// Original signature has injected Context + five caller JSON fields.
// Native command-only options remain explicit here,
// rather than silently widening the public schema.
struct FRequest
{
    FString Code;
    FString Mode = TEXT("ExecuteFile");
    bool DeferToTicker = false;
    bool AllowUnsafeEditorScriptingDuringPIE = false;
    FString InterchangePolicy = TEXT("auto");
    FString TransactionPolicy = TEXT("command");
    bool AllowUnsafePCGSelectorPython = false;
};

// The concrete Raw owner supplies this lease. No execution/transport is owned here.
struct FDispatch
{
    bool StandaloneTicker = false;
    bool SynchronousNested = false;
    TFunction<bool()> IsCurrent;
};

bool ParsePublicRequest(const TSharedRef<FJsonObject>& Input, FRequest& Request,
    FString& ErrorCode, FString& Error);
bool ParseNativeRequest(const TSharedRef<FJsonObject>& Input, FRequest& Request,
    FString& ErrorCode, FString& Error);
bool RequiresStandaloneTicker(const FRequest& Request);
bool IsExecuting(); // GT-only concrete recursion state, including private Batch entry.
void Activate();   // Concrete registration owner only; no Python initialization.
void Deactivate(); // Stops new private/named calls; dispatched returns remain unknown.
TSharedRef<FJsonObject> Unexecuted(const FString& Code, const FString& Error);
TSharedRef<FJsonObject> Execute(const FRequest& Request, const FDispatch& Dispatch);
// Internal synchronous Batch scope. Never held across a frame/ticker continuation.
// Observe actions are check/end/abandon; a lost scope is quarantined, never closed speculatively.
TSharedRef<FJsonObject> BeginBatchTransaction(const FString& OperationId);
TSharedRef<FJsonObject> ObserveBatchTransaction(const FString& ScopeId, const FString& Action);
}
