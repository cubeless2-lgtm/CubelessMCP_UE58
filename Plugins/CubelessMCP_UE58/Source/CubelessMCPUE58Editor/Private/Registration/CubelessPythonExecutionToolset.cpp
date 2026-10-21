#include "Registration/CubelessPythonExecutionToolset.h"

#include "Compatibility/UE58/CubelessPythonExecutionApi.h"
#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "Async/Async.h"
#include "Async/Future.h"
#include "Containers/Ticker.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/Transactor.h"
#include "Misc/CoreMisc.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/Atomic.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::Registration
{
namespace
{
using FOutput = TValueOrError<FString, FString>;
using FToolset = UE::ToolsetRegistry::FToolset;
using FRequest = Cubeless::UE58::PythonExecution::FRequest;
constexpr const TCHAR* ToolsetName = TEXT("CubelessMCPUE58Editor.CubelessPythonExecutionTools");
constexpr const TCHAR* MethodName = TEXT("execute_unreal_python");
constexpr const TCHAR* NativeMethodName = TEXT("execute_python");
bool KnownMethod(const FString& Name)
{ return Name.Equals(MethodName, ESearchCase::CaseSensitive) || Name.Equals(NativeMethodName, ESearchCase::CaseSensitive); }

FString Json(const TSharedRef<FJsonObject>& Object)
{
    FString Output;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output));
    return Output;
}
FOutput ProjectPythonReceipt(const TSharedRef<FJsonObject>& Actual)
{
    auto Envelope = MakeShared<FJsonObject>(); Envelope->SetObjectField(TEXT("returnValue"), Actual);
    return MakeValue(Json(Envelope));
}
FOutput Rejected(const TCHAR* Code, const FString& Error)
{ return ProjectPythonReceipt(Cubeless::UE58::PythonExecution::Unexecuted(Code, Error)); }

