// DJ: original dispatcher declaration6, call7 and bind7 on fixed named official routes.
#include "Registration/CubelessBlueprintDispatcherRawToolset.h"
#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Base64.h"
#include "Misc/Guid.h"
#include "Misc/CoreMisc.h"
#include "PythonScriptTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/Atomic.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogCubelessBlueprintDispatcherRaw, Log, All);

namespace Cubeless::Registration
{
struct FBlueprintDispatcherRawCallState
{
    TAtomic<bool> Active { false };
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    const UE::ToolsetRegistry::FToolset* Identity = nullptr;
};
namespace
{
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* ToolsetName = TEXT("CubelessMCPUE58Editor.CubelessBlueprintDispatcherTools");
constexpr const TCHAR* DeclareMethod = TEXT("add_blueprint_event_dispatcher");
constexpr const TCHAR* CallMethod = TEXT("add_blueprint_event_dispatcher_call_node");
constexpr const TCHAR* BindMethod = TEXT("add_blueprint_event_dispatcher_bind_node");
constexpr const TCHAR* AssignMethod = TEXT("add_blueprint_event_dispatcher_assign_node");
constexpr const TCHAR* UnbindMethod = TEXT("add_blueprint_event_dispatcher_unbind_node");
constexpr const TCHAR* ClearMethod = TEXT("add_blueprint_event_dispatcher_clear_node");
bool AllowedMethod(const FString& Name) { return Name == DeclareMethod || Name == CallMethod || Name == BindMethod || Name == AssignMethod || Name == UnbindMethod || Name == ClearMethod; }
constexpr const TCHAR* ResultPrefix = TEXT("__CUBELESS_BLUEPRINT_DISPATCHER_RESULT__");
constexpr int32 MaxInputBytes = 131072, MaxResultBytes = 16 * 1024 * 1024;

FString Schema()
{
    return FString(TEXT("{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintDispatcherTools\",\"version\":\"1.0\",\"tools\":[{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher\",\"description\":\"Original dispatcher declaration with opaque typed input descriptors and public truthy omission defaults. Native owns variable/signature graph/pin metadata and best-effort failure cleanup; structural helpers may regenerate skeleton synchronously. No atomic restoration, explicit compiler/save, runtime delegate firing or complete lifecycle pipeline claim. Also admits already-loaded exact WidgetBlueprints directly based on UserWidget, with an empty Canvas root, bounded own dispatcher state and an existing EventGraph. Dirty hosts can continue declaration/Call/Bind. Missing Widget graphs, broader Widget state and runtime execution remain incomplete; Widget Unbind/Clear/Assign use their separate lifecycle tools.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"dispatcher_name\":{\"type\":\"string\"},\"inputs\":{\"type\":[\"array\",\"null\"],\"items\":{\"type\":\"object\"},\"default\":null},\"category\":{\"type\":\"string\",\"default\":\"\"},\"tooltip\":{\"type\":\"string\",\"default\":\"\"},\"friendly_name\":{\"type\":\"string\",\"default\":\"\"}},\"required\":[\"blueprint_name\",\"dispatcher_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_call_node\",\"description\":\"Original dispatcher call-node construction with Any position and graph selectors. Native owns property/signature owner lookup, self context, allocation/reference/lifecycle and typed signature pins; graph creation and late failure prefixes remain. No explicit compile/save, dispatcher invocation, cleanup/replay or full inherited/cold/runtime pipeline claim. Also admits already-loaded exact WidgetBlueprints directly based on UserWidget, with an empty Canvas root, bounded own dispatcher state and an existing EventGraph. Dirty hosts can continue declaration/Call/Bind. Missing Widget graphs, broader Widget state and runtime execution remain incomplete; Widget Unbind/Clear/Assign use their separate lifecycle tools.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"dispatcher_name\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"dispatcher_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_bind_node\",\"description\":\"Original dispatcher bind-node construction with Any position and graph selectors. Native owns assignable property/signature/self context and actual reference/const Delegate pin plus lifecycle prefix. No actual event binding at runtime, explicit compile/save, cleanup/replay or full inherited/cold/lifecycle pipeline claim. Also admits already-loaded exact WidgetBlueprints directly based on UserWidget, with an empty Canvas root, bounded own dispatcher state and an existing EventGraph. Dirty hosts can continue declaration/Call/Bind. Missing Widget graphs, broader Widget state and runtime execution remain incomplete; Widget Unbind/Clear/Assign use their separate lifecycle tools.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"dispatcher_name\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"bl")) + TEXT("ueprint_name\",\"dispatcher_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_unbind_node\",\"description\":\"Original Unbind node with Any position and literal graph selectors; actual member/self/signature and reference+const Delegate pins are native-owned. Late allocation/compatibility prefix preserved; authors future execution, no immediate unbinding or explicit compiler/save/retry/runtime completion. Also admits bounded already-loaded exact WidgetBlueprints directly based on UserWidget, with an empty Canvas root, an own dispatcher and an existing EventGraph. Dirty hosts can continue lifecycle authoring. Assign preserves its actual implicit typed CustomEvent and Delegate link; Unbind/Clear author nodes without invoking runtime cleanup. Broader Widget state, missing Widget graph creation and runtime broadcast remain incomplete.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"dispatcher_name\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"dispatcher_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_clear_node\",\"description\":\"Original Clear node with Any position and literal graph selectors; actual exec/then/self only, no Delegate pin. Late allocation/reference/compatibility prefix preserved; authors future execution, no immediate delegate clearing or explicit compiler/save/retry/runtime completion. Also admits bounded already-loaded exact WidgetBlueprints directly based on UserWidget, with an empty Canvas root, an own dispatcher and an existing EventGraph. Dirty hosts can continue lifecycle authoring. Assign preserves its actual implicit typed CustomEvent and Delegate link; Unbind/Clear author nodes without invoking runtime cleanup. Broader Widget state, missing Widget graph creation and runtime broadcast remain incomplete.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"dispatcher_name\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"dispatcher_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_assign_node\",\"description\":\"Original compound Assign node with Any position and literal graph selectors. Native preserves Allocate-before-PostPlaced and actual automat") + TEXT("ic typed CustomEvent/offset/grid/link plus structural work. A nonnull or void helper result alone is not a complete wiring proof; full partial main/event/link/source prefix survives errors. No atomic rollback, explicit compile/save/retry/runtime or full pipeline claim. Also admits bounded already-loaded exact WidgetBlueprints directly based on UserWidget, with an empty Canvas root, an own dispatcher and an existing EventGraph. Dirty hosts can continue lifecycle authoring. Assign preserves its actual implicit typed CustomEvent and Delegate link; Unbind/Clear author nodes without invoking runtime cleanup. Broader Widget state, missing Widget graph creation and runtime broadcast remain incomplete.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"dispatcher_name\":{\"type\":\"string\"},\"node_position\":{\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"dispatcher_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}}]}");
}

