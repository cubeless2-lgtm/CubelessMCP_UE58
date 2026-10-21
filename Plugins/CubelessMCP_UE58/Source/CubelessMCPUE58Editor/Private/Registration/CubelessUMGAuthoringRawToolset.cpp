// DS: original UMG Create3/Text7/Button8 on fixed named official routes.
#include "Registration/CubelessUMGAuthoringRawToolset.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogCubelessUMGAuthoringRaw, Log, All);

namespace Cubeless::Registration
{
struct FUMGAuthoringRawCallState
{
    TAtomic<bool> Active { false };
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    const UE::ToolsetRegistry::FToolset* Identity = nullptr;
};
namespace
{
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* ToolsetName = TEXT("CubelessMCPUE58Editor.CubelessUMGAuthoringTools");
constexpr const TCHAR* CreateMethod = TEXT("create_umg_widget_blueprint");
constexpr const TCHAR* TextMethod = TEXT("add_text_block_to_widget");
constexpr const TCHAR* ButtonMethod = TEXT("add_button_to_widget");
bool AllowedMethod(const FString& Name) { return Name == CreateMethod || Name == TextMethod || Name == ButtonMethod; }
constexpr const TCHAR* ResultPrefix = TEXT("__CUBELESS_UMG_AUTHORING_RESULT__");
constexpr int32 MaxInputBytes = 131072, MaxResultBytes = 16 * 1024 * 1024;

FString Schema()
{
    return FString(TEXT("{\"name\":\"CubelessMCPUE58Editor.CubelessUMGAuthoringTools\",\"version\":\"1.0\",\"tools\":[{\"name\":\"CubelessMCPUE58Editor.CubelessUMGAuthoringTools.create_umg_widget_blueprint\",\"description\":\"Original Create3 preserves parent_class UserWidget and path /Game/UI. Native resolves supported UUserWidget parent, creates WidgetBlueprint/tree/RootCanvas, registers asset and performs original structural/full compiler then force-save. Actual flags, legacy result and partial tree/package/save prefixes precede errors; compiler and save are not Python flags or coalesced Batch work. Ordinary UMG assets use stock Engine/UMG runtime types. EditorUtility/widget class variants, renderer/cold/runtime/full pipeline remain separately scoped.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"widget_name\":{\"type\":\"string\"},\"parent_class\":{\"type\":\"string\",\"default\":\"UserWidget\"},\"path\":{\"type\":\"string\",\"default\":\"/Game/UI\"}},\"required\":[\"widget_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessUMGAuthoringTools.add_text_block_to_widget\",\"description\":\"Original Text7 preserves numeric arrays and default values without length or geometry clamps. Native adds named TextBlock/text/RootCanvas/slot position-size and generated variable, then original structural/full compiler and force-save. font_size/color are forwarded but ignored by original handlers; no style-applied claim. Bare target names resolve original /Game/Widgets, while create defaults /Game/UI; use returned full asset path. Full actual partial/known-null/raw/compiler/save prefixes precede errors, no wrapper retry or extra save.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"widget_name\":{\"type\":\"string\"},\"text_block_name\":{\"type\":\"string\"},\"text\":{\"type\":\"string\",\"default\":\"\"},\"position\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"default\":[0.0,0.0]},\"size\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"default\":[200.0,50.0]},\"font_size\":{\"type\":\"integer\",\"default\":12},\"color\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"default\":[1.0,1.0,1.0,1.0]}},\"required\":[\"widget_name\",\"text_block_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessUMGAuthoringTools.add_button_to_widget\",\"description\":\"Original Button8 creates Button plus separately named button_name_Text TextBlock child before RootCanvas/slot position-size/generated variable work and original structural/full compiler/force-save. Numeric arrays/defaults remain whole data. font_size/color/background_color are forwarded but ignored by original handlers. Full returned legacy/tree/child/slot/partial/compiler/save prefix is retained before errors; no Python style/geometry mutation, compiler/save c")) + TEXT("oalescing, atomic cleanup, replay or UMG render/runtime/cold/full pipeline claim.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"widget_name\":{\"type\":\"string\"},\"button_name\":{\"type\":\"string\"},\"text\":{\"type\":\"string\",\"default\":\"\"},\"position\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"default\":[0.0,0.0]},\"size\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"default\":[200.0,50.0]},\"font_size\":{\"type\":\"integer\",\"default\":12},\"color\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"default\":[1.0,1.0,1.0,1.0]},\"background_color\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"default\":[0.1,0.1,0.1,1.0]}},\"required\":[\"widget_name\",\"button_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}}]}");
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

FOutput Run(const FString& Name, const FString& Input, const TSharedRef<FUMGAuthoringRawCallState, ESPMode::ThreadSafe>& State)
{
    if (!AllowedMethod(Name) || !IsInGameThread() || IsEngineExitRequested() || !State->Active.Load())
    { return MakeError(FString(TEXT("umg_authoring_raw_not_dispatched: game-thread/live owned handler required."))); }
    auto Subsystem = State->Subsystem.Get();
    if (!Subsystem || Subsystem->ToolsetRegistry.Find(ToolsetName).Get() != State->Identity)
    { return MakeError(FString(TEXT("umg_authoring_raw_not_dispatched: exact retained Registry handler is unavailable or filtered."))); }
    IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
    if (!Python || !Python->IsPythonAvailable() || !Python->IsPythonInitialized())
    { return MakeError(FString(TEXT("umg_authoring_raw_not_dispatched: existing initialized Python required; no load/force-enable/fallback."))); }
    const FTCHARToUTF8 UTF8(*Input, Input.Len());
    if (UTF8.Length() > MaxInputBytes)
    { return MakeError(FString(TEXT("umg_authoring_raw_not_dispatched: raw request exceeds128KiB UTF8 limit."))); }
    const FUTF8ToTCHAR InputRoundTrip(reinterpret_cast<const UTF8CHAR*>(UTF8.Get()), UTF8.Length());
    if (FString(InputRoundTrip.Length(), InputRoundTrip.Get()) != Input)
    { return MakeError(FString(TEXT("umg_authoring_raw_not_dispatched: input is not lossless valid Unicode."))); }
    const FString Encoded = FBase64::Encode(reinterpret_cast<const uint8*>(UTF8.Get()), UTF8.Length());
    const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
    FPythonCommandEx Command; Command.Flags = EPythonCommandFlags::Unattended;
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    Command.Command = FString::Printf(TEXT("from cubeless_mcp_ue58.registration.raw_umg_authoring import emit_result as _cubeless_umg_authoring_emit; _cubeless_umg_authoring_emit('%s','%s','%s')"), *Name, *Encoded, *Nonce);
    // Interpolated method is one of three exact literals; remaining values are local Base64/GUID.
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
    { return MakeError(FailureReceipt(Command, Executed, TEXT("umg_authoring_raw_return_unverified: missing/multiple/invalid/oversized exact nonce marker."))); }
    const FUTF8ToTCHAR Decoded(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
    const FString Result(Decoded.Length(), Decoded.Get()); const FTCHARToUTF8 RoundTrip(*Result, Result.Len());
    if (RoundTrip.Length() != Bytes.Num() || FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) != 0 || !TypedResult(Result))
    { return MakeError(FailureReceipt(Command, Executed, TEXT("umg_authoring_raw_return_unverified: emitted bytes are not exact UTF8/standard typed returnValue JSON."))); }
    // A valid emitted receipt remains authoritative even if later Python logging
    // reported an exception. Never synthesize known0 after Python dispatch.
    if (!Executed) { UE_LOG(LogCubelessUMGAuthoringRaw, Warning, TEXT("UMG authoring Python statement reported failure after a valid operation receipt; returned receipt preserved.")); }
    return MakeValue(Result);
}

class FUMGAuthoringRawToolset final : public UE::ToolsetRegistry::FToolset
{
public:
    explicit FUMGAuthoringRawToolset(const TSharedRef<FUMGAuthoringRawCallState, ESPMode::ThreadSafe>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return ToolsetName; }
    virtual FString GetToolsetVersion() const override { return TEXT("1.0"); }
    virtual FString GetToolsetDescription() const override { return TEXT("Original UMG Create3/Text7/Button8 retain Canvas root, authored variable callbacks and compound Button plus named Text child. Native owns original structural/full compiler/force-save and actual partial tree/package prefixes. Numeric array defaults remain whole data; original font/color/background arguments are forwarded but ignored, not claimed applied. Bare target paths preserve original /Game/Widgets lookup versus create /Game/UI default. Editor-only UMG/UMGEditor link dependencies; no custom game runtime class, coalesced compile/save, rollback, replay or full runtime/pipeline proof."); }
protected:
    virtual FString GetJsonSchemaInternal() const override { return Schema(); }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Name, const FString& Input) override
    {
        if (!AllowedMethod(Name)) { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("umg_authoring_raw_not_dispatched: exact named method required.")))).GetFuture(); }
        const FTCHARToUTF8 InputBytes(*Input, Input.Len());
        if (InputBytes.Length() > MaxInputBytes)
        { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("umg_authoring_raw_not_dispatched: raw request exceeds128KiB UTF8 limit.")))).GetFuture(); }
        // Capture only strings and shared lifetime state; no UObject is accessed off the game thread.
        if (IsInGameThread()) { return MakeFulfilledPromise<FOutput>(Run(Name, Input, State)).GetFuture(); }
        auto Promise = MakeShared<TPromise<FOutput>, ESPMode::ThreadSafe>(); auto Future = Promise->GetFuture();
        const auto LiveState = State;
        AsyncTask(ENamedThreads::GameThread, [Promise, LiveState, Name, Input]() { Promise->SetValue(Run(Name, Input, LiveState)); });
        return Future;
    }