FString Schema()
{
    auto Code = MakeShared<FJsonObject>(); Code->SetStringField(TEXT("type"), TEXT("string"));
    auto Mode = MakeShared<FJsonObject>(); Mode->SetStringField(TEXT("type"), TEXT("string")); Mode->SetStringField(TEXT("default"), TEXT("ExecuteFile"));
    // Do not add enum to mode: the original raw parser falls back on invalid strings.
    auto Defer = MakeShared<FJsonObject>(); Defer->SetStringField(TEXT("type"), TEXT("boolean")); Defer->SetBoolField(TEXT("default"), false);
    auto PIE = MakeShared<FJsonObject>(); PIE->SetStringField(TEXT("type"), TEXT("boolean")); PIE->SetBoolField(TEXT("default"), false);
    auto Policy = MakeShared<FJsonObject>(); Policy->SetStringField(TEXT("type"), TEXT("string")); Policy->SetStringField(TEXT("default"), TEXT("auto"));
    TArray<TSharedPtr<FJsonValue>> Policies;
    for (const TCHAR* Name : { TEXT("auto"), TEXT("none"), TEXT("legacy"), TEXT("preserve"), TEXT("interchange") })
    { Policies.Add(MakeShared<FJsonValueString>(Name)); }
    Policy->SetArrayField(TEXT("enum"), Policies);
    auto Properties = MakeShared<FJsonObject>();
    Properties->SetObjectField(TEXT("code"), Code); Properties->SetObjectField(TEXT("mode"), Mode);
    Properties->SetObjectField(TEXT("defer_to_ticker"), Defer);
    Properties->SetObjectField(TEXT("allow_unsafe_editor_scripting_during_pie"), PIE);
    Properties->SetObjectField(TEXT("interchange_policy"), Policy);
    auto Input = MakeShared<FJsonObject>(); Input->SetStringField(TEXT("type"), TEXT("object")); Input->SetObjectField(TEXT("properties"), Properties);
    Input->SetArrayField(TEXT("required"), { MakeShared<FJsonValueString>(TEXT("code")) }); Input->SetBoolField(TEXT("additionalProperties"), false);
    // The original result is a native JSON object, including variable failure/unknown fields.
    auto Legacy = MakeShared<FJsonObject>(); Legacy->SetStringField(TEXT("type"), TEXT("object")); Legacy->SetBoolField(TEXT("additionalProperties"), true);
    auto OutputProperties = MakeShared<FJsonObject>(); OutputProperties->SetObjectField(TEXT("returnValue"), Legacy);
    auto Output = MakeShared<FJsonObject>(); Output->SetStringField(TEXT("type"), TEXT("object")); Output->SetObjectField(TEXT("properties"), OutputProperties);
    Output->SetArrayField(TEXT("required"), { MakeShared<FJsonValueString>(TEXT("returnValue")) }); Output->SetBoolField(TEXT("additionalProperties"), false);
    auto Tool = MakeShared<FJsonObject>(); Tool->SetStringField(TEXT("name"), FString(ToolsetName) + TEXT(".") + MethodName);
    Tool->SetStringField(TEXT("description"), TEXT("Execute original Unreal Editor Python with code and original mode/defer/PIE/import defaults. Auto conservatively uses legacy import bypass on the next standalone tick, with captured CVar restoration; preserve/interchange keeps Interchange. None is a trusted no-import declaration. SDK logs/result and actual partial/unknown failures are preserved; this is not a sandbox, atomic rollback, async-import completion proof or migration of other APIs."));
    Tool->SetObjectField(TEXT("inputSchema"), Input); Tool->SetObjectField(TEXT("outputSchema"), Output);
    auto NativeProperties = MakeShared<FJsonObject>();
    for (const auto& Pair : Properties->Values) { NativeProperties->SetField(Pair.Key, Pair.Value); }
    auto NativeMode = MakeShared<FJsonObject>(); NativeMode->SetStringField(TEXT("type"), TEXT("string")); NativeMode->SetStringField(TEXT("default"), TEXT("ExecuteStatement"));
    NativeProperties->SetObjectField(TEXT("mode"), NativeMode);
    auto Transaction = MakeShared<FJsonObject>(); Transaction->SetStringField(TEXT("type"), TEXT("string")); Transaction->SetStringField(TEXT("default"), TEXT("command"));
    Transaction->SetArrayField(TEXT("enum"), { MakeShared<FJsonValueString>(TEXT("command")), MakeShared<FJsonValueString>(TEXT("script")) });
    auto UnsafePCG = MakeShared<FJsonObject>(); UnsafePCG->SetStringField(TEXT("type"), TEXT("boolean")); UnsafePCG->SetBoolField(TEXT("default"), false);
    NativeProperties->SetObjectField(TEXT("transaction_policy"), Transaction); NativeProperties->SetObjectField(TEXT("allow_unsafe_pcg_selector_python"), UnsafePCG);
    auto NativeInput = MakeShared<FJsonObject>(); NativeInput->SetStringField(TEXT("type"), TEXT("object")); NativeInput->SetObjectField(TEXT("properties"), NativeProperties);
    NativeInput->SetArrayField(TEXT("required"), { MakeShared<FJsonValueString>(TEXT("code")) }); NativeInput->SetBoolField(TEXT("additionalProperties"), false);
    auto NativeTool = MakeShared<FJsonObject>(); NativeTool->SetStringField(TEXT("name"), FString(ToolsetName) + TEXT(".") + NativeMethodName);
    NativeTool->SetStringField(TEXT("description"), TEXT("Original native execute_python7 entry uses ExecuteStatement when mode is omitted, unlike the public ExecuteFile default. Adds transaction_policy command/script and allow_unsafe_pcg_selector_python, with the same shared execution/guard/restore logic. Script transactions require a standalone request outside active/nested transactions. This is general Editor execution, not a security sandbox or completion of other tools."));
    NativeTool->SetObjectField(TEXT("inputSchema"), NativeInput); NativeTool->SetObjectField(TEXT("outputSchema"), Output);
    auto Full = MakeShared<FJsonObject>(); Full->SetStringField(TEXT("name"), ToolsetName); Full->SetStringField(TEXT("version"), TEXT("1.0"));
    Full->SetArrayField(TEXT("tools"), { MakeShared<FJsonValueObject>(Tool), MakeShared<FJsonValueObject>(NativeTool) }); return Json(Full);
}
struct FQueuedCall
{
    FRequest Request;
    FString Method;
    TSharedRef<TPromise<FOutput>, ESPMode::ThreadSafe> Promise;
    FTSTicker::FDelegateHandle Ticker;
    bool Resolved = false; // Game thread only; off-thread entry captures an independent promise.
    explicit FQueuedCall(const FRequest& In, const FString& InMethod, const TSharedRef<TPromise<FOutput>, ESPMode::ThreadSafe>& InPromise)
        : Request(In), Method(InMethod), Promise(InPromise) {}
    void Resolve(FOutput&& Value)
    { if (!Resolved) { Resolved = true; Promise->SetValue(MoveTemp(Value)); } }
};
}