bool TypedResult(const FString& Text)
{
    TSharedPtr<FJsonObject> O;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O.IsValid() || O->Values.Num() != 1) { return false; }
    const TSharedPtr<FJsonObject>* Value = nullptr;
    if (!O->TryGetObjectField(TEXT("returnValue"), Value) || !Value || !Value->IsValid() || (*Value)->Values.Num() != 11) { return false; }
    const auto Success = (*Value)->TryGetField(TEXT("success"));
    if (!Success.IsValid() || Success->Type != EJson::Boolean) { return false; }
    for (const TCHAR* Key : { TEXT("operation_id"), TEXT("stage"), TEXT("error_code"), TEXT("error"), TEXT("changes_json"),
        TEXT("compile_json"), TEXT("save_json"), TEXT("validation_json"), TEXT("retry_json"), TEXT("data_json") })
    { const auto Field = (*Value)->TryGetField(Key); if (!Field.IsValid() || Field->Type != EJson::String) { return false; } }
    return true;
}

FString FailureReceipt(const FPythonCommandEx& Command, bool Executed, const TCHAR* Reason)
{
    auto O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("error"), Reason);
    O->SetStringField(TEXT("scope"), TEXT("Python statement was dispatched; mutation outcome is unverified unless the exact emitted operation receipt is independently recovered. No retry/rollback/secondary save."));
    O->SetBoolField(TEXT("python_statement_completed"), Executed); O->SetStringField(TEXT("command_result"), Command.CommandResult);
    TArray<TSharedPtr<FJsonValue>> Logs;
    for (const auto& Entry : Command.LogOutput)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("type"), LexToString(Entry.Type)); Row->SetStringField(TEXT("output"), Entry.Output);
        Logs.Add(MakeShared<FJsonValueObject>(Row));
    }
    O->SetArrayField(TEXT("captured_log_output"), Logs);
    FString Out; FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Out)); return Out;
}