private:
    TSharedRef<FUMGAuthoringRawCallState, ESPMode::ThreadSafe> State;
};
}

bool FUMGAuthoringRawRegistration::Register()
{
    if (!IsInGameThread() || !UObjectInitialized() || IsEngineExitRequested()) { return false; }
    if (Handler.IsValid())
    { auto Subsystem = RegistrySubsystem.Get(); return Subsystem && Subsystem->ToolsetRegistry.Find(ToolsetName) == Handler; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless original UMG authoring raw registration"));
    if (!Subsystem.HasValue()) { return false; }
    if (Subsystem.GetValue()->ToolsetRegistry.Find(ToolsetName).IsValid()) { return false; } // Never replace any existing handler.
    auto State = MakeShared<FUMGAuthoringRawCallState, ESPMode::ThreadSafe>(); State->Subsystem = Subsystem.GetValue().Get();
    TSharedPtr<UE::ToolsetRegistry::FToolset> Candidate = MakeShared<FUMGAuthoringRawToolset>(State);
    State->Identity = Candidate.Get();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterToolset(Candidate)) { return false; }
    RegistrySubsystem = Subsystem.GetValue().Get(); Handler = Candidate; CallState = State; State->Active.Store(true);
    return true;
}

void FUMGAuthoringRawRegistration::Unregister()
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