struct FPythonExecutionCallState
{
    TAtomic<bool> Active { false };
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    const UE::ToolsetRegistry::FToolset* Identity = nullptr;
    int32 ExecutionDepth = 0; // Only accessed on GT; includes nested none execution.
    TArray<TSharedPtr<FQueuedCall>> Queued;
};
namespace
{
bool Current(const TSharedRef<FPythonExecutionCallState, ESPMode::ThreadSafe>& State, const FString& Method)
{
    if (!IsInGameThread() || IsEngineExitRequested() || IsGarbageCollecting() || !State->Active.Load()) { return false; }
    auto* Subsystem = State->Subsystem.Get();
    if (!IsValid(Subsystem) || Subsystem->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed | RF_NeedLoad | RF_NeedPostLoad)) { return false; }
    const auto Actual = Subsystem->ToolsetRegistry.Find(ToolsetName);
    return Actual.Get() == State->Identity && Actual.IsValid() && KnownMethod(Method) && Actual->IsToolEnabled(FString(ToolsetName) + TEXT(".") + Method);
}
bool ActiveTransaction()
{
    if (!IsValid(GEditor) || GEditor->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) || !GEditor->Trans.IsResolved()) { return false; }
    auto* Transaction = GEditor->Trans.Get();
    return IsValid(Transaction) && !Transaction->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) && Transaction->IsActive();
}
FOutput Execute(const FRequest& Request, const FString& Method, bool Standalone, const TSharedRef<FPythonExecutionCallState, ESPMode::ThreadSafe>& State)
{
    if (!Current(State, Method)) { return Rejected(TEXT("python_execution_not_dispatched"), TEXT("Exact retained active handler unavailable/filtered; Python not dispatched")); }
    const bool Nested = State->ExecutionDepth > 0 || Cubeless::UE58::PythonExecution::IsExecuting() || ActiveTransaction();
    if (Cubeless::UE58::PythonExecution::RequiresStandaloneTicker(Request) && (!Standalone || Nested))
    { return Rejected(TEXT("python_import_requires_standalone_ticker"), TEXT("Import/deferred execution requires a standalone next-tick call, not synchronous Batch/reentrant execution")); }
    ++State->ExecutionDepth;
    Cubeless::UE58::PythonExecution::FDispatch Dispatch;
    Dispatch.StandaloneTicker = Standalone; Dispatch.SynchronousNested = Nested;
    Dispatch.IsCurrent = [State, Method]() { return Current(State, Method); };
    auto Actual = Cubeless::UE58::PythonExecution::Execute(Request, Dispatch);
    --State->ExecutionDepth; // Native state only, even after arbitrary Python retired the Registry.
    return ProjectPythonReceipt(Actual);
}
void DispatchOnGT(const FString& Name, const FString& Input,
    const TSharedRef<TPromise<FOutput>, ESPMode::ThreadSafe>& Promise,
    const TSharedRef<FPythonExecutionCallState, ESPMode::ThreadSafe>& State)
{
    if (!KnownMethod(Name) || !Current(State, Name))
    { Promise->SetValue(Rejected(TEXT("python_execution_not_dispatched"), TEXT("Named retained handler unavailable before dispatch"))); return; }
    TSharedPtr<FJsonObject> Args;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Args) || !Args.IsValid())
    { Promise->SetValue(Rejected(TEXT("invalid_python_arguments"), TEXT("Public JSON object required"))); return; }
    FRequest Request; FString ErrorCode, Error;
    const bool Parsed = Name == NativeMethodName
        ? Cubeless::UE58::PythonExecution::ParseNativeRequest(Args.ToSharedRef(), Request, ErrorCode, Error)
        : Cubeless::UE58::PythonExecution::ParsePublicRequest(Args.ToSharedRef(), Request, ErrorCode, Error);
    if (!Parsed)
    { Promise->SetValue(ProjectPythonReceipt(Cubeless::UE58::PythonExecution::Unexecuted(ErrorCode, Error))); return; }
    if (!Cubeless::UE58::PythonExecution::RequiresStandaloneTicker(Request))
    { Promise->SetValue(Execute(Request, Name, false, State)); return; }
    if (State->ExecutionDepth > 0 || Cubeless::UE58::PythonExecution::IsExecuting() || ActiveTransaction())
    { Promise->SetValue(Rejected(TEXT("python_import_requires_standalone_ticker"), TEXT("Import/deferred request cannot be queued inside synchronous Batch/reentrant transaction"))); return; }
    auto Job = MakeShared<FQueuedCall>(Request, Name, Promise); State->Queued.Add(Job);
    // Remove the state->job entry before execution; the ticker holds local Job/State.
    Job->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State, Job](float)
    {
        Job->Ticker.Reset(); State->Queued.Remove(Job);
        if (Job->Resolved) { return false; }
        if (!Current(State, Job->Method))
        { Job->Resolve(Rejected(TEXT("editor_shutdown_in_progress"), TEXT("Queued Python cancelled before execution; retained owner unavailable or shutdown started"))); return false; }
        Job->Resolve(Execute(Job->Request, Job->Method, true, State)); return false;
    }));
    if (!Job->Ticker.IsValid())
    { State->Queued.Remove(Job); Job->Resolve(Rejected(TEXT("python_ticker_unavailable"), TEXT("Could not schedule standalone Python; no execution"))); }
}
class FPythonExecutionToolset final : public FToolset
{
public:
    explicit FPythonExecutionToolset(const TSharedRef<FPythonExecutionCallState, ESPMode::ThreadSafe>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return ToolsetName; }
    virtual FString GetToolsetVersion() const override { return TEXT("1.0"); }
    virtual FString GetToolsetDescription() const override
    { return TEXT("Original Unreal Editor Python execution: public five caller fields (injected FastMCP context is not a JSON argument) and native seven fields with original ExecuteStatement default plus transaction/unsafe-PCG options. Both use one SDK core; actual logs/result, import policy/next-tick/CVar restoration and unknown outcome are retained. Does not complete other APIs."); }
protected:
    virtual FString GetJsonSchemaInternal() const override { return Schema(); }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Name, const FString& Input) override
    {
        auto Promise = MakeShared<TPromise<FOutput>, ESPMode::ThreadSafe>(); auto Future = Promise->GetFuture();
        if (IsInGameThread()) { DispatchOnGT(Name, Input, Promise, State); }
        else
        {
            // No UObject, weak pointer resolution or registry access on the worker thread.
            const auto Lease = State;
            AsyncTask(ENamedThreads::GameThread, [Name, Input, Promise, Lease]() { DispatchOnGT(Name, Input, Promise, Lease); });
        }
        return Future;
    }
