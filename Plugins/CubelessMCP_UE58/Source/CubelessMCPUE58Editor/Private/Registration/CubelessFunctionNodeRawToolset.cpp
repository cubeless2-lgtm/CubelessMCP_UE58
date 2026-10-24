// One original9 tool on the official raw FToolset seam. No generic evaluator.
#include "Registration/CubelessFunctionNodeRawToolset.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogCubelessFunctionNodeRaw, Log, All);

namespace Cubeless::Registration
{
struct FFunctionNodeRawCallState
{
    TAtomic<bool> Active { false };
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    const UE::ToolsetRegistry::FToolset* Identity = nullptr;
};
namespace
{
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* ToolsetName = TEXT("CubelessMCPUE58Editor.CubelessFunctionNodeTools");
constexpr const TCHAR* MethodName = TEXT("add_blueprint_function_node");
constexpr const TCHAR* ResultPrefix = TEXT("__CUBELESS_FUNCTION_NODE_RESULT__");
constexpr int32 MaxInputBytes = 131072, MaxResultBytes = 16 * 1024 * 1024;

FString Schema()
{
    // Explicit raw schema, not a reflected property TYPE schema or Duplicate callback.
    // params is original unrestricted JSON; native still has a finite function/graph slice.
    return FString(TEXT("{\"name\":\"CubelessMCPUE58Editor.CubelessFunctionNodeTools\",\"version\":\"1.0\",\"description\":\"Original function-node9 with guarded native getter/physics/self paths and additional KismetMath/String/System/Text static library short-name routes.\",\"tools\":[{\"name\":\"CubelessMCPUE58Editor.CubelessFunctionNodeTools.add_blueprint_function_node\",\"description\":\"The four short native Kismet library routes now preserve create_graph_if_missing through the existing shared graph-creating AddCall worker. Already existing graphs reuse that worker; missing bounded own function graphs on admitted Actor/ActorComponent/SceneComponent Blueprints can receive supported primitive by-value static calls/defaults. Original conversion/ignore rules, source/target and outer transaction endpoint guards, actual graph/node/default partial prefixes and no automatic retry remain. WorldContext, other owners/instance/generated/own missing-graph routes and broader defaults remain incomplete or usage pending. Additional known native KismetMathLibrary, KismetStringLibrary, KismetSystemLibrary and KismetTextLibrary short names resolve to the existing independently admitted AddCall catalogue. Native FName function lookup preserves canonical case, original scalar/Vector params conversion and unknown/nonobject/array ignore rules through shared OwnDefaults; source/target lifetimes, actual partial/unknown state and outer transaction endpoint remain guarded. No runtime function is executed or compile/save added. The full library catalogue, arbitrary/inherited/fallback/reference/container/enum/class defaults remain incomplete or usage pending. Create one native GameplayStatics.GetPlayerController/GetPlayerPawn/GetPlayerCharacter or Actor K2_GetActorLocation/K2_GetActorRotation/GetActorScale3D node in an existing owned runt"))
        + TEXT("ime Blueprint instance graph. Actor getters require the original target Actor class short name and reuse audited native instance/self, Vector/Rotator return and callback checks. Original whole nonobject params and unknown inputs are ignored; Actor self string/array/object/null values are also ignored as in the original value-kind dispatch, while numeric/bool self raw defaults remain unsupported. Existing PlayerIndex rounding and float32 position projection remain source-qualified. Explicit PrimitiveComponent or UPrimitiveComponent target also authors native AddForce/AddImpulse/AddTorqueInRadians/SetMassScale through the same checked node writer; the U prefix is a narrow owner-alias defect correction. Original unknown/nonobject/null/object params and short Vector arrays are ignored, as are BoneName strings. Vector arrays use original AsNumber to float32 and six-decimal X/Y/Z literal conversion; bool strings use original AsBool. Mass numbers preserve the original float32/six-decimal parsed value while actual checked pin text may differ and is reported separately. Numeric/bool name literals are adapted; unsafe raw-default bypasses and duplicate actual pin writes remain unsupported. A successful core is observed across outer transaction destruction; failed or unknown core prefixes are retained with unknown outer endpoint and no retry. Exact self (case-insensitive) or empty target additionally calls an already compiled own nonoverride function, or an exact own non-RPC primitive Custom Event, on an already-loaded normal Blueprint in an existing graph, with source-qualified original JSON-kind defaults for by-value int32, real, bool, Name, String and exact Vector inputs. Unknown/output keys, whole nonobject params, null/object values, Name/String strings, real strings under mod")
        + TEXT("ern PC_Real, and mismatching arrays retain original ignored behavior. Number/string int32 rounding, float32 real numbers, bool/string AsBool and numeric/bool Name/String literals are adapted; exact three-item Vector arrays use original float32/six-decimal conversion. Unsafe raw-default bypass, case-equivalent duplicate writes, overflow, references/containers and other value structures remain explicit limitations. Shared AddCall independently verifies the current own function/GUID/signature, applied defaults and actual returned prefix. Own Custom Event calls additionally require matching declared event data outputs and compiled caller inputs with by-value primitive types, exact local event identity and current source checks. Struct/ref/container event parameters, wider RPC/override/inherited calls, arbitrary failed-target fallback, graph creation and runtime execution remain outside that own slice. No graph creation, compile/save, runtime getter or force execution, broad self/component inference or full original pipeline claim. Own compiled nonoverride CustomEvent calls additionally admit exact server/client/multicast RPC flags with optional Reliable, for zero arguments or the existing by-value primitive signature profile. Authored, generated and skeleton net masks must agree; every new RPC uses the existing source/current lease. Existing defaults, partial outcomes and API compile/save0 are preserved. This authors a call node without executing networking or changing replication. External/inherited/override, wider flags and complex/ref/container RPC remain incomplete; actual use and persistence are validation pending.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"target\":{\"type\":\"string\"},\"function_name\":{\"type\":\"string\"},\"params\":{\"default\":null},\"node_position\":{\"type\":[\"array\",\"null\"],\"items\":{\"type\":\"number\"},\"default\":null},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"create_graph_if_missing\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\",\"target\",\"function_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"}")
        + TEXT(",\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}}]}");
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

FOutput Run(const FString& Input, const TSharedRef<FFunctionNodeRawCallState, ESPMode::ThreadSafe>& State)
{
    if (!IsInGameThread() || IsEngineExitRequested() || !State->Active.Load())
    { return MakeError(FString(TEXT("function_node_raw_not_dispatched: game-thread/live owned handler required."))); }
    auto Subsystem = State->Subsystem.Get();
    if (!Subsystem || Subsystem->ToolsetRegistry.Find(ToolsetName).Get() != State->Identity)
    { return MakeError(FString(TEXT("function_node_raw_not_dispatched: exact retained Registry handler is unavailable or filtered."))); }
    IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
    if (!Python || !Python->IsPythonAvailable() || !Python->IsPythonInitialized())
    { return MakeError(FString(TEXT("function_node_raw_not_dispatched: existing initialized Python required; no load/force-enable/fallback."))); }
    const FTCHARToUTF8 UTF8(*Input, Input.Len());
    if (UTF8.Length() > MaxInputBytes)
    { return MakeError(FString(TEXT("function_node_raw_not_dispatched: raw request exceeds128KiB UTF8 limit."))); }
    const FUTF8ToTCHAR InputRoundTrip(reinterpret_cast<const UTF8CHAR*>(UTF8.Get()), UTF8.Length());
    if (FString(InputRoundTrip.Length(), InputRoundTrip.Get()) != Input)
    { return MakeError(FString(TEXT("function_node_raw_not_dispatched: input is not lossless valid Unicode."))); }
    const FString Encoded = FBase64::Encode(reinterpret_cast<const uint8*>(UTF8.Get()), UTF8.Length());
    const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
    FPythonCommandEx Command; Command.Flags = EPythonCommandFlags::Unattended;
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    Command.Command = FString::Printf(TEXT("from cubeless_mcp_ue58.registration.raw_function_node import emit_result as _cubeless_function_node_emit; _cubeless_function_node_emit('%s','%s')"), *Encoded, *Nonce);
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
    { return MakeError(FailureReceipt(Command, Executed, TEXT("function_node_raw_return_unverified: missing/multiple/invalid/oversized exact nonce marker."))); }
    const FUTF8ToTCHAR Decoded(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
    const FString Result(Decoded.Length(), Decoded.Get()); const FTCHARToUTF8 RoundTrip(*Result, Result.Len());
    if (RoundTrip.Length() != Bytes.Num() || FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) != 0 || !TypedResult(Result))
    { return MakeError(FailureReceipt(Command, Executed, TEXT("function_node_raw_return_unverified: emitted bytes are not exact UTF8/standard typed returnValue JSON."))); }
    // A valid emitted receipt remains authoritative even if later Python logging
    // reported an exception. Never synthesize known0 after Python dispatch.
    if (!Executed) { UE_LOG(LogCubelessFunctionNodeRaw, Warning, TEXT("Function-node Python statement reported failure after a valid operation receipt; returned receipt preserved.")); }
    return MakeValue(Result);
}

class FFunctionNodeRawToolset final : public UE::ToolsetRegistry::FToolset
{
public:
    explicit FFunctionNodeRawToolset(const TSharedRef<FFunctionNodeRawCallState, ESPMode::ThreadSafe>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return ToolsetName; }
    virtual FString GetToolsetVersion() const override { return TEXT("1.0"); }
    virtual FString GetToolsetDescription() const override { return TEXT("One original function-node9 with intact raw JSON and a fixed named journaled action."); }
protected:
    virtual FString GetJsonSchemaInternal() const override { return Schema(); }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Name, const FString& Input) override
    {
        if (Name != MethodName) { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("function_node_raw_not_dispatched: exact named method required.")))).GetFuture(); }
        const FTCHARToUTF8 InputBytes(*Input, Input.Len());
        if (InputBytes.Length() > MaxInputBytes)
        { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("function_node_raw_not_dispatched: raw request exceeds128KiB UTF8 limit.")))).GetFuture(); }
        // Capture only strings and shared lifetime state; no UObject is accessed off the game thread.
        if (IsInGameThread()) { return MakeFulfilledPromise<FOutput>(Run(Input, State)).GetFuture(); }
        auto Promise = MakeShared<TPromise<FOutput>, ESPMode::ThreadSafe>(); auto Future = Promise->GetFuture();
        const auto LiveState = State;
        AsyncTask(ENamedThreads::GameThread, [Promise, LiveState, Input]() { Promise->SetValue(Run(Input, LiveState)); });
        return Future;
    }