FOutput Run(const FString& Name, const FString& Input, const TSharedRef<FBlueprintDispatcherRawCallState, ESPMode::ThreadSafe>& State)
{
    if (!AllowedMethod(Name) || !IsInGameThread() || IsEngineExitRequested() || !State->Active.Load())
    { return MakeError(FString(TEXT("blueprint_dispatcher_raw_not_dispatched: game-thread/live owned handler required."))); }
    auto Subsystem = State->Subsystem.Get();
    if (!Subsystem || Subsystem->ToolsetRegistry.Find(ToolsetName).Get() != State->Identity)
    { return MakeError(FString(TEXT("blueprint_dispatcher_raw_not_dispatched: exact retained Registry handler is unavailable or filtered."))); }
    IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
    if (!Python || !Python->IsPythonAvailable() || !Python->IsPythonInitialized())
    { return MakeError(FString(TEXT("blueprint_dispatcher_raw_not_dispatched: existing initialized Python required; no load/force-enable/fallback."))); }
    const FTCHARToUTF8 UTF8(*Input, Input.Len());
    if (UTF8.Length() > MaxInputBytes)
    { return MakeError(FString(TEXT("blueprint_dispatcher_raw_not_dispatched: raw request exceeds128KiB UTF8 limit."))); }
    const FUTF8ToTCHAR InputRoundTrip(reinterpret_cast<const UTF8CHAR*>(UTF8.Get()), UTF8.Length());
    if (FString(InputRoundTrip.Length(), InputRoundTrip.Get()) != Input)
    { return MakeError(FString(TEXT("blueprint_dispatcher_raw_not_dispatched: input is not lossless valid Unicode."))); }
    const FString Encoded = FBase64::Encode(reinterpret_cast<const uint8*>(UTF8.Get()), UTF8.Length());
    const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
    FPythonCommandEx Command; Command.Flags = EPythonCommandFlags::Unattended;
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    Command.Command = FString::Printf(TEXT("from cubeless_mcp_ue58.registration.raw_blueprint_dispatcher import emit_result as _cubeless_blueprint_dispatcher_emit; _cubeless_blueprint_dispatcher_emit('%s','%s','%s')"), *Name, *Encoded, *Nonce);
    // Interpolated method is one of six exact literals; remaining values are local Base64/GUID.
    const bool Executed = Python->ExecPythonCommandEx(Command);
    const FString Prefix = FString(ResultPrefix) + Nonce + TEXT(":"); FString Payload; int32 Markers = 0;
    for (const auto& Entry : Command.LogOutput)
    {
        TArray<FString> Lines; Entry.Output.ParseIntoArrayLines(Lines, false);
        for (const auto& Line : Lines) { if (Line.StartsWith(Prefix, ESearchCase::CaseSensitive)) { ++Markers; Payload = Line.Mid(Prefix.Len()); } }
    }
    TArray<uint8> Bytes;
    if (Markers != 1 || Payload.Len() > ((MaxResultBytes + 2) / 3) * 4 || !FBase64::Decode(Payload, Bytes)
        || Bytes.Num() == 0 || Bytes.Num() > MaxResultBytes || FBase64::Encode(Bytes) != Payload)
    { return MakeError(FailureReceipt(Command, Executed, TEXT("blueprint_dispatcher_raw_return_unverified: missing/multiple/invalid/oversized exact nonce marker."))); }
    const FUTF8ToTCHAR Decoded(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
    const FString Result(Decoded.Length(), Decoded.Get()); const FTCHARToUTF8 RoundTrip(*Result, Result.Len());
    if (RoundTrip.Length() != Bytes.Num() || FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) != 0 || !TypedResult(Result))
    { return MakeError(FailureReceipt(Command, Executed, TEXT("blueprint_dispatcher_raw_return_unverified: emitted bytes are not exact UTF8/standard typed returnValue JSON."))); }
    // A valid emitted receipt remains authoritative even if later Python logging
    // reported an exception. Never synthesize known0 after Python dispatch.
    if (!Executed) { UE_LOG(LogCubelessBlueprintDispatcherRaw, Warning, TEXT("Dispatcher Python statement reported failure after a valid operation receipt; returned receipt preserved.")); }
    return MakeValue(Result);
}

