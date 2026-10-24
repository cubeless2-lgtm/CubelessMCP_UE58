// One original12 tool on the official raw FToolset seam. No generic evaluator.
#include "Registration/CubelessLocalVariableRawToolset.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogCubelessLocalVariableRaw, Log, All);

namespace Cubeless::Registration
{
struct FLocalVariableRawCallState
{
    TAtomic<bool> Active { false };
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    const UE::ToolsetRegistry::FToolset* Identity = nullptr;
};
namespace
{
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* ToolsetName = TEXT("CubelessMCPUE58Editor.CubelessLocalVariableTools");
constexpr const TCHAR* MethodName = TEXT("add_blueprint_local_variable");
constexpr const TCHAR* ResultPrefix = TEXT("__CUBELESS_LOCAL_VARIABLE_RESULT__");
constexpr int32 MaxInputBytes = 131072, MaxResultBytes = 16 * 1024 * 1024;

FString Schema()
{
    // Explicit raw schema, not a reflected property TYPE schema or Duplicate callback.
    // default_value is original unrestricted JSON; held CD has a finite scalar local slice.
    return
        TEXT("{\"name\":\"CubelessMCPUE58Editor.CubelessLocalVariableTools\",\"version\":\"1.0\",\"description\":\"Original local-variable12 with guarded declarations and additional missing-function existing supported descriptor declarations.\",\"tools\":[{\"name\":\"CubelessMCPUE58Editor.CubelessLocalVariableTools.add_blueprint_local_variable\",\"description\":\"The same guarded missing-function continuation additionally admits existing normal Actor supported primitive/reference arrays, Enum, native Object/Class, Vector/Rotator/Transform, scalar UDS and exact String-double Map declarations. Existing type parsing and source preservation stay authoritative. Only known successful function creation refreshes the target class roles using existing guarded RefreshClasses; source snapshots remain unchanged, and failed/unknown creation never dispatches refresh/local writes. Existing current local baseline, lease retirement, creation/partial receipts and one outer transaction remain. New descriptors, BFL/other owners/selector variants and runtime values remain incomplete or usage pending. Primitive scalar bool/int/int64/float/double/string/name/text additionally creates one missing explicitly named own Actor function when graph_type=function, create_graph_if_missing=True and graph_id empty. Same loaded Blueprint and existing helper/local writer share one outer transaction; creation prefix and later local partial state remain visible without rollback or retry. Source-backed/container/new-owner missing-function variants remain incomplete. No explicit full compile/save or runtime initialization proof. Add one ordinary scalar, exact native Vector/Rotator, enum/byteenum, or exact native object/class local declaration to an existing editable own user function of a direct native Actor Blueprint. Enum/byteenum and native")
        TEXT(" Object/Class additionally support is_array=true own-function local declarations and Get/Set. Enum arrays require omitted/null default_value; nonnull defaults retain the original prewriter rejection. Native Object/Class arrays retain original Any-to-single-declaration-string storage, distinct from stock pin defaults and runtime array initialization. Inner ownership, source Enum/PropertyClass/MetaClass and current/GUID/pin preservation are checked. Enum requires an exact read-only Game/Engine UserDefinedEnum type_object and an optional entry-name or integral-byte default. Object/class constraints admit currently registered /Script/Engine.Actor or /Script/CoreUObject.Object (short Actor/Object; omitted type_object defaults to Object), observed read-only without class loading or instance/CDO creation. Object defaults admit omitted/null/empty/None; Class also admits the exact constraint path as an unchanged declaration literal, unlike function-parameter pin defaults. SDK object-Actor local setup may add DisableEditOnTemplate. Transform additionally supports is_array=true local declaration using its exact held CoreUObject Transform/Quat/Vector source and original single transform declaration string, with location/translation, rotation and scale/scale3d aliases/defaults; this does not initialize array elements or runtime values. Exact CoreUObject.Transform additionally supports scalar declarations in the existing own Actor function, with current scalar Get/Set source and whole-pin guards separate from Array/Inner. Checked original Any/group/axis/fallback conversion stores one scalar declaration string, with finite-double numbers and ignored implicit type_object; omitted/null stores empty text. Held Transform/Quat/Vector direct bindings and nested native fields/layout/metadata")
        TEXT(" are preserved. Stored declaration text does not prove stock pin defaults, default-buffer import or runtime initialization. A separate scalar struct/scriptstruct route admits type_object naming an exact already-loaded saved-clean Game UDS with 1..32 own FString/int32/int64/float/double/bool/Name fields. Original Any fallback stores one declaration string, with omitted/null retaining omission; no UDS field-name guessing or Python value-shape restriction is added. Held source GUID/EditorData/direct fields/layout/metadata/file and scalar function ownership/local GUID/current whole pins remain guarded for declaration/Get/Set. Stored text does not prove stock pin defaults, default-buffer import or runtime initialization. A separate is_array=true struct/scriptstruct local route now uses that same exact already-loaded saved-clean 1..32 own scalar-field UDS profile for declaration/Get/Set. Current Array/Inner ownership and held UDS binding preserve source GUID/EditorData/fields/layout/metadata/file, local GUID and whole stock pins. Original Any still stores one declaration string; it does not initialize array elements or prove stock raw/Auto/Object/FText defaults. No guessed field names, source loader, default-buffer/value sampling or runtime evaluation is added. UDS nested/math/reference fields, loaders, other scalar structs and broader contexts remain incomplete. Vector/Rotator use exact CoreUObject value structures for scalar or is_array=true local declarations with no type_object loader; their array/object defaults retain original three-axis declaration text storage, not per-element array initialization or runtime buffer import: array first3 axes or lowercase x/y/z and pitch/yaw/roll object axes become original SanitizeFloat storage; Missing axes or failed numeric conversio")
        TEXT("ns become0 and extra axes are ignored; bool/numeric-string axes retain original TryGetNumber conversion within finite bounds. Strings and bool/number fallback remain unchanged declaration literals, without value parsing/runtime evaluation. Exact map_string_double additionally admits an empty own-function String-key/double-value Map declaration with omitted/null or empty-object default_value, is_array=false and empty type_object. It does not import Map values or evaluate runtime locals. The exact own PresetLoader function of a member-free native BlueprintFunctionLibrary additionally admits this String-double Map local declaration; scalar local behavior is unchanged and full runtime/pipeline proof remains incomplete. Primitive bool/int/int64/float/double/string/name/text additionally support is_array=true with empty type_object. Boolean array explicit defaults retain the original JSON-boolean requirement; other primitive array defaults retain original literal/JSON storage, distinct from stock pin defaults and runtime array initialization. After the existing structural and preservation checks, an exact current own-local Array/Inner plan is reused; a failed read preserves the known declaration prefix and returns incomplete without retry. No explicit full compile/save; SDK structural refresh may compile skeleton state. Omitted/null stores empty text, unlike empty array/object zero axes. Source identity/member signature/metadata/package stay readonly. Original default_value accepts intact JSON object/array/scalar/null; native finite admission may refuse unsupported shapes. No generated/plugin classes, broader class loader, non-null object instance defaults, broader graph creation, explicit full compile/save, runtime local evaluation or complete arbitrary-type parity.\",\"inputS")
        TEXT("chema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"variable_name\":{\"type\":\"string\"},\"variable_type\":{\"type\":\"string\"},\"default_value\":{\"default\":null},\"category\":{\"type\":\"string\",\"default\":\"\"},\"tooltip\":{\"type\":\"string\",\"default\":\"\"},\"type_object\":{\"type\":\"string\",\"default\":\"\"},\"is_array\":{\"type\":\"boolean\",\"default\":false},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"function\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"variable_name\",\"variable_type\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}}]}");
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

FOutput Run(const FString& Input, const TSharedRef<FLocalVariableRawCallState, ESPMode::ThreadSafe>& State)
{
    if (!IsInGameThread() || IsEngineExitRequested() || !State->Active.Load())
    { return MakeError(FString(TEXT("local_variable_raw_not_dispatched: game-thread/live owned handler required."))); }
    auto Subsystem = State->Subsystem.Get();
    if (!Subsystem || Subsystem->ToolsetRegistry.Find(ToolsetName).Get() != State->Identity)
    { return MakeError(FString(TEXT("local_variable_raw_not_dispatched: exact retained Registry handler is unavailable or filtered."))); }
    IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
    if (!Python || !Python->IsPythonAvailable() || !Python->IsPythonInitialized())
    { return MakeError(FString(TEXT("local_variable_raw_not_dispatched: existing initialized Python required; no load/force-enable/fallback."))); }
    const FTCHARToUTF8 UTF8(*Input, Input.Len());
    if (UTF8.Length() > MaxInputBytes)
    { return MakeError(FString(TEXT("local_variable_raw_not_dispatched: raw request exceeds128KiB UTF8 limit."))); }
    const FUTF8ToTCHAR InputRoundTrip(reinterpret_cast<const UTF8CHAR*>(UTF8.Get()), UTF8.Length());
    if (FString(InputRoundTrip.Length(), InputRoundTrip.Get()) != Input)
    { return MakeError(FString(TEXT("local_variable_raw_not_dispatched: input is not lossless valid Unicode."))); }
    const FString Encoded = FBase64::Encode(reinterpret_cast<const uint8*>(UTF8.Get()), UTF8.Length());
    const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
    FPythonCommandEx Command; Command.Flags = EPythonCommandFlags::Unattended;
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    Command.Command = FString::Printf(TEXT("from cubeless_mcp_ue58.registration.raw_local_variable import emit_result as _cubeless_local_variable_emit; _cubeless_local_variable_emit('%s','%s')"), *Encoded, *Nonce);
    // Interpolated values are only locally generated Base64 and lowercase GUID.
    const bool Executed = Python->ExecPythonCommandEx(Command);
    const FString Prefix = FString(ResultPrefix) + Nonce + TEXT(":"); FString Payload; int32 Markers = 0;
    for (const auto& Entry : Command.LogOutput)
    {
        TArray<FString> Lines; Entry.Output.ParseIntoArrayLines(Lines, false);
        for (const auto& Line : Lines) { if (Line.StartsWith(Prefix)) { ++Markers; Payload = Line.Mid(Prefix.Len()); } }
    }
    TArray<uint8> Bytes;
    if (Markers != 1 || Payload.Len() > ((MaxResultBytes + 2) / 3) * 4 || !FBase64::Decode(Payload, Bytes)
        || Bytes.Num() == 0 || Bytes.Num() > MaxResultBytes || FBase64::Encode(Bytes) != Payload)
    { return MakeError(FailureReceipt(Command, Executed, TEXT("local_variable_raw_return_unverified: missing/multiple/invalid/oversized exact nonce marker."))); }
    const FUTF8ToTCHAR Decoded(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
    const FString Result(Decoded.Length(), Decoded.Get()); const FTCHARToUTF8 RoundTrip(*Result, Result.Len());
    if (RoundTrip.Length() != Bytes.Num() || FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) != 0 || !TypedResult(Result))
    { return MakeError(FailureReceipt(Command, Executed, TEXT("local_variable_raw_return_unverified: emitted bytes are not exact UTF8/standard typed returnValue JSON."))); }
    // A valid emitted receipt remains authoritative even if later Python logging
    // reported an exception. Never synthesize known0 after Python dispatch.
    if (!Executed) { UE_LOG(LogCubelessLocalVariableRaw, Warning, TEXT("Local-variable Python statement reported failure after a valid operation receipt; returned receipt preserved.")); }
    return MakeValue(Result);
}

class FLocalVariableRawToolset final : public UE::ToolsetRegistry::FToolset
{
public:
    explicit FLocalVariableRawToolset(const TSharedRef<FLocalVariableRawCallState, ESPMode::ThreadSafe>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return ToolsetName; }
    virtual FString GetToolsetVersion() const override { return TEXT("1.0"); }
    virtual FString GetToolsetDescription() const override { return TEXT("One original local-variable12 with intact raw JSON and a fixed named journaled action."); }
protected:
    virtual FString GetJsonSchemaInternal() const override { return Schema(); }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Name, const FString& Input) override
    {
        if (Name != MethodName) { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("local_variable_raw_not_dispatched: exact named method required.")))).GetFuture(); }
        const FTCHARToUTF8 InputBytes(*Input, Input.Len());
        if (InputBytes.Length() > MaxInputBytes)
        { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("local_variable_raw_not_dispatched: raw request exceeds128KiB UTF8 limit.")))).GetFuture(); }
        // Capture only strings and shared lifetime state; no UObject is accessed off the game thread.
        if (IsInGameThread()) { return MakeFulfilledPromise<FOutput>(Run(Input, State)).GetFuture(); }
        auto Promise = MakeShared<TPromise<FOutput>, ESPMode::ThreadSafe>(); auto Future = Promise->GetFuture();
        const auto LiveState = State;
        AsyncTask(ENamedThreads::GameThread, [Promise, LiveState, Input]() { Promise->SetValue(Run(Input, LiveState)); });
        return Future;
    }
private:
    TSharedRef<FLocalVariableRawCallState, ESPMode::ThreadSafe> State;
};
}

bool FLocalVariableRawRegistration::Register()
{
    if (!IsInGameThread() || !UObjectInitialized() || IsEngineExitRequested()) { return false; }
    if (Handler.IsValid())
    { auto Subsystem = RegistrySubsystem.Get(); return Subsystem && Subsystem->ToolsetRegistry.Find(ToolsetName) == Handler; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless original local-variable raw registration"));
    if (!Subsystem.HasValue()) { return false; }
    if (Subsystem.GetValue()->ToolsetRegistry.Find(ToolsetName).IsValid()) { return false; } // Never replace any existing handler.
    auto State = MakeShared<FLocalVariableRawCallState, ESPMode::ThreadSafe>(); State->Subsystem = Subsystem.GetValue().Get();
    TSharedPtr<UE::ToolsetRegistry::FToolset> Candidate = MakeShared<FLocalVariableRawToolset>(State);
    State->Identity = Candidate.Get();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterToolset(Candidate)) { return false; }
    RegistrySubsystem = Subsystem.GetValue().Get(); Handler = Candidate; CallState = State; State->Active.Store(true);
    return true;
}

void FLocalVariableRawRegistration::Unregister()
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