private:
    TSharedRef<FPythonExecutionCallState, ESPMode::ThreadSafe> State;
};
}

bool FPythonExecutionRegistration::Register()
{
    if (!IsInGameThread() || !UObjectInitialized() || IsEngineExitRequested()) { return false; }
    if (Handler.IsValid())
    { auto* Actual = RegistrySubsystem.Get(); return IsValid(Actual) && Actual->ToolsetRegistry.Find(ToolsetName) == Handler; }
    const auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless original Python execution registration"));
    if (!Subsystem.HasValue() || Subsystem.GetValue()->ToolsetRegistry.Find(ToolsetName).IsValid()) { return false; }
    TStrongObjectPtr<UToolsetRegistrySubsystem> HeldSubsystem(Subsystem.GetValue().Get());
    auto State = MakeShared<FPythonExecutionCallState, ESPMode::ThreadSafe>(); State->Subsystem = HeldSubsystem.Get();
    TSharedPtr<FToolset> Candidate = MakeShared<FPythonExecutionToolset>(State); State->Identity = Candidate.Get();
    if (!HeldSubsystem->ToolsetRegistry.RegisterToolset(Candidate)) { return false; }
    // Registration broadcasts a public delegate; do not activate a stale/replaced owner.
    if (!IsValid(HeldSubsystem.Get()) || HeldSubsystem->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)
        || State->Subsystem.Get() != HeldSubsystem.Get() || !HasRetainedToolsetIdentity(HeldSubsystem->ToolsetRegistry, Candidate)) { return false; }
    RegistrySubsystem = HeldSubsystem.Get(); Handler = Candidate; CallState = State;
    Cubeless::UE58::PythonExecution::Activate(); State->Active.Store(true); return true;
}
void FPythonExecutionRegistration::Unregister()
{
    if (CallState.IsValid()) { CallState->Active.Store(false); }
    if (IsInGameThread())
    {
        if (CallState.IsValid()) { Cubeless::UE58::PythonExecution::Deactivate(); }
        if (CallState.IsValid())
        {
            const auto Pending = MoveTemp(CallState->Queued);
            for (const auto& Job : Pending)
            {
                if (Job->Ticker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Job->Ticker); Job->Ticker.Reset(); }
                Job->Resolve(Rejected(TEXT("editor_shutdown_in_progress"), TEXT("Queued Python cancelled before SDK dispatch by registration shutdown")));
            }
        }
        if (UObjectInitialized())
        {
            if (auto* Actual = RegistrySubsystem.Get())
            {
                if (Handler.IsValid() && HasRetainedToolsetIdentity(Actual->ToolsetRegistry, Handler))
                { Actual->ToolsetRegistry.UnregisterToolset(Handler); }
            }
        }
        // Never create a subsystem during teardown. Executing calls keep their copied state;
        // their next continuation observes Active=false and preserves dispatched-unknown.
        Handler.Reset(); CallState.Reset(); RegistrySubsystem.Reset();
    }
}
}