private:
    TSharedRef<FFunctionNodeRawCallState, ESPMode::ThreadSafe> State;
};
}

bool FFunctionNodeRawRegistration::Register()
{
    if (!IsInGameThread() || !UObjectInitialized() || IsEngineExitRequested()) { return false; }
    if (Handler.IsValid())
    { auto Subsystem = RegistrySubsystem.Get(); return Subsystem && Subsystem->ToolsetRegistry.Find(ToolsetName) == Handler; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless original function-node raw registration"));
    if (!Subsystem.HasValue()) { return false; }
    if (Subsystem.GetValue()->ToolsetRegistry.Find(ToolsetName).IsValid()) { return false; } // Never replace any existing handler.
    auto State = MakeShared<FFunctionNodeRawCallState, ESPMode::ThreadSafe>(); State->Subsystem = Subsystem.GetValue().Get();
    TSharedPtr<UE::ToolsetRegistry::FToolset> Candidate = MakeShared<FFunctionNodeRawToolset>(State);
    State->Identity = Candidate.Get();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterToolset(Candidate)) { return false; }
    RegistrySubsystem = Subsystem.GetValue().Get(); Handler = Candidate; CallState = State; State->Active.Store(true);
    return true;
}

void FFunctionNodeRawRegistration::Unregister()
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