class FBlueprintDispatcherRawToolset final : public UE::ToolsetRegistry::FToolset
{
public:
    explicit FBlueprintDispatcherRawToolset(const TSharedRef<FBlueprintDispatcherRawCallState, ESPMode::ThreadSafe>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return ToolsetName; }
    virtual FString GetToolsetVersion() const override { return TEXT("1.0"); }
    virtual FString GetToolsetDescription() const override { return TEXT("Original dispatcher declaration6, call7, bind7, assign7, unbind7 and clear7. Fixed journaled Engine authoring preserves signature descriptors, Assign implicit typed event/link creation and partial cleanup/lifecycle reports. No atomic rollback or complete runtime broadcast/pipeline proof."); }
protected:
    virtual FString GetJsonSchemaInternal() const override { return Schema(); }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Name, const FString& Input) override
    {
        if (!AllowedMethod(Name)) { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("blueprint_dispatcher_raw_not_dispatched: exact named method required.")))).GetFuture(); }
        const FTCHARToUTF8 InputBytes(*Input, Input.Len());
        if (InputBytes.Length() > MaxInputBytes)
        { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("blueprint_dispatcher_raw_not_dispatched: raw request exceeds128KiB UTF8 limit.")))).GetFuture(); }
        // Capture only strings and shared lifetime state; no UObject is accessed off the game thread.
        if (IsInGameThread()) { return MakeFulfilledPromise<FOutput>(Run(Name, Input, State)).GetFuture(); }
        auto Promise = MakeShared<TPromise<FOutput>, ESPMode::ThreadSafe>(); auto Future = Promise->GetFuture();
        const auto LiveState = State;
        AsyncTask(ENamedThreads::GameThread, [Promise, LiveState, Name, Input]() { Promise->SetValue(Run(Name, Input, LiveState)); });
        return Future;
    }
private:
    TSharedRef<FBlueprintDispatcherRawCallState, ESPMode::ThreadSafe> State;
};
}

bool FBlueprintDispatcherRawRegistration::Register()
{
    if (!IsInGameThread() || !UObjectInitialized() || IsEngineExitRequested()) { return false; }
    if (Handler.IsValid())
    { auto Subsystem = RegistrySubsystem.Get(); return Subsystem && Subsystem->ToolsetRegistry.Find(ToolsetName) == Handler; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless original blueprint dispatcher raw registration"));
    if (!Subsystem.HasValue()) { return false; }
    if (Subsystem.GetValue()->ToolsetRegistry.Find(ToolsetName).IsValid()) { return false; } // Never replace any existing handler.
    auto State = MakeShared<FBlueprintDispatcherRawCallState, ESPMode::ThreadSafe>(); State->Subsystem = Subsystem.GetValue().Get();
    TSharedPtr<UE::ToolsetRegistry::FToolset> Candidate = MakeShared<FBlueprintDispatcherRawToolset>(State);
    State->Identity = Candidate.Get();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterToolset(Candidate)) { return false; }
    RegistrySubsystem = Subsystem.GetValue().Get(); Handler = Candidate; CallState = State; State->Active.Store(true);
    return true;
}

void FBlueprintDispatcherRawRegistration::Unregister()
{
    if (CallState.IsValid()) { CallState->Active.Store(false); } // Queued requests must not begin Python after shutdown.
    if (IsInGameThread() && UObjectInitialized())
    {
        if (auto Subsystem = RegistrySubsystem.Get())
        {
            if (Handler.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, Handler))
            { Subsystem->ToolsetRegistry.UnregisterToolset(Handler); }
        }
    }
    Handler.Reset(); CallState.Reset(); RegistrySubsystem.Reset(); // No new subsystem discovery during teardown.
}
}
